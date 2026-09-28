#include "smooth_motion_sm86.h"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>

int wmain()
{
    if (!smooth_motion_sm86::Initialize(
        [](const wchar_t* line) { wprintf(L"%ls\n", line); },
        smooth_motion_sm86::ApiMode::Vulkan))
        return 1;
    if (smooth_motion_sm86::TryForceVulkanProfileGate(GetModuleHandleW(L"kernel32.dll")))
    {
        wprintf(L"Foreign module was not refused.\n");
        return 6;
    }
    HMODULE present = GetModuleHandleW(L"NvPresent64.dll");
    wchar_t path[32768]{};
    if (!present || !GetModuleFileNameW(present, path, _countof(path))) return 2;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash || wcscpy_s(slash + 1, _countof(path) - (slash + 1 - path), L"nvoglv64.dll"))
        return 3;
    HMODULE driver = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!driver)
    {
        wprintf(L"Could not load nvoglv64.dll: Win32 error %lu\n", GetLastError());
        return 4;
    }
    const bool patched = smooth_motion_sm86::TryForceVulkanProfileGate(driver);
    static constexpr std::array<uint8_t, 6> nop6{0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    const auto* branch = reinterpret_cast<const uint8_t*>(driver) + 0xda33b9;
    const bool bytesMatch = std::memcmp(branch, nop6.data(), nop6.size()) == 0;
    wprintf(L"Vulkan gate probe: patched=%d enableBranchNops=%d\n", patched, bytesMatch);
    return patched && bytesMatch ? 0 : 5;
}
