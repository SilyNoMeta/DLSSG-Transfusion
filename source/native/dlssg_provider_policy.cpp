#include "dlssg_provider_policy.h"

#include <winver.h>
#include <cwctype>
#include <string>

namespace dlssg_provider_policy
{
namespace
{
std::wstring ToLower(const wchar_t* str)
{
    if (!str) return L"";
    std::wstring result = str;
    for (auto& ch : result) ch = static_cast<wchar_t>(std::towlower(ch));
    return result;
}
} // namespace

bool ReadProviderVersion(
    const wchar_t* path, VersionTriplet& version) noexcept
{
    version = {};
    if (!path || !*path)
        return false;

    DWORD ignored = 0;
    const DWORD versionBytes = GetFileVersionInfoSizeW(path, &ignored);
    if (!versionBytes)
        return false;

    void* const versionData = VirtualAlloc(nullptr, versionBytes,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!versionData)
        return false;

    VS_FIXEDFILEINFO* fixedInfo = nullptr;
    UINT fixedInfoBytes = 0;
    const bool versionRead = GetFileVersionInfoW(
            path, 0, versionBytes, versionData)
        && VerQueryValueW(versionData, L"\\",
            reinterpret_cast<void**>(&fixedInfo), &fixedInfoBytes)
        && fixedInfo && fixedInfoBytes >= sizeof(VS_FIXEDFILEINFO)
        && fixedInfo->dwSignature == VS_FFI_SIGNATURE;
    if (versionRead)
    {
        version.major = HIWORD(fixedInfo->dwFileVersionMS);
        version.minor = LOWORD(fixedInfo->dwFileVersionMS);
        version.build = HIWORD(fixedInfo->dwFileVersionLS);
    }
    VirtualFree(versionData, 0, MEM_RELEASE);
    return versionRead;
}

bool SupportedProviderVersionMatches(const wchar_t* path) noexcept
{
    VersionTriplet version{};
    if (ReadProviderVersion(path, version))
        return IsSupportedVersion(version);
    // Unversioned OTA driver model caches (.bin) or memory-only staged modules
    return true;
}

bool HasKnownDlssgPath(HMODULE module, const wchar_t* path) noexcept
{
    std::wstring lowerPath = ToLower(path);
    if (lowerPath.find(L"nvngx_dlssg") != std::wstring::npos
        || lowerPath.find(L"\\models\\dlssg\\") != std::wstring::npos)
    {
        return true;
    }
    if (module)
    {
        wchar_t modPath[MAX_PATH]{};
        if (GetModuleFileNameW(module, modPath, MAX_PATH))
        {
            std::wstring lowerMod = ToLower(modPath);
            if (lowerMod.find(L"nvngx_dlssg") != std::wstring::npos
                || lowerMod.find(L"\\models\\dlssg\\") != std::wstring::npos)
            {
                return true;
            }
        }
    }
    return false;
}

bool IsDlssgImplementationModule(HMODULE module) noexcept
{
    if (!module)
        return false;
    const bool populateDevice = GetProcAddress(module, kD3d12ImplementationExport) != nullptr;
    const bool directSr = GetProcAddress(module, kDirectSrImplementationExport) != nullptr;
    return HasDlssgExportIdentity(populateDevice, directSr) || HasKnownDlssgPath(module, nullptr);
}

bool IsSupportedProvider(HMODULE module, const wchar_t* path) noexcept
{
    if (!module)
        return false;
    const bool isDlssg = IsDlssgImplementationModule(module) || HasKnownDlssgPath(module, path);
    if (!isDlssg)
        return false;
    return SupportedProviderVersionMatches(path);
}
}
