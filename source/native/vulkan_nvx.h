#pragma once
// Vulkan transport for the provider's kernels (VK_NVX_binary_import).
//
// In D3D12 the provider creates its CUDA modules through NvAPI, where
// cu_module_hook replaces the images. In Vulkan it uses vkCreateCuModuleNVX
// and vkCmdCuLaunchKernelNVX, resolved through the vkGetInstanceProcAddr and
// vkGetDeviceProcAddr it receives in NVSDK_NGX_VULKAN_Init_Ext2 or looks up
// itself with GetProcAddress. Both routes are wrapped, for the provider only:
//   - its GetProcAddress import is redirected (IAT of the provider module);
//   - its Init_Ext2 export is detoured to an assembly thunk that rewrites the
//     two resolver arguments in place and jumps to the original, so the
//     provider still sees NGX's return address (it validates its caller).
// Wrapped resolvers hand the provider our vkCreateCuModuleNVX, which gives the
// driver the same images as the D3D12 path (cu_module_hook::Replacement: sm_75
// lowering, exact image kernels), and our vkCmdCuLaunchKernelNVX, which only
// counts launches. Calls from any other module are forwarded unchanged.
//
// The in-place retarget and Blackwell Transfusion happen at provider load and
// do not depend on the API. Transfusion redirects descriptors to rebuilt,
// larger fatbins while the provider still passes the original size: NVX takes
// an explicit size, so a fatbin is always passed with the size its header
// declares. The network optimizer and its launch fusions are not ported: they
// hook NvAPI launches and stay inactive in Vulkan.
//
// Design of dlssg_for_sm86's Vulkan transport (src/companion/vulkan_transport.hpp,
// itself adapted from RTX-Unlocker-RenoDX 941a7f5, MIT), rewritten around this
// engine's images instead of precompiled, hash-matched kernels.
//
// Included after cu_module_hook.h and detours.h.
#include <intrin.h>

namespace vulkan_nvx
{
inline constexpr unsigned kSlots = 16;     // distinct resolvers / NVX entry points
inline constexpr unsigned kProviders = 4;  // provider modules with a detoured Init_Ext2
inline constexpr uint32_t kCuModuleCreateInfo = 1000029000; // VK_STRUCTURE_TYPE_CU_MODULE_CREATE_INFO_NVX

struct ModuleCreateInfo { uint32_t sType; const void* pNext; size_t dataSize; const void* pData; };
static_assert(sizeof(ModuleCreateInfo) == 32);

using Resolver = void*(__stdcall*)(void* object, const char* name);
using CreateModule = int32_t(__stdcall*)(void* device, const ModuleCreateInfo* info, const void* allocator, void** module);
using LaunchKernel = void(__stdcall*)(void* commandBuffer, const void* info);
using ProcAddress = FARPROC(WINAPI*)(HMODULE module, LPCSTR name);

inline std::array<std::atomic<void*>, kSlots> resolvers{}, modules{}, launchers{};
inline SRWLOCK bindLock = SRWLOCK_INIT;
inline std::atomic<uint32_t> accepted{0}, rejected{0}, replaced{0}, resized{0};
inline std::atomic<uint64_t> launches{0};
inline std::atomic<bool> slotsExhausted{false};

// Provider modules and their state. A slot keeps its module until the module
// is unloaded; Install re-checks the hooks on every inspection.
struct Provider
{
    std::atomic<HMODULE> module{nullptr};
    uint8_t* initExport = nullptr;
    std::array<uint8_t, 16> hooked{}; // Init_Ext2's first bytes once detoured
    std::atomic<bool> initialized{false};
};
inline std::array<Provider, kProviders> providers;
// What the provider imported, taken from its import table before redirection.
inline std::atomic<ProcAddress> realGetProcAddress{nullptr};
inline std::mutex installMutex;

inline bool SafeRead(const void* address, void* out, size_t size)
{
    SIZE_T read = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), address, out, size, &read) && read == size;
}

// Whether [address, address + size) is committed, readable memory.
inline bool Readable(const void* address, size_t size)
{
    auto cursor = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = cursor + size;
    if (!address || end < cursor) return false;
    constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ
        | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    while (cursor < end)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) || info.State != MEM_COMMIT
            || (info.Protect & kReadable) == 0 || (info.Protect & PAGE_GUARD) != 0)
            return false;
        cursor = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    }
    return true;
}

