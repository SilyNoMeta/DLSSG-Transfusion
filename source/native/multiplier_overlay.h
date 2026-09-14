#pragma once

#include <Windows.h>
#include <atomic>

namespace multiplier_overlay
{
// Installs process-wide DXGI hooks. The displayed value is read from the
// authoritative DLSS-G state atomics on every native Present.
bool Install(const std::atomic<uint32_t>* actualMultiplier,
    const std::atomic<uint32_t>* appliedMultiplier,
    const std::atomic<bool>* frameGenerationOn,
    const std::atomic<uint64_t>* frameGenerationSession,
    const std::atomic<uint64_t>* stateSampleTick);
void SetVisible(bool visible);
bool IsVisible();
void Uninstall();
}
