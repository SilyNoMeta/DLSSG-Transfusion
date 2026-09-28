#pragma once
// DL1 and DL2 network optimization for the DLSS-G 310.9.1 provider
// (optimization steps 2 to 4): the kernels and launch shapes of the
// dlssg_for_sm86 0.3.5 backend, whose output is bit-identical to NVIDIA's.
//
// The DL1 network runs between Kernel_DL1Net_Input and Kernel_DL1Net_Output:
// 17 k_conv_fp16_nhwc, 5 k_pooling, and on each of the 5 decoder levels
//     k_upscale(low -> tmp) ; k_element_wise(tmp + skip -> x) ; k_conv(W, x -> out)
// Transfusion replaces, launch by launch (each arrives in its own chain call):
//   - every conv and pooling by its specialized kernel: NVIDIA's parameters
//     unchanged, new function, grid and block;
//   - each decoder level by k_conv_fp16_nhwc_fused_up(W, low, skip, out,
//     lw, lh, hw, hh), which never writes tmp or x (no later kernel reads them).
// The upscale and add are held with copied parameters until the conv that
// consumes them; their pointer chain and dimensions are checked. Layers are
// identified by their rank in the network and their parameter sizes; the
// first inconsistency stops the optimization until the next network run, and
// held launches are always replayed unchanged, in order.
//
// The DL2 network (k_initial_merge .. custom_block1_conv_bot1_hf, once per
// generated frame) is optimized the same way, layer by layer, identified by
// kernel name and checked against its exact shape (parameter size, channels,
// kernel size):
//   - conv0/conv1/conv2 of block0, conv1 of block1, bot1 of both blocks and
//     the upsample by their specialized kernels, NVIDIA's parameters unchanged;
//   - k_initial_merge + block0 convPre by conv_dl2_merge_pool, and
//     k_central_block + block1 conv0 by conv_dl2_central_pool (the merge and
//     central outputs are overwritten before any later read);
//   - block1's residual chain (sdli: a persistent kernel with a grid barrier
//     on its own counter buffer, gain within noise) and both bot0 layers keep
//     NVIDIA's kernels.
// Everything uses NVIDIA's own weights.
//
// Kernels are PTX resources (private/kernels.rc): 40xx for sm_86 (JIT-compiled for
// sm_89 on Ada) and 110xx for sm_75. They are created on the provider's device
// when it creates its own network. Enabled with "optimizedKernels", only when
// the provider's kernels are the exact 310.9.1 ones. Included after
// cu_module_hook.h.
namespace network_optimizer
{
using CreateFunction = int(__cdecl*)(void* device, void* module, const char* name, void** function);
using Launch = int(__cdecl*)(void* commandList, const void* entries, uint32_t count);
inline std::atomic<CreateFunction> functionOriginal{nullptr};
inline std::atomic<Launch> launchOriginal{nullptr};

enum class Kind : uint8_t { Other, NetworkStart, NetworkEnd, Upscale, ElementWise, Conv, Pool, Dl2 };
enum class Grid : uint8_t { None, W16H4, Flat16, Flat32, Flat64, Pool2048 };

struct Rule
{
    uint16_t resource; // sm_86 PTX resource; sm_75 is resource + 7000
    Grid grid;
    uint32_t z;
    uint32_t block;
    uint32_t paramBytes;
};

// Per NVIDIA conv rank. Fused ranks (the conv after each decoder add) have no
// plain rule: the fusion consumes them.
inline constexpr Rule kConvRules[17] = {
    {4000, Grid::W16H4, 1, 128, 48}, {4008, Grid::Flat16, 1, 128, 40}, {4002, Grid::W16H4, 4, 128, 48},
    {4004, Grid::W16H4, 4, 128, 48}, {4006, Grid::W16H4, 8, 256, 48}, {4010, Grid::Flat32, 8, 512, 40},
    {4012, Grid::Flat32, 8, 128, 40}, {0, Grid::None, 0, 0, 40},      {4014, Grid::Flat32, 2, 512, 40},
    {0, Grid::None, 0, 0, 40},        {4016, Grid::Flat16, 1, 256, 40}, {0, Grid::None, 0, 0, 40},
    {4018, Grid::Flat16, 1, 128, 40}, {0, Grid::None, 0, 0, 40},        {4020, Grid::Flat32, 1, 128, 40},
    {0, Grid::None, 0, 0, 40},        {4022, Grid::Flat16, 1, 64, 40}};
inline constexpr Rule kFusedRules[5] = {
    {4024, Grid::Flat16, 1, 256, 48}, {4026, Grid::Flat16, 1, 256, 48}, {4028, Grid::Flat16, 1, 256, 48},
    {4030, Grid::Flat16, 1, 128, 48}, {4032, Grid::Flat64, 1, 128, 48}};
inline constexpr Rule kPoolRule{4039, Grid::Pool2048, 1, 256, 36};

// DL2 layers, in network order. Producers (merge, central block) are held for
// the fusion with the next conv, whose `resource` is the fused kernel.
enum class Dl2 : uint8_t { InitialMerge, ConvPre, B0Conv0, B0Conv1, B0Conv2, B0Bot1, Upsample, CentralBlock, B1Conv0,
    B1Conv1, B1Bot1, Count };
struct Dl2Rule
{
    const char* name;        // NVIDIA kernel
    uint16_t resource;       // sm_86 PTX resource; sm_75 is resource + 7000
    const char* entry;
    uint32_t paramBytes;
    uint8_t hWord, wWord;    // output height and width, as 32-bit parameter words
    uint16_t dx, dy, z;      // grid: ceil(w/dx), ceil(h/dy), z; dy == 0: ceil(w*h/dx), z, 1
    uint32_t block;
    std::array<std::pair<uint8_t, uint32_t>, 6> shape; // expected parameter words (channels, kernel size)
};
inline constexpr Dl2Rule kDl2Rules[size_t(Dl2::Count)] = {
    {"k_initial_merge", 0, nullptr, 44, 0, 0, 0, 0, 0, 0, {}},
    {"custom_block0_convPre_kernel", 4042, "conv_dl2_merge_pool", 80, 17, 18, 8, 4, 1, 256,
        {{{4, 0x20}, {5, 3}, {6, 3}, {7, 0xa}, {13, 0xa}, {19, 0x20}}}},
    {"custom_block0_conv0_c8_kernel", 4044, "conv_dl2_pool", 80, 17, 18, 8, 2, 1, 128,
        {{{4, 0x20}, {5, 3}, {6, 3}, {7, 0x20}, {13, 0x20}, {19, 0x20}}}},
    {"custom_block0_conv1_kernel", 4046, "conv_dl2_pool", 80, 17, 18, 8, 2, 1, 256,
        {{{4, 0x40}, {5, 3}, {6, 3}, {7, 0x20}, {13, 0x20}, {19, 0x40}}}},
    {"custom_block0_conv2_kernel", 4050, "conv_dl2_resid", 92, 19, 20, 16, 2, 2, 128,
        {{{4, 0x40}, {5, 3}, {6, 3}, {7, 0x40}, {15, 0x40}, {21, 0x40}}}},
    {"custom_block0_conv_bot1_hf_kernel", 4062, "conv_dl2_bot1_block0", 144, 21, 22, 16, 4, 1, 128,
        {{{4, 8}, {7, 0x20}, {17, 0x20}, {23, 4}, {29, 1}, {35, 3}}}},
    {"custom_upsample_hf_kernel", 4059, "upsample_hf", 144, 21, 22, 256, 0, 3, 256,
        {{{5, 4}, {11, 1}, {17, 3}, {23, 4}, {29, 1}, {35, 3}}}},
    {"k_central_block", 0, nullptr, 68, 0, 0, 0, 0, 0, 0, {}},
    {"custom_block1_conv0_kernel", 4060, "conv_dl2_central_pool", 80, 17, 18, 8, 4, 1, 256,
        {{{4, 0x10}, {5, 3}, {6, 3}, {7, 0x12}, {13, 0x12}, {19, 0x10}}}},
    {"custom_block1_conv1_kernel", 4048, "conv_dl2_pool", 80, 17, 18, 8, 2, 1, 128,
        {{{4, 0x20}, {5, 3}, {6, 3}, {7, 0x10}, {13, 0x10}, {19, 0x20}}}},
    {"custom_block1_conv_bot1_hf_kernel", 4058, "conv_dl2_bot1_block1", 152, 27, 28, 16, 8, 1, 256,
        {{{4, 8}, {7, 0x10}, {23, 0x10}, {29, 4}, {33, 1}, {37, 3}}}}};

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

inline std::mutex mutex;
struct Role
{
    Kind kind = Kind::Other;
    Dl2 layer = Dl2::Count;
};
inline std::unordered_map<void*, Role> kinds;          // provider function handle -> role
inline std::unordered_map<uint16_t, void*> functions;  // our function per rule resource
inline void* device = nullptr;
inline std::atomic<bool> ready{false};
inline std::atomic<bool> failed{false};
inline std::atomic<uint64_t> optimizedRuns{0};
inline std::atomic<uint64_t> optimizedDl2Runs{0};
inline std::vector<std::unique_ptr<std::vector<uint8_t>>> images; // module sources, kept alive

// Built before `ready` is published and never modified while it is set.
inline void* Function(uint16_t resource)
{
    const auto found = functions.find(resource);
    return found != functions.end() ? found->second : nullptr;
}

inline bool Enabled()
{
    return midpoint_fix::OptimizedKernelsEnabled() && midpoint_fix::ExactProviderKernels();
}

inline Kind Classify(const char* name)
{
    if (!name) return Kind::Other;
    if (std::strcmp(name, "Kernel_DL1Net_Input") == 0) return Kind::NetworkStart;
    if (std::strcmp(name, "Kernel_DL1Net_Output") == 0) return Kind::NetworkEnd;
    if (std::strcmp(name, "k_upscale") == 0) return Kind::Upscale;
    if (std::strcmp(name, "k_element_wise") == 0) return Kind::ElementWise;
    if (std::strcmp(name, "k_conv_fp16_nhwc") == 0) return Kind::Conv;
    if (std::strcmp(name, "k_pooling") == 0) return Kind::Pool;
    return Kind::Other;
}

inline Role ClassifyRole(const char* name)
{
    for (size_t i = 0; name && i < std::size(kDl2Rules); ++i)
        if (std::strcmp(name, kDl2Rules[i].name) == 0) return {Kind::Dl2, static_cast<Dl2>(i)};
    return {Classify(name)};
}

inline std::string_view Resource(uint16_t id)
{
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&Resource), &self);
    const HRSRC resource = self ? FindResourceW(self, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10)) : nullptr;
    const HGLOBAL loaded = resource ? LoadResource(self, resource) : nullptr;
    const auto* data = loaded ? static_cast<const char*>(LockResource(loaded)) : nullptr;
    return data ? std::string_view(data, SizeofResource(self, resource)) : std::string_view{};
}

