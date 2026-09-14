#include "proxy.h"
#include <cwctype>
#include <string>

extern "C" void DummyFunc();

extern "C" {
void* g_Real_GetFileVersionInfoA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_GetFileVersionInfoByHandle = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_GetFileVersionInfoExA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_GetFileVersionInfoExW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_GetFileVersionInfoSizeA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_GetFileVersionInfoSizeExA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_GetFileVersionInfoSizeExW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_GetFileVersionInfoSizeW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_GetFileVersionInfoW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_VerFindFileA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_VerFindFileW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_VerInstallFileA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_VerInstallFileW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_VerLanguageNameA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_VerLanguageNameW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_VerQueryValueA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_VerQueryValueW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_ApplyCompatResolutionQuirking = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_CompatString = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_CompatValue = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_CreateDXGIFactory = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_CreateDXGIFactory1 = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_CreateDXGIFactory2 = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DXGID3D10CreateDevice = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DXGID3D10CreateLayeredDevice = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DXGID3D10GetLayeredDeviceSize = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DXGID3D10RegisterLayers = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DXGIDeclareAdapterRemovalSupport = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DXGIDisableVBlankVirtualization = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DXGIDumpJournal = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DXGIGetDebugInterface1 = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DXGIReportAdapterConfiguration = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_PIXBeginCapture = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_PIXEndCapture = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_PIXGetCaptureState = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_SetAppCompatStringPointer = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_UpdateHMDEmulationStatus = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_CloseDriver = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DefDriverProc = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DriverCallback = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DrvGetModuleHandle = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_GetDriverModuleHandle = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_OpenDriver = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_PlaySound = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_PlaySoundA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_PlaySoundW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_SendDriverMessage = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_WOWAppExit = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_auxGetDevCapsA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_auxGetDevCapsW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_auxGetNumDevs = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_auxGetVolume = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_auxOutMessage = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_auxSetVolume = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joyConfigChanged = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joyGetDevCapsA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joyGetDevCapsW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joyGetNumDevs = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joyGetPos = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joyGetPosEx = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joyGetThreshold = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joyReleaseCapture = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joySetCapture = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_joySetThreshold = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciDriverNotify = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciDriverYield = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciExecute = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciFreeCommandResource = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciGetCreatorTask = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciGetDeviceIDA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciGetDeviceIDFromElementIDA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciGetDeviceIDFromElementIDW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciGetDeviceIDW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciGetDriverData = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciGetErrorStringA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciGetErrorStringW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciGetYieldProc = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciLoadCommandResource = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciSendCommandA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciSendCommandW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciSendStringA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciSendStringW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciSetDriverData = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mciSetYieldProc = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiConnect = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiDisconnect = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInAddBuffer = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInClose = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInGetDevCapsA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInGetDevCapsW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInGetErrorTextA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInGetErrorTextW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInGetID = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInGetNumDevs = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInMessage = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInOpen = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInPrepareHeader = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInReset = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInStart = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInStop = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiInUnprepareHeader = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutCacheDrumPatches = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutCachePatches = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutClose = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutGetDevCapsA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutGetDevCapsW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutGetErrorTextA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutGetErrorTextW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutGetID = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutGetNumDevs = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutGetVolume = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutLongMsg = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutMessage = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutOpen = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutPrepareHeader = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutReset = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutSetVolume = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutShortMsg = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiOutUnprepareHeader = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiStreamClose = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiStreamOpen = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiStreamOut = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiStreamPause = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiStreamPosition = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiStreamProperty = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiStreamRestart = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_midiStreamStop = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerClose = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetControlDetailsA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetControlDetailsW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetDevCapsA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetDevCapsW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetID = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetLineControlsA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetLineControlsW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetLineInfoA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetLineInfoW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerGetNumDevs = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerMessage = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerOpen = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mixerSetControlDetails = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmDrvInstall = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmGetCurrentTask = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmTaskBlock = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmTaskCreate = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmTaskSignal = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmTaskYield = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioAdvance = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioAscend = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioClose = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioCreateChunk = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioDescend = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioFlush = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioGetInfo = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioInstallIOProcA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioInstallIOProcW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioOpenA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioOpenW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioRead = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioRenameA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioRenameW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioSeek = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioSendMessage = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioSetBuffer = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioSetInfo = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioStringToFOURCCA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioStringToFOURCCW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmioWrite = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_mmsystemGetVersion = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_sndPlaySoundA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_sndPlaySoundW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_timeBeginPeriod = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_timeEndPeriod = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_timeGetDevCaps = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_timeGetSystemTime = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_timeGetTime = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_timeKillEvent = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_timeSetEvent = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInAddBuffer = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInClose = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInGetDevCapsA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInGetDevCapsW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInGetErrorTextA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInGetErrorTextW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInGetID = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInGetNumDevs = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInGetPosition = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInMessage = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInOpen = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInPrepareHeader = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInReset = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInStart = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInStop = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveInUnprepareHeader = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutBreakLoop = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutClose = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetDevCapsA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetDevCapsW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetErrorTextA = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetErrorTextW = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetID = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetNumDevs = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetPitch = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetPlaybackRate = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetPosition = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutGetVolume = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutMessage = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutOpen = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutPause = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutPrepareHeader = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutReset = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutRestart = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutSetPitch = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutSetPlaybackRate = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutSetVolume = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutUnprepareHeader = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_waveOutWrite = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_Ordinal2 = reinterpret_cast<void*>(&DummyFunc);
void* g_Real_DirectInput8Create = reinterpret_cast<void*>(&DummyFunc);

} // extern "C"

