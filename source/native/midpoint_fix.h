#pragma once

#include <Windows.h>

#include <cstdint>

namespace midpoint_fix
{
using LogCallback = void (*)(const wchar_t* message);

void SetLogCallback(LogCallback callback) noexcept;
void SetBlackwellTransfusionEnabled(bool enabled) noexcept;
// Provider PTX is patched at load time; changing this requires a game restart.
void SetQualityFixEnabled(bool enabled) noexcept;
bool IsBlackwellTransfusionActive() noexcept;
void SetMvDilationDisabled(bool disabled) noexcept;
bool IsMvDilationDisabled() noexcept;
size_t GetTransfusedFatbinCount() noexcept;
size_t GetTransfusedDescriptorCount() noexcept;
bool ObserveD3D12Device(void* device) noexcept;
bool ObserveVulkanPhysicalDevice(void* physicalDevice) noexcept;
bool PatchProvider(HMODULE module, const wchar_t* path) noexcept;
bool AdapterVerified() noexcept;
bool Ready() noexcept;
uint32_t FailureCode() noexcept;
}

