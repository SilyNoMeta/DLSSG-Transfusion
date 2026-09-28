#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace smooth_motion_fatbin
{
enum class Result { pass, rewritten, rejected };

inline uint16_t U16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }
inline uint32_t U32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
inline uint64_t U64(const uint8_t* p) { uint64_t v; std::memcpy(&v, p, 8); return v; }
inline void W32(uint8_t* p, uint32_t v) { std::memcpy(p, &v, 4); }

inline bool HasName(const uint8_t* data, size_t length, std::string_view name)
{
    const auto* first = reinterpret_cast<const char*>(data);
    const auto* last = first + length;
    auto* it = first;
    while ((it = std::search(it, last, name.begin(), name.end())) != last)
    {
        const bool left = it == first || (it[-1] != '_' && !(it[-1] >= '0' && it[-1] <= '9')
            && !(it[-1] >= 'A' && it[-1] <= 'Z') && !(it[-1] >= 'a' && it[-1] <= 'z'));
        const char* end = it + name.size();
        const bool right = end == last || (*end != '_' && !(*end >= '0' && *end <= '9')
            && !(*end >= 'A' && *end <= 'Z') && !(*end >= 'a' && *end <= 'z'));
        if (left && right) return true;
        ++it;
    }
    return false;
}

inline std::string_view FindFp16Kernel(const uint8_t* data, size_t length)
{
    constexpr std::string_view names[] = {
        "conv1", "conv2", "conv3", "conv4", "conv5", "conv6", "conv7", "conv8",
        "conv_fused", "conv_proj1", "conv_proj2", "conv_out1", "conv_out2", "conv_out3",
        "attn1", "attn2", "depth_to_space", "downscale_kernel", "warp_coarse_kernel", "main_kernel"
    };
    for (const auto name : names)
        if (HasName(data, length, name)) return name;
    return {};
}

// The caller supplies the exact readable container length from its header.
// Every byte except the SM89 entry arch and ELF e_flags arch byte is preserved.
inline Result Rewrite(const uint8_t* input, size_t size, std::vector<uint8_t>& output,
    std::string_view* kernelName = nullptr)
{
    output.clear();
    if (kernelName) *kernelName = {};
    if (!input || size < 16 || U32(input) != 0xba55ed50u) return Result::pass;
    const size_t header = U16(input + 6);
    const uint64_t payload = U64(input + 8);
    if (header < 16 || header > 256 || header > size
        || payload > size - header || header + payload != size)
        return Result::rejected;
    output.assign(input, input + size);
    size_t cursor = header;
    unsigned rewritten = 0;
    while (cursor < size)
    {
        if (size - cursor < 32) return Result::rejected;
        const uint16_t kind = U16(input + cursor);
        const size_t entryHeader = U32(input + cursor + 4);
        const size_t dataSize = U32(input + cursor + 8);
        const uint32_t arch = U32(input + cursor + 28);
        if (entryHeader < 32 || entryHeader > 1024 || entryHeader > size - cursor)
            return Result::rejected;
        const size_t dataStart = cursor + entryHeader;
        if (dataSize > size - dataStart) return Result::rejected;
        if (kind == 2 && arch == 89)
        {
            const auto* elf = input + dataStart;
            if (dataSize < 0x34 || U32(elf) != 0x464c457fu || elf[4] != 2)
                return Result::rejected;
            constexpr std::string_view fp8Suffix = "_fp8";
            const auto* begin = reinterpret_cast<const char*>(elf);
            const bool fp8 = std::search(begin, begin + dataSize, fp8Suffix.begin(), fp8Suffix.end())
                != begin + dataSize;
            if (fp8) return Result::rejected;
            const auto name = FindFp16Kernel(elf, dataSize);
            if (!name.empty())
            {
                const uint32_t flags = U32(elf + 0x30);
                if (((flags >> 8) & 0xffu) != 89) return Result::rejected;
                W32(output.data() + cursor + 28, 86);
                W32(output.data() + dataStart + 0x30, (flags & ~0xff00u) | 0x5600u);
                if (kernelName) *kernelName = name;
                ++rewritten;
            }
        }
        const size_t next = (dataStart + dataSize + 7u) & ~size_t(7u);
        if (next <= cursor || next > size) return Result::rejected;
        cursor = next;
    }
    if (cursor != size || rewritten > 1) return Result::rejected;
    if (!rewritten) output.clear();
    return rewritten ? Result::rewritten : Result::pass;
}
}
