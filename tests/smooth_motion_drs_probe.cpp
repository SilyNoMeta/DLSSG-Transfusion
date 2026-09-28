// Read-only probe for the three DRS settings used by nvoglv64.dll 617.14
// before it calls NVP_Init_Vulkan. Requires the official NVIDIA NVAPI headers.
#include <Windows.h>
#include <nvapi.h>

#include <cstdint>
#include <cwchar>
#include <vector>

namespace
{
using QueryInterface = void* (__cdecl*)(NvU32);

template <typename Function>
Function Resolve(QueryInterface query, NvU32 id) noexcept
{
    return reinterpret_cast<Function>(query(id));
}

void PrintSetting(decltype(&NvAPI_DRS_GetSetting) getSetting,
    NvDRSSessionHandle session, NvDRSProfileHandle profile,
    NvU32 id, const wchar_t* label) noexcept
{
    NVDRS_SETTING setting{};
    setting.version = NVDRS_SETTING_VER;
    const NvAPI_Status status = getSetting(session, profile, id, &setting);
    if (status != NVAPI_OK)
    {
        wprintf(L"%ls (0x%08X): GetSetting status=%d\n", label, id, status);
        return;
    }
    wprintf(L"%ls (0x%08X): type=%u location=%u predefined=%u value=0x%08X (%u)\n",
        label, id, static_cast<unsigned>(setting.settingType),
        static_cast<unsigned>(setting.settingLocation),
        setting.isCurrentPredefined, setting.u32CurrentValue, setting.u32CurrentValue);
}

void PrintEnumeratedSettings(decltype(&NvAPI_DRS_EnumSettings) enumerate,
    NvDRSSessionHandle session, NvDRSProfileHandle profile,
    NvU32 expectedCount) noexcept
{
    wprintf(L"Enumerated application-profile settings (declared=%u):\n", expectedCount);
    if (expectedCount == 0 || expectedCount > 4096) return;
    std::vector<NVDRS_SETTING> settings(expectedCount);
    for (auto& setting : settings) setting.version = NVDRS_SETTING_VER;
    NvU32 count = expectedCount;
    const NvAPI_Status status = enumerate(session, profile, 0, &count, settings.data());
    if (status != NVAPI_OK)
    {
        wprintf(L"  enumeration failed: status=%d count=%u\n", status, count);
        return;
    }
    for (NvU32 index = 0; index < count && index < settings.size(); ++index)
    {
        const auto& setting = settings[index];
        wprintf(L"  0x%08X type=%u value=0x%08X name=%ls\n", setting.settingId,
            static_cast<unsigned>(setting.settingType), setting.u32CurrentValue,
            reinterpret_cast<const wchar_t*>(setting.settingName));
    }
}
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2)
    {
        fwprintf(stderr, L"Usage: smooth_motion_drs_probe.exe <absolute-game-exe-path>\n");
        return 2;
    }
    if (wcslen(argv[1]) >= NVAPI_UNICODE_STRING_MAX)
    {
        fwprintf(stderr, L"Game path is too long for NVAPI.\n");
        return 2;
    }
    HMODULE library = LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library)
    {
        fwprintf(stderr, L"Cannot load nvapi64.dll (Windows error %lu).\n", GetLastError());
        return 1;
    }
    auto query = reinterpret_cast<QueryInterface>(GetProcAddress(library, "nvapi_QueryInterface"));
    if (!query) query = reinterpret_cast<QueryInterface>(GetProcAddress(library, "nvapi64_QueryInterface"));
    if (!query)
    {
        fwprintf(stderr, L"NVAPI QueryInterface export not found.\n");
        FreeLibrary(library);
        return 1;
    }
    const auto initialize = Resolve<decltype(&NvAPI_Initialize)>(query, 0x0150e828);
    const auto createSession = Resolve<decltype(&NvAPI_DRS_CreateSession)>(query, 0x0694d52e);
    const auto destroySession = Resolve<decltype(&NvAPI_DRS_DestroySession)>(query, 0xdad9cff8);
    const auto loadSettings = Resolve<decltype(&NvAPI_DRS_LoadSettings)>(query, 0x375dbd6b);
    const auto findApplication = Resolve<decltype(&NvAPI_DRS_FindApplicationByName)>(query, 0xeee566b2);
    const auto getSetting = Resolve<decltype(&NvAPI_DRS_GetSetting)>(query, 0x73bf8338);
    const auto enumerateSettings = Resolve<decltype(&NvAPI_DRS_EnumSettings)>(query, 0xae3039da);
    const auto getBaseProfile = Resolve<decltype(&NvAPI_DRS_GetBaseProfile)>(query, 0xda8466a0);
    const auto getProfileInfo = Resolve<decltype(&NvAPI_DRS_GetProfileInfo)>(query, 0x61cd6fd6);
    if (!initialize || !createSession || !destroySession || !loadSettings || !findApplication
        || !getSetting || !getBaseProfile || !getProfileInfo || !enumerateSettings)
    {
        fwprintf(stderr, L"Required read-only NVAPI DRS functions are unavailable.\n");
        FreeLibrary(library);
        return 1;
    }
    NvAPI_Status status = initialize();
    if (status != NVAPI_OK)
    {
        fwprintf(stderr, L"NvAPI_Initialize status=%d\n", status);
        FreeLibrary(library);
        return 1;
    }
    NvDRSSessionHandle session = nullptr;
    status = createSession(&session);
    if (status != NVAPI_OK)
    {
        fwprintf(stderr, L"NvAPI_DRS_CreateSession status=%d\n", status);
        FreeLibrary(library);
        return 1;
    }
    status = loadSettings(session);
    if (status != NVAPI_OK)
    {
        fwprintf(stderr, L"NvAPI_DRS_LoadSettings status=%d\n", status);
        destroySession(session);
        FreeLibrary(library);
        return 1;
    }
    NvAPI_UnicodeString path{};
    for (size_t index = 0; argv[1][index]; ++index)
        path[index] = static_cast<NvU16>(argv[1][index]);
    NVDRS_APPLICATION application{};
    application.version = NVDRS_APPLICATION_VER;
    NvDRSProfileHandle profile = nullptr;
    status = findApplication(session, path, &profile, &application);
    if (status != NVAPI_OK)
    {
        fwprintf(stderr, L"NvAPI_DRS_FindApplicationByName status=%d for %ls\n", status, argv[1]);
        destroySession(session);
        FreeLibrary(library);
        return 1;
    }
    NVDRS_PROFILE profileInfo{};
    profileInfo.version = NVDRS_PROFILE_VER;
    const NvAPI_Status profileStatus = getProfileInfo(session, profile, &profileInfo);
    wprintf(L"Application: %ls\nApplication profile: %ls (status=%d)\n", argv[1],
        profileStatus == NVAPI_OK ? reinterpret_cast<const wchar_t*>(profileInfo.profileName) : L"<unavailable>",
        profileStatus);
    wprintf(L"Application profile settings:\n");
    PrintSetting(getSetting, session, profile, 0xB0D384C0u, L"Smooth Motion Enable");
    PrintSetting(getSetting, session, profile, 0xB0CC0875u, L"Enabled APIs (Vulkan bit 4)");
    PrintSetting(getSetting, session, profile, 0xB09B15AFu, L"Intermediate gate (meaning unverified)");
    PrintSetting(getSetting, session, profile, 0x1034CB89u, L"NVAPI control: predefined FXAA usage");
    if (profileStatus == NVAPI_OK)
        PrintEnumeratedSettings(enumerateSettings, session, profile, profileInfo.numOfSettings);
    NvDRSProfileHandle baseProfile = nullptr;
    const NvAPI_Status baseStatus = getBaseProfile(session, &baseProfile);
    if (baseStatus == NVAPI_OK)
    {
        wprintf(L"Base profile settings:\n");
        PrintSetting(getSetting, session, baseProfile, 0xB0D384C0u, L"Smooth Motion Enable");
        PrintSetting(getSetting, session, baseProfile, 0xB0CC0875u, L"Enabled APIs (Vulkan bit 4)");
        PrintSetting(getSetting, session, baseProfile, 0xB09B15AFu, L"Intermediate gate (meaning unverified)");
    }
    else wprintf(L"Base profile unavailable: status=%d\n", baseStatus);
    destroySession(session);
    FreeLibrary(library);
    return 0;
}