// Whether a provider was initialized for Vulkan (its Init_Ext2 was reached).
inline bool InUse()
{
    for (const auto& provider : providers)
        if (provider.initialized.load(std::memory_order_acquire) && provider.module.load(std::memory_order_acquire))
            return true;
    return false;
}

inline HMODULE ModuleOf(const void* address)
{
    MEMORY_BASIC_INFORMATION info{};
    return address && VirtualQuery(address, &info, sizeof(info)) ? static_cast<HMODULE>(info.AllocationBase) : nullptr;
}

inline bool FromProvider(const void* returnAddress)
{
    const HMODULE module = ModuleOf(returnAddress);
    if (!module) return false;
    for (const auto& provider : providers)
        if (provider.module.load(std::memory_order_acquire) == module) return true;
    return false;
}

inline void* Route(void* function, const char* name, bool provider);

// Returns the wrapper bound to `original` in `slots`, binding a free slot the
// first time. A wrapper passed back in is returned as is.
inline void* Bind(void* original, std::array<std::atomic<void*>, kSlots>& slots, void* const* wrappers)
{
    if (!original) return nullptr;
    AcquireSRWLockExclusive(&bindLock);
    void* result = nullptr;
    for (unsigned i = 0; i < kSlots && !result; ++i)
        if (original == wrappers[i] || original == slots[i].load(std::memory_order_acquire)) result = wrappers[i];
    for (unsigned i = 0; i < kSlots && !result; ++i)
        if (!slots[i].load(std::memory_order_acquire))
        {
            slots[i].store(original, std::memory_order_release);
            result = wrappers[i];
        }
    ReleaseSRWLockExclusive(&bindLock);
    if (!result && !slotsExhausted.exchange(true))
        Log(L"[VK-NVX] all %u slots in use: further Vulkan entry points are left unwrapped", kSlots);
    return result ? result : original;
}

template<unsigned N> void* __stdcall ResolveWrapper(void* object, const char* name)
{
    void* const function = reinterpret_cast<Resolver>(resolvers[N].load(std::memory_order_acquire))(object, name);
    return Route(function, name, FromProvider(_ReturnAddress()));
}

// The size the driver receives: the fatbin header's, when it is larger than
// the one the provider passes (a descriptor redirected by Transfusion).
inline size_t ImageSize(const void* data, size_t size)
{
    uint8_t header[16]{};
    if (!SafeRead(data, header, sizeof(header))) return size;
    uint32_t magic = 0;
    uint64_t declared = 0;
    std::memcpy(&magic, header, sizeof(magic));
    std::memcpy(&declared, header + 8, sizeof(declared));
    if (magic != 0xBA55ED50u || declared > (16u << 20)) return size;
    const size_t total = static_cast<size_t>(declared) + 16;
    return total > size && Readable(data, total) ? total : size;
}

inline int32_t CreateFromProvider(CreateModule original, void* device, const ModuleCreateInfo* info,
    const void* allocator, void** module)
{
    if (!info || info->sType != kCuModuleCreateInfo || !info->pData || !info->dataSize || info->dataSize > MAXDWORD)
        return original(device, info, allocator, module);
    const std::vector<uint8_t>* image = nullptr;
    try { image = cu_module_hook::Replacement(info->pData, static_cast<uint32_t>(info->dataSize)); } catch (...) { image = nullptr; }
    ModuleCreateInfo changed = *info; // keeps the provider's pNext chain
    const size_t sourceSize = ImageSize(info->pData, info->dataSize);
    if (image)
    {
        changed.pData = image->data();
        changed.dataSize = image->size();
    }
    else
    {
        changed.dataSize = sourceSize;
    }
    int32_t status = original(device, &changed, allocator, module);
    bool fallback = false;
    if (status != 0 && image)
    {
        // The driver refused our image: the provider's own image is the
        // behavior of an unmodified runtime.
        changed.pData = info->pData;
        changed.dataSize = sourceSize;
        status = original(device, &changed, allocator, module);
        fallback = true;
    }
    const uint32_t index = accepted.load(std::memory_order_relaxed) + rejected.load(std::memory_order_relaxed);
    (status == 0 ? accepted : rejected).fetch_add(1, std::memory_order_relaxed);
    if (image && !fallback) replaced.fetch_add(1, std::memory_order_relaxed);
    if (!image && sourceSize != info->dataSize) resized.fetch_add(1, std::memory_order_relaxed);
    if (index < 160 || status != 0 || fallback)
    {
        wchar_t description[128]{};
        cu_module_hook::SafeDescribe(info->pData, static_cast<uint32_t>(sourceSize), description, _countof(description));
        const wchar_t* route = fallback ? L" -> rewritten image refused, provider image used"
            : image                     ? L" -> rewritten"
            : sourceSize != info->dataSize ? L" -> size from fatbin header"
                                           : L"";
        Log(L"[VK-NVX] module #%u %s size=%zu%s status=%d (accepted=%u rejected=%u rewritten=%u resized=%u)", index,
            description, info->dataSize, route, status, accepted.load(std::memory_order_relaxed),
            rejected.load(std::memory_order_relaxed), replaced.load(std::memory_order_relaxed),
            resized.load(std::memory_order_relaxed));
    }
    return status;
}