// One uncompressed PTX entry; header fields as in the 310.9.1 provider's own
// fatbins (entry header 0x50 bytes, ISA 8.5).
inline std::vector<uint8_t> BuildFatbin(std::string_view ptx, uint32_t sm)
{
    const size_t padded = (ptx.size() + 1 + 7) & ~size_t{7};
    std::vector<uint8_t> out(16 + 0x50 + padded, 0);
    const uint32_t magic = 0xBA55ED50u;
    const uint16_t version = 1, outerHeader = 0x10;
    const uint64_t outerSize = 0x50 + padded;
    std::memcpy(out.data(), &magic, 4);
    std::memcpy(out.data() + 4, &version, 2);
    std::memcpy(out.data() + 6, &outerHeader, 2);
    std::memcpy(out.data() + 8, &outerSize, 8);
    uint8_t* entry = out.data() + 16;
    const uint16_t kind = 1, entryVersion = 0x0101, isaMinor = 5, isaMajor = 8;
    const uint32_t headerSize = 0x50, field20 = 0x40, extra = 0x48;
    const uint64_t payload = padded, flags = 0x41;
    std::memcpy(entry, &kind, 2);
    std::memcpy(entry + 2, &entryVersion, 2);
    std::memcpy(entry + 4, &headerSize, 4);
    std::memcpy(entry + 8, &payload, 8);
    std::memcpy(entry + 20, &field20, 4);
    std::memcpy(entry + 24, &isaMinor, 2);
    std::memcpy(entry + 26, &isaMajor, 2);
    std::memcpy(entry + 28, &sm, 4);
    std::memcpy(entry + 40, &flags, 8);
    std::memcpy(entry + 64, &extra, 4);
    std::memcpy(entry + 0x50, ptx.data(), ptx.size());
    return out;
}

