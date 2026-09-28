#pragma once
/*
 * Generic live control of Unreal Engine's r.ScreenPercentage (UE4 / UE5).
 *
 * Unreal decides its render resolution from r.ScreenPercentage and does not
 * ask DLSS for it at runtime, so the NGX optimal-settings override has no
 * effect there. Every Unreal build declares the variable the same way:
 *
 *   static TAutoConsoleVariable<float> CVarScreenPercentage(TEXT("r.ScreenPercentage"), ...);
 *
 * whose static initializer loads the address of the UTF-16 name
 * (lea reg, [rip+name]) and either stores the registered object and its data
 * pointer into two adjacent slots of the static object ({vptr, Target, Ref},
 * constructor inlined), or passes the static object to the constructor
 * (lea rcx, [rip+object] next to the name).
 * Ref points to TConsoleVariableData<float> = { float ShadowedValue[2]; },
 * the game-thread and render-thread copies read by the renderer every frame.
 *
 * We locate that code by pattern (no per-game address), validate the slots at
 * runtime (both pointers inside readable memory, the object's vtable inside
 * the executable, both values a plausible percentage), then write the two
 * shadowed values when a DLSS render scale is configured, and restore the
 * game's value when it is cleared. Anything that does not validate is left
 * alone: the result is visible in the panel (found / not found).
 */
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

namespace unreal_screen_percentage
{
enum class State : uint32_t { Searching = 0, NotFound = 1, Found = 2 };

inline std::atomic<State> g_state{State::Searching};
inline std::atomic<float> g_current{0.f};  // last value read (percent)
inline void (*g_log)(const char* text) = nullptr;

namespace internal
{
inline uint8_t* g_base = nullptr;
inline size_t g_size = 0;
inline std::vector<uintptr_t> g_candidates;  // Target slot addresses (Ref slot = +8)
inline float* g_data = nullptr;              // ShadowedValue[2]
inline bool g_owned = false;
inline float g_original = 0.f, g_last = 0.f;

// Copy through the OS so an unreadable page is a failed read, not an AV.
inline bool Copy(const void* source, void* destination, size_t length)
{
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), source, destination, length, &copied) && copied == length;
}

inline bool Readable(const void* address, size_t length)
{
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT
        || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return false;
    const auto end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return reinterpret_cast<uintptr_t>(address) + length <= end;
}

inline bool InImage(uintptr_t address)
{
    return address >= reinterpret_cast<uintptr_t>(g_base) && address < reinterpret_cast<uintptr_t>(g_base) + g_size;
}

struct Section
{
    uint8_t* start;
    size_t size;
    bool code;
};

