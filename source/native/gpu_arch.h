#pragma once

#include <Windows.h>

#include <cstdint>

// Which NVIDIA architecture the DLSS-G provider patches target.
//
// On Ada the provider already runs; Transfusion only unlocks MFG and swaps in
// Blackwell kernels. On Ampere and Turing the provider refuses to start (its
// minimum architecture is Ada) and ships no SM86/SM75 build of its image
// kernels, so the same patches must lower every gate to the real architecture
// and retarget the kernels' PTX to that SM.
namespace gpu_arch
{
enum class Family : uint8_t
{
    Unknown,
    Turing,
    Ampere,
    Ada,
    Blackwell,
};

// Detects the adapter from PCI IDs (D3DKMT, safe under the loader lock) unless
// the configuration forces a family. Called once from DLL_PROCESS_ATTACH.
void Initialize(Family forced) noexcept;
bool TryParse(const char* text, size_t length, Family& family) noexcept;
const char* ConfigName(Family family) noexcept;

// The family the provider patches are built for: the detected one, or Ada
// when nothing older than Ada was found (original Transfusion behavior).
Family Target() noexcept;
bool PreAda() noexcept;
// NGX architecture code (0x160 Turing, 0x170 Ampere, 0x190 Ada).
uint32_t NgxArchitecture() noexcept;
// PTX/SASS target (75, 86, 89).
uint32_t SmVersion() noexcept;
const wchar_t* Describe() noexcept;
}
