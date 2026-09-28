// Vulkan NVX transport (source/native/vulkan_nvx.h) without a GPU.
//
// The test executable plays the provider: it exports NVSDK_NGX_VULKAN_Init_Ext2
// and imports GetProcAddress, so Install redirects its own import. A fake
// driver stands behind the resolvers. Detours is stubbed: NGX's call to the
// detoured export is simulated by calling the thunk, which must reach the
// original with the caller's return address and wrapped resolvers.
#include <Windows.h>
#include <intrin.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

static int gFailures = 0;
#define CHECK(condition)                                                     \
    do                                                                       \
    {                                                                        \
        if (!(condition))                                                    \
        {                                                                    \
            std::printf("FAILED line %d: %s\n", __LINE__, #condition);       \
            ++gFailures;                                                     \
        }                                                                    \
    } while (false)

void Log(const wchar_t* format, ...)
{
    wchar_t line[1024]{};
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(line, _countof(line), _TRUNCATE, format, args);
    va_end(args);
    std::printf("%ls\n", line);
}

// Images the D3D12 path would rewrite start with 'R'; the replacement starts with 'X'.
namespace cu_module_hook
{
inline std::vector<uint8_t> rewritten{'X', 1, 2, 3};
inline const std::vector<uint8_t>* Replacement(const void* blob, uint32_t size)
{
    return size && static_cast<const uint8_t*>(blob)[0] == 'R' ? &rewritten : nullptr;
}
inline void SafeDescribe(const void*, uint32_t, wchar_t* out, size_t capacity)
{
    wcscpy_s(out, capacity, L"test image");
}
}

// Detours stub: attaching leaves the pointer on the original, as a
// trampoline would behave.
static int gDetourAttached = 0;
LONG DetourTransactionBegin() { return NO_ERROR; }
LONG DetourUpdateThread(HANDLE) { return NO_ERROR; }
LONG DetourAttach(PVOID*, PVOID) { ++gDetourAttached; return NO_ERROR; }
LONG DetourTransactionCommit() { return NO_ERROR; }
LONG DetourTransactionAbort() { return NO_ERROR; }

#include "../../source/native/vulkan_nvx.h"

// Fake driver.
struct Created
{
    const void* data = nullptr;
    size_t size = 0;
    const void* next = nullptr;
    int calls = 0;
};
static Created gCreated;
static bool gRejectRewritten = false;
static int gDriverLaunches = 0;

static int32_t __stdcall DriverCreateModule(void*, const vulkan_nvx::ModuleCreateInfo* info, const void*, void** module)
{
    ++gCreated.calls;
    gCreated.data = info->pData;
    gCreated.size = info->dataSize;
    gCreated.next = info->pNext;
    if (gRejectRewritten && static_cast<const uint8_t*>(info->pData)[0] == 'X') return -3;
    *module = reinterpret_cast<void*>(0x1234);
    return 0;
}
static void __stdcall DriverLaunch(void*, const void*) { ++gDriverLaunches; }
static void* __stdcall DriverDeviceResolver(void*, const char* name)
{
    if (std::strcmp(name, "vkCreateCuModuleNVX") == 0) return reinterpret_cast<void*>(&DriverCreateModule);
    if (std::strcmp(name, "vkCmdCuLaunchKernelNVX") == 0) return reinterpret_cast<void*>(&DriverLaunch);
    if (std::strcmp(name, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<void*>(&DriverDeviceResolver);
    return nullptr;
}
static void* __stdcall DriverInstanceResolver(void*, const char* name)
{
    if (std::strcmp(name, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<void*>(&DriverDeviceResolver);
    return nullptr;
}

extern "C" __declspec(dllexport) void* __stdcall vkGetDeviceProcAddr(void* device, const char* name)
{
    return DriverDeviceResolver(device, name);
}

// The provider's Init_Ext2: records what it received.
static void* gSeenReturn = nullptr;
static void* gSeenInstanceResolver = nullptr;
static void* gSeenDeviceResolver = nullptr;
static uint64_t gSeenArgs[9]{};
extern "C" __declspec(dllexport) __declspec(noinline) uint32_t __stdcall NVSDK_NGX_VULKAN_Init_Ext2(uint64_t app,
    const wchar_t* path, void* instance, void* physical, void* device, void* gipa, void* gdpa, uint32_t sdk,
    const void* features)
{
    gSeenReturn = _ReturnAddress();
    gSeenInstanceResolver = gipa;
    gSeenDeviceResolver = gdpa;
    const uint64_t args[9] = {app, reinterpret_cast<uint64_t>(path), reinterpret_cast<uint64_t>(instance),
        reinterpret_cast<uint64_t>(physical), reinterpret_cast<uint64_t>(device), reinterpret_cast<uint64_t>(gipa),
        reinterpret_cast<uint64_t>(gdpa), sdk, reinterpret_cast<uint64_t>(features)};
    std::memcpy(gSeenArgs, args, sizeof(args));
    return 0x51;
}

using InitFn = uint32_t(__stdcall*)(uint64_t, const wchar_t*, void*, void*, void*, void*, void*, uint32_t, const void*);

// NGX's call site. Its range bounds the return address the provider must see.
static void* volatile gCallerReturn = nullptr;
__declspec(noinline) static uint32_t CallInitLikeNgx(InitFn init)
{
    const uint32_t status = init(0x1111, L"path", reinterpret_cast<void*>(0x3333), reinterpret_cast<void*>(0x4444),
        reinterpret_cast<void*>(0x5555), reinterpret_cast<void*>(&DriverInstanceResolver),
        reinterpret_cast<void*>(&DriverDeviceResolver), 0x15, reinterpret_cast<void*>(0x9999));
    gCallerReturn = _ReturnAddress();
    return status;
}

static int32_t CreateThroughProvider(vulkan_nvx::Resolver resolver, const void* data, size_t size)
{
    const auto create = reinterpret_cast<vulkan_nvx::CreateModule>(resolver(nullptr, "vkCreateCuModuleNVX"));
    const vulkan_nvx::ModuleCreateInfo info{vulkan_nvx::kCuModuleCreateInfo, reinterpret_cast<void*>(0x77), size, data};
    void* module = nullptr;
    return create(nullptr, &info, nullptr, &module);
}

int main()
{
    const HMODULE self = GetModuleHandleW(nullptr);
    CHECK(vulkan_nvx::Install(self, L"test provider"));
    CHECK(gDetourAttached == 1);
    CHECK(g_vulkanNvxInitTrampolines[0] == reinterpret_cast<void*>(&NVSDK_NGX_VULKAN_Init_Ext2));
    CHECK(vulkan_nvx::Install(self, L"test provider")); // idempotent
    CHECK(gDetourAttached == 1);

    // NGX calls the detoured export.
    const uint32_t status = CallInitLikeNgx(reinterpret_cast<InitFn>(&VulkanNvxInitThunk0));
    CHECK(status == 0x51);
    const auto caller = reinterpret_cast<uintptr_t>(&CallInitLikeNgx);
    const auto seen = reinterpret_cast<uintptr_t>(gSeenReturn);
    CHECK(seen > caller && seen < caller + 512); // NGX's return address, not the thunk's
    CHECK(gSeenArgs[0] == 0x1111 && gSeenArgs[2] == 0x3333 && gSeenArgs[3] == 0x4444 && gSeenArgs[4] == 0x5555);
    CHECK(gSeenArgs[7] == 0x15 && gSeenArgs[8] == 0x9999);
    CHECK(std::wcscmp(reinterpret_cast<const wchar_t*>(gSeenArgs[1]), L"path") == 0);
    CHECK(gSeenInstanceResolver == vulkan_nvx::kResolverWrappers[0]);
    CHECK(gSeenDeviceResolver == vulkan_nvx::kResolverWrappers[1]);

    // The provider resolves NVX through the wrapped resolvers.
    const auto instanceResolver = reinterpret_cast<vulkan_nvx::Resolver>(gSeenInstanceResolver);
    const auto deviceResolver = reinterpret_cast<vulkan_nvx::Resolver>(gSeenDeviceResolver);
    CHECK(instanceResolver(nullptr, "vkGetDeviceProcAddr") == vulkan_nvx::kResolverWrappers[1]);
    CHECK(deviceResolver(nullptr, "vkCreateCuModuleNVX") == vulkan_nvx::kModuleWrappers[0]);
    CHECK(deviceResolver(nullptr, "vkCmdCuLaunchKernelNVX") == vulkan_nvx::kLaunchWrappers[0]);
    CHECK(deviceResolver(nullptr, "vkUnknown") == nullptr);

    // Images the D3D12 path rewrites are rewritten; the pNext chain is kept.
    const uint8_t rewritable[8] = {'R'};
    CHECK(CreateThroughProvider(deviceResolver, rewritable, sizeof(rewritable)) == 0);
    CHECK(gCreated.data == cu_module_hook::rewritten.data() && gCreated.size == cu_module_hook::rewritten.size());
    CHECK(gCreated.next == reinterpret_cast<void*>(0x77));

    // A refused rewrite falls back to the provider's image.
    gRejectRewritten = true;
    CHECK(CreateThroughProvider(deviceResolver, rewritable, sizeof(rewritable)) == 0);
    CHECK(gCreated.data == rewritable && gCreated.size == sizeof(rewritable));
    gRejectRewritten = false;

    // A fatbin redirected by Transfusion is passed with its header's size.
    std::vector<uint8_t> fatbin(4096, 0);
    const uint32_t magic = 0xBA55ED50u;
    const uint64_t declared = fatbin.size() - 16;
    std::memcpy(fatbin.data(), &magic, sizeof(magic));
    std::memcpy(fatbin.data() + 8, &declared, sizeof(declared));
    CHECK(CreateThroughProvider(deviceResolver, fatbin.data(), 1024) == 0);
    CHECK(gCreated.data == fatbin.data() && gCreated.size == fatbin.size());
    // A header declaring more than is readable keeps the provider's size.
    auto* page = static_cast<uint8_t*>(VirtualAlloc(nullptr, 2 * 4096, MEM_RESERVE, PAGE_NOACCESS));
    CHECK(page && VirtualAlloc(page, 4096, MEM_COMMIT, PAGE_READWRITE));
    std::memcpy(page, fatbin.data(), 16);
    const uint64_t pastCommitted = 2 * 4096 - 16;
    std::memcpy(page + 8, &pastCommitted, sizeof(pastCommitted));
    CHECK(CreateThroughProvider(deviceResolver, page, 1024) == 0);
    CHECK(gCreated.data == page && gCreated.size == 1024);
    CHECK(vulkan_nvx::resized.load() == 1 && vulkan_nvx::replaced.load() == 1);

    // Launches are forwarded and counted.
    const auto launch = reinterpret_cast<vulkan_nvx::LaunchKernel>(deviceResolver(nullptr, "vkCmdCuLaunchKernelNVX"));
    launch(nullptr, nullptr);
    launch(nullptr, nullptr);
    CHECK(gDriverLaunches == 2 && vulkan_nvx::launches.load() == 2);

    // The provider's GetProcAddress import is redirected.
    const auto viaImport = GetProcAddress(self, "vkGetDeviceProcAddr");
    CHECK(reinterpret_cast<void*>(viaImport) == vulkan_nvx::kResolverWrappers[2]);
    CHECK(GetProcAddress(self, "NVSDK_NGX_VULKAN_Init_Ext2") == reinterpret_cast<FARPROC>(&NVSDK_NGX_VULKAN_Init_Ext2));

    // Calls from outside the provider are forwarded untouched.
    const HMODULE owned = vulkan_nvx::providers[0].module.exchange(nullptr);
    CHECK(CreateThroughProvider(deviceResolver, rewritable, sizeof(rewritable)) == 0);
    CHECK(gCreated.data == rewritable && gCreated.size == sizeof(rewritable));
    CHECK(deviceResolver(nullptr, "vkCreateCuModuleNVX") == reinterpret_cast<void*>(&DriverCreateModule));
    launch(nullptr, nullptr);
    CHECK(gDriverLaunches == 3 && vulkan_nvx::launches.load() == 2);
    vulkan_nvx::providers[0].module.store(owned);

    // A fresh image at the same base (its export no longer carries the
    // detour) is hooked again, in the same slot.
    vulkan_nvx::providers[0].hooked[0] ^= 0xFF;
    CHECK(vulkan_nvx::Install(self, L"test provider"));
    CHECK(gDetourAttached == 2 && vulkan_nvx::providers[0].module.load() == self);
    CHECK(!vulkan_nvx::providers[0].initialized.load());

    // An unloaded provider frees its slot.
    vulkan_nvx::Forget(self);
    CHECK(!vulkan_nvx::providers[0].module.load() && !g_vulkanNvxInitTrampolines[0]);
    CHECK(vulkan_nvx::Install(self, L"test provider"));
    CHECK(gDetourAttached == 3 && vulkan_nvx::providers[0].module.load() == self);

    // NGX loads and unloads its candidate providers faster than the module
    // scan forgets them: with every slot held by an unloaded provider, a new
    // one reclaims a slot instead of going without transport.
    vulkan_nvx::Forget(self);
    static uint8_t unloadedExport[16] = {0xCC};
    for (size_t i = 0; i < vulkan_nvx::providers.size(); ++i)
    {
        vulkan_nvx::providers[i].module.store(reinterpret_cast<HMODULE>(0x10000 * (i + 1)));
        vulkan_nvx::providers[i].initExport = unloadedExport;
    }
    CHECK(vulkan_nvx::Stale(vulkan_nvx::providers[0]));
    CHECK(vulkan_nvx::Install(self, L"test provider"));
    CHECK(gDetourAttached == 4 && vulkan_nvx::providers[0].module.load() == self);
    CHECK(!vulkan_nvx::Stale(vulkan_nvx::providers[0]));

    std::printf(gFailures ? "vulkan_nvx: %d failure(s)\n" : "vulkan_nvx: all checks passed\n", gFailures);
    return gFailures ? 1 : 0;
}
