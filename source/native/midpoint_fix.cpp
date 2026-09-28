#include "midpoint_fix.h"
#include "quality_fix.h"
#include "ptx_lowering.h"
#include "image_kernels.h"
#include "scatter_experiment.h"
#include "input_motion_experiment.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
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
constexpr uint32_t kArchParked = 122;
constexpr uint64_t kUncompressedFlags = 0x41;

// SM the rebuilt and retargeted PTX is compiled for by the driver: Ada by
// default, Ampere (86) or Turing (75) when the provider runs below Ada.
std::atomic<uint32_t> gTargetSm{kAdaArch};
std::atomic<bool> gOptimizedKernels{true};
// Set once a provider kernel matched the exact 310.9.1 source it was derived from.
std::atomic<bool> gExactProviderKernels{false};

inline std::string GetPtxEntryName(std::string_view ptx)
{
    size_t pos = ptx.find(".entry ");
    if (pos == std::string_view::npos) return {};
    pos += sizeof(".entry ") - 1;
    size_t end = pos;
    while (end < ptx.size() && ptx[end] != '(' && ptx[end] != ' ' && ptx[end] != '\r' && ptx[end] != '\n') ++end;
    return std::string(ptx.substr(pos, end - pos));
}

// Rewrites the PTX .target directive to the target SM. The rebuilt entries
// are uncompressed, so the directive may change length.
inline bool RetargetPtxText(std::string& text, uint32_t sm)
{
    const size_t directive = text.find(".target sm_");
    if (directive == std::string::npos) return false;
    size_t end = directive + sizeof(".target ") - 1;
    while (end < text.size() && text[end] != '\r' && text[end] != '\n' && text[end] != ' ' && text[end] != ',')
        ++end;
    text.replace(directive, end - directive, ".target sm_" + std::to_string(sm));
    return true;
}

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
        const size_t entry_remain = fat_size - p;
        if (hdr < 64 || payload == 0 || hdr > entry_remain || payload > entry_remain - hdr) return false;
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
        const size_t entry_remain = fat_size - p;
        if (hdr < 64 || payload == 0 || hdr > entry_remain || payload > entry_remain - hdr) return false;
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

    const uint32_t target_sm = gTargetSm.load(std::memory_order_relaxed);
    if (target_sm != kAdaArch)
    {
        std::string text(patched.begin(), patched.end());
        if (!RetargetPtxText(text, target_sm))
        {
            why = "temporal PTX has no .target directive";
            return false;
        }
        patched.assign(text.begin(), text.end());
    }

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
    std::memcpy(out.data() + entry + 28, &target_sm, sizeof(target_sm));
    const uint64_t outer = final_size - kOuterHeader;
    std::memcpy(out.data() + 8, &outer, sizeof(outer));
    return true;
}

void Log(const wchar_t* format, ...);
static std::atomic<bool> gMvDilationDisabled{false};
static std::atomic<bool> gQualityFixEnabled{true};
static std::atomic<bool> gQualityPolicyExplainedWarp{true};