inline bool CreateVariant(void* providerDevice, uint16_t resource, const char* entry)
{
    const auto createModule = cu_module_hook::original.load(std::memory_order_acquire);
    const auto createFunction = functionOriginal.load(std::memory_order_acquire);
    const uint32_t sm = gpu_arch::SmVersion();
    const std::string_view source = Resource(static_cast<uint16_t>(sm < 80 ? resource + 7000 : resource));
    if (source.empty())
    {
        Log(L"Network optimization unavailable: kernel %u is not embedded in this build (docs/PUBLIC-SOURCE.md)", resource);
        return false;
    }
    if (!createModule || !createFunction) return false;
    std::string ptx(source);
    if (sm >= 80 && sm != 86)
    {
        const size_t target = ptx.find(".target sm_86");
        if (target == std::string::npos) return false;
        ptx.replace(target, sizeof(".target sm_86") - 1, ".target sm_" + std::to_string(sm));
    }
    auto image = std::make_unique<std::vector<uint8_t>>(BuildFatbin(ptx, sm < 80 ? 75 : sm));
    void* module = nullptr;
    const int moduleStatus = createModule(providerDevice, image->data(), static_cast<uint32_t>(image->size()), &module);
    images.push_back(std::move(image));
    void* function = nullptr;
    const int status = moduleStatus == 0 ? createFunction(providerDevice, module, entry, &function) : moduleStatus;
    if (status != 0 || !function)
    {
        Log(L"Network optimization disabled: kernel %u not created (status %d)", resource, status);
        return false;
    }
    functions[resource] = function;
    return true;
}

