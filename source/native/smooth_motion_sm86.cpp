#include "smooth_motion_sm86.h"
#include "smooth_motion_fatbin.h"

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace smooth_motion_sm86
{
namespace
{
constexpr wchar_t kDriverRelativePath[] =
    L"\\DriverStore\\FileRepository\\nvami.inf_amd64_b259952749ea1a39\\NvPresent64.dll";
constexpr std::array<uint8_t, 32> kExpectedSha256 = {
    0x17, 0x16, 0xd1, 0x3d, 0x16, 0x93, 0x20, 0xa2,
    0xdc, 0x71, 0x48, 0x67, 0x33, 0x20, 0x2e, 0xd7,
    0x51, 0x27, 0xe8, 0xf3, 0x9c, 0xd6, 0x79, 0xa9,
    0x19, 0x38, 0x48, 0xfc, 0x1f, 0xa4, 0x5e, 0x7f
};
constexpr std::array<uint8_t, 32> kExpectedNvoglvSha256 = {
    0x68, 0xb2, 0xd0, 0xf8, 0x2e, 0x69, 0xe6, 0xbb,
    0x7a, 0xf6, 0x7e, 0xa5, 0x54, 0xc1, 0x1c, 0xab,
    0xa7, 0x83, 0xe2, 0xc7, 0x5f, 0x5a, 0xb4, 0x75,
    0xdb, 0x10, 0x8f, 0xf5, 0x5b, 0xf1, 0x31, 0x3c
};
constexpr uint64_t kNvPresentFileSize = 8525544;
constexpr uint64_t kNvoglvFileSize = 48639720;
constexpr size_t kMaxFatbin = 64u * 1024u * 1024u;
constexpr uint32_t kConfigRva = 0x7f0cd0;
constexpr uint32_t kCudaSlotRva = 0x7fb628;
constexpr uint32_t kGateImmediateRva = 0xc41f;
constexpr uint32_t kVulkanEnableBranchRva = 0xda33b9;
constexpr uint32_t kVulkanIntermediateBranchRva = 0xda33f0;
constexpr uint32_t kVulkanApiBranchRva = 0xda341f;

using CuModuleLoadData = int (WINAPI*)(void**, const void*);
using InitD3D = bool (WINAPI*)();
using ResolveCuda = int (WINAPI*)();

std::mutex gMutex;
LogCallback gLog = nullptr;
HMODULE gNvPresent = nullptr;
std::atomic<CuModuleLoadData> gOriginalCuda{nullptr};
void** gCudaSlot = nullptr;
uint8_t* gGate = nullptr;
uint8_t* gConfig = nullptr;
uint8_t gOriginalGate = 0;
uint8_t gOriginalOverride = 0;
uint8_t gOriginalEnabled = 0;
uint8_t gOriginalVulkan = 0;
uint8_t gOriginalD3D11 = 0;
uint8_t gOriginalD3D12 = 0;
std::atomic<bool> gActive{false};
ApiMode gMode = ApiMode::D3D12;
std::atomic<unsigned> gRewritten{0};
std::atomic<unsigned> gRejected{0};
std::atomic<bool> gVulkanGatePatched{false};
HMODULE gPatchedNvoglv = nullptr;

void Say(const wchar_t* message) noexcept
{
    if (gLog) gLog(message);
}

bool SafeCopy(void* destination, const void* source, size_t length) noexcept
{
    __try { std::memcpy(destination, source, length); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool Readable(const void* address, size_t length) noexcept
{
    auto* cursor = static_cast<const uint8_t*>(address);
    while (length)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(cursor, &info, sizeof(info)) || info.State != MEM_COMMIT
            || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto* end = static_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize;
        if (cursor >= end) return false;
        const size_t chunk = (std::min)(length, static_cast<size_t>(end - cursor));
        cursor += chunk;
        length -= chunk;
    }
    return true;
}

bool PatchByte(uint8_t* address, uint8_t expected, uint8_t replacement) noexcept
{
    if (*address != expected) return false;
    DWORD protection = 0;
    if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &protection)) return false;
    *address = replacement;
    FlushInstructionCache(GetCurrentProcess(), address, 1);
    DWORD ignored = 0;
    VirtualProtect(address, 1, protection, &ignored);
    return true;
}

bool HashFile(const wchar_t* path, uint64_t expectedSize,
    std::array<uint8_t, 32>& digest) noexcept
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(file, &size)
        && size.QuadPart == static_cast<LONGLONG>(expectedSize);
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectLength = 0, returned = 0;
    if (ok) ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &returned, 0) >= 0;
    std::array<uint8_t, 4096> object{};
    if (ok) ok = objectLength > 0 && objectLength <= object.size();
    if (ok) ok = BCryptCreateHash(algorithm, &hash, object.data(), objectLength, nullptr, 0, 0) >= 0;
    std::array<uint8_t, 64 * 1024> block{};
    while (ok)
    {
        DWORD read = 0;
        if (!ReadFile(file, block.data(), static_cast<DWORD>(block.size()), &read, nullptr)) { ok = false; break; }
        if (!read) break;
        ok = BCryptHashData(hash, block.data(), read, 0) >= 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return ok;
}

