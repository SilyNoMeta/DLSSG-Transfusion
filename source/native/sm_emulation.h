#pragma once
// Test-only: DLSSG_TRANSFUSION_EMULATE_SM=75|86|89|120 makes NvAPI report that
// GPU (SM version and architecture) to every caller, so the Turing path can be
// validated on newer hardware, which executes sm_75 PTX. It is never enabled
// by configuration and never persisted. Included after Log is declared.
namespace sm_emulation
{
inline uint32_t emulated = [] {
    char value[8]{};
    const DWORD length = GetEnvironmentVariableA("DLSSG_TRANSFUSION_EMULATE_SM", value, sizeof(value));
    const uint32_t sm = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
    return length && (sm == 75 || sm == 86 || sm == 89 || sm == 120) ? sm : 0u;
}();

// NvAPI_D3D12_GetGraphicsCapabilities: NV_D3D12_GRAPHICS_CAPS has the SM
// major/minor version as two NvU16 at offset 4. Only rewritten when emulating.
using GraphicsCaps = int(__cdecl*)(void* device, uint32_t version, void* caps);
inline std::atomic<GraphicsCaps> capsOriginal{nullptr};
inline int __cdecl HookCaps(void* device, uint32_t version, void* caps)
{
    const int status = capsOriginal.load(std::memory_order_acquire)(device, version, caps);
    if (status == 0 && caps && emulated)
    {
        const uint16_t sm[2] = {static_cast<uint16_t>(emulated / 10), static_cast<uint16_t>(emulated % 10)};
        std::memcpy(static_cast<uint8_t*>(caps) + 4, sm, sizeof(sm));
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true)) Log(L"[SM-EMULATION] GetGraphicsCapabilities reports SM %u.%u", sm[0], sm[1]);
    }
    return status;
}

// NvAPI_GPU_GetArchInfo: NV_GPU_ARCH_INFO has the architecture at offset 4.
using ArchInfo = int(__cdecl*)(void* gpu, void* info);
inline std::atomic<ArchInfo> archOriginal{nullptr};
inline int __cdecl HookArch(void* gpu, void* info)
{
    const int status = archOriginal.load(std::memory_order_acquire)(gpu, info);
    if (status == 0 && info && emulated)
    {
        const uint32_t arch = emulated == 75 ? 0x160u : emulated == 86 ? 0x170u : emulated == 89 ? 0x190u : 0x1B0u;
        std::memcpy(static_cast<uint8_t*>(info) + 4, &arch, sizeof(arch));
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true)) Log(L"[SM-EMULATION] GetArchInfo reports 0x%x", arch);
    }
    return status;
}

template<typename T>
inline void* Wrap(std::atomic<T>& original, void* function, void* hook)
{
    const auto fn = reinterpret_cast<T>(function);
    T expected = nullptr;
    return original.compare_exchange_strong(expected, fn) || expected == fn ? hook : function;
}

inline void* Intercept(uint32_t id, void* function)
{
    if (!function) return function;
    if (id == 0x01E87354u && emulated) return Wrap(capsOriginal, function, reinterpret_cast<void*>(&HookCaps));
    if (id == 0xD8265D24u && emulated)
        return Wrap(archOriginal, function, reinterpret_cast<void*>(&HookArch));
    return function;
}
}
