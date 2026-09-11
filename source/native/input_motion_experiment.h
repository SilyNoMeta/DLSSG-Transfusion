#pragma once
#include "scatter_experiment.h"
namespace input_motion_experiment {
inline bool Patch(std::string& ptx, std::string& why)
{
    if constexpr (scatter_experiment::kMode != 3) { why = "stock input motion"; return true; }
    std::string text = ptx;
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    while (!text.empty() && text.back() == '\0') text.pop_back();
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : text) hash = (hash ^ c) * 1099511628211ull;
    if (text.size() != 8068 || hash != 0x17144f73201965f5ull)
    { why = "unknown input motion PTX"; return false; }
    if (!scatter_experiment::ReplaceOnce(text, ".reg .pred %p<34>;", ".reg .pred %p<35>;") ||
        !scatter_experiment::ReplaceOnce(text, ".reg .f32 %f<177>;", ".reg .f32 %f<178>;"))
    { why = "input register declaration mismatch"; return false; }
    // Existing linearized center depth is f94. A diagonal must be less
    // than 0.98 * center depth as well as winning the original comparison.
    // Preserve selected depth, encoded diagonal offset and motion lookup together.
    for (int i = 0; i < 4; ++i)
    {
        const int predicates[] = {17, 19, 20, 22};
        const int candidate[] = {88, 102, 118, 132};
        const int current[] = {94, 108, 124, 138};
        const auto pred = "%p" + std::to_string(predicates[i]);
        const auto depth = "%f" + std::to_string(candidate[i]);
        const auto line = "setp.geu.ftz.f32 " + pred + ", " + depth + ", %f" + std::to_string(current[i]) + ";\n";
        const auto extra = "mul.ftz.f32 %f177, %f94, 0f3F7AE148;\nsetp.geu.ftz.f32 %p34, " + depth + ", %f177;\nor.pred " + pred + ", " + pred + ", %p34;\n";
        if (!scatter_experiment::ReplaceOnce(text, line, line + extra))
        { why = "input depth selection site mismatch"; return false; }
    }
    ptx = std::move(text);
    why = "C: diagonal selection requires 2 percent closer depth";
    return true;
}
}