inline bool BuildBlackwellTransfusionFatbin(const uint8_t* fat, size_t fat_size,
                                           const char* kernel_name,
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

    std::string ptx_text(reinterpret_cast<const char*>(bw_ptx.data()), bw_ptx.size());
    if (kernel_name && std::strcmp(kernel_name, "Kernel_BlendCandidatesFused") == 0
        && gQualityFixEnabled.load(std::memory_order_relaxed))
    {
        std::string quality_status;
        const bool patched = quality_fix::Patch(ptx_text, quality_status,
            gQualityPolicyExplainedWarp.load(std::memory_order_relaxed)
                ? quality_fix::Policy::ExplainedWarp : quality_fix::Policy::Transfusion);
        Log(L"Quality PTX preparation: %hs (patched=%d)", quality_status.c_str(), patched);
    }
    if (kernel_name && std::strcmp(kernel_name, "Kernel_EstimateIntermMvecsScatter") == 0
        && scatter_experiment::kMode != 0 && scatter_experiment::kMode != 7)
    {
        std::string experimentStatus;
        const bool patched = scatter_experiment::Patch(ptx_text, experimentStatus);
        Log(L"Scatter experiment: %hs; %hs (prepared=%d)", scatter_experiment::kName, experimentStatus.c_str(), patched);
        if (!patched) { why = experimentStatus; return false; }
    }
    if (kernel_name && std::strcmp(kernel_name, "Kernel_InputMvecProcessing") == 0
        && scatter_experiment::kMode == 3)
    {
        std::string status;
        const bool patched = input_motion_experiment::Patch(ptx_text, status);
        Log(L"Input motion experiment: %hs; %hs (prepared=%d)", scatter_experiment::kName, status.c_str(), patched);
        if (!patched) { why = status; return false; }
    }
    const size_t target_pos = ptx_text.find(".target sm_120");
    if (target_pos == std::string::npos)
    {
        why = "could not find .target sm_120 in Blackwell PTX";
        return false;
    }

    size_t after = target_pos + sizeof(".target sm_120") - 1;
    while (after < ptx_text.size() && (ptx_text[after] == '\r' || ptx_text[after] == '\n' || ptx_text[after] == ' '))
        ++after;
    const uint32_t target_sm = gTargetSm.load(std::memory_order_relaxed);
    ptx_text.replace(target_pos, after - target_pos, ".target sm_" + std::to_string(target_sm) + "\n");

    // Preserve the provider's scatter rejection and spatial bounds checks.
    // The former regex bypass removed depth/motion consistency branches, not
    // texture address clamping, and did not establish which input was optical flow.
    // The bounded experiments preserve Blackwell temporal arithmetic.

    const uint32_t ada_hdr = ReadU32(fat + ada_entry + 4);
    const size_t padded = (ptx_text.size() + 7) & ~size_t{7};
    const size_t final_size = ada_entry + ada_hdr + padded;

    out.assign(fat, fat + ada_entry + ada_hdr);
    out.resize(final_size, 0);
    std::memcpy(out.data() + ada_entry + ada_hdr, ptx_text.data(), ptx_text.size());

    const uint64_t payload64 = padded;
    const uint32_t zero32 = 0;
    const uint64_t zero64 = 0;
    std::memcpy(out.data() + ada_entry + 8, &payload64, sizeof(payload64));
    std::memcpy(out.data() + ada_entry + 16, &zero32, sizeof(zero32));
    std::memcpy(out.data() + ada_entry + 40, &kUncompressedFlags, sizeof(kUncompressedFlags));
    std::memcpy(out.data() + ada_entry + 56, &zero64, sizeof(zero64));
    // The rewritten entry replaces the Ada PTX; label it for the target SM.
    std::memcpy(out.data() + ada_entry + 28, &target_sm, sizeof(target_sm));
    const uint64_t outer = final_size - kOuterHeader;
    std::memcpy(out.data() + 8, &outer, sizeof(outer));
    return true;
}

inline std::string GetFatbinKernelName(const uint8_t* fat, size_t fat_size)
{
    size_t bw_entry = 0;
    if (!FindBlackwellPtxEntry(fat, fat_size, bw_entry)) return {};
    const uint32_t bw_hdr = ReadU32(fat + bw_entry + 4);
    const uint32_t bw_comp = ReadU32(fat + bw_entry + 16);
    const uint64_t bw_raw = ReadU64(fat + bw_entry + 56);
    if (bw_comp == 0 || bw_raw == 0 || bw_raw > (8u << 20)) return {};
    std::vector<uint8_t> bw_ptx(static_cast<size_t>(bw_raw));
    if (!Lz4BlockDecompress(fat + bw_entry + bw_hdr, bw_comp, bw_ptx.data(), bw_ptx.size())) return {};

    std::string_view ptx_sv(reinterpret_cast<const char*>(bw_ptx.data()), bw_ptx.size());
    size_t entry_pos = ptx_sv.find(".entry ");
    if (entry_pos == std::string_view::npos) return {};
    entry_pos += sizeof(".entry ") - 1;
    while (entry_pos < ptx_sv.size() && (ptx_sv[entry_pos] == ' ' || ptx_sv[entry_pos] == '\t')) ++entry_pos;
    size_t name_end = entry_pos;
    while (name_end < ptx_sv.size() && ptx_sv[name_end] != '(' && ptx_sv[name_end] != '\r' && ptx_sv[name_end] != '\n' && ptx_sv[name_end] != ' ') ++name_end;
    return std::string(ptx_sv.substr(entry_pos, name_end - entry_pos));
}