template<unsigned N> int32_t __stdcall CreateModuleWrapper(void* device, const ModuleCreateInfo* info,
    const void* allocator, void** module)
{
    const auto original = reinterpret_cast<CreateModule>(modules[N].load(std::memory_order_acquire));
    if (!FromProvider(_ReturnAddress())) return original(device, info, allocator, module);
    return CreateFromProvider(original, device, info, allocator, module);
}

template<unsigned N> void __stdcall LaunchKernelWrapper(void* commandBuffer, const void* info)
{
    reinterpret_cast<LaunchKernel>(launchers[N].load(std::memory_order_acquire))(commandBuffer, info);
    if (!FromProvider(_ReturnAddress())) return;
    const uint64_t count = launches.fetch_add(1, std::memory_order_relaxed) + 1;
    if (count == 1 || count == 1000 || count == 100000 || count == 10000000)
        Log(L"[VK-NVX] %llu provider kernel launches recorded", static_cast<unsigned long long>(count));
}

#define VULKAN_NVX_TABLE(F) {                                                                                  \
    reinterpret_cast<void*>(&F<0>), reinterpret_cast<void*>(&F<1>), reinterpret_cast<void*>(&F<2>),             \
    reinterpret_cast<void*>(&F<3>), reinterpret_cast<void*>(&F<4>), reinterpret_cast<void*>(&F<5>),             \
    reinterpret_cast<void*>(&F<6>), reinterpret_cast<void*>(&F<7>), reinterpret_cast<void*>(&F<8>),             \
    reinterpret_cast<void*>(&F<9>), reinterpret_cast<void*>(&F<10>), reinterpret_cast<void*>(&F<11>),           \
    reinterpret_cast<void*>(&F<12>), reinterpret_cast<void*>(&F<13>), reinterpret_cast<void*>(&F<14>),          \
    reinterpret_cast<void*>(&F<15>) }
inline void* const kResolverWrappers[kSlots] = VULKAN_NVX_TABLE(ResolveWrapper);
inline void* const kModuleWrappers[kSlots] = VULKAN_NVX_TABLE(CreateModuleWrapper);
inline void* const kLaunchWrappers[kSlots] = VULKAN_NVX_TABLE(LaunchKernelWrapper);
#undef VULKAN_NVX_TABLE

// Resolvers are wrapped for every caller that received them from us; the NVX
// entry points only when the provider asks, so other modules keep the driver's.
inline void* Route(void* function, const char* name, bool provider)
{
    if (!function || reinterpret_cast<uintptr_t>(name) <= 0xFFFF) return function;
    if (std::strcmp(name, "vkGetInstanceProcAddr") == 0 || std::strcmp(name, "vkGetDeviceProcAddr") == 0)
        return Bind(function, resolvers, kResolverWrappers);
    if (provider && std::strcmp(name, "vkCreateCuModuleNVX") == 0) return Bind(function, modules, kModuleWrappers);
    if (provider && std::strcmp(name, "vkCmdCuLaunchKernelNVX") == 0) return Bind(function, launchers, kLaunchWrappers);
    return function;
}

inline FARPROC WINAPI ProviderGetProcAddress(HMODULE module, LPCSTR name)
{
    const FARPROC function = realGetProcAddress.load(std::memory_order_acquire)(module, name);
    return reinterpret_cast<FARPROC>(Route(reinterpret_cast<void*>(function), name, true));
}
}

