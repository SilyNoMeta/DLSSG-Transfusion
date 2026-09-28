#pragma once
// Exact image-kernel optimizations for the DLSS-G 310.9.1 Blackwell kernels
// (the sm_120 PTX Transfusion already retargets). Recovered from the
// dlssg_for_sm86 0.3.5 backend; output is bit-identical to NVIDIA's kernels.
//
//   Kernel_BlendCandidatesFused  pairs of contiguous 32-bit stores become one
//                                st.global.v2.u32 (applied on top of the
//                                valid-warp policy, same registers)
//   Kernel_OutputPull            inpainting masks packed per row into bits in
//                                shared memory, neighbourhood tests by bits
//   Kernel_OutputPushFine        collective work test, early exit of blocks
//                                with no contribution
//
// OutputPull/OutputPushFine are replaced only when the provider's PTX is the
// exact NVIDIA kernel they were derived from (hash below, .target line
// excluded); otherwise nothing changes.
#include <cstdint>
#include <string>
#include <string_view>

// The replacement PTX lives in private/image_kernels_ptx.h, which the public
// source tree does not contain (docs/PUBLIC-SOURCE.md). Without it the
// provider's OutputPull and OutputPushFine run unchanged; the Blend store
// vectorization below needs no payload and stays active.
#if __has_include("private/image_kernels_ptx.h")
#include "private/image_kernels_ptx.h"
#define DLSSG_IMAGE_KERNEL_REPLACEMENTS 1
#endif

namespace image_kernels
{
#ifdef DLSSG_IMAGE_KERNEL_REPLACEMENTS
inline constexpr bool kHaveReplacements = true;
#else
inline constexpr bool kHaveReplacements = false;
inline constexpr std::string_view kOutputPull{};
inline constexpr std::string_view kOutputPushFine{};
#endif

inline constexpr uint64_t kOutputPullSourceHash = 0xf4194a9d30dea518ull;     // NVIDIA 310.9.1 sm_120
inline constexpr uint64_t kOutputPushFineSourceHash = 0x3a728f2af0a08fc8ull; // NVIDIA 310.9.1 sm_120

// FNV-1a over the PTX with CR, trailing NULs and the first .target line removed,
// so a copy retargeted in place hashes like the original.
inline uint64_t SourceHash(std::string_view ptx)
{
    std::string text;
    text.reserve(ptx.size());
    for (char c : ptx) if (c != '\r') text.push_back(c);
    while (!text.empty() && text.back() == '\0') text.pop_back();
    const size_t target = text.find(".target ");
    if (target != std::string::npos)
    {
        const size_t end = text.find('\n', target);
        text.erase(target, end == std::string::npos ? std::string::npos : end - target + 1);
    }
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : text) hash = (hash ^ c) * 1099511628211ull;
    return hash;
}

struct StorePair { const char* base; const char* first; const char* second; };
inline constexpr StorePair kBlendStorePairs[] = {
    {"%rd83", "%r553", "%r554"},
    {"%rd84", "%r555", "%r556"},
    {"%rd72", "%r543", "%r544"},
    {"%rd73", "%r545", "%r546"},
    {"%rd61", "%r491", "%r492"},
    {"%rd62", "%r493", "%r494"},
    {"%rd50", "%r382", "%r383"},
    {"%rd51", "%r384", "%r385"},
    {"%rd39", "%r234", "%r235"},
    {"%rd40", "%r236", "%r237"}
};

// Merges the ten store pairs; all ten must be found exactly once, in order,
// or the text is left unchanged.
inline bool VectorizeBlendStores(std::string& ptx)
{
    std::string text = ptx;
    for (const auto& pair : kBlendStorePairs)
    {
        const std::string first = std::string("st.global.u32 [") + pair.base + "], " + pair.first + ";";
        const std::string second = std::string("st.global.u32 [") + pair.base + "+4], " + pair.second + ";";
        const size_t a = text.find(first);
        const size_t b = text.find(second);
        if (a == std::string::npos || b == std::string::npos || b < a
            || text.find(first, a + 1) != std::string::npos || text.find(second, b + 1) != std::string::npos)
            return false;
        const std::string merged = std::string("st.global.v2.u32 [") + pair.base + "], {" + pair.first + ", " + pair.second + "};";
        text.replace(b, second.size(), merged);
        text.erase(a, first.size());
    }
    ptx.swap(text);
    return true;
}
}
