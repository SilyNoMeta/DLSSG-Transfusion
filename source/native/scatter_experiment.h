#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#ifndef SCATTER_EXPERIMENT
#define SCATTER_EXPERIMENT 0
#endif
namespace scatter_experiment {
static_assert(SCATTER_EXPERIMENT >= 0 && SCATTER_EXPERIMENT <= 7);
inline constexpr int kMode = SCATTER_EXPERIMENT;
inline constexpr const char* kName = kMode == 1 ? "quality-v4-A-consistency"
    : kMode == 7 ? "v1.4.0-pure-warp"
    : kMode == 6 ? "quality-v4-E2-geometric-warp"
    : kMode == 5 ? "quality-v4-E1-relaxed-agreement"
    : kMode == 4 ? "quality-v4-D-working-1080p"
    : kMode == 3 ? "quality-v4-C-center-depth"
    : kMode == 2 ? "quality-v4-B-spread" : "v1.4.0-pure-warp";
inline constexpr const char* kPolicy = kMode == 1 ? "A: squared motion-error threshold x0.5"
    : kMode == 2 ? "B: neighborhood expansion cap 5->3"
    : kMode == 7 ? "E3: pure 100% warped motion"
    : kMode == 6 ? "E2: geometric-only warp boost"
    : kMode == 5 ? "E1: relaxed agreement threshold 25%" : "stock scatter rejection";
inline bool ReplaceOnce(std::string& text, const std::string& from, const std::string& to)
{
    const auto pos = text.find(from);
    if (pos == std::string::npos || text.find(from, pos + from.size()) != std::string::npos) return false;
    text.replace(pos, from.size(), to);
    return true;
}
inline bool Patch(std::string& ptx, std::string& why)
{
    if constexpr (kMode == 0 || kMode == 3 || kMode == 4 || kMode == 5 || kMode == 6 || kMode == 7) { why = kPolicy; return true; }
    std::string text = ptx;
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    while (!text.empty() && text.back() == '\0') text.pop_back();
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : text) hash = (hash ^ c) * 1099511628211ull;
    if (text.size() != 90731 || hash != 0xb1a2811b29625d41ull)
    { why = "experiment skipped: unknown scatter PTX"; return false; }
    if constexpr (kMode == 1)
    {
        // Halve the squared error tolerance in both directions. Preserve every
        // depth predicate and its original AND/branch structure, including the
        // branches that reject motion without a separate depth condition.
        for (int direction = 0; direction < 2; ++direction)
            for (int i = 0; i < 25; ++i)
            {
                const int reg = direction == 0 ? 184 + 26*i : 956 + 12*i;
                const auto output = "%f" + std::to_string(reg);
                const std::string line = "max.ftz.f32 " + output + ", %f" + std::to_string(reg-1) + ", 0f3F800000;\n";
                if (!ReplaceOnce(text, line, line + "mul.ftz.f32 " + output + ", " + output + ", 0f3F000000;\n"))
                { why = "experiment skipped: consistency site mismatch"; return false; }
            }
    }
    else if constexpr (kMode == 2)
    {
        // Only limit expansion. The existing minimum of 2, depth-dependent
        // expansion decision, motion rejection and coordinate bounds remain.
        for (int reg : {108, 110, 656, 658})
        {
            const std::string prefix = "min.s32 %r" + std::to_string(reg) + ", %r" + std::to_string(reg-1);
            if (!ReplaceOnce(text, prefix + ", 5;", prefix + ", 3;"))
            { why = "experiment skipped: spread site mismatch"; return false; }
        }
    }
    ptx = std::move(text);
    why = kPolicy;
    return true;
}
}