// The assembly thunks (vulkan_nvx_thunks.asm) jump through these after
// VulkanNvxRouteInit has rewritten the resolver arguments. Both are referenced
// only from assembly, so they are defined here, not inline: this header is
// included by one translation unit (patcher.cpp).
extern "C"
{
void* g_vulkanNvxInitTrampolines[vulkan_nvx::kProviders] = {};
}
extern "C" void VulkanNvxInitThunk0();
extern "C" void VulkanNvxInitThunk1();
extern "C" void VulkanNvxInitThunk2();
extern "C" void VulkanNvxInitThunk3();

// Called by the thunk of provider `slot` with the addresses of Init_Ext2's
// vkGetInstanceProcAddr and vkGetDeviceProcAddr arguments (the caller's stack
// slots, which belong to the callee). Never fails: an argument that cannot be
// wrapped is passed unchanged.
extern "C" void VulkanNvxRouteInit(void** instanceResolver, void** deviceResolver, uint32_t slot)
{
    using namespace vulkan_nvx;
    *instanceResolver = Route(*instanceResolver, "vkGetInstanceProcAddr", false);
    *deviceResolver = Route(*deviceResolver, "vkGetDeviceProcAddr", false);
    if (slot >= kProviders) return;
    Provider& provider = providers[slot];
    if (provider.initialized.exchange(true)) return;
    // Vulkan is in use: keep the provider mapped, so the detour and the
    // redirected import can never outlive it.
    HMODULE pinned = nullptr;
    const HMODULE module = provider.module.load(std::memory_order_acquire);
    const bool pin = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(module), &pinned) != FALSE;
    Log(L"[VK-NVX] NVSDK_NGX_VULKAN_Init_Ext2 reached (provider %p): %s, provider pinned=%d", module,
        *instanceResolver || *deviceResolver ? L"resolvers wrapped"
                                             : L"no resolvers passed, the provider looks them up (redirected import)",
        pin ? 1 : 0);
}

namespace vulkan_nvx
{
inline void* const kInitThunks[kProviders] = {
    reinterpret_cast<void*>(&VulkanNvxInitThunk0), reinterpret_cast<void*>(&VulkanNvxInitThunk1),
    reinterpret_cast<void*>(&VulkanNvxInitThunk2), reinterpret_cast<void*>(&VulkanNvxInitThunk3)};

// Redirects every GetProcAddress import of `module` to ProviderGetProcAddress.
inline size_t RedirectGetProcAddress(HMODULE module)
{
    auto* base = reinterpret_cast<uint8_t*>(module);
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!SafeRead(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || !SafeRead(base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE)
        return 0;
    const auto& directory = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    size_t redirected = 0;
    for (uint32_t d = 0; directory.VirtualAddress && d < 256; ++d)
    {
        IMAGE_IMPORT_DESCRIPTOR descriptor{};
        if (!SafeRead(base + directory.VirtualAddress + d * sizeof(descriptor), &descriptor, sizeof(descriptor))
            || !descriptor.Name)
            break;
        if (!descriptor.OriginalFirstThunk || !descriptor.FirstThunk) continue;
        for (uint32_t i = 0; i < 8192; ++i)
        {
            IMAGE_THUNK_DATA64 lookup{};
            if (!SafeRead(base + descriptor.OriginalFirstThunk + i * sizeof(lookup), &lookup, sizeof(lookup))
                || !lookup.u1.AddressOfData)
                break;
            if (IMAGE_SNAP_BY_ORDINAL64(lookup.u1.Ordinal)) continue;
            char name[sizeof("GetProcAddress")]{};
            if (!SafeRead(base + lookup.u1.AddressOfData + 2, name, sizeof(name))
                || std::memcmp(name, "GetProcAddress", sizeof(name)) != 0)
                continue;
            auto* slot = reinterpret_cast<void**>(base + descriptor.FirstThunk + i * sizeof(void*));
            void* const hook = reinterpret_cast<void*>(&ProviderGetProcAddress);
            if (*slot == hook)
            {
                ++redirected;
                continue;
            }
            // Chain to what the provider imported (another hook, if any): the
            // same function for every provider, so the first one seen is kept.
            ProcAddress expected = nullptr;
            const auto previous = reinterpret_cast<ProcAddress>(*slot);
            if (!realGetProcAddress.compare_exchange_strong(expected, previous) && expected != previous)
                Log(L"[VK-NVX] provider GetProcAddress import differs from the first one seen; chaining to the first");
            DWORD protect = 0, ignored = 0;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protect)) continue;
            *slot = hook;
            VirtualProtect(slot, sizeof(void*), protect, &ignored);
            ++redirected;
        }
    }
    return redirected;
}