// Retargets kernel containers in-place so the driver compiles their PTX for
// the target SM. The .target directive sits in an LZ4 literal run in every
// container of the supported providers, so a same-length rewrite is enough.
//
// Ada (Transfusion's original path): every container holding a Blackwell
// sm_120 PTX is relabelled sm_89 and its Ada images are parked at arch 122,
// so the driver JIT-compiles the Blackwell build instead of the Ada SASS.
//
// Below Ada no container has a usable image: the provider ships Ada/Blackwell
// SASS and PTX only. Every container therefore gets one PTX relabelled to the
// target SM (the Blackwell build when preferred and present, else the Ada
// PTX) and every other image parked, including the Ada-only network modules.
// Returns the number of retargeted kernel containers.
static unsigned int RetargetContainers(uint8_t* base, size_t image_size,
                                       uint16_t num_sections,
                                       const IMAGE_SECTION_HEADER* section,
                                       bool prefer_blackwell, uint32_t target_sm)
{
    unsigned int rewritten = 0;
    const bool pre_ada = target_sm < kAdaArch;
    char target[16]{};
    std::snprintf(target, sizeof(target), "sm_%u", target_sm);

    for (WORD i = 0; i < num_sections; ++i, ++section)
    {
        if ((section->Characteristics & IMAGE_SCN_MEM_READ) == 0) continue;
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) continue;
        if (section->VirtualAddress >= image_size) continue;
        const size_t available = image_size - section->VirtualAddress;
        const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));
        if (size < kOuterHeader) continue;
        uint8_t* start = base + section->VirtualAddress;

        for (uint8_t* c = start; c + kOuterHeader <= start + size;)
        {
            if (ReadU32(c) != kFatbinMagic)
            {
                ++c;
                continue;
            }
            const uint16_t headerSize = ReadU16(c + 6);
            const uint64_t fatSize = ReadU64(c + 8);
            const size_t remain = static_cast<size_t>((start + size) - c);
            if (headerSize != 0x10 || fatSize == 0 || remain < 16 || fatSize > remain - 16)
            {
                c += 4;
                continue;
            }

            const size_t totalContainerBytes = static_cast<size_t>(fatSize) + 16;
            struct Image { uint8_t* header; size_t hdr; size_t payload; uint16_t kind; uint32_t arch; };
            std::vector<Image> images;
            for (uint8_t* img = c + 16; img + 32 <= c + totalContainerBytes;)
            {
                const uint32_t imgHeader = ReadU32(img + 4);
                const uint64_t payload = ReadU64(img + 8);
                const size_t imgRemain = static_cast<size_t>((c + totalContainerBytes) - img);
                if (imgHeader < 32 || payload == 0 || imgHeader > imgRemain || payload > imgRemain - imgHeader)
                    break;
                images.push_back({img, imgHeader, static_cast<size_t>(payload), ReadU16(img), ReadU32(img + 28)});
                img += imgHeader + payload;
            }

            const Image* blackwell = nullptr;
            const Image* ada = nullptr;
            bool hasAdaImage = false;
            for (const auto& image : images)
            {
                if (image.kind == kPtxKind && image.arch == kBlackwellArch) blackwell = &image;
                if (image.kind == kPtxKind && image.arch == kAdaArch) ada = &image;
                if (image.arch == kAdaArch) hasAdaImage = true;
            }

            const Image* chosen = nullptr;
            if (!pre_ada)
                chosen = hasAdaImage ? blackwell : nullptr;
            else
                chosen = (prefer_blackwell && blackwell) ? blackwell : ada;

            if (chosen)
            {
                // Same-length directive rewrite: "sm_120" -> "sm_89 ", "sm_89" -> "sm_86".
                char from[24]{}, to[24]{};
                std::snprintf(from, sizeof(from), ".target sm_%u", chosen->arch);
                std::snprintf(to, sizeof(to), ".target %-*s", static_cast<int>(std::strlen(from) - 8), target);
                uint8_t* body = chosen->header + chosen->hdr;
                uint8_t* bodyEnd = body + chosen->payload;
                const size_t fromLength = std::strlen(from);
                auto at = std::search(body, bodyEnd, reinterpret_cast<const uint8_t*>(from),
                                      reinterpret_cast<const uint8_t*>(from) + fromLength);
                DWORD oldProtect = 0;
                if (at != bodyEnd && std::strlen(to) == fromLength
                    && VirtualProtect(c, totalContainerBytes, PAGE_READWRITE, &oldProtect))
                {
                    std::memcpy(at, to, fromLength);
                    std::memcpy(chosen->header + 28, &target_sm, sizeof(target_sm));
                    for (const auto& image : images)
                    {
                        if (&image == chosen) continue;
                        if (pre_ada || image.arch == kAdaArch)
                            std::memcpy(image.header + 28, &kArchParked, sizeof(kArchParked));
                    }
                    DWORD ignored = 0;
                    VirtualProtect(c, totalContainerBytes, oldProtect, &ignored);
                    FlushInstructionCache(GetCurrentProcess(), c, totalContainerBytes);
                    ++rewritten;
                    c += totalContainerBytes;
                    continue;
                }
            }
            c += 4;
        }
    }
    return rewritten;
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
    std::vector<void*> allocations;
};