// Called with the lock held, from the provider's own network creation.
inline void CreateVariants(void* providerDevice)
{
    ready.store(false, std::memory_order_release);
    functions.clear();
    bool ok = CreateVariant(providerDevice, kPoolRule.resource, "k_pooling");
    for (const Rule& rule : kConvRules)
        if (ok && rule.resource) ok = CreateVariant(providerDevice, rule.resource, "k_conv_fp16_nhwc");
    for (const Rule& rule : kFusedRules)
        if (ok) ok = CreateVariant(providerDevice, rule.resource, "k_conv_fp16_nhwc_fused_up");
    for (const Dl2Rule& rule : kDl2Rules)
        if (ok && rule.resource) ok = CreateVariant(providerDevice, rule.resource, rule.entry);
    if (!ok)
    {
        failed = true;
        return;
    }
    device = providerDevice;
    ready.store(true, std::memory_order_release);
    Log(L"Network optimization ready: %zu DL1/DL2 kernels for sm_%u", functions.size(), gpu_arch::SmVersion());
}

inline int __cdecl HookFunction(void* providerDevice, void* module, const char* name, void** function)
{
    const int status = functionOriginal.load(std::memory_order_acquire)(providerDevice, module, name, function);
    if (status != 0 || !function || !*function) return status;
    const Role role = ClassifyRole(name);
    std::lock_guard lock(mutex);
    kinds[*function] = role; // handles are recycled: always take the latest meaning
    // The provider builds its network: build ours next to it, on its device.
    if (role.kind == Kind::Upscale && midpoint_fix::OptimizedKernelsEnabled() && !failed.load()
        && (!ready.load() || device != providerDevice))
    {
        ready.store(false, std::memory_order_release);
        CreateVariants(providerDevice);
    }
    return status;
}

struct Held
{
    Entry entry{};
    std::array<uint8_t, 96> params{};
};
struct State
{
    void* list = nullptr;
    bool active = false;   // inside DL1, optimization still consistent
    int conv = 0;          // NVIDIA conv rank
    int level = 0;         // decoder level
    bool haveUpscale = false, haveAdd = false;
    Held upscale, add;
    Dl2 held = Dl2::Count; // DL2 producer held for fusion (merge or central block)
    Held dl2;
};
inline thread_local State t_state;