// Whether a slot's provider is gone: unloaded, or its image replaced (the
// detour is no longer in its Init_Ext2). NGX loads its candidate providers
// (game, NGX cache, driver store) and unloads the ones it does not keep,
// sometimes several times, before the periodic module scan calls Forget.
inline bool Stale(const Provider& provider)
{
    const HMODULE module = provider.module.load(std::memory_order_acquire);
    if (!module) return false;
    HMODULE owner = nullptr;
    std::array<uint8_t, 16> current{};
    return !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
               reinterpret_cast<LPCWSTR>(provider.initExport), &owner)
        || owner != module || !SafeRead(provider.initExport, current.data(), current.size())
        || current != provider.hooked;
}

// Installs both routes on a provider that exports NVSDK_NGX_VULKAN_Init_Ext2.
// Idempotent; a provider unloaded and loaded again is hooked again.
inline bool Install(HMODULE module, const wchar_t* path)
{
    if (!module) return false;
    auto* initExport = reinterpret_cast<uint8_t*>(GetProcAddress(module, "NVSDK_NGX_VULKAN_Init_Ext2"));
    if (!initExport) return false;
    std::lock_guard lock(installMutex);
    Provider* provider = nullptr;
    for (auto& candidate : providers)
        if (candidate.module.load(std::memory_order_acquire) == module) provider = &candidate;
    if (provider)
    {
        std::array<uint8_t, 16> current{};
        if (provider->initExport == initExport && SafeRead(initExport, current.data(), current.size())
            && current == provider->hooked)
            return true;
        // Same base, fresh mapping: the old detour vanished with the old image.
        provider->module.store(nullptr, std::memory_order_release);
        provider->initialized.store(false, std::memory_order_release);
    }
    else
    {
        for (auto& candidate : providers)
            if (!candidate.module.load(std::memory_order_acquire))
            {
                provider = &candidate;
                break;
            }
        for (auto& candidate : providers)
            if (!provider && Stale(candidate))
            {
                provider = &candidate;
                provider->module.store(nullptr, std::memory_order_release);
                provider->initialized.store(false, std::memory_order_release);
            }
        if (!provider)
        {
            Log(L"[VK-NVX] no free provider slot: Vulkan transport not installed on %s", path ? path : L"");
            return false;
        }
    }
    const auto slot = static_cast<size_t>(provider - providers.data());
    // Owned before the detour goes live: Init_Ext2 may run as soon as it does.
    provider->module.store(module, std::memory_order_release);
    g_vulkanNvxInitTrampolines[slot] = initExport;
    LONG status = DetourTransactionBegin();
    if (status == NO_ERROR)
    {
        DetourUpdateThread(GetCurrentThread());
        status = DetourAttach(&g_vulkanNvxInitTrampolines[slot], kInitThunks[slot]);
        status = status == NO_ERROR ? DetourTransactionCommit() : (DetourTransactionAbort(), status);
    }
    if (status != NO_ERROR)
    {
        g_vulkanNvxInitTrampolines[slot] = nullptr;
        provider->module.store(nullptr, std::memory_order_release);
        Log(L"[VK-NVX] Init_Ext2 detour failed (%ld): Vulkan transport not installed on %s", status, path ? path : L"");
        return false;
    }
    provider->initExport = initExport;
    SafeRead(initExport, provider->hooked.data(), provider->hooked.size());
    const size_t imports = RedirectGetProcAddress(module);
    Log(L"[VK-NVX] transport installed on %s: Init_Ext2 detoured, %zu GetProcAddress import(s) redirected",
        path ? path : L"", imports);
    return true;
}

// Releases the slot of a provider that is no longer loaded (the detour and
// the redirected import went away with its image).
inline void Forget(HMODULE module)
{
    if (!module) return;
    std::lock_guard lock(installMutex);
    for (size_t slot = 0; slot < providers.size(); ++slot)
    {
        Provider& provider = providers[slot];
        if (provider.module.load(std::memory_order_acquire) != module) continue;
        provider.module.store(nullptr, std::memory_order_release);
        provider.initExport = nullptr;
        provider.hooked = {};
        provider.initialized.store(false, std::memory_order_release);
        g_vulkanNvxInitTrampolines[slot] = nullptr;
    }
}
}
