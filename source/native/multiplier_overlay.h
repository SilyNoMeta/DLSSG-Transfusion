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
void SetVisible(bool visible);
bool IsVisible();
void SetPosition(Position pos);
Position GetPosition();
void CyclePosition();
const char* PositionToString(Position pos);
Position PositionFromString(std::string_view str);
void Uninstall();
}