template<typename T> inline T Field(const void* params, size_t offset)
{
    T value{};
    std::memcpy(&value, static_cast<const uint8_t*>(params) + offset, sizeof(value));
    return value;
}

inline bool Capture(const Entry& entry, Held& held)
{
    if (entry.paramBytes > held.params.size() || !entry.params) return false;
    held.entry = entry;
    std::memcpy(held.params.data(), entry.params, entry.paramBytes);
    held.entry.params = held.params.data();
    return true;
}

// Replays held launches unchanged, in their original order.
inline void Flush(Launch original)
{
    auto& s = t_state;
    if (s.haveUpscale) original(s.list, &s.upscale.entry, 1);
    if (s.haveAdd) original(s.list, &s.add.entry, 1);
    if (s.held != Dl2::Count) original(s.list, &s.dl2.entry, 1);
    s.haveUpscale = s.haveAdd = false;
    s.held = Dl2::Count;
}

inline void Stop(Launch original)
{
    Flush(original);
    t_state.active = false;
}

inline uint32_t Ceil(uint64_t value, uint64_t divisor)
{
    return static_cast<uint32_t>((value + divisor - 1) / divisor);
}

inline void Shape(const Rule& rule, uint32_t w, uint32_t h, uint32_t channels, Entry& entry)
{
    entry.grid[1] = entry.grid[2] = 1;
    switch (rule.grid)
    {
    case Grid::W16H4: entry.grid[0] = Ceil(w, 16); entry.grid[1] = Ceil(h, 4); entry.grid[2] = rule.z; break;
    case Grid::Flat16: entry.grid[0] = Ceil(uint64_t(w) * h, 16); entry.grid[2] = rule.z; break;
    case Grid::Flat32: entry.grid[0] = Ceil(uint64_t(w) * h, 32); entry.grid[2] = rule.z; break;
    case Grid::Flat64: entry.grid[0] = Ceil(uint64_t(w) * h, 64); entry.grid[2] = rule.z; break;
    case Grid::Pool2048: entry.grid[0] = Ceil(uint64_t(w) * h * channels, 2048); break;
    default: break;
    }
    entry.block[0] = rule.block;
    entry.block[1] = entry.block[2] = 1;
    entry.sharedBytes = 0;
}

// A specialized kernel with NVIDIA's parameters; dimensions are the output's.
inline int LaunchVariant(Launch original, void* list, const Entry& entry, const Rule& rule,
    uint32_t w, uint32_t h, uint32_t channels)
{
    Entry variant = entry;
    variant.function = Function(rule.resource);
    Shape(rule, w, h, channels, variant);
    return original(list, &variant, 1);
}

inline bool Dl2Shaped(const Dl2Rule& rule, const Entry& entry)
{
    if (!entry.params || entry.paramBytes != rule.paramBytes) return false;
    for (const auto& [word, value] : rule.shape)
        if (word && Field<uint32_t>(entry.params, word * 4u) != value) return false;
    return true;
}

inline void Dl2Shape(const Dl2Rule& rule, const void* params, Entry& entry)
{
    const uint32_t h = Field<uint32_t>(params, rule.hWord * 4u), w = Field<uint32_t>(params, rule.wWord * 4u);
    if (rule.dy)
    {
        entry.grid[0] = Ceil(w, rule.dx);
        entry.grid[1] = Ceil(h, rule.dy);
        entry.grid[2] = rule.z;
    }
    else
    {
        entry.grid[0] = Ceil(uint64_t(w) * h, rule.dx);
        entry.grid[1] = rule.z;
        entry.grid[2] = 1;
    }
    entry.block[0] = rule.block;
    entry.block[1] = entry.block[2] = 1;
    entry.sharedBytes = 0;
}

