#pragma once
// ABI recovered from the 310.9.1 provider: CreateCuFunction wrapper RVA 0x2a50,
// LaunchCuKernelChain wrapper 0x2b40, chain construction 0x31476..0x314f7.
// Included after Log is declared. Observation only; arguments are never changed.
namespace nvapi_motion_trace
{
using Create = int(__stdcall*)(void*, void*, const char*, void**);
using Launch = int(__stdcall*)(void*, const void*, uint32_t);
inline std::atomic<Create> createOriginal{nullptr};
inline std::atomic<Launch> launchOriginal{nullptr};
struct ChainEntry
{
    void* function;
    uint32_t grid[3];
    uint32_t block[3];
    uint32_t sharedBytes, reserved;
    const void* params;
    uint32_t paramBytes, reserved2;
};
static_assert(sizeof(ChainEntry) == 56);
static_assert(offsetof(ChainEntry, params) == 40);
struct Record { void* function{}; uint32_t kind{}; uint64_t calls{}; };
inline std::array<Record, 128> records{};
inline std::mutex mutex;
inline bool Read(const void* source, void* destination, size_t size)
{
    SIZE_T copied = 0;
    return source && ReadProcessMemory(GetCurrentProcess(), source, destination, size, &copied)
        && copied == size;
}
inline uint32_t Identify(const char* name)
{
    char copy[64]{};
    if (!name) return 0;
    for (size_t i = 0; i < sizeof(copy) - 1; ++i)
    {
        if (!Read(name + i, copy + i, 1)) return 0;
        if (!copy[i]) break;
    }
    if (std::strcmp(copy, "Kernel_EstimateIntermMvecsScatter") == 0) return 1;
    if (std::strcmp(copy, "Kernel_InputMvecProcessing") == 0) return 2;
#if QUALITY_CAPTURE
    if (std::strcmp(copy, "Kernel_BlendCandidatesFused") == 0) return 3;
#endif
    return 0;
}
inline int __stdcall HookCreate(void* device, void* module, const char* name, void** function)
{
    const auto original = createOriginal.load(std::memory_order_acquire);
    const int result = original(device, module, name, function);
    if (!gConfigLogMotionTracing.load(std::memory_order_relaxed)) return result;
    void* handle = nullptr;
    if (result != 0 || !Read(function, &handle, sizeof(handle)) || !handle) return result;
    const uint32_t kind = Identify(name);
    std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return result;
    auto found = std::find_if(records.begin(), records.end(), [&](const auto& r) { return r.function == handle; });
    if (found != records.end()) *found = {}; // Clear stale identity if a handle is reused.
    if (!kind) return result;
    if (found == records.end())
        found = std::find_if(records.begin(), records.end(), [](const auto& r) { return !r.function; });
    if (found == records.end()) return result;
    *found = {handle, kind, 0};
    lock.unlock();
    Log(L"[KERNEL-TRACE] registered kind=%u (1=scatter,2=input-motion) function=%p", kind, handle);
    return result;
}
template<typename T> inline T Field(const std::array<uint8_t, 160>& bytes, size_t offset)
{
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}
inline void Observe(const ChainEntry& entry, uint32_t index, uint32_t count)
{
    if (!gConfigLogMotionTracing.load(std::memory_order_relaxed)) return;
    uint32_t kind = 0;
    uint64_t call = 0;
    {
        std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock()) return;
        auto found = std::find_if(records.begin(), records.end(), [&](const auto& r) { return r.function == entry.function; });
        if (found == records.end() || !found->kind) return;
        kind = found->kind;
        call = found->calls++;
    }
    // Consecutive bursts preserve subframe sequences; sampling isolated calls
    // could alias one interpolation phase. No GPU synchronization or readback.
    if (call % 1200 >= 24) return;
    const uint32_t expected = kind == 1 ? 144 : 160;
    std::array<uint8_t, 160> bytes{};
    if (entry.paramBytes != expected || !Read(entry.params, bytes.data(), expected))
    {
        Log(L"[KERNEL-TRACE] unreadable/unsupported kind=%u bytes=%u expected=%u", kind, entry.paramBytes, expected);
        return;
    }
    if (kind == 1)
        Log(L"[KERNEL-SCATTER] function=%p call=%llu chain=%u/%u t=%.9g size=%ux%u "
            L"grid=%ux%ux%u motion0=%llx motion1=%llx depth=%llx",
            entry.function, static_cast<unsigned long long>(call), index, count,
            Field<float>(bytes,32), Field<uint32_t>(bytes,112), Field<uint32_t>(bytes,116),
            entry.grid[0], entry.grid[1], entry.grid[2], Field<unsigned long long>(bytes,0),
            Field<unsigned long long>(bytes,8), Field<unsigned long long>(bytes,16));
    else
        Log(L"[KERNEL-INPUT] function=%p call=%llu mvRect=(%u,%u %ux%u) "
            L"depthRect=(%u,%u %ux%u) output=%ux%u scale=(%.9g,%.9g) "
            L"mvInvTexture=(%.9g,%.9g) depthInvTexture=(%.9g,%.9g)",
            entry.function, static_cast<unsigned long long>(call),
            Field<uint32_t>(bytes,8), Field<uint32_t>(bytes,12), Field<uint32_t>(bytes,16), Field<uint32_t>(bytes,20),
            Field<uint32_t>(bytes,48), Field<uint32_t>(bytes,52), Field<uint32_t>(bytes,56), Field<uint32_t>(bytes,60),
            Field<uint32_t>(bytes,144), Field<uint32_t>(bytes,148), Field<float>(bytes,152), Field<float>(bytes,156),
            Field<float>(bytes,24), Field<float>(bytes,28), Field<float>(bytes,64), Field<float>(bytes,68));
}
inline int __stdcall HookLaunch(void* commandList, const void* entries, uint32_t count)
{
    const auto original = launchOriginal.load(std::memory_order_acquire);
    static thread_local bool inside = false;
    if (inside) return original(commandList, entries, count);
    inside = true;
    struct Exit { bool& flag; ~Exit() { flag = false; } } exit{inside};
    if (!gConfigLogMotionTracing.load(std::memory_order_relaxed))
        return original(commandList, entries, count);
    static std::atomic<bool> logged{false};
    if (!logged.exchange(true)) Log(L"[KERNEL-TRACE] chain dispatch observed count=%u", count);
    if (entries && count <= 64)
        for (uint32_t i = 0; i < count; ++i)
        {
            ChainEntry entry{};
            if (!Read(static_cast<const uint8_t*>(entries) + i * sizeof(entry), &entry, sizeof(entry))) break;
            Observe(entry, i, count);
        }
    return original(commandList, entries, count);
}
inline void* Intercept(uint32_t id, void* original)
{
    if (!gConfigLogMotionTracing.load(std::memory_order_relaxed)) return original;
    // Log queries before the null check: an unavailable interface is evidence
    // of a different path, not evidence that the resolver was never reached.
    static std::atomic<uint32_t> queryCount{0};
    const auto query = queryCount.fetch_add(1, std::memory_order_relaxed);
    if (query < 48 || id == 0xe2436e22 || id == 0x24973538 || id == 0x5c52bb86)
        Log(L"[KERNEL-QUERY] id=0x%08x result=%p", id, original);
    if (!original) return original;
    if (id == 0xe2436e22)
    {
        const auto fn = reinterpret_cast<Create>(original);
        Create expected = nullptr;
        if (createOriginal.compare_exchange_strong(expected, fn) || expected == fn)
            return reinterpret_cast<void*>(&HookCreate);
    }
    if (id == 0x24973538)
    {
        const auto fn = reinterpret_cast<Launch>(original);
        Launch expected = nullptr;
        if (launchOriginal.compare_exchange_strong(expected, fn) || expected == fn)
            return reinterpret_cast<void*>(&HookLaunch);
    }
    if (id == 0x5c52bb86)
    {
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true)) Log(L"[KERNEL-TRACE] legacy LaunchCubinShader queried; this trace covers CuKernelChain only");
    }
    return original;
}
}