namespace proxy
{
namespace
{
HMODULE g_SystemLibrary = nullptr;
ProxyType g_CurrentType = ProxyType::None;
std::wstring g_OriginalLibraryPath;

std::wstring ToLower(std::wstring str)
{
    for (auto& ch : str)
        ch = static_cast<wchar_t>(std::towlower(ch));
    return str;
}
} // namespace

ProxyType GetCurrentType()
{
    return g_CurrentType;
}

const wchar_t* GetCurrentTypeName()
{
    switch (g_CurrentType)
    {
    case ProxyType::Version: return L"version.dll";
    case ProxyType::Winmm: return L"winmm.dll";
    case ProxyType::Dxgi: return L"dxgi.dll";
    case ProxyType::Dinput8: return L"dinput8.dll";
    case ProxyType::Asi: return L"DLSSG-Transfusion.asi";
    case ProxyType::Unsupported: return L"unsupported filename";
    default: return L"standalone / ASI";
    }
}

const wchar_t* GetOriginalLibraryPath()
{
    return g_OriginalLibraryPath.c_str();
}

ProxyType Initialize(HINSTANCE instance)
{
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(instance, path, MAX_PATH))
        return ProxyType::None;

    std::wstring modulePath = path;
    const size_t lastSlash = modulePath.find_last_of(L"\\/");
    const std::wstring filename = (lastSlash != std::wstring::npos) ? modulePath.substr(lastSlash + 1) : modulePath;
    const std::wstring lowerName = ToLower(filename);

    wchar_t sysDir[MAX_PATH]{};
    GetSystemDirectoryW(sysDir, MAX_PATH);

