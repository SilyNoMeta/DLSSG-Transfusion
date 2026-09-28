#pragma once

#include <Windows.h>
#include <atomic>
#include <string_view>

namespace multiplier_overlay
{
enum class Position : uint32_t
{
    TopLeft = 0,
    TopRight = 1,
    BottomRight = 2,
    BottomLeft = 3
};

// Installs process-wide DXGI hooks. The displayed value is read from the
// authoritative DLSS-G state atomics on every native Present.
bool Install(const std::atomic<uint32_t>* actualMultiplier,
    const std::atomic<uint32_t>* appliedMultiplier,
    const std::atomic<bool>* frameGenerationOn,
    const std::atomic<uint64_t>* frameGenerationSession,
    const std::atomic<uint64_t>* stateSampleTick);
// Optional lines drawn under the FPS/multiplier line (uppercase letters,
// digits, ". : - /" and spaces). Called on Present while the overlay is shown.
constexpr size_t kMaxExtraLines = 8;
constexpr size_t kExtraLineLength = 48;
using ExtraLinesFn = size_t (*)(char (*lines)[kExtraLineLength], size_t maxLines);
void SetExtraLines(ExtraLinesFn provider);
using LogFn = void (*)(const char* text);
void SetLog(LogFn log);
// Pacing of displayed frames over the last ~512 presents (FG on only).
struct PacingStats
{
    uint32_t averageUs = 0;
    uint32_t p99Us = 0;     // 99th percentile frame interval
    uint32_t jitterUs = 0;  // standard deviation
};
bool GetPacing(PacingStats& stats);
// GetTickCount64 of the last overlay actually drawn through DXGI (0: never).
// The ReShade add-on draws the overlay itself when this one cannot (Vulkan).
uint64_t LastDrawTick();
void SetVisible(bool visible);
bool IsVisible();
void SetPosition(Position pos);
Position GetPosition();
void CyclePosition();
const char* PositionToString(Position pos);
Position PositionFromString(std::string_view str);
void Uninstall();
}