// Held producer (merge or central block) + its conv: one fused launch. The
// producer's output is the conv's input; the fused kernel never writes it.
// Returns false, launching nothing, when the two do not chain.
inline bool Dl2Fuse(Launch original, void* list, const Dl2Rule& rule, const Entry& conv, bool central, int& status)
{
    const uint8_t* producer = t_state.dl2.params.data();
    const void* p = conv.params;
    const uint32_t ih = Field<uint32_t>(p, 44), iw = Field<uint32_t>(p, 48);
    // merge: out, 3 inputs, h, w, factor; central block: out, 6 inputs, w, h, factor.
    const size_t inputs = central ? 6 : 3;
    const uint32_t ph = Field<uint32_t>(producer, 8 + inputs * 8 + (central ? 4 : 0));
    const uint32_t pw = Field<uint32_t>(producer, 8 + inputs * 8 + (central ? 0 : 4));
    if (Field<uint64_t>(producer, 0) != Field<uint64_t>(p, 32) || ph != ih || pw != iw) return false;
    // weights, bias, the producer's inputs, the conv's output, then
    // in h, w, out h, w and the producer's blend factor.
    std::array<uint8_t, 92> params{};
    const uint32_t tail[5] = {ih, iw, Field<uint32_t>(p, 68), Field<uint32_t>(p, 72),
        Field<uint32_t>(producer, 16 + inputs * 8)};
    std::memcpy(params.data(), p, 16);
    std::memcpy(params.data() + 16, producer + 8, inputs * 8);
    std::memcpy(params.data() + 16 + inputs * 8, static_cast<const uint8_t*>(p) + 56, 8);
    std::memcpy(params.data() + 24 + inputs * 8, tail, sizeof(tail));
    Entry fused{};
    fused.function = Function(rule.resource);
    Dl2Shape(rule, p, fused);
    fused.params = params.data();
    fused.paramBytes = static_cast<uint32_t>(24 + inputs * 8 + sizeof(tail)); // 68 or 92
    t_state.held = Dl2::Count;
    status = original(list, &fused, 1);
    return true;
}

inline int Dl2Launch(Launch original, void* list, const Entry& entry, Dl2 layer)
{
    auto& s = t_state;
    const Dl2Rule& rule = kDl2Rules[size_t(layer)];
    if (layer == Dl2::InitialMerge || layer == Dl2::CentralBlock)
    {
        Flush(original);
        if (entry.params && entry.paramBytes == rule.paramBytes && Capture(entry, s.dl2))
        {
            s.held = layer;
            return 0;
        }
        return original(list, &entry, 1);
    }
    const Dl2 producer = layer == Dl2::ConvPre ? Dl2::InitialMerge
        : layer == Dl2::B1Conv0                ? Dl2::CentralBlock
                                               : Dl2::Count;
    if (producer != Dl2::Count)
    {
        int status = 0;
        if (s.held == producer && Dl2Shaped(rule, entry)
            && Dl2Fuse(original, list, rule, entry, producer == Dl2::CentralBlock, status))
        {
            if (layer == Dl2::ConvPre && optimizedDl2Runs.fetch_add(1, std::memory_order_relaxed) == 0)
                Log(L"Network optimization: first optimized DL2 run (%ux%u)", Field<uint32_t>(entry.params, 72),
                    Field<uint32_t>(entry.params, 68));
            return status;
        }
        Flush(original);
        return original(list, &entry, 1);
    }
    Flush(original);
    if (!Dl2Shaped(rule, entry)) return original(list, &entry, 1);
    Entry variant = entry;
    variant.function = Function(rule.resource);
    Dl2Shape(rule, entry.params, variant);
    return original(list, &variant, 1);
}

