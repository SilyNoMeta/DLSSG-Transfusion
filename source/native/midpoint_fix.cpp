#include "midpoint_fix.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace midpoint_fix
{
namespace
{
constexpr uint32_t kFatbinMagic = 0xBA55ED50u;
constexpr size_t kOuterHeader = 16;
constexpr uint32_t kPtxKind = 1;
constexpr uint32_t kAdaArch = 89;
constexpr uint32_t kBlackwellArch = 120;
constexpr uint64_t kUncompressedFlags = 0x41;

struct TemporalProfile
{
    size_t ptx_bytes;
    const char* entry_name;
    const char* descriptor_name;
    ptrdiff_t entry_name_offset;
    ptrdiff_t descriptor_name_offset;
    const char* reg_decl;
    const char* patched_reg_decl;
    const char* param_signature;
    const char* temporal_input;
    const char* curr_to_prev;
    const char* prev_to_curr;
};

// Structural expectations:
// Profile 1: DLSS-G 3.x (Cyberpunk, etc., 310.4 - 310.8)
// Profile 2: DLSS-G 310.9+ / Blackwell-generation OTA models
// Profile 3: Early DLSS-G 310.1.0 - 310.3.0 (with register expansion)
constexpr TemporalProfile kTemporalProfiles[] = {
    {
        99362,
        "main_kernel",
        "dlfg_kernel",
        0x10,
        0x28,
        ".reg .f32 %f<1362>;",
        nullptr,
        ".param .align 8 .b8 main_kernel_param_0[144]",
        "ld.param.f32 %f134, [main_kernel_param_0+32];\r\n"
        "mov.f32 %f135, 0f3F800000;\r\n"
        "sub.ftz.f32 %f136, %f135, %f134;\r\n",
        "%f136",
        "%f134",
    },
    {
        99626,
        "Kernel_EstimateIntermMvecsScatter",
        "EstimateIntermMvecsScatter",
        0x10,
        -0x08,
        ".reg .f32 %f<1362>;",
        nullptr,
        ".param .align 8 .b8 Kernel_EstimateIntermMvecsScatter_param_0[144]",
        "ld.param.f32 %f134, [Kernel_EstimateIntermMvecsScatter_param_0+32];\r\n"
        "mov.f32 %f135, 0f3F800000;\r\n"
        "sub.ftz.f32 %f136, %f135, %f134;\r\n",
        "%f136",
        "%f134",
    },
    {
        210951,
        "main_kernel",
        "dlfg_kernel",
        0x10,
        0x28,
        ".reg .f32 %f<4010>;",
        ".reg .f32 %f<4013>;",
        ".param .f32 main_kernel_param_5",
        "ld.param.f32 %f4010, [main_kernel_param_5];\r\n"
        "mov.f32 %f4011, 0f3F800000;\r\n"
        "sub.ftz.f32 %f4012, %f4011, %f4010;\r\n",
        "%f4012",
        "%f4010",
    },
};

constexpr size_t kExpectedMidpoints = 104;
constexpr char kJoinLabel[] = "$L__BB0_3:";
constexpr char kMidpointBits[] = "0f3F000000";
constexpr char kMulPrefix[] = "mul.ftz.f32 ";

inline uint16_t ReadU16(const uint8_t* p)
{
    uint16_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

inline uint32_t ReadU32(const uint8_t* p)
{
    uint32_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

inline uint64_t ReadU64(const uint8_t* p)
{
    uint64_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

// Plain LZ4 block decompressor
inline bool Lz4BlockDecompress(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size)
{
    size_t in = 0;
    size_t out = 0;
    while (in < src_size)
    {
        const uint8_t token = src[in++];
        size_t literals = token >> 4;
        if (literals == 15)
        {
            uint8_t ext = 0;
            do
            {
                if (in >= src_size) return false;
                ext = src[in++];
                literals += ext;
            } while (ext == 0xFF);
        }
        if (literals > src_size - in || literals > dst_size - out) return false;
        std::memcpy(dst + out, src + in, literals);
        in += literals;
        out += literals;
        if (in == src_size) break;
        if (src_size - in < 2) return false;
        const size_t back = static_cast<size_t>(src[in]) | (static_cast<size_t>(src[in + 1]) << 8);
        in += 2;
        if (back == 0 || back > out) return false;
        size_t match = 4 + (token & 0x0F);
        if ((token & 0x0F) == 15)
        {
            uint8_t ext = 0;
            do
            {
                if (in >= src_size) return false;
                ext = src[in++];
                match += ext;
            } while (ext == 0xFF);
        }
        if (match > dst_size - out) return false;
        for (size_t i = 0; i < match; ++i) dst[out + i] = dst[out + i - back];
        out += match;
    }
    return in == src_size && out == dst_size;
}

inline bool FindAdaPtxEntry(const uint8_t* fat, size_t fat_size, size_t& entry_offset)
{
    if (fat_size < kOuterHeader || ReadU32(fat) != kFatbinMagic) return false;
    if (ReadU16(fat + 6) != kOuterHeader) return false;
    const uint64_t declared = ReadU64(fat + 8);
    if (declared + kOuterHeader != fat_size) return false;

    size_t p = kOuterHeader;
    while (p + 64 <= fat_size)
    {
        const uint32_t kind = ReadU16(fat + p);
        const uint32_t hdr = ReadU32(fat + p + 4);
        const uint64_t payload = ReadU64(fat + p + 8);
        if (hdr < 64 || payload == 0) return false;
        if (p + hdr + payload > fat_size) return false;
        if (kind == kPtxKind && ReadU32(fat + p + 28) == kAdaArch)
        {
            entry_offset = p;
            return true;
        }
        p += hdr + payload;
    }
    return false;
}

inline bool FindBlackwellPtxEntry(const uint8_t* fat, size_t fat_size, size_t& entry_offset)
{
    if (fat_size < kOuterHeader || ReadU32(fat) != kFatbinMagic) return false;
    if (ReadU16(fat + 6) != kOuterHeader) return false;
    const uint64_t declared = ReadU64(fat + 8);
    if (declared + kOuterHeader != fat_size) return false;

    size_t p = kOuterHeader;
    while (p + 64 <= fat_size)
    {
        const uint32_t kind = ReadU16(fat + p);
        const uint32_t hdr = ReadU32(fat + p + 4);
        const uint64_t payload = ReadU64(fat + p + 8);
        if (hdr < 64 || payload == 0) return false;
        if (p + hdr + payload > fat_size) return false;
        if (kind == kPtxKind && ReadU32(fat + p + 28) == kBlackwellArch)
        {
            entry_offset = p;
            return true;
        }
        p += hdr + payload;
    }
    return false;
}

inline bool BuildTemporalFatbin(const uint8_t* fat, size_t fat_size,
                                const TemporalProfile& profile,
                                std::vector<uint8_t>& out, std::string& why)
{
    size_t entry = 0;
    if (!FindAdaPtxEntry(fat, fat_size, entry))
    {
        why = "no sm_89 PTX entry";
        return false;
    }

    const uint32_t hdr = ReadU32(fat + entry + 4);
    const uint32_t compressed = ReadU32(fat + entry + 16);
    const uint64_t raw = ReadU64(fat + entry + 56);
    if (compressed == 0 || raw == 0 || raw > (8u << 20))
    {
        why = "PTX entry is not compressed as expected";
        return false;
    }
    if (raw != profile.ptx_bytes)
    {
        std::stringstream s;
        s << "PTX is " << raw << " bytes, expected " << profile.ptx_bytes;
        why = s.str();
        return false;
    }

    std::vector<uint8_t> ptx(static_cast<size_t>(raw));
    if (!Lz4BlockDecompress(fat + entry + hdr, compressed, ptx.data(), ptx.size()))
    {
        why = "LZ4 decompression failed";
        return false;
    }

    const std::string entry_signature = std::string(".entry ") + profile.entry_name + "(";
    const std::string ptx_text(reinterpret_cast<const char*>(ptx.data()), ptx.size());
    if (ptx_text.find(entry_signature) == std::string::npos ||
        (profile.param_signature && ptx_text.find(profile.param_signature) == std::string::npos) ||
        ptx_text.find(profile.reg_decl) == std::string::npos)
    {
        why = "temporal kernel signature changed";
        return false;
    }

    if (profile.patched_reg_decl != nullptr)
    {
        const size_t decl_pos = ptx_text.find(profile.reg_decl);
        if (decl_pos != std::string::npos)
        {
            std::memcpy(ptx.data() + decl_pos, profile.patched_reg_decl, std::strlen(profile.patched_reg_decl));
        }
    }

    const char* begin = reinterpret_cast<const char*>(ptx.data());
    const size_t n = ptx.size();
    const size_t label_len = sizeof(kJoinLabel) - 1;
    size_t label = SIZE_MAX;
    for (size_t i = 0; i + label_len <= n; ++i)
    {
        if (std::memcmp(begin + i, kJoinLabel, label_len) != 0) continue;
        if (label != SIZE_MAX)
        {
            why = "join label is not unique";
            return false;
        }
        label = i;
    }
    if (label == SIZE_MAX)
    {
        why = "join label not found";
        return false;
    }
    size_t insertion = label + label_len;
    while (insertion < n && begin[insertion] != '\n') ++insertion;
    if (insertion >= n)
    {
        why = "join label has no line end";
        return false;
    }
    ++insertion;

    const size_t mid_len = sizeof(kMidpointBits) - 1;
    const size_t mul_len = sizeof(kMulPrefix) - 1;
    std::vector<size_t> marks;
    marks.reserve(kExpectedMidpoints);
    for (size_t i = 0; i + mid_len < n; ++i)
    {
        if (std::memcmp(begin + i, kMidpointBits, mid_len) != 0) continue;
        if (begin[i + mid_len] != ';') continue;
        size_t line = i;
        while (line > 0 && begin[line - 1] != '\n') --line;
        if (i - line < mul_len) continue;
        if (std::memcmp(begin + line, kMulPrefix, mul_len) != 0) continue;
        marks.push_back(i);
    }
    if (marks.size() != kExpectedMidpoints)
    {
        std::stringstream s;
        s << "found " << marks.size() << " midpoint multiplies, expected " << kExpectedMidpoints;
        why = s.str();
        return false;
    }
    if (marks.front() <= insertion)
    {
        why = "first midpoint precedes the insertion point";
        return false;
    }

    const size_t temporal_input_len = std::strlen(profile.temporal_input);
    std::vector<uint8_t> patched;
    patched.reserve(n + temporal_input_len);
    auto append = [&patched](const void* p, size_t bytes) {
        const auto* b = static_cast<const uint8_t*>(p);
        patched.insert(patched.end(), b, b + bytes);
    };
    append(ptx.data(), insertion);
    append(profile.temporal_input, temporal_input_len);
    size_t src = insertion;
    const size_t half = kExpectedMidpoints / 2;
    for (size_t i = 0; i < marks.size(); ++i)
    {
        append(ptx.data() + src, marks[i] - src);
        const char* scale = (i < half) ? profile.curr_to_prev : profile.prev_to_curr;
        append(scale, std::strlen(scale));
        src = marks[i] + mid_len;
    }
    append(ptx.data() + src, n - src);

    const size_t padded = (patched.size() + 7) & ~size_t{7};
    const size_t final_size = entry + hdr + padded;

    out.assign(fat, fat + entry + hdr);
    out.resize(final_size, 0);
    std::memcpy(out.data() + entry + hdr, patched.data(), patched.size());

    const uint64_t payload64 = padded;
    const uint32_t zero32 = 0;
    const uint64_t zero64 = 0;
    std::memcpy(out.data() + entry + 8, &payload64, sizeof(payload64));
    std::memcpy(out.data() + entry + 16, &zero32, sizeof(zero32));
    std::memcpy(out.data() + entry + 40, &kUncompressedFlags, sizeof(kUncompressedFlags));
    std::memcpy(out.data() + entry + 56, &zero64, sizeof(zero64));
    const uint64_t outer = final_size - kOuterHeader;
    std::memcpy(out.data() + 8, &outer, sizeof(outer));
    return true;
}

inline bool BuildBlackwellTransfusionFatbin(const uint8_t* fat, size_t fat_size,
                                           std::vector<uint8_t>& out, std::string& why)
{
    size_t ada_entry = 0;
    if (!FindAdaPtxEntry(fat, fat_size, ada_entry))
    {
        why = "no sm_89 Ada PTX entry in fatbin";
        return false;
    }

    size_t bw_entry = 0;
    if (!FindBlackwellPtxEntry(fat, fat_size, bw_entry))
    {
        why = "no sm_120 Blackwell PTX entry in fatbin";
        return false;
    }

    const uint32_t bw_hdr = ReadU32(fat + bw_entry + 4);
    const uint32_t bw_comp = ReadU32(fat + bw_entry + 16);
    const uint64_t bw_raw = ReadU64(fat + bw_entry + 56);
    if (bw_comp == 0 || bw_raw == 0 || bw_raw > (8u << 20))
    {
        why = "Blackwell PTX entry is not compressed as expected";
        return false;
    }

    std::vector<uint8_t> bw_ptx(static_cast<size_t>(bw_raw));
    if (!Lz4BlockDecompress(fat + bw_entry + bw_hdr, bw_comp, bw_ptx.data(), bw_ptx.size()))
    {
        why = "Blackwell LZ4 decompression failed";
        return false;
    }

    constexpr char kTargetBw[] = ".target sm_120";
    constexpr char kTargetAda[] = ".target sm_89\n";
    constexpr size_t kTargetBwLen = sizeof(kTargetBw) - 1;

    const char* ptx_str = reinterpret_cast<const char*>(bw_ptx.data());
    const size_t ptx_size = bw_ptx.size();
    size_t target_pos = SIZE_MAX;
    for (size_t i = 0; i + kTargetBwLen <= ptx_size; ++i)
    {
        if (std::memcmp(ptx_str + i, kTargetBw, kTargetBwLen) == 0)
        {
            target_pos = i;
            break;
        }
    }

    if (target_pos == SIZE_MAX)
    {
        why = "could not find .target sm_120 in Blackwell PTX";
        return false;
    }

    std::vector<uint8_t> patched;
    patched.reserve(ptx_size + 16);
    patched.insert(patched.end(), bw_ptx.begin(), bw_ptx.begin() + target_pos);
    patched.insert(patched.end(), kTargetAda, kTargetAda + std::strlen(kTargetAda));
    size_t after = target_pos + kTargetBwLen;
    while (after < ptx_size && (ptx_str[after] == '\r' || ptx_str[after] == '\n' || ptx_str[after] == ' '))
        ++after;
    patched.insert(patched.end(), bw_ptx.begin() + after, bw_ptx.end());

    const uint32_t ada_hdr = ReadU32(fat + ada_entry + 4);
    const size_t padded = (patched.size() + 7) & ~size_t{7};
    const size_t final_size = ada_entry + ada_hdr + padded;

    out.assign(fat, fat + ada_entry + ada_hdr);
    out.resize(final_size, 0);
    std::memcpy(out.data() + ada_entry + ada_hdr, patched.data(), patched.size());

    const uint64_t payload64 = padded;
    const uint32_t zero32 = 0;
    const uint64_t zero64 = 0;
    std::memcpy(out.data() + ada_entry + 8, &payload64, sizeof(payload64));
    std::memcpy(out.data() + ada_entry + 16, &zero32, sizeof(zero32));
    std::memcpy(out.data() + ada_entry + 40, &kUncompressedFlags, sizeof(kUncompressedFlags));
    std::memcpy(out.data() + ada_entry + 56, &zero64, sizeof(zero64));
    const uint64_t outer = final_size - kOuterHeader;
    std::memcpy(out.data() + 8, &outer, sizeof(outer));
    return true;
}

inline const TemporalProfile* FindTemporalProfile(const uint8_t* fat, size_t fat_size)

{
    size_t entry = 0;
    if (!FindAdaPtxEntry(fat, fat_size, entry)) return nullptr;
    const uint64_t raw = ReadU64(fat + entry + 56);
    for (const auto& profile : kTemporalProfiles)
    {
        if (raw == profile.ptx_bytes) return &profile;
    }
    return nullptr;
}

inline bool PointsToCString(const uint8_t* base, size_t image_size, uint64_t value,
                            const char* expected)
{
    const auto start = reinterpret_cast<uintptr_t>(base);
    if (value < start || value >= start + image_size) return false;
    const char* s = reinterpret_cast<const char*>(value);
    const size_t len = std::strlen(expected);
    if (value + len + 1 > start + image_size) return false;
    return std::memcmp(s, expected, len + 1) == 0;
}

inline bool ReadRelativePointer(const uint8_t* base, size_t image_size, const uint8_t* slot,
                                ptrdiff_t displacement, uint64_t& value)
{
    const uintptr_t start = reinterpret_cast<uintptr_t>(base);
    const uintptr_t end = start + image_size;
    const uintptr_t address = reinterpret_cast<uintptr_t>(slot);
    uintptr_t field = address;
    if (displacement >= 0)
    {
        const auto distance = static_cast<uintptr_t>(displacement);
        if (distance > UINTPTR_MAX - address) return false;
        field = address + distance;
    }
    else
    {
        const auto distance = static_cast<uintptr_t>(-(displacement + 1)) + 1;
        if (distance > address) return false;
        field = address - distance;
    }
    if (field < start || field > end || sizeof(value) > end - field) return false;
    std::memcpy(&value, reinterpret_cast<const void*>(field), sizeof(value));
    return true;
}

struct DescriptorPatch
{
    uint64_t* slot;
    uint64_t original;
};

struct ModuleMidpointRecord
{
    HMODULE module;
    std::vector<DescriptorPatch> patches;
    void* allocation;
};

std::mutex gMidpointMutex;
std::vector<ModuleMidpointRecord> gPatchedModules;
std::atomic<bool> gReady{false};
std::atomic<uint32_t> gFailureCode{0};
std::atomic<LogCallback> gLogCallback{nullptr};
std::atomic<bool> gBlackwellTransfusionEnabled{true};
std::atomic<bool> gBlackwellTransfusionActive{false};

void Log(const wchar_t* format, ...)
{
    auto* callback = gLogCallback.load(std::memory_order_acquire);
    if (!callback) return;
    wchar_t buffer[1024]{};
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
    va_end(args);
    callback(buffer);
}

} // anonymous namespace

void SetLogCallback(LogCallback callback) noexcept
{
    gLogCallback.store(callback, std::memory_order_release);
}

void SetBlackwellTransfusionEnabled(bool enabled) noexcept
{
    gBlackwellTransfusionEnabled.store(enabled, std::memory_order_relaxed);
}

bool IsBlackwellTransfusionActive() noexcept
{
    return gBlackwellTransfusionActive.load(std::memory_order_acquire);
}

bool ObserveD3D12Device(void*) noexcept
{
    return true;
}

bool ObserveVulkanPhysicalDevice(void* physicalDevice) noexcept
{
    if (!physicalDevice)
        return false;

    struct VulkanPhysicalDeviceIdProperties
    {
        uint32_t sType = 1000071004u;
        uint32_t padding = 0;
        void* pNext = nullptr;
        std::array<uint8_t, 16> deviceUuid{};
        std::array<uint8_t, 16> driverUuid{};
        std::array<uint8_t, 8> deviceLuid{};
        uint32_t deviceNodeMask = 0;
        uint32_t deviceLuidValid = 0;
    };

    struct VulkanPhysicalDeviceProperties2
    {
        uint32_t sType = 1000059001u;
        uint32_t padding = 0;
        void* pNext = nullptr;
        alignas(8) std::array<uint8_t, 1024> properties{};
    };

    using GetPhysicalDeviceProperties2 = void (WINAPI*)(void*, VulkanPhysicalDeviceProperties2*);
    HMODULE vulkan = GetModuleHandleW(L"vulkan-1.dll");
    if (!vulkan)
        return false;
    auto* getProperties = reinterpret_cast<GetPhysicalDeviceProperties2>(
        GetProcAddress(vulkan, "vkGetPhysicalDeviceProperties2"));
    if (!getProperties)
        getProperties = reinterpret_cast<GetPhysicalDeviceProperties2>(
            GetProcAddress(vulkan, "vkGetPhysicalDeviceProperties2KHR"));

    if (getProperties)
    {
        VulkanPhysicalDeviceIdProperties identity{};
        VulkanPhysicalDeviceProperties2 properties{};
        properties.pNext = &identity;
        __try
        {
            getProperties(physicalDevice, &properties);
            if (identity.deviceLuidValid)
            {
                LUID luid{};
                std::memcpy(&luid, identity.deviceLuid.data(), sizeof(luid));
                Log(L"D157 Vulkan physical device observed: LUID=0x%08X%08X", luid.HighPart, luid.LowPart);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    return true;
}

bool AdapterVerified() noexcept
{
    return true;
}

bool Ready() noexcept
{
    return gReady.load(std::memory_order_acquire);
}

uint32_t FailureCode() noexcept
{
    return gFailureCode.load(std::memory_order_acquire);
}

static bool SafeGetDosAndNtHeaders(HMODULE module, size_t& outImageSize, uint16_t& outNumSections, const IMAGE_SECTION_HEADER*& outSections)
{
    if (!module) return false;
    __try
    {
        const auto* base = reinterpret_cast<const uint8_t*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 1024 * 1024)
            return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            return false;
        outImageSize = nt->OptionalHeader.SizeOfImage;
        outNumSections = nt->FileHeader.NumberOfSections;
        outSections = IMAGE_FIRST_SECTION(nt);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool PatchProvider(HMODULE module, const wchar_t* path) noexcept
{
    if (!module) return false;

    std::lock_guard lock(gMidpointMutex);
    for (const auto& rec : gPatchedModules)
    {
        if (rec.module == module)
            return true;
    }

    size_t image_size = 0;
    uint16_t num_sections = 0;
    const IMAGE_SECTION_HEADER* section = nullptr;
    if (!SafeGetDosAndNtHeaders(module, image_size, num_sections, section))
        return false;

    auto* base = reinterpret_cast<uint8_t*>(module);
    const auto start = reinterpret_cast<uintptr_t>(base);

    std::vector<uint64_t*> slots;
    const uint8_t* fat = nullptr;
    size_t fat_size = 0;
    const TemporalProfile* selected_profile = nullptr;

    for (WORD i = 0; i < num_sections; ++i, ++section)
    {
        if ((section->Characteristics & IMAGE_SCN_MEM_READ) == 0) continue;
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) continue;
        uint8_t* sec = base + section->VirtualAddress;
        const size_t size = section->Misc.VirtualSize;
        for (size_t off = 0; off + sizeof(uint64_t) <= size; off += sizeof(uint64_t))
        {
            uint64_t value = 0;
            std::memcpy(&value, sec + off, sizeof(value));
            if (value < start || value >= start + image_size) continue;
            const auto* candidate = reinterpret_cast<const uint8_t*>(value);
            if (ReadU32(candidate) != kFatbinMagic) continue;

            const TemporalProfile* name_profile = nullptr;
            for (const auto& profile : kTemporalProfiles)
            {
                uint64_t entry_name = 0;
                uint64_t desc_name = 0;
                if (!ReadRelativePointer(base, image_size, sec + off,
                                         profile.entry_name_offset, entry_name) ||
                    !ReadRelativePointer(base, image_size, sec + off,
                                         profile.descriptor_name_offset, desc_name))
                {
                    continue;
                }
                if (PointsToCString(base, image_size, entry_name, profile.entry_name) &&
                    PointsToCString(base, image_size, desc_name, profile.descriptor_name))
                {
                    name_profile = &profile;
                    break;
                }
            }
            if (name_profile == nullptr) continue;

            const uint64_t declared = ReadU64(candidate + 8);
            const size_t total = static_cast<size_t>(declared) + kOuterHeader;
            if (total < 1024 || total > (16u << 20)) continue;
            if (value + total > start + image_size) continue;
            const auto* fat_profile = FindTemporalProfile(candidate, total);
            if (fat_profile == nullptr || fat_profile != name_profile) continue;
            if (fat == nullptr)
            {
                fat = candidate;
                fat_size = total;
                selected_profile = fat_profile;
            }
            else if (candidate != fat)
            {
                continue;
            }
            slots.push_back(reinterpret_cast<uint64_t*>(sec + off));
        }
    }

    if (fat == nullptr || slots.empty() || selected_profile == nullptr)
    {
        Log(L"D157 midpoint fix: no supported temporal descriptor found in %s", path ? path : L"");
        gFailureCode.store(4, std::memory_order_release); // Layout
        return false;
    }

    std::vector<uint8_t> rebuilt;
    std::string why;
    bool transfusionApplied = false;

    if (gBlackwellTransfusionEnabled.load(std::memory_order_relaxed))
    {
        size_t bw_dummy = 0;
        if (FindBlackwellPtxEntry(fat, fat_size, bw_dummy))
        {
            if (BuildBlackwellTransfusionFatbin(fat, fat_size, rebuilt, why))
            {
                transfusionApplied = true;
                gBlackwellTransfusionActive.store(true, std::memory_order_release);
            }
            else
            {
                Log(L"D157 midpoint fix: Blackwell transfusion failed (%hs); falling back to Ada temporal patch", why.c_str());
            }
        }
    }

    if (!transfusionApplied)
    {
        gBlackwellTransfusionActive.store(false, std::memory_order_release);
        if (!BuildTemporalFatbin(fat, fat_size, *selected_profile, rebuilt, why))
        {
            Log(L"D157 midpoint fix: fatbin rebuild failed in %s (%hs)", path ? path : L"", why.c_str());
            gFailureCode.store(6, std::memory_order_release);
            return false;
        }
    }

    void* mem = VirtualAlloc(nullptr, rebuilt.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem)
    {
        Log(L"D157 midpoint fix: memory allocation failed for %s", path ? path : L"");
        gFailureCode.store(9, std::memory_order_release);
        return false;
    }
    std::memcpy(mem, rebuilt.data(), rebuilt.size());

    std::vector<DescriptorPatch> patches;
    patches.reserve(slots.size());
    for (uint64_t* slot : slots)
    {
        DWORD old_protect = 0;
        if (VirtualProtect(slot, sizeof(uint64_t), PAGE_READWRITE, &old_protect) == 0) continue;
        patches.push_back({slot, *slot});
        *slot = reinterpret_cast<uint64_t>(mem);
        DWORD ignored = 0;
        VirtualProtect(slot, sizeof(uint64_t), old_protect, &ignored);
    }

    if (patches.empty())
    {
        VirtualFree(mem, 0, MEM_RELEASE);
        Log(L"D157 midpoint fix: no descriptor slots writable in %s", path ? path : L"");
        gFailureCode.store(10, std::memory_order_release);
        return false;
    }

    gPatchedModules.push_back({module, std::move(patches), mem});
    gReady.store(true, std::memory_order_release);
    gFailureCode.store(0, std::memory_order_release);

    if (transfusionApplied)
    {
        Log(L"D157 midpoint fix: redirected %zu %hs descriptor(s) in %s to BLACKWELL TRANSFUSION (sm_120 -> sm_89, branchless, %zu bytes)",
            slots.size(), selected_profile->descriptor_name, path ? path : L"", rebuilt.size());
    }
    else
    {
        Log(L"D157 midpoint fix: redirected %zu %hs descriptor(s) in %s to temporal-corrected rebuild (%zu bytes)",
            slots.size(), selected_profile->descriptor_name, path ? path : L"", rebuilt.size());
    }
    return true;
}

void Restore() noexcept
{
    std::lock_guard lock(gMidpointMutex);
    for (auto& rec : gPatchedModules)
    {
        for (const auto& patch : rec.patches)
        {
            DWORD old_protect = 0;
            if (VirtualProtect(patch.slot, sizeof(uint64_t), PAGE_READWRITE, &old_protect))
            {
                *patch.slot = patch.original;
                DWORD ignored = 0;
                VirtualProtect(patch.slot, sizeof(uint64_t), old_protect, &ignored);
            }
        }
        if (rec.allocation)
        {
            VirtualFree(rec.allocation, 0, MEM_RELEASE);
            rec.allocation = nullptr;
        }
    }
    gPatchedModules.clear();
    gReady.store(false, std::memory_order_release);
    gBlackwellTransfusionActive.store(false, std::memory_order_release);
}

} // namespace midpoint_fix