    if (lowerName == L"version.dll")
    {
        g_CurrentType = ProxyType::Version;
        g_OriginalLibraryPath = std::wstring(sysDir) + L"\\version.dll";
        g_SystemLibrary = LoadLibraryExW(g_OriginalLibraryPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (g_SystemLibrary)
        {
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetFileVersionInfoA")) g_Real_GetFileVersionInfoA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetFileVersionInfoByHandle")) g_Real_GetFileVersionInfoByHandle = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetFileVersionInfoExA")) g_Real_GetFileVersionInfoExA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetFileVersionInfoExW")) g_Real_GetFileVersionInfoExW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetFileVersionInfoSizeA")) g_Real_GetFileVersionInfoSizeA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetFileVersionInfoSizeExA")) g_Real_GetFileVersionInfoSizeExA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetFileVersionInfoSizeExW")) g_Real_GetFileVersionInfoSizeExW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetFileVersionInfoSizeW")) g_Real_GetFileVersionInfoSizeW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetFileVersionInfoW")) g_Real_GetFileVersionInfoW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "VerFindFileA")) g_Real_VerFindFileA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "VerFindFileW")) g_Real_VerFindFileW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "VerInstallFileA")) g_Real_VerInstallFileA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "VerInstallFileW")) g_Real_VerInstallFileW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "VerLanguageNameA")) g_Real_VerLanguageNameA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "VerLanguageNameW")) g_Real_VerLanguageNameW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "VerQueryValueA")) g_Real_VerQueryValueA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "VerQueryValueW")) g_Real_VerQueryValueW = reinterpret_cast<void*>(p);
        }
        return g_CurrentType;
    }

    if (lowerName == L"dxgi.dll")
    {
        g_CurrentType = ProxyType::Dxgi;
        g_OriginalLibraryPath = std::wstring(sysDir) + L"\\dxgi.dll";
        g_SystemLibrary = LoadLibraryExW(g_OriginalLibraryPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (g_SystemLibrary)
        {
            if (auto* p = GetProcAddress(g_SystemLibrary, "ApplyCompatResolutionQuirking")) g_Real_ApplyCompatResolutionQuirking = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "CompatString")) g_Real_CompatString = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "CompatValue")) g_Real_CompatValue = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "CreateDXGIFactory")) g_Real_CreateDXGIFactory = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "CreateDXGIFactory1")) g_Real_CreateDXGIFactory1 = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "CreateDXGIFactory2")) g_Real_CreateDXGIFactory2 = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DXGID3D10CreateDevice")) g_Real_DXGID3D10CreateDevice = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DXGID3D10CreateLayeredDevice")) g_Real_DXGID3D10CreateLayeredDevice = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DXGID3D10GetLayeredDeviceSize")) g_Real_DXGID3D10GetLayeredDeviceSize = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DXGID3D10RegisterLayers")) g_Real_DXGID3D10RegisterLayers = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DXGIDeclareAdapterRemovalSupport")) g_Real_DXGIDeclareAdapterRemovalSupport = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DXGIDisableVBlankVirtualization")) g_Real_DXGIDisableVBlankVirtualization = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DXGIDumpJournal")) g_Real_DXGIDumpJournal = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DXGIGetDebugInterface1")) g_Real_DXGIGetDebugInterface1 = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DXGIReportAdapterConfiguration")) g_Real_DXGIReportAdapterConfiguration = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "PIXBeginCapture")) g_Real_PIXBeginCapture = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "PIXEndCapture")) g_Real_PIXEndCapture = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "PIXGetCaptureState")) g_Real_PIXGetCaptureState = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "SetAppCompatStringPointer")) g_Real_SetAppCompatStringPointer = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "UpdateHMDEmulationStatus")) g_Real_UpdateHMDEmulationStatus = reinterpret_cast<void*>(p);
        }
        return g_CurrentType;
    }

    if (lowerName == L"winmm.dll")
    {
        g_CurrentType = ProxyType::Winmm;
        g_OriginalLibraryPath = std::wstring(sysDir) + L"\\winmm.dll";
        g_SystemLibrary = LoadLibraryExW(g_OriginalLibraryPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (g_SystemLibrary)
        {
            if (auto* p = GetProcAddress(g_SystemLibrary, "CloseDriver")) g_Real_CloseDriver = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DefDriverProc")) g_Real_DefDriverProc = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DriverCallback")) g_Real_DriverCallback = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "DrvGetModuleHandle")) g_Real_DrvGetModuleHandle = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "GetDriverModuleHandle")) g_Real_GetDriverModuleHandle = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "OpenDriver")) g_Real_OpenDriver = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "PlaySound")) g_Real_PlaySound = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "PlaySoundA")) g_Real_PlaySoundA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "PlaySoundW")) g_Real_PlaySoundW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "SendDriverMessage")) g_Real_SendDriverMessage = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "WOWAppExit")) g_Real_WOWAppExit = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "auxGetDevCapsA")) g_Real_auxGetDevCapsA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "auxGetDevCapsW")) g_Real_auxGetDevCapsW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "auxGetNumDevs")) g_Real_auxGetNumDevs = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "auxGetVolume")) g_Real_auxGetVolume = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "auxOutMessage")) g_Real_auxOutMessage = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "auxSetVolume")) g_Real_auxSetVolume = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joyConfigChanged")) g_Real_joyConfigChanged = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joyGetDevCapsA")) g_Real_joyGetDevCapsA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joyGetDevCapsW")) g_Real_joyGetDevCapsW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joyGetNumDevs")) g_Real_joyGetNumDevs = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joyGetPos")) g_Real_joyGetPos = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joyGetPosEx")) g_Real_joyGetPosEx = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joyGetThreshold")) g_Real_joyGetThreshold = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joyReleaseCapture")) g_Real_joyReleaseCapture = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joySetCapture")) g_Real_joySetCapture = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "joySetThreshold")) g_Real_joySetThreshold = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciDriverNotify")) g_Real_mciDriverNotify = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciDriverYield")) g_Real_mciDriverYield = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciExecute")) g_Real_mciExecute = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciFreeCommandResource")) g_Real_mciFreeCommandResource = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciGetCreatorTask")) g_Real_mciGetCreatorTask = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciGetDeviceIDA")) g_Real_mciGetDeviceIDA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciGetDeviceIDFromElementIDA")) g_Real_mciGetDeviceIDFromElementIDA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciGetDeviceIDFromElementIDW")) g_Real_mciGetDeviceIDFromElementIDW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciGetDeviceIDW")) g_Real_mciGetDeviceIDW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciGetDriverData")) g_Real_mciGetDriverData = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciGetErrorStringA")) g_Real_mciGetErrorStringA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciGetErrorStringW")) g_Real_mciGetErrorStringW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciGetYieldProc")) g_Real_mciGetYieldProc = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciLoadCommandResource")) g_Real_mciLoadCommandResource = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciSendCommandA")) g_Real_mciSendCommandA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciSendCommandW")) g_Real_mciSendCommandW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciSendStringA")) g_Real_mciSendStringA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciSendStringW")) g_Real_mciSendStringW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciSetDriverData")) g_Real_mciSetDriverData = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mciSetYieldProc")) g_Real_mciSetYieldProc = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiConnect")) g_Real_midiConnect = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiDisconnect")) g_Real_midiDisconnect = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInAddBuffer")) g_Real_midiInAddBuffer = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInClose")) g_Real_midiInClose = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInGetDevCapsA")) g_Real_midiInGetDevCapsA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInGetDevCapsW")) g_Real_midiInGetDevCapsW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInGetErrorTextA")) g_Real_midiInGetErrorTextA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInGetErrorTextW")) g_Real_midiInGetErrorTextW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInGetID")) g_Real_midiInGetID = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInGetNumDevs")) g_Real_midiInGetNumDevs = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInMessage")) g_Real_midiInMessage = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInOpen")) g_Real_midiInOpen = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInPrepareHeader")) g_Real_midiInPrepareHeader = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInReset")) g_Real_midiInReset = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInStart")) g_Real_midiInStart = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInStop")) g_Real_midiInStop = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiInUnprepareHeader")) g_Real_midiInUnprepareHeader = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutCacheDrumPatches")) g_Real_midiOutCacheDrumPatches = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutCachePatches")) g_Real_midiOutCachePatches = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutClose")) g_Real_midiOutClose = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutGetDevCapsA")) g_Real_midiOutGetDevCapsA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutGetDevCapsW")) g_Real_midiOutGetDevCapsW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutGetErrorTextA")) g_Real_midiOutGetErrorTextA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutGetErrorTextW")) g_Real_midiOutGetErrorTextW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutGetID")) g_Real_midiOutGetID = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutGetNumDevs")) g_Real_midiOutGetNumDevs = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutGetVolume")) g_Real_midiOutGetVolume = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutLongMsg")) g_Real_midiOutLongMsg = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutMessage")) g_Real_midiOutMessage = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutOpen")) g_Real_midiOutOpen = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutPrepareHeader")) g_Real_midiOutPrepareHeader = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutReset")) g_Real_midiOutReset = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutSetVolume")) g_Real_midiOutSetVolume = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutShortMsg")) g_Real_midiOutShortMsg = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiOutUnprepareHeader")) g_Real_midiOutUnprepareHeader = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiStreamClose")) g_Real_midiStreamClose = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiStreamOpen")) g_Real_midiStreamOpen = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiStreamOut")) g_Real_midiStreamOut = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiStreamPause")) g_Real_midiStreamPause = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiStreamPosition")) g_Real_midiStreamPosition = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiStreamProperty")) g_Real_midiStreamProperty = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiStreamRestart")) g_Real_midiStreamRestart = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "midiStreamStop")) g_Real_midiStreamStop = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerClose")) g_Real_mixerClose = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetControlDetailsA")) g_Real_mixerGetControlDetailsA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetControlDetailsW")) g_Real_mixerGetControlDetailsW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetDevCapsA")) g_Real_mixerGetDevCapsA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetDevCapsW")) g_Real_mixerGetDevCapsW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetID")) g_Real_mixerGetID = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetLineControlsA")) g_Real_mixerGetLineControlsA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetLineControlsW")) g_Real_mixerGetLineControlsW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetLineInfoA")) g_Real_mixerGetLineInfoA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetLineInfoW")) g_Real_mixerGetLineInfoW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerGetNumDevs")) g_Real_mixerGetNumDevs = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerMessage")) g_Real_mixerMessage = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerOpen")) g_Real_mixerOpen = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mixerSetControlDetails")) g_Real_mixerSetControlDetails = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmDrvInstall")) g_Real_mmDrvInstall = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmGetCurrentTask")) g_Real_mmGetCurrentTask = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmTaskBlock")) g_Real_mmTaskBlock = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmTaskCreate")) g_Real_mmTaskCreate = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmTaskSignal")) g_Real_mmTaskSignal = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmTaskYield")) g_Real_mmTaskYield = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioAdvance")) g_Real_mmioAdvance = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioAscend")) g_Real_mmioAscend = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioClose")) g_Real_mmioClose = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioCreateChunk")) g_Real_mmioCreateChunk = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioDescend")) g_Real_mmioDescend = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioFlush")) g_Real_mmioFlush = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioGetInfo")) g_Real_mmioGetInfo = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioInstallIOProcA")) g_Real_mmioInstallIOProcA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioInstallIOProcW")) g_Real_mmioInstallIOProcW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioOpenA")) g_Real_mmioOpenA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioOpenW")) g_Real_mmioOpenW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioRead")) g_Real_mmioRead = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioRenameA")) g_Real_mmioRenameA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioRenameW")) g_Real_mmioRenameW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioSeek")) g_Real_mmioSeek = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioSendMessage")) g_Real_mmioSendMessage = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioSetBuffer")) g_Real_mmioSetBuffer = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioSetInfo")) g_Real_mmioSetInfo = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioStringToFOURCCA")) g_Real_mmioStringToFOURCCA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioStringToFOURCCW")) g_Real_mmioStringToFOURCCW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmioWrite")) g_Real_mmioWrite = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "mmsystemGetVersion")) g_Real_mmsystemGetVersion = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "sndPlaySoundA")) g_Real_sndPlaySoundA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "sndPlaySoundW")) g_Real_sndPlaySoundW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "timeBeginPeriod")) g_Real_timeBeginPeriod = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "timeEndPeriod")) g_Real_timeEndPeriod = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "timeGetDevCaps")) g_Real_timeGetDevCaps = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "timeGetSystemTime")) g_Real_timeGetSystemTime = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "timeGetTime")) g_Real_timeGetTime = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "timeKillEvent")) g_Real_timeKillEvent = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "timeSetEvent")) g_Real_timeSetEvent = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInAddBuffer")) g_Real_waveInAddBuffer = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInClose")) g_Real_waveInClose = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInGetDevCapsA")) g_Real_waveInGetDevCapsA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInGetDevCapsW")) g_Real_waveInGetDevCapsW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInGetErrorTextA")) g_Real_waveInGetErrorTextA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInGetErrorTextW")) g_Real_waveInGetErrorTextW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInGetID")) g_Real_waveInGetID = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInGetNumDevs")) g_Real_waveInGetNumDevs = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInGetPosition")) g_Real_waveInGetPosition = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInMessage")) g_Real_waveInMessage = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInOpen")) g_Real_waveInOpen = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInPrepareHeader")) g_Real_waveInPrepareHeader = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInReset")) g_Real_waveInReset = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInStart")) g_Real_waveInStart = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInStop")) g_Real_waveInStop = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveInUnprepareHeader")) g_Real_waveInUnprepareHeader = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutBreakLoop")) g_Real_waveOutBreakLoop = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutClose")) g_Real_waveOutClose = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetDevCapsA")) g_Real_waveOutGetDevCapsA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetDevCapsW")) g_Real_waveOutGetDevCapsW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetErrorTextA")) g_Real_waveOutGetErrorTextA = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetErrorTextW")) g_Real_waveOutGetErrorTextW = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetID")) g_Real_waveOutGetID = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetNumDevs")) g_Real_waveOutGetNumDevs = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetPitch")) g_Real_waveOutGetPitch = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetPlaybackRate")) g_Real_waveOutGetPlaybackRate = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetPosition")) g_Real_waveOutGetPosition = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutGetVolume")) g_Real_waveOutGetVolume = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutMessage")) g_Real_waveOutMessage = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutOpen")) g_Real_waveOutOpen = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutPause")) g_Real_waveOutPause = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutPrepareHeader")) g_Real_waveOutPrepareHeader = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutReset")) g_Real_waveOutReset = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutRestart")) g_Real_waveOutRestart = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutSetPitch")) g_Real_waveOutSetPitch = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutSetPlaybackRate")) g_Real_waveOutSetPlaybackRate = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutSetVolume")) g_Real_waveOutSetVolume = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutUnprepareHeader")) g_Real_waveOutUnprepareHeader = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, "waveOutWrite")) g_Real_waveOutWrite = reinterpret_cast<void*>(p);
            if (auto* p = GetProcAddress(g_SystemLibrary, MAKEINTRESOURCEA(2))) g_Real_Ordinal2 = reinterpret_cast<void*>(p);
        }
        return g_CurrentType;
    }

    if (lowerName == L"dinput8.dll")
    {
        g_CurrentType = ProxyType::Dinput8;
        g_OriginalLibraryPath = std::wstring(sysDir) + L"\\dinput8.dll";
        g_SystemLibrary = LoadLibraryExW(g_OriginalLibraryPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (g_SystemLibrary)
        {
            if (auto* p = GetProcAddress(g_SystemLibrary, "DirectInput8Create"))
                g_Real_DirectInput8Create = reinterpret_cast<void*>(p);
        }
        return g_CurrentType;
    }

    if (lowerName == L"dlssg-transfusion.asi")
    {
        g_CurrentType = ProxyType::Asi;
        return g_CurrentType;
    }

    g_CurrentType = ProxyType::Unsupported;
    return g_CurrentType;
}

void Shutdown()
{
    if (g_SystemLibrary)
    {
        FreeLibrary(g_SystemLibrary);
        g_SystemLibrary = nullptr;
    }
}

} // namespace proxy