std::recursive_mutex gMidpointMutex;
std::vector<ModuleMidpointRecord> gPatchedModules;
std::atomic<bool> gReady{false};
std::atomic<uint32_t> gFailureCode{0};
std::atomic<LogCallback> gLogCallback{nullptr};
std::atomic<bool> gBlackwellTransfusionEnabled{true};
std::atomic<bool> gBlackwellTransfusionActive{false};
std::atomic<size_t> gTransfusedFatbinCount{0};
std::atomic<size_t> gTransfusedDescriptorCount{0};

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

void SetOptimizedKernels(bool enabled) noexcept
{
    gOptimizedKernels.store(enabled, std::memory_order_relaxed);
}

bool OptimizedKernelsEnabled() noexcept
{
    return gOptimizedKernels.load(std::memory_order_relaxed);
}

bool ExactProviderKernels() noexcept
{
    return gExactProviderKernels.load(std::memory_order_acquire);
}

void SetTargetSm(uint32_t sm) noexcept
{
    gTargetSm.store(sm, std::memory_order_relaxed);
}

void SetQualityPolicyExplainedWarp(bool explainedWarp) noexcept
{
    gQualityPolicyExplainedWarp.store(explainedWarp, std::memory_order_relaxed);
}

void SetQualityFixEnabled(bool enabled) noexcept
{
    gQualityFixEnabled.store(enabled, std::memory_order_relaxed);
}

bool IsBlackwellTransfusionActive() noexcept
{
    return gBlackwellTransfusionActive.load(std::memory_order_acquire);
}

void SetMvDilationDisabled(bool disabled) noexcept
{
    gMvDilationDisabled.store(disabled, std::memory_order_relaxed);
}

bool IsMvDilationDisabled() noexcept
{
    return gMvDilationDisabled.load(std::memory_order_acquire);
}

size_t GetTransfusedFatbinCount() noexcept
{
    return gTransfusedFatbinCount.load(std::memory_order_relaxed);
}

