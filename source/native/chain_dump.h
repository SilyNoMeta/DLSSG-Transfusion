#pragma once
// Test-only: DLSSG_TRANSFUSION_CHAIN_DUMP=<file> records the CUDA work the
// provider submits through NvAPI (module images, function names, and every
// NvAPI_D3D12_LaunchCuKernelChain entry with its grid, block, shared memory and
// parameter bytes) as tab-separated lines. Installed innermost, next to the
// real NvAPI functions, it shows exactly what reaches the driver, whoever
// rewrote it. DLSSG_TRANSFUSION_CHAIN_DUMP_LIMIT bounds the recorded chains
// (default 400). Never enabled by configuration. Included after Log.
namespace chain_dump
{
inline FILE* file = [] {
    char path[1024]{};
    const DWORD length = GetEnvironmentVariableA("DLSSG_TRANSFUSION_CHAIN_DUMP", path, sizeof(path));
    return length && length < sizeof(path) ? _fsopen(path, "w", _SH_DENYWR) : nullptr;
}();
inline uint32_t limit = [] {
    char value[16]{};
    const DWORD length = GetEnvironmentVariableA("DLSSG_TRANSFUSION_CHAIN_DUMP_LIMIT", value, sizeof(value));
    return length ? static_cast<uint32_t>(std::strtoul(value, nullptr, 10)) : 400u;
}();
inline std::mutex mutex;
inline std::atomic<uint32_t> chains{0};

using CreateModule = int(__cdecl*)(void* device, const void* blob, uint32_t size, void** module);
using CreateFunction = int(__cdecl*)(void* device, void* module, const char* name, void** function);
using Launch = int(__cdecl*)(void* commandList, const void* entries, uint32_t count);
inline std::atomic<CreateModule> moduleOriginal{nullptr};
inline std::atomic<CreateFunction> functionOriginal{nullptr};
inline std::atomic<Launch> launchOriginal{nullptr};

struct Entry
{
    void* function;
    uint32_t grid[3];
    uint32_t block[3];
    uint32_t sharedBytes, reserved;
    const void* params;
    uint32_t paramBytes, reserved2;
};
static_assert(sizeof(Entry) == 56);

inline uint64_t Fnv(const uint8_t* data, size_t size)
{
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * 1099511628211ull;
    return hash;
}

inline bool SafeHash(const void* data, size_t size, uint64_t& hash)
{
    __try { hash = Fnv(static_cast<const uint8_t*>(data), size); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

inline bool SafeRead(const void* source, void* destination, size_t size)
{
    __try { std::memcpy(destination, source, size); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

inline int __cdecl HookModule(void* device, const void* blob, uint32_t size, void** module)
{
    const int status = moduleOriginal.load(std::memory_order_acquire)(device, blob, size, module);
    void* handle = nullptr;
    uint64_t hash = 0;
    uint32_t magic = 0;
    if (status == 0 && module && SafeRead(module, &handle, sizeof(handle)) && SafeRead(blob, &magic, sizeof(magic)))
    {
        uint64_t length = size;
        if (magic == 0xBA55ED50u) SafeRead(static_cast<const uint8_t*>(blob) + 8, &length, sizeof(length)), length += 16;
        SafeHash(blob, static_cast<size_t>(length), hash);
        std::lock_guard lock(mutex);
        fprintf(file, "module\t%p\t%s\t%u\t%016llx\n", handle, magic == 0xBA55ED50u ? "fatbin" : "elf",
            static_cast<unsigned>(length), static_cast<unsigned long long>(hash));
        fflush(file);
    }
    return status;
}

inline int __cdecl HookFunction(void* device, void* module, const char* name, void** function)
{
    const int status = functionOriginal.load(std::memory_order_acquire)(device, module, name, function);
    void* handle = nullptr;
    char copy[96]{};
    if (status == 0 && function && SafeRead(function, &handle, sizeof(handle)))
    {
        for (size_t i = 0; name && i + 1 < sizeof(copy) && SafeRead(name + i, copy + i, 1) && copy[i]; ++i) {}
        std::lock_guard lock(mutex);
        fprintf(file, "function\t%p\t%p\t%s\n", handle, module, copy);
        fflush(file);
    }
    return status;
}

inline int __cdecl HookLaunch(void* commandList, const void* entries, uint32_t count)
{
    const uint32_t chain = chains.fetch_add(1, std::memory_order_relaxed);
    if (chain < limit && entries && count <= 256)
    {
        std::lock_guard lock(mutex);
        for (uint32_t i = 0; i < count; ++i)
        {
            Entry entry{};
            if (!SafeRead(static_cast<const uint8_t*>(entries) + i * sizeof(Entry), &entry, sizeof(entry))) break;
            std::vector<uint8_t> params(entry.paramBytes <= 4096 ? entry.paramBytes : 0);
            if (!params.empty() && !SafeRead(entry.params, params.data(), params.size())) params.clear();
            fprintf(file, "launch\t%u\t%u\t%p\t%u,%u,%u\t%u,%u,%u\t%u\t%u\t", chain, i, entry.function,
                entry.grid[0], entry.grid[1], entry.grid[2], entry.block[0], entry.block[1], entry.block[2],
                entry.sharedBytes, entry.paramBytes);
            for (uint8_t byte : params) fprintf(file, "%02x", byte);
            fputc('\n', file);
        }
        fflush(file);
    }
    return launchOriginal.load(std::memory_order_acquire)(commandList, entries, count);
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
    if (!file || !function) return function;
    if (id == 0xAD1A677Du) return Wrap(moduleOriginal, function, reinterpret_cast<void*>(&HookModule));
    if (id == 0xE2436E22u) return Wrap(functionOriginal, function, reinterpret_cast<void*>(&HookFunction));
    if (id == 0x24973538u) return Wrap(launchOriginal, function, reinterpret_cast<void*>(&HookLaunch));
    return function;
}
}
