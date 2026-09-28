#pragma once
// NvAPI_D3D12_CreateCuModule (0xAD1A677D) below Ada.
//
// Every kernel image the provider hands to the driver must be either NVIDIA's
// own SM86 network SASS or PTX retargeted by midpoint_fix. On Turing the PTX
// also needs the sm_75 lowering, which cannot be done in place: the image is
// replaced here by midpoint_fix::PrepareModuleImage's rebuilt fatbin. The log
// shows which image each module came from and whether the driver accepted it.
// Included after Log is declared.
namespace cu_module_hook
{
using CreateModule = int(__cdecl*)(void* device, const void* blob, uint32_t size, void** module);
inline std::atomic<CreateModule> original{nullptr};
inline std::atomic<uint32_t> accepted{0};
inline std::atomic<uint32_t> rejected{0};
inline std::atomic<uint32_t> replaced{0};

// Replacement images keyed by the content of the original image. They are
// never freed: the driver may keep referring to a module's source image.
inline std::mutex cacheMutex;
inline std::unordered_map<uint64_t, std::unique_ptr<std::vector<uint8_t>>> cache;

inline bool SafeHash(const uint8_t* data, size_t size, uint64_t& hash)
{
    __try
    {
        hash = 1469598103934665603ull;
        for (size_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * 1099511628211ull;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

inline const std::vector<uint8_t>* Replacement(const void* blob, uint32_t size)
{
    if (!blob || !size) return nullptr;
    uint64_t key = 0;
    if (!SafeHash(static_cast<const uint8_t*>(blob), size, key)) return nullptr;
    key ^= size;
    std::lock_guard lock(cacheMutex);
    const auto found = cache.find(key);
    if (found != cache.end()) return found->second.get();
    auto image = std::make_unique<std::vector<uint8_t>>();
    if (!midpoint_fix::PrepareModuleImage(blob, size, *image)) image.reset();
    return cache.emplace(key, std::move(image)).first->second.get();
}

inline void Describe(const uint8_t* blob, uint32_t size, wchar_t* out, size_t capacity)
{
    out[0] = L'\0';
    if (!blob || size < 64) return;
    uint32_t magic = 0;
    std::memcpy(&magic, blob, sizeof(magic));
    if (magic == 0x464C457Fu) // ELF
    {
        uint32_t flags = 0;
        std::memcpy(&flags, blob + 48, sizeof(flags));
        swprintf_s(out, capacity, L"ELF sm_%u", flags & 0xFF);
        return;
    }
    if (magic != 0xBA55ED50u) { swprintf_s(out, capacity, L"unknown 0x%08x", magic); return; }
    uint64_t total = 0;
    std::memcpy(&total, blob + 8, sizeof(total));
    size_t written = static_cast<size_t>(swprintf_s(out, capacity, L"fatbin"));
    for (uint64_t p = 16; p + 64 <= total + 16 && p + 64 <= size && written + 16 < capacity;)
    {
        uint16_t kind = 0; uint32_t header = 0, arch = 0; uint64_t payload = 0;
        std::memcpy(&kind, blob + p, sizeof(kind));
        std::memcpy(&header, blob + p + 4, sizeof(header));
        std::memcpy(&payload, blob + p + 8, sizeof(payload));
        std::memcpy(&arch, blob + p + 28, sizeof(arch));
        if (header < 64 || !payload) break;
        written += static_cast<size_t>(swprintf_s(out + written, capacity - written, L" %s%u",
            kind == 1 ? L"ptx" : kind == 2 ? L"elf" : L"?", arch));
        p += header + payload;
    }
}

inline void SafeDescribe(const void* blob, uint32_t size, wchar_t* out, size_t capacity)
{
    __try { Describe(static_cast<const uint8_t*>(blob), size, out, capacity); }
    __except (EXCEPTION_EXECUTE_HANDLER) { wcscpy_s(out, capacity, L"unreadable"); }
}

inline int __cdecl Hook(void* device, const void* blob, uint32_t size, void** module)
{
    const std::vector<uint8_t>* image = nullptr;
    try { image = Replacement(blob, size); } catch (...) { image = nullptr; }
    const void* source = image ? image->data() : blob;
    const uint32_t sourceSize = image ? static_cast<uint32_t>(image->size()) : size;
    const int status = original.load(std::memory_order_acquire)(device, source, sourceSize, module);
    const uint32_t index = accepted.load(std::memory_order_relaxed) + rejected.load(std::memory_order_relaxed);
    (status == 0 ? accepted : rejected).fetch_add(1, std::memory_order_relaxed);
    if (image) replaced.fetch_add(1, std::memory_order_relaxed);
    if (index < 160 || status != 0)
    {
        wchar_t description[128]{};
        SafeDescribe(blob, size, description, _countof(description));
        Log(L"[CU-MODULE] #%u %s size=%u%s status=%d (accepted=%u rejected=%u rewritten=%u)", index, description, size,
            image ? L" -> rewritten" : L"", status, accepted.load(std::memory_order_relaxed),
            rejected.load(std::memory_order_relaxed), replaced.load(std::memory_order_relaxed));
    }
    return status;
}

inline void* Intercept(uint32_t id, void* function)
{
    if (id != 0xAD1A677Du || !function || (!gpu_arch::PreAda() && !midpoint_fix::OptimizedKernelsEnabled()))
        return function;
    const auto fn = reinterpret_cast<CreateModule>(function);
    CreateModule expected = nullptr;
    if (original.compare_exchange_strong(expected, fn) || expected == fn)
        return reinterpret_cast<void*>(&Hook);
    return function;
}
}