inline int __cdecl HookLaunch(void* list, const void* entries, uint32_t count)
{
    const auto original = launchOriginal.load(std::memory_order_acquire);
    auto& s = t_state;
    if (!ready.load(std::memory_order_acquire) || count != 1 || !entries || !Enabled())
    {
        Stop(original);
        return original(list, entries, count);
    }
    if (s.list != list)
    {
        Stop(original);
        s.list = list;
    }
    const Entry& entry = *static_cast<const Entry*>(entries);
    Role role;
    {
        std::lock_guard lock(mutex);
        const auto found = kinds.find(entry.function);
        if (found != kinds.end()) role = found->second;
    }
    const Kind kind = role.kind;
    if (kind == Kind::Dl2)
        return Dl2Launch(original, list, entry, role.layer);
    if (s.held != Dl2::Count) // a held DL2 producer not followed by its conv
        Flush(original);

    if (kind == Kind::NetworkStart)
    {
        Stop(original);
        s.active = true;
        s.conv = s.level = 0;
        return original(list, entries, count);
    }
    if (!s.active)
        return original(list, entries, count);

    switch (kind)
    {
    case Kind::Pool:
        Flush(original);
        if (entry.paramBytes == kPoolRule.paramBytes && entry.params)
            return LaunchVariant(original, list, entry, kPoolRule, Field<uint32_t>(entry.params, 24),
                Field<uint32_t>(entry.params, 28), Field<uint32_t>(entry.params, 32));
        break;
    case Kind::Upscale:
        Flush(original);
        if (++s.level <= 5 && entry.paramBytes == 40 && Capture(entry, s.upscale))
        {
            s.haveUpscale = true;
            return 0;
        }
        break;
    case Kind::ElementWise:
        if (s.haveUpscale && !s.haveAdd && entry.paramBytes == 52 && Capture(entry, s.add)
            && Field<uint64_t>(s.add.params.data(), 0) == Field<uint64_t>(s.upscale.params.data(), 8))
        {
            s.haveAdd = true;
            return 0;
        }
        break;
    case Kind::Conv:
    {
        if (s.conv >= 17 || entry.paramBytes != kConvRules[s.conv].paramBytes || !entry.params) break;
        const Rule& rule = kConvRules[s.conv++];
        const uint32_t w = Field<uint32_t>(entry.params, entry.paramBytes - 8);
        const uint32_t h = Field<uint32_t>(entry.params, entry.paramBytes - 4);
        if (rule.resource)
        {
            if (s.haveUpscale || s.haveAdd) break;
            return LaunchVariant(original, list, entry, rule, w, h, 0);
        }
        // The conv after a decoder add: fuse upscale + add + conv.
        if (!s.haveUpscale || !s.haveAdd || s.level < 1 || s.level > 5) break;
        const uint8_t* up = s.upscale.params.data();
        const uint8_t* add = s.add.params.data();
        const uint32_t lh = Field<uint32_t>(up, 16), lw = Field<uint32_t>(up, 20);
        const uint32_t hh = Field<uint32_t>(up, 24), hw = Field<uint32_t>(up, 28);
        const bool chained = Field<uint64_t>(entry.params, 8) == Field<uint64_t>(add, 16)
            && w == hw && h == hh && Field<uint32_t>(add, 24) == hw && Field<uint32_t>(add, 28) == hh
            && hw == 2 * lw && hh == 2 * lh;
        if (!chained) break;
        struct Params { uint64_t weights, low, skip, out; uint32_t lw, lh, hw, hh; } params{
            Field<uint64_t>(entry.params, 0), Field<uint64_t>(up, 0), Field<uint64_t>(add, 8),
            Field<uint64_t>(entry.params, 24), lw, lh, hw, hh};
        static_assert(sizeof(Params) == 48);
        const Rule& fusedRule = kFusedRules[s.level - 1];
        Entry fused{};
        fused.function = Function(fusedRule.resource);
        Shape(fusedRule, hw, hh, 0, fused);
        fused.params = &params;
        fused.paramBytes = sizeof(params);
        s.haveUpscale = s.haveAdd = false;
        if (optimizedRuns.fetch_add(1, std::memory_order_relaxed) == 0)
            Log(L"Network optimization: first optimized DL1 run (decoder level %d fused, %ux%u)", s.level, hw, hh);
        return original(list, &fused, 1);
    }
    case Kind::NetworkEnd:
        Stop(original);
        return original(list, entries, count);
    default:
        break;
    }
    // Anything unexpected: replay what is held, run this launch as is, and
    // leave the rest of this network run untouched.
    Stop(original);
    return original(list, entries, count);
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
    if (!function || !midpoint_fix::OptimizedKernelsEnabled()) return function;
    if (id == 0xE2436E22u) return Wrap(functionOriginal, function, reinterpret_cast<void*>(&HookFunction));
    if (id == 0x24973538u) return Wrap(launchOriginal, function, reinterpret_cast<void*>(&HookLaunch));
    return function;
}
}
