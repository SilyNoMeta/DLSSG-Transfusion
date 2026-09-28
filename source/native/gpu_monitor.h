#pragma once
// GPU load, temperature, power, clocks and VRAM through NVML (nvml.dll, shipped
// with the NVIDIA driver). Loaded on first use; call from the worker thread only.
#include <Windows.h>
#include <cstdint>

namespace gpu_monitor
{
struct Sample
{
    uint32_t utilization = 0;      // %
    uint32_t temperatureC = 0;
    uint32_t powerMilliwatts = 0;
    uint32_t graphicsClockMhz = 0;
    uint32_t memoryClockMhz = 0;
    uint64_t vramUsedBytes = 0;    // whole GPU, all processes
    uint64_t vramTotalBytes = 0;
};

namespace detail
{
using Device = void*;
struct Utilization { unsigned int gpu, memory; };
struct Memory { unsigned long long total, free, used; };
using InitFn = int (*)();
using HandleFn = int (*)(unsigned int, Device*);
using UtilizationFn = int (*)(Device, Utilization*);
using TemperatureFn = int (*)(Device, int, unsigned int*);
using PowerFn = int (*)(Device, unsigned int*);
using MemoryFn = int (*)(Device, Memory*);
using ClockFn = int (*)(Device, int, unsigned int*);

struct Api
{
    bool attempted = false, ready = false;
    Device device = nullptr;
    UtilizationFn utilization = nullptr;
    TemperatureFn temperature = nullptr;
    PowerFn power = nullptr;
    MemoryFn memory = nullptr;
    ClockFn clock = nullptr;
};

inline Api& State()
{
    static Api api;
    return api;
}

template <typename T>
T Proc(HMODULE module, const char* name)
{
    return reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(module, name)));
}

inline bool Load()
{
    Api& api = State();
    if (api.attempted)
        return api.ready;
    api.attempted = true;
    HMODULE module = LoadLibraryW(L"nvml.dll");  // System32 on current drivers
    if (!module)
        module = LoadLibraryW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
    if (!module)
        return false;
    const auto init = Proc<InitFn>(module, "nvmlInit_v2");
    const auto handle = Proc<HandleFn>(module, "nvmlDeviceGetHandleByIndex_v2");
    api.utilization = Proc<UtilizationFn>(module, "nvmlDeviceGetUtilizationRates");
    api.temperature = Proc<TemperatureFn>(module, "nvmlDeviceGetTemperature");
    api.power = Proc<PowerFn>(module, "nvmlDeviceGetPowerUsage");
    api.memory = Proc<MemoryFn>(module, "nvmlDeviceGetMemoryInfo");
    api.clock = Proc<ClockFn>(module, "nvmlDeviceGetClockInfo");
    // NVML only lists NVIDIA GPUs: index 0 is the NVIDIA card even next to an iGPU.
    api.ready = init && handle && init() == 0 && handle(0, &api.device) == 0 && api.device;
    return api.ready;
}
} // namespace detail

inline bool Poll(Sample& sample)
{
    if (!detail::Load())
        return false;
    detail::Api& api = detail::State();
    sample = {};
    detail::Utilization utilization{};
    if (api.utilization && api.utilization(api.device, &utilization) == 0)
        sample.utilization = utilization.gpu;
    if (api.temperature)
        api.temperature(api.device, 0 /* NVML_TEMPERATURE_GPU */, &sample.temperatureC);
    if (api.power)
        api.power(api.device, &sample.powerMilliwatts);
    if (api.clock)
    {
        api.clock(api.device, 0 /* NVML_CLOCK_GRAPHICS */, &sample.graphicsClockMhz);
        api.clock(api.device, 2 /* NVML_CLOCK_MEM */, &sample.memoryClockMhz);
    }
    detail::Memory memory{};
    if (api.memory && api.memory(api.device, &memory) == 0)
    {
        sample.vramUsedBytes = memory.used;
        sample.vramTotalBytes = memory.total;
    }
    return true;
}
} // namespace gpu_monitor