bool Signature(const uint8_t* base, size_t imageSize, size_t rva,
    const uint8_t* bytes, size_t size) noexcept
{
    return rva <= imageSize && size <= imageSize - rva
        && std::memcmp(base + rva, bytes, size) == 0;
}

bool ValidateModule(HMODULE module) noexcept
{
    const auto* base = reinterpret_cast<const uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000)
        return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return false;
    const size_t size = nt->OptionalHeader.SizeOfImage;
    if (size < kConfigRva + 0x12a6 || size < kCudaSlotRva + sizeof(void*)) return false;
    static constexpr uint8_t gate[] = {0x83, 0x79, 0x14, 0x03};
    static constexpr uint8_t setge[] = {0x40, 0x0f, 0x9d, 0xc6};
    static constexpr uint8_t getter[] = {0x8b, 0x41, 0x14, 0xc3};
    static constexpr uint8_t selector[] = {0x83, 0xbd, 0x50, 0x04, 0x00, 0x00, 0x03, 0x7c};
    static constexpr uint8_t resolver[] = {0x48, 0x83, 0xec, 0x28, 0x45, 0x33, 0xc9, 0x48};
    return Signature(base, size, 0xc41c, gate, sizeof(gate))
        && Signature(base, size, 0xc437, setge, sizeof(setge))
        && Signature(base, size, 0xbd10, getter, sizeof(getter))
        && Signature(base, size, 0x7ffce, selector, sizeof(selector))
        && Signature(base, size, 0x1348d0, resolver, sizeof(resolver))
        && GetProcAddress(module, "NVP_CreateSwapchain_D3D11") == reinterpret_cast<FARPROC>(base + 0x5990)
        && GetProcAddress(module, "NVP_CreateSwapchain_D3D12") == reinterpret_cast<FARPROC>(base + 0x59a0)
        && GetProcAddress(module, "NVP_Init_D3D") == reinterpret_cast<FARPROC>(base + 0x59e0)
        && GetProcAddress(module, "NVP_Init_Vulkan") == reinterpret_cast<FARPROC>(base + 0x5a50);
}

int WINAPI HookCuModuleLoadData(void** module, const void* image) noexcept
{
    const auto original = gOriginalCuda.load(std::memory_order_acquire);
    if (!original) return 200;
    uint8_t header[16]{};
    if (!image || !Readable(image, sizeof(header)) || !SafeCopy(header, image, sizeof(header)))
        return original(module, image);
    if (smooth_motion_fatbin::U32(header) != 0xba55ed50u)
        return original(module, image);
    const uint64_t total = smooth_motion_fatbin::U64(header + 8)
        + smooth_motion_fatbin::U16(header + 6);
    if (total < 16 || total > kMaxFatbin || !Readable(image, static_cast<size_t>(total)))
    {
        gRejected.fetch_add(1, std::memory_order_relaxed);
        Say(L"[SM86] Refused malformed or unreadable CUDA fatbin");
        return 200;
    }
    try
    {
        std::vector<uint8_t> source(static_cast<size_t>(total));
        if (!SafeCopy(source.data(), image, source.size())) return 200;
        std::vector<uint8_t> rewritten;
        std::string_view kernelName;
        const auto result = smooth_motion_fatbin::Rewrite(source.data(), source.size(), rewritten, &kernelName);
        if (result == smooth_motion_fatbin::Result::rejected)
        {
            gRejected.fetch_add(1, std::memory_order_relaxed);
            Say(L"[SM86] Refused FP8 or invalid CUDA fatbin");
            return 200;
        }
        if (result == smooth_motion_fatbin::Result::rewritten)
        {
            const unsigned index = gRewritten.fetch_add(1, std::memory_order_relaxed) + 1;
            const int status = original(module, rewritten.data());
            if (index <= 20 || status != 0)
            {
                wchar_t line[160]{};
                swprintf_s(line, L"[SM86] #%u %hs sm89->sm86 CUDA status=%d",
                    index, kernelName.data(), status);
                Say(line);
            }
            return status;
        }
        return original(module, image);
    }
    catch (...) { Say(L"[SM86] CUDA fatbin rewrite failed"); return 200; }
}

