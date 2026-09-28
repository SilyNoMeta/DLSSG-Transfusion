#pragma once

#include <Windows.h>

#include <cstdint>
#include <vector>

namespace midpoint_fix
{
using LogCallback = void (*)(const wchar_t* message);

void SetLogCallback(LogCallback callback) noexcept;
void SetBlackwellTransfusionEnabled(bool enabled) noexcept;
// PTX target for rebuilt and retargeted kernels: 89 (Ada), 86 (Ampere) or 75 (Turing).
void SetTargetSm(uint32_t sm) noexcept;
// Exact optimized image kernels (Blend vector stores, OutputPull, OutputPushFine),
// substituted when the provider's kernel is the one they were derived from.
void SetOptimizedKernels(bool enabled) noexcept;
bool OptimizedKernelsEnabled() noexcept;
// True once a provider kernel matched the exact DLSS-G 310.9.1 build the
// optimized network kernels were derived from.
bool ExactProviderKernels() noexcept;
// Provider PTX is patched at load time; changing this requires a game restart.
void SetQualityFixEnabled(bool enabled) noexcept;
// Valid-warp tuning: false = Transfusion's (default), true = dlssg_for_sm86's. Load-time setting.
void SetQualityPolicyExplainedWarp(bool explainedWarp) noexcept;
bool IsBlackwellTransfusionActive() noexcept;
void SetMvDilationDisabled(bool disabled) noexcept;
bool IsMvDilationDisabled() noexcept;
size_t GetTransfusedFatbinCount() noexcept;
size_t GetTransfusedDescriptorCount() noexcept;
bool ObserveD3D12Device(void* device) noexcept;
bool ObserveVulkanPhysicalDevice(void* physicalDevice) noexcept;
bool PatchProvider(HMODULE module, const wchar_t* path) noexcept;
// Below sm_80 (Turing) some provider PTX must be lowered before the driver
// JIT-compiles it. Returns true with a replacement fatbin for such an image.
bool PrepareModuleImage(const void* blob, size_t size, std::vector<uint8_t>& replacement) noexcept;
bool AdapterVerified() noexcept;
bool Ready() noexcept;
uint32_t FailureCode() noexcept;
}

