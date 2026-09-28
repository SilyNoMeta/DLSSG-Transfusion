#pragma once

#include <Windows.h>

namespace smooth_motion_sm86
{
using LogCallback = void (*)(const wchar_t*);
enum class ApiMode { D3D12, D3D11, Vulkan };

// Experimental, process-local enablement for the inspected 617.14 driver.
bool Initialize(LogCallback log, ApiMode mode = ApiMode::D3D12) noexcept;
bool Active() noexcept;
// Call only on the first LoadLibrary return for nvoglv64.dll. Refuses every
// build except the inspected 617.14 module and never touches the disk file.
bool TryForceVulkanProfileGate(HMODULE module) noexcept;
}