void Restore() noexcept
{
    if (gConfig)
    {
        gConfig[0x4c] = gOriginalOverride;
        gConfig[0xe9] = gOriginalEnabled;
        gConfig[0x4e] = gOriginalVulkan;
        gConfig[0x4f] = gOriginalD3D11;
        gConfig[0x50] = gOriginalD3D12;
    }
    if (gGate) PatchByte(gGate, 2, gOriginalGate);
    if (gCudaSlot)
        InterlockedCompareExchangePointer(gCudaSlot,
            reinterpret_cast<void*>(gOriginalCuda.load(std::memory_order_acquire)),
            reinterpret_cast<void*>(&HookCuModuleLoadData));
    gActive.store(false, std::memory_order_release);
    gConfig = nullptr;
    gGate = nullptr;
    gCudaSlot = nullptr;
}
}

bool Initialize(LogCallback log, ApiMode mode) noexcept
{
    std::lock_guard lock(gMutex);
    gLog = log;
    if (gActive.load(std::memory_order_acquire)) return mode == gMode;
    if (mode != ApiMode::D3D12 && mode != ApiMode::D3D11 && mode != ApiMode::Vulkan)
        return false;
    wchar_t path[32768]{};
    HMODULE loaded = GetModuleHandleW(L"NvPresent64.dll");
    if (loaded)
    {
        if (!GetModuleFileNameW(loaded, path, _countof(path))) return false;
    }
    else
    {
        wchar_t systemDirectory[MAX_PATH]{};
        if (!GetSystemDirectoryW(systemDirectory, _countof(systemDirectory))) return false;
        if (wcscpy_s(path, systemDirectory) || wcscat_s(path, kDriverRelativePath)) return false;
    }
    std::array<uint8_t, 32> digest{};
    if (!HashFile(path, kNvPresentFileSize, digest) || digest != kExpectedSha256)
    {
        Say(L"[SM86] NvPresent64.dll missing or SHA256 differs from inspected 617.14 build");
        return false;
    }
    wchar_t pathMessage[1024]{};
    swprintf_s(pathMessage, L"[SM86] Verified NvPresent path: %.940s", path);
    Say(pathMessage);
    gNvPresent = loaded ? loaded : LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    wchar_t loadedPath[32768]{};
    std::array<uint8_t, 32> loadedDigest{};
    if (!gNvPresent || !GetModuleFileNameW(gNvPresent, loadedPath, _countof(loadedPath))
        || !HashFile(loadedPath, kNvPresentFileSize, loadedDigest)
        || loadedDigest != kExpectedSha256
        || !ValidateModule(gNvPresent))
    {
        Say(L"[SM86] NvPresent signature validation failed");
        return false;
    }
    auto* base = reinterpret_cast<uint8_t*>(gNvPresent);
    const int resolved = reinterpret_cast<ResolveCuda>(base + 0x1348d0)();
    if (resolved != 0)
    {
        Say(L"[SM86] CUDA dispatch resolver failed");
        return false;
    }
    auto* slot = reinterpret_cast<void**>(base + kCudaSlotRva);
    void* original = *slot;
    HMODULE owner = nullptr;
    HMODULE cuda = GetModuleHandleW(L"nvcuda.dll");
    MEMORY_BASIC_INFORMATION info{};
    const bool hasOwner = original && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(original), &owner);
    const bool hasPage = original && VirtualQuery(original, &info, sizeof(info));
    wchar_t ownerPath[32768]{};
    if (hasOwner) GetModuleFileNameW(owner, ownerPath, _countof(ownerPath));
    std::wstring expectedCuda64 = loadedPath;
    const size_t lastSlash = expectedCuda64.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) expectedCuda64.resize(lastSlash + 1);
    expectedCuda64 += L"nvcuda64.dll";
    const bool trustedOwner = owner == cuda || _wcsicmp(ownerPath, expectedCuda64.c_str()) == 0;
    if (!original || !cuda || !hasOwner
        || !trustedOwner || !hasPage
        || !(info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
    {
        wchar_t detail[256]{};
        swprintf_s(detail, L"[SM86] CUDA slot=%p nvcuda=%p owner=%p protect=0x%lx export=%p",
            original, cuda, owner, info.Protect,
            cuda ? GetProcAddress(cuda, "cuModuleLoadData") : nullptr);
        Say(detail);
        if (ownerPath[0]) Say(ownerPath);
        Say(L"[SM86] CUDA load slot is not trusted executable driver code");
        return false;
    }
    gOriginalCuda.store(reinterpret_cast<CuModuleLoadData>(original), std::memory_order_release);
    gCudaSlot = slot;
    gGate = base + kGateImmediateRva;
    gConfig = base + kConfigRva;
    gOriginalGate = *gGate;
    gOriginalOverride = gConfig[0x4c];
    gOriginalEnabled = gConfig[0xe9];
    gOriginalVulkan = gConfig[0x4e];
    gOriginalD3D11 = gConfig[0x4f];
    gOriginalD3D12 = gConfig[0x50];
    // NvPresent retains our function pointer. A normal FreeLibrary of the
    // proxy must not unmap the hook while the game is still running.
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&HookCuModuleLoadData), &self))
    {
        Say(L"[SM86] Could not pin the proxy module");
        return false;
    }
    if (InterlockedCompareExchangePointer(slot, reinterpret_cast<void*>(&HookCuModuleLoadData), original) != original)
    {
        gCudaSlot = nullptr;
        Say(L"[SM86] CUDA load slot changed during installation");
        return false;
    }
    if (!PatchByte(gGate, 3, 2))
    {
        Say(L"[SM86] Gate patch failed");
        Restore();
        return false;
    }
    const auto applyConfig = [mode]() noexcept
    {
        gConfig[0x4c] = 1;
        gConfig[0xe9] = 1;
        gConfig[0x4e] = mode == ApiMode::Vulkan ? 1 : 0;
        gConfig[0x4f] = mode == ApiMode::D3D11 ? 1 : 0;
        gConfig[0x50] = mode == ApiMode::D3D12 ? 1 : 0;
    };
    applyConfig();
    // NVP_Init_Vulkan takes private layer callbacks. The NVIDIA Vulkan layer
    // must invoke it itself; calling it here with guessed arguments is unsafe.
    const bool initSucceeded = mode == ApiMode::Vulkan
        || reinterpret_cast<InitD3D>(base + 0x59e0)();
    applyConfig();
    if (!initSucceeded || (mode != ApiMode::Vulkan && !gConfig[0x12a5]))
    {
        Say(L"[SM86] NvPresent D3D gate did not activate");
        Restore();
        return false;
    }
    gMode = mode;
    gActive.store(true, std::memory_order_release);
    if (mode == ApiMode::D3D11)
        Say(L"[SM86] NvPresent 617.14 D3D11 backend initialized; frame generation unverified");
    else if (mode == ApiMode::Vulkan)
        Say(L"[SM86] Vulkan backend armed; NVIDIA present layer must initialize itself; generation unverified");
    else
        Say(L"[SM86] NvPresent 617.14 D3D12 backend initialized");
    return true;
}