size_t GetTransfusedDescriptorCount() noexcept
{
    return gTransfusedDescriptorCount.load(std::memory_order_relaxed);
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
    HMODULE vulkan = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    bool shouldFree = true;
    if (!vulkan)
    {
        vulkan = GetModuleHandleW(L"vulkan-1.dll");
        shouldFree = false;
    }
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
    if (shouldFree && vulkan)
        FreeLibrary(vulkan);
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

static bool SafeValidateFatbinCandidate(const uint8_t* candidate, size_t max_bytes, size_t& out_total) noexcept
{
    __try
    {
        if (ReadU32(candidate) != kFatbinMagic)
            return false;

        if (ReadU16(candidate + 6) != kOuterHeader)
            return false;

        const uint64_t declared = ReadU64(candidate + 8);
        if (declared > (16u << 20) || declared < (1024 - kOuterHeader))
            return false;

        const size_t total = static_cast<size_t>(declared) + kOuterHeader;
        if (total > max_bytes)
            return false;

        out_total = total;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool SafeCopyBlob(const void* blob, size_t size, std::vector<uint8_t>& out) noexcept
{
    __try
    {
        std::memcpy(out.data(), blob, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool PrepareModuleImage(const void* blob, size_t size, std::vector<uint8_t>& replacement) noexcept
{
    const uint32_t target_sm = gTargetSm.load(std::memory_order_relaxed);
    const bool lower = target_sm < 80;
    const bool optimize = gOptimizedKernels.load(std::memory_order_relaxed);
    if ((!lower && !optimize) || !blob) return false;
    try
    {
        // The in-place retarget cannot grow the compressed PTX, so the exact
        // image-kernel optimizations and Turing's lowering happen here, on the
        // image the provider hands to the driver.
        std::vector<uint8_t> fat(kOuterHeader);
        if (!SafeCopyBlob(blob, kOuterHeader, fat) || ReadU32(fat.data()) != kFatbinMagic
            || ReadU16(fat.data() + 6) != kOuterHeader)
            return false;
        // The size the provider passes is its descriptor's, which still
        // describes the original image when Transfusion redirected the pointer
        // to a larger rebuilt fatbin; the driver follows the fatbin header.
        (void)size;
        const uint64_t total = ReadU64(fat.data() + 8) + kOuterHeader;
        if (total > (16u << 20)) return false;
        fat.resize(static_cast<size_t>(total));
        if (!SafeCopyBlob(blob, fat.size(), fat)) return false;

        for (size_t p = kOuterHeader; p + 64 <= fat.size();)
        {
            const uint16_t kind = ReadU16(fat.data() + p);
            const uint32_t hdr = ReadU32(fat.data() + p + 4);
            const uint64_t payload = ReadU64(fat.data() + p + 8);
            if (hdr < 64 || payload == 0 || hdr > fat.size() - p || payload > fat.size() - p - hdr) return false;
            if (kind != kPtxKind || ReadU32(fat.data() + p + 28) != target_sm)
            {
                p += hdr + payload;
                continue;
            }

            const uint32_t compressed = ReadU32(fat.data() + p + 16);
            const uint64_t raw = ReadU64(fat.data() + p + 56);
            std::string text;
            if (compressed)
            {
                if (raw == 0 || raw > (8u << 20) || compressed > payload) return false;
                text.resize(static_cast<size_t>(raw));
                if (!Lz4BlockDecompress(fat.data() + p + hdr, compressed,
                        reinterpret_cast<uint8_t*>(text.data()), text.size()))
                    return false;
            }
            else
            {
                text.assign(reinterpret_cast<const char*>(fat.data() + p + hdr), static_cast<size_t>(payload));
            }
            while (!text.empty() && text.back() == '\0') text.pop_back();

            const char* optimized = nullptr;
            if (optimize)
            {
                const std::string entry = GetPtxEntryName(text);
                if (entry == "Kernel_OutputPull" || entry == "Kernel_OutputPushFine")
                {
                    const bool pull = entry == "Kernel_OutputPull";
                    if (image_kernels::SourceHash(text)
                        == (pull ? image_kernels::kOutputPullSourceHash : image_kernels::kOutputPushFineSourceHash))
                    {
                        gExactProviderKernels.store(true, std::memory_order_release);
                        if (image_kernels::kHaveReplacements)
                        {
                            text.assign(pull ? image_kernels::kOutputPull : image_kernels::kOutputPushFine);
                            RetargetPtxText(text, target_sm);
                            optimized = pull ? "OutputPull (packed row masks)" : "OutputPushFine (early block exit)";
                        }
                    }
                }
                else if (entry == "Kernel_BlendCandidatesFused" && image_kernels::VectorizeBlendStores(text))
                {
                    optimized = "BlendCandidatesFused (vector stores)";
                }
            }

            ptx_lowering::Stats stats;
            const bool lowered = lower && ptx_lowering::NeedsSm75Lowering(text);
            if (lowered && !ptx_lowering::LowerToSm75(text, stats))
            {
                Log(L"sm_75 lowering: unsupported instruction form; image left unchanged");
                return false;
            }
            if (!optimized && !lowered) return false;
            if (optimized) Log(L"Optimized image kernel: %hs for sm_%u", optimized, target_sm);

            // One uncompressed PTX entry for the target SM.
            const size_t padded = (text.size() + 1 + 7) & ~size_t{7};
            replacement.assign(fat.begin(), fat.begin() + kOuterHeader);
            replacement.insert(replacement.end(), fat.begin() + p, fat.begin() + p + hdr);
            replacement.resize(kOuterHeader + hdr + padded, 0);
            std::memcpy(replacement.data() + kOuterHeader + hdr, text.data(), text.size());
            uint8_t* entry = replacement.data() + kOuterHeader;
            const uint64_t payload64 = padded;
            const uint32_t zero32 = 0;
            const uint64_t zero64 = 0;
            std::memcpy(entry + 8, &payload64, sizeof(payload64));
            std::memcpy(entry + 16, &zero32, sizeof(zero32));
            std::memcpy(entry + 40, &kUncompressedFlags, sizeof(kUncompressedFlags));
            std::memcpy(entry + 56, &zero64, sizeof(zero64));
            const uint64_t outer = replacement.size() - kOuterHeader;
            std::memcpy(replacement.data() + 8, &outer, sizeof(outer));
            if (lowered)
                Log(L"sm_75 lowering: %zu mma, %zu cvt, %zu min/max rewritten (%zu -> %zu bytes of PTX)",
                    stats.mma, stats.cvt, stats.minmax, static_cast<size_t>(compressed ? raw : payload), text.size());
            return true;
        }
    }
    catch (...)
    {
    }
    return false;
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

    const IMAGE_SECTION_HEADER* first_section = section;

    auto* base = reinterpret_cast<uint8_t*>(module);
    const auto start = reinterpret_cast<uintptr_t>(base);

    std::map<const uint8_t*, std::vector<uint64_t*>> fatbin_slots;
    for (WORD i = 0; i < num_sections; ++i, ++section)
    {
        if ((section->Characteristics & IMAGE_SCN_MEM_READ) == 0) continue;
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) continue;
        if (section->VirtualAddress >= image_size) continue;
        uint8_t* sec = base + section->VirtualAddress;
        const size_t available = image_size - section->VirtualAddress;
        const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));
        for (size_t off = 0; off + sizeof(uint64_t) <= size; off += sizeof(uint64_t))
        {
            uint64_t value = 0;
            std::memcpy(&value, sec + off, sizeof(value));
            if (value < start || value >= start + image_size) continue;
            const size_t remaining = static_cast<size_t>((start + image_size) - value);
            if (remaining < 16) continue;

            const auto* candidate = reinterpret_cast<const uint8_t*>(value);
            size_t total = 0;
            if (!SafeValidateFatbinCandidate(candidate, remaining, total)) continue;

            fatbin_slots[candidate].push_back(reinterpret_cast<uint64_t*>(sec + off));
        }
    }

    struct TargetFatbin
    {
        const char* kernel_name;
        const char* short_name;
        const uint8_t* fat = nullptr;
        size_t fat_size = 0;
        std::vector<uint64_t*> slots;
    };

    TargetFatbin targets[] = {
        {"Kernel_EstimateIntermMvecsScatter", "IntermMvecsScatter"},
        {"Kernel_BlendCandidatesFused", "BlendCandidatesFused"},
    };

    const uint8_t* legacy_temporal_fat = nullptr;
    size_t legacy_temporal_fat_size = 0;
    const TemporalProfile* legacy_profile = nullptr;
    std::vector<uint64_t*> legacy_slots;

    for (const auto& pair : fatbin_slots)
    {
        const uint8_t* candidate = pair.first;
        const size_t total = static_cast<size_t>(ReadU64(candidate + 8)) + kOuterHeader;
        const auto* fat_profile = FindTemporalProfile(candidate, total);
        if (fat_profile != nullptr && legacy_profile == nullptr)
        {
            legacy_temporal_fat = candidate;
            legacy_temporal_fat_size = total;
            legacy_profile = fat_profile;
            legacy_slots = pair.second;
        }

        const std::string name = GetFatbinKernelName(candidate, total);
        if (!name.empty())
        {
            for (auto& target : targets)
            {
                if (target.fat == nullptr && name == target.kernel_name)
                {
                    target.fat = candidate;
                    target.fat_size = total;
                    target.slots = pair.second;
                    break;
                }
            }
        }
    }

    std::vector<DescriptorPatch> all_patches;
    std::vector<void*> all_allocations;
    size_t transfused_fatbin_count = 0;

    // Blackwell transfusion is activated when enabled and the primary scatter kernel is found (DLSS 310.9+)
    if (gBlackwellTransfusionEnabled.load(std::memory_order_relaxed) && targets[0].fat != nullptr)
    {
        for (const auto& target : targets)
        {
            if (std::strcmp(target.kernel_name, "Kernel_BlendCandidatesFused") == 0
                && !gQualityFixEnabled.load(std::memory_order_relaxed)) continue;
            // The quality target must never make an incomplete temporal patch ready.
            if (&target != &targets[0] && all_patches.empty()) break;
            if (target.fat == nullptr || target.slots.empty()) continue;

            std::vector<uint8_t> rebuilt;
            std::string why;
            if (BuildBlackwellTransfusionFatbin(target.fat, target.fat_size, target.kernel_name, rebuilt, why))
            {
                void* mem = VirtualAlloc(nullptr, rebuilt.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (mem)
                {
                    std::memcpy(mem, rebuilt.data(), rebuilt.size());
                    all_allocations.push_back(mem);

                    for (uint64_t* slot : target.slots)
                    {
                        DWORD old_protect = 0;
                        if (VirtualProtect(slot, sizeof(uint64_t), PAGE_READWRITE, &old_protect) == 0) continue;
                        all_patches.push_back({slot, *slot});
                        *slot = reinterpret_cast<uint64_t>(mem);
                        DWORD ignored = 0;
                        VirtualProtect(slot, sizeof(uint64_t), old_protect, &ignored);
                    }
                    transfused_fatbin_count++;
                    Log(L"D157 midpoint fix: redirected %zu %hs descriptor(s)", target.slots.size(), target.short_name);
                }
            }
            else
            {
                Log(L"D157 midpoint fix: Blackwell transfusion for %hs failed (%hs)", target.short_name, why.c_str());
            }
        }

        if (!all_patches.empty())
        {
            // Retarget all remaining containers in-place so the entire pipeline
            // (Kernel_BlendCandidatesFused, Pyramids, Tensor feeds, etc.) runs Blackwell code
            // compiled for the target SM; below Ada this also covers the Ada-only network.
            const uint32_t target_sm = gTargetSm.load(std::memory_order_relaxed);
            const unsigned int retargeted_count =
                RetargetContainers(base, image_size, num_sections, first_section, true, target_sm);

            const size_t total_descriptors = all_patches.size();
            gBlackwellTransfusionActive.store(true, std::memory_order_release);
            gTransfusedFatbinCount.store(retargeted_count > 0 ? retargeted_count : transfused_fatbin_count, std::memory_order_release);
            gTransfusedDescriptorCount.store(total_descriptors, std::memory_order_release);
            gPatchedModules.push_back({module, std::move(all_patches), std::move(all_allocations)});
            gReady.store(true, std::memory_order_release);
            gFailureCode.store(0, std::memory_order_release);

            Log(L"D157 midpoint fix: %u container(s) retargeted in-place (Blackwell preferred -> sm_%u), %zu descriptor(s) redirected; scatter=%hs in %s",
                retargeted_count, target_sm, total_descriptors, scatter_experiment::kPolicy, path ? path : L"");
            return true;
        }
    }
    else if (legacy_profile != nullptr && std::strcmp(legacy_profile->descriptor_name, "EstimateIntermMvecsScatter") != 0)
    {
        Log(L"D157 midpoint fix: module uses legacy %hs; Blackwell transfusion bypassed in favor of native Ada temporal patch",
            legacy_profile->descriptor_name);
    }

    // Fallback path: Ada temporal midpoint fix on EstimateIntermMvecsScatter / dlfg_kernel
    gBlackwellTransfusionActive.store(false, std::memory_order_release);
    gTransfusedFatbinCount.store(0, std::memory_order_release);
    gTransfusedDescriptorCount.store(0, std::memory_order_release);

    const bool has_temporal = legacy_temporal_fat != nullptr && !legacy_slots.empty() && legacy_profile != nullptr;
    std::vector<uint8_t> rebuilt;
    std::string why;
    const bool built = has_temporal
        && BuildTemporalFatbin(legacy_temporal_fat, legacy_temporal_fat_size, *legacy_profile, rebuilt, why);

    // Below Ada nothing in the provider is loadable as shipped. Retarget every
    // container (after the temporal rebuild read its original Ada PTX) even
    // when the midpoint fix fails, so native 2x frame generation still runs.
    const uint32_t target_sm = gTargetSm.load(std::memory_order_relaxed);
    if (target_sm < kAdaArch)
    {
        const unsigned int retargeted_count =
            RetargetContainers(base, image_size, num_sections, first_section, false, target_sm);
        Log(L"D157 midpoint fix: %u container(s) retargeted in-place (Ada PTX -> sm_%u) in %s",
            retargeted_count, target_sm, path ? path : L"");
    }

    if (!has_temporal)
    {
        Log(L"D157 midpoint fix: no supported temporal descriptor found in %s", path ? path : L"");
        gFailureCode.store(4, std::memory_order_release); // Layout
        return false;
    }

    if (!built)
    {
        Log(L"D157 midpoint fix: fatbin rebuild failed in %s (%hs)", path ? path : L"", why.c_str());
        gFailureCode.store(6, std::memory_order_release);
        return false;
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
    patches.reserve(legacy_slots.size());
    for (uint64_t* slot : legacy_slots)
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

    std::vector<void*> allocations;
    allocations.push_back(mem);
    gPatchedModules.push_back({module, std::move(patches), std::move(allocations)});
    gReady.store(true, std::memory_order_release);
    gFailureCode.store(0, std::memory_order_release);

    Log(L"D157 midpoint fix: redirected %zu %hs descriptor(s) in %s to temporal-corrected rebuild (%zu bytes)",
        legacy_slots.size(), legacy_profile->descriptor_name, path ? path : L"", rebuilt.size());
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
        for (void* alloc : rec.allocations)
        {
            if (alloc)
                VirtualFree(alloc, 0, MEM_RELEASE);
        }
        rec.allocations.clear();
    }
    gPatchedModules.clear();
    gReady.store(false, std::memory_order_release);
    gFailureCode.store(0, std::memory_order_release);
    gBlackwellTransfusionActive.store(false, std::memory_order_release);
    gTransfusedFatbinCount.store(0, std::memory_order_release);
    gTransfusedDescriptorCount.store(0, std::memory_order_release);
}

} // namespace midpoint_fix