inline std::vector<Section> Sections()
{
    std::vector<Section> sections;
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!Copy(g_base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || !Copy(g_base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE)
        return sections;
    g_size = nt.OptionalHeader.SizeOfImage;
    const auto* first = reinterpret_cast<const uint8_t*>(g_base + dos.e_lfanew)
        + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    for (unsigned i = 0; i < nt.FileHeader.NumberOfSections && i < 96; ++i)
    {
        IMAGE_SECTION_HEADER header{};
        if (!Copy(first + i * sizeof(header), &header, sizeof(header)))
            break;
        const size_t size = header.Misc.VirtualSize ? header.Misc.VirtualSize : header.SizeOfRawData;
        if (!size || header.VirtualAddress + size > g_size)
            continue;
        sections.push_back({g_base + header.VirtualAddress, size, (header.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0});
    }
    return sections;
}

// Calls visit(chunk, chunkSize, chunkStart) over a section, 4 MB at a time,
// with enough overlap for a 64-byte pattern window.
template <typename Visit>
void ForEachChunk(const Section& section, Visit visit)
{
    constexpr size_t kChunk = 4u << 20, kOverlap = 256;
    std::vector<uint8_t> buffer(kChunk + kOverlap);
    for (size_t offset = 0; offset < section.size; offset += kChunk)
    {
        const size_t length = (std::min)(kChunk + kOverlap, section.size - offset);
        if (!Copy(section.start + offset, buffer.data(), length))
            continue;  // unreadable (still packed): skipped
        visit(buffer.data(), length, section.start + offset);
    }
}

// True for UTF-16 text shaped like a console variable name: "r.X", "sg.X", "fx.X"...
inline bool IsConsoleVariableName(uintptr_t address)
{
    wchar_t text[8]{};
    if (!InImage(address) || !Copy(reinterpret_cast<void*>(address), text, sizeof(text)))
        return false;
    size_t prefix = 0;
    while (prefix < 6 && text[prefix] >= L'a' && text[prefix] <= L'z')
        ++prefix;
    return prefix >= 1 && text[prefix] == L'.'
        && ((text[prefix + 1] >= L'A' && text[prefix + 1] <= L'Z') || (text[prefix + 1] >= L'a' && text[prefix + 1] <= L'z'));
}

// Finds the static initializer(s) of r.ScreenPercentage. Returns false when the
// executable does not look like an Unreal build that declares it.
inline bool Locate()
{
    g_base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    const std::vector<Section> sections = Sections();
    if (sections.empty())
        return false;

    // 1. The exact UTF-16 name, NUL-terminated and not the tail of a longer name.
    static const wchar_t kName[] = L"r.ScreenPercentage";
    constexpr size_t kNameBytes = sizeof(kName);  // includes the terminator
    std::vector<uintptr_t> names;
    for (const Section& section : sections)
    {
        if (section.code)
            continue;
        ForEachChunk(section, [&](const uint8_t* data, size_t length, uint8_t* start)
        {
            for (size_t i = 2; i + kNameBytes <= length; i += 2)
                if (data[i] == 'r' && std::memcmp(data + i, kName, kNameBytes) == 0 && data[i - 1] == 0 && data[i - 2] == 0)
                {
                    const uintptr_t address = reinterpret_cast<uintptr_t>(start + i);
                    if (std::find(names.begin(), names.end(), address) == names.end())
                        names.push_back(address);
                }
        });
    }
    if (names.empty())
        return false;

    // 2. lea r64, [rip+name], then either two REX.W mov [rip+slot], r64 to
    //    adjacent slots (inlined constructor), or lea rcx, [rip+object] next to
    //    it (object passed to the constructor): Target slot = object + 8.
    for (const Section& section : sections)
    {
        if (!section.code)
            continue;
        ForEachChunk(section, [&](const uint8_t* data, size_t length, uint8_t* start)
        {
            for (size_t i = 0; i + 7 <= length; ++i)
            {
                if ((data[i] != 0x48 && data[i] != 0x4C) || data[i + 1] != 0x8D || (data[i + 2] & 0xC7) != 0x05)
                    continue;
                int32_t displacement = 0;
                std::memcpy(&displacement, data + i + 3, 4);
                const uintptr_t target = reinterpret_cast<uintptr_t>(start + i + 7) + displacement;
                if (std::find(names.begin(), names.end(), target) == names.end())
                    continue;
                const auto add = [](uintptr_t slot)
                {
                    if (std::find(g_candidates.begin(), g_candidates.end(), slot) == g_candidates.end())
                        g_candidates.push_back(slot);
                };
                // Inlined constructor: adjacent slots stored after the name, up to
                // the next console variable's initializer (Unreal registers many in
                // a row). {vptr, Target} and {Target, Ref} both pair up; Resolve()
                // keeps the one whose values validate.
                size_t windowEnd = (std::min)(length, i + 7 + 0x180);
                for (size_t j = i + 7; j + 7 <= windowEnd; ++j)
                {
                    if ((data[j] != 0x48 && data[j] != 0x4C) || data[j + 1] != 0x8D || (data[j + 2] & 0xC7) != 0x05)
                        continue;
                    int32_t other = 0;
                    std::memcpy(&other, data + j + 3, 4);
                    if (IsConsoleVariableName(reinterpret_cast<uintptr_t>(start + j + 7) + other))
                    {
                        windowEnd = j;
                        break;
                    }
                }
                std::vector<uintptr_t> stores;
                for (size_t j = i + 7; j + 7 <= windowEnd; ++j)
                {
                    if ((data[j] != 0x48 && data[j] != 0x4C) || data[j + 1] != 0x89 || (data[j + 2] & 0xC7) != 0x05)
                        continue;
                    int32_t slot = 0;
                    std::memcpy(&slot, data + j + 3, 4);
                    const uintptr_t address = reinterpret_cast<uintptr_t>(start + j + 7) + slot;
                    if (!InImage(address))
                        continue;
                    for (uintptr_t previous : stores)
                        if (address == previous + 8 || previous == address + 8)
                            add((std::min)(address, previous));
                    stores.push_back(address);
                }
                // Constructor called with the static object: lea rcx, [rip+object]
                // among the same call's arguments, i.e. between the previous and
                // the next call instruction around the name.
                size_t blockStart = i > 0x40 ? i - 0x40 : 0, blockEnd = (std::min)(length, i + 0x40);
                for (size_t j = i; j > blockStart; --j)
                    if (data[j - 1] == 0xE8 && j - 1 + 5 <= i) { blockStart = j - 1 + 5; break; }
                for (size_t j = i + 7; j + 5 <= blockEnd; ++j)
                    if (data[j] == 0xE8) { blockEnd = j; break; }
                for (size_t j = blockStart; j + 7 <= blockEnd; ++j)
                {
                    if (j == i || data[j] != 0x48 || data[j + 1] != 0x8D || data[j + 2] != 0x0D)  // lea rcx, [rip+x]
                        continue;
                    int32_t object = 0;
                    std::memcpy(&object, data + j + 3, 4);
                    const uintptr_t address = reinterpret_cast<uintptr_t>(start + j + 7) + object;
                    if (InImage(address) && std::find(names.begin(), names.end(), address) == names.end())
                        add(address + 8);
                }
            }
        });
    }
    return !g_candidates.empty();
}

inline bool Plausible(float value) { return value >= 1.f && value <= 400.f; }

// Once the static initializers ran: picks the candidate whose slots hold a
// registered console variable with a plausible percentage.
inline float* Resolve()
{
    for (uintptr_t slot : g_candidates)
    {
        uintptr_t target = 0, ref = 0, vtable = 0;
        float values[2]{};
        if (!Copy(reinterpret_cast<void*>(slot), &target, 8) || !Copy(reinterpret_cast<void*>(slot + 8), &ref, 8)
            || !target || !ref || InImage(target) || !Copy(reinterpret_cast<void*>(target), &vtable, 8) || !InImage(vtable)
            || !Readable(reinterpret_cast<void*>(ref), sizeof(values)) || !Copy(reinterpret_cast<void*>(ref), values, sizeof(values))
            || !Plausible(values[0]) || !Plausible(values[1])
            || ref <= target || ref - target > 0x400)  // the data lives inside the console object
            continue;
        return reinterpret_cast<float*>(ref);
    }
    return nullptr;
}
} // namespace internal

// Worker thread, every 250 ms. `scale` is percent * 1000 (0 = the game decides).
inline void Tick(unsigned scale)
{
    using namespace internal;
    static uint32_t attempts = 0;
    static bool located = false;
    const State state = g_state.load(std::memory_order_relaxed);
    if (state == State::NotFound)
        return;
    if (state == State::Searching)
    {
        // Every 5 s for 3 minutes: packed code and late static initializers.
        if (attempts++ % 20 != 0)
            return;
        // The declaration is searched for 30 s (packed code), the live object for 3 minutes.
        if (!located && attempts <= 20 * 6)
            located = Locate();
        if (located)
            g_data = Resolve();
        if (g_data)
        {
            g_state.store(State::Found);
            if (g_log) g_log("[UE] r.ScreenPercentage found: live DLSS render-resolution control available.");
        }
        else if ((!located && attempts > 20 * 6) || attempts > 20 * 36)
        {
            g_state.store(State::NotFound);
            if (g_log) g_log(located ? "[UE] r.ScreenPercentage declaration found but never validated; live control disabled."
                                     : "[UE] Not an Unreal Engine game with r.ScreenPercentage; live control disabled.");
        }
        return;
    }

    float current[2]{};
    if (!Copy(g_data, current, sizeof(current)) || !Plausible(current[0]))
        return;
    g_current.store(current[0], std::memory_order_relaxed);
    const auto write = [](float value)
    {
        float values[2] = {value, value};
        SIZE_T written = 0;
        WriteProcessMemory(GetCurrentProcess(), g_data, values, sizeof(values), &written);
    };
    if (scale)
    {
        const float desired = scale / 1000.f;
        if (!g_owned || current[0] != g_last)
            g_original = current[0];  // first time, or the game changed it since
        if (current[0] != desired || current[1] != desired)
            write(desired);
        g_last = desired;
        g_owned = true;
    }
    else if (g_owned)
    {
        if (current[0] == g_last)
            write(g_original);
        g_owned = false;
    }
}
} // namespace unreal_screen_percentage