bool Active() noexcept { return gActive.load(std::memory_order_acquire); }

bool TryForceVulkanProfileGate(HMODULE module) noexcept
{
    std::lock_guard lock(gMutex);
    if (!module || !gActive.load(std::memory_order_acquire) || gMode != ApiMode::Vulkan)
        return false;
    if (gVulkanGatePatched.load(std::memory_order_acquire))
        return module == gPatchedNvoglv;

    wchar_t nvoglvPath[32768]{}, nvPresentPath[32768]{};
    if (!GetModuleFileNameW(module, nvoglvPath, _countof(nvoglvPath))
        || !GetModuleFileNameW(gNvPresent, nvPresentPath, _countof(nvPresentPath)))
    {
        Say(L"[SM86] Vulkan profile gate refused: module path unavailable");
        return false;
    }
    const std::wstring driverPath(nvoglvPath), presentPath(nvPresentPath);
    const size_t driverSlash = driverPath.find_last_of(L"\\/");
    const size_t presentSlash = presentPath.find_last_of(L"\\/");
    if (driverSlash == std::wstring::npos || presentSlash == std::wstring::npos
        || _wcsicmp(driverPath.c_str() + driverSlash + 1, L"nvoglv64.dll") != 0
        || _wcsicmp(driverPath.substr(0, driverSlash).c_str(),
            presentPath.substr(0, presentSlash).c_str()) != 0)
    {
        Say(L"[SM86] Vulkan profile gate refused: not the matching DriverStore module");
        return false;
    }
    std::array<uint8_t, 32> digest{};
    if (!HashFile(nvoglvPath, kNvoglvFileSize, digest)
        || digest != kExpectedNvoglvSha256)
    {
        Say(L"[SM86] Vulkan profile gate refused: nvoglv64.dll SHA256 differs from inspected 617.14 build");
        return false;
    }
    auto* base = reinterpret_cast<uint8_t*>(module);
    IMAGE_DOS_HEADER dos{};
    if (!SafeCopy(&dos, base, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew < 0 || dos.e_lfanew > 0x1000)
        return false;
    IMAGE_NT_HEADERS64 nt{};
    if (!SafeCopy(&nt, base + dos.e_lfanew, sizeof(nt))
        || nt.Signature != IMAGE_NT_SIGNATURE
        || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC
        || nt.OptionalHeader.SizeOfImage <= kVulkanApiBranchRva + 2)
        return false;

    static constexpr uint8_t enableId[] = {0xba, 0xc0, 0x84, 0xd3, 0xb0};
    static constexpr uint8_t intermediateId[] = {0xba, 0xaf, 0x15, 0x9b, 0xb0};
    static constexpr uint8_t apiId[] = {0xba, 0x75, 0x08, 0xcc, 0xb0};
    static constexpr uint8_t enableBranch[] = {0x0f, 0x84, 0xdd, 0x00, 0x00, 0x00};
    static constexpr uint8_t intermediateBranch[] = {0x0f, 0x84, 0xa6, 0x00, 0x00, 0x00};
    static constexpr uint8_t apiBranch[] = {0x74, 0x7b};
    static constexpr uint8_t initCall[] = {0xff, 0xd0};
    const size_t size = nt.OptionalHeader.SizeOfImage;
    if (!Readable(base + 0xda33a4, 0xda3473 - 0xda33a4)
        || !Signature(base, size, 0xda33a4, enableId, sizeof(enableId))
        || !Signature(base, size, 0xda33cc, intermediateId, sizeof(intermediateId))
        || !Signature(base, size, 0xda3403, apiId, sizeof(apiId))
        || !Signature(base, size, kVulkanEnableBranchRva, enableBranch, sizeof(enableBranch))
        || !Signature(base, size, kVulkanIntermediateBranchRva,
            intermediateBranch, sizeof(intermediateBranch))
        || !Signature(base, size, kVulkanApiBranchRva, apiBranch, sizeof(apiBranch))
        || !Signature(base, size, 0xda3471, initCall, sizeof(initCall)))
    {
        Say(L"[SM86] Vulkan profile gate refused: expected instructions differ");
        return false;
    }

    auto* const first = base + kVulkanEnableBranchRva;
    auto* const last = first + sizeof(enableBranch);
    MEMORY_BASIC_INFORMATION page{};
    if (!VirtualQuery(first, &page, sizeof(page)) || page.State != MEM_COMMIT
        || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS))
        || last > static_cast<uint8_t*>(page.BaseAddress) + page.RegionSize)
    {
        Say(L"[SM86] Vulkan profile gate refused: code page is not safely mapped");
        return false;
    }
    DWORD oldProtection = 0;
    if (!VirtualProtect(first, static_cast<SIZE_T>(last - first),
        PAGE_EXECUTE_READWRITE, &oldProtection))
    {
        Say(L"[SM86] Vulkan profile gate refused: VirtualProtect failed");
        return false;
    }
    // Recheck after protection change. This runs on the first LoadLibrary
    // return, before the Vulkan loader can call the module's entry points.
    const bool unchanged = std::memcmp(first, enableBranch, sizeof(enableBranch)) == 0;
    if (unchanged)
    {
        std::memset(first, 0x90, sizeof(enableBranch));
        FlushInstructionCache(GetCurrentProcess(), first, static_cast<SIZE_T>(last - first));
    }
    DWORD ignored = 0;
    const bool restored = VirtualProtect(first, static_cast<SIZE_T>(last - first),
        oldProtection, &ignored) != 0;
    if (!unchanged || !restored)
    {
        if (unchanged && !restored)
        {
            std::memcpy(first, enableBranch, sizeof(enableBranch));
            FlushInstructionCache(GetCurrentProcess(), first, static_cast<SIZE_T>(last - first));
            VirtualProtect(first, static_cast<SIZE_T>(last - first), oldProtection, &ignored);
        }
        Say(L"[SM86] Vulkan profile gate refused: patch transaction did not complete");
        return false;
    }
    gPatchedNvoglv = module;
    gVulkanGatePatched.store(true, std::memory_order_release);
    Say(L"[SM86] Vulkan profile enable branch bypassed in memory for verified nvoglv64.dll 617.14; other gates preserved");
    return true;
}
}
