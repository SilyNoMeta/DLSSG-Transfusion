#pragma once

#include <cstdint>

namespace pacing_policy
{
// Modern Streamline pacing is the native/default path and needs no memory patch.
// Only an explicit request for legacy software-flip compatibility makes
// the old metering-field patch a prerequisite for raising the multiplier.
inline constexpr bool IsReady(bool legacySoftwareFlipRequested,
                              bool legacyPatchApplied)
{
    return !legacySoftwareFlipRequested || legacyPatchApplied;
}

// Reflex expresses its limiter as an integer frame interval in microseconds.
// Round to nearest instead of truncating so common targets (60/100/120/144)
// do not acquire a systematic high-FPS bias.
inline constexpr uint32_t TargetFpsToFrameLimitUs(uint32_t targetFps)
{
    if (targetFps == 0) return 0;
    return static_cast<uint32_t>((1000000ull + targetFps / 2u) / targetFps);
}

inline constexpr bool ShouldApplyReflexSourceCap(
    bool dynamicEnabled, bool d3d12, bool supportSeen, bool supported,
    bool dynamicApplied, bool explicitSourceCap, uint32_t targetFps)
{
    return dynamicEnabled && d3d12 && supportSeen && supported &&
           dynamicApplied && explicitSourceCap && targetFps != 0;
}
} // namespace pacing_policy
