; Auto-generated 64-bit Proxy Thunks for version, dxgi, winmm
.code

public DummyFunc
DummyFunc proc
    xor eax, eax
    ret
DummyFunc endp

extern g_Real_GetFileVersionInfoA : qword
public Proxy_GetFileVersionInfoA
Proxy_GetFileVersionInfoA proc
    jmp qword ptr [g_Real_GetFileVersionInfoA]
Proxy_GetFileVersionInfoA endp

extern g_Real_GetFileVersionInfoByHandle : qword
public Proxy_GetFileVersionInfoByHandle
Proxy_GetFileVersionInfoByHandle proc
    jmp qword ptr [g_Real_GetFileVersionInfoByHandle]
Proxy_GetFileVersionInfoByHandle endp

extern g_Real_GetFileVersionInfoExA : qword
public Proxy_GetFileVersionInfoExA
Proxy_GetFileVersionInfoExA proc
    jmp qword ptr [g_Real_GetFileVersionInfoExA]
Proxy_GetFileVersionInfoExA endp

extern g_Real_GetFileVersionInfoExW : qword
public Proxy_GetFileVersionInfoExW
Proxy_GetFileVersionInfoExW proc
    jmp qword ptr [g_Real_GetFileVersionInfoExW]
Proxy_GetFileVersionInfoExW endp

extern g_Real_GetFileVersionInfoSizeA : qword
public Proxy_GetFileVersionInfoSizeA
Proxy_GetFileVersionInfoSizeA proc
    jmp qword ptr [g_Real_GetFileVersionInfoSizeA]
Proxy_GetFileVersionInfoSizeA endp

extern g_Real_GetFileVersionInfoSizeExA : qword
public Proxy_GetFileVersionInfoSizeExA
Proxy_GetFileVersionInfoSizeExA proc
    jmp qword ptr [g_Real_GetFileVersionInfoSizeExA]
Proxy_GetFileVersionInfoSizeExA endp

extern g_Real_GetFileVersionInfoSizeExW : qword
public Proxy_GetFileVersionInfoSizeExW
Proxy_GetFileVersionInfoSizeExW proc
    jmp qword ptr [g_Real_GetFileVersionInfoSizeExW]
Proxy_GetFileVersionInfoSizeExW endp

extern g_Real_GetFileVersionInfoSizeW : qword
public Proxy_GetFileVersionInfoSizeW
Proxy_GetFileVersionInfoSizeW proc
    jmp qword ptr [g_Real_GetFileVersionInfoSizeW]
Proxy_GetFileVersionInfoSizeW endp

extern g_Real_GetFileVersionInfoW : qword
public Proxy_GetFileVersionInfoW
Proxy_GetFileVersionInfoW proc
    jmp qword ptr [g_Real_GetFileVersionInfoW]
Proxy_GetFileVersionInfoW endp

extern g_Real_VerFindFileA : qword
public Proxy_VerFindFileA
Proxy_VerFindFileA proc
    jmp qword ptr [g_Real_VerFindFileA]
Proxy_VerFindFileA endp

extern g_Real_VerFindFileW : qword
public Proxy_VerFindFileW
Proxy_VerFindFileW proc
    jmp qword ptr [g_Real_VerFindFileW]
Proxy_VerFindFileW endp

extern g_Real_VerInstallFileA : qword
public Proxy_VerInstallFileA
Proxy_VerInstallFileA proc
    jmp qword ptr [g_Real_VerInstallFileA]
Proxy_VerInstallFileA endp

extern g_Real_VerInstallFileW : qword
public Proxy_VerInstallFileW
Proxy_VerInstallFileW proc
    jmp qword ptr [g_Real_VerInstallFileW]
Proxy_VerInstallFileW endp

extern g_Real_VerLanguageNameA : qword
public Proxy_VerLanguageNameA
Proxy_VerLanguageNameA proc
    jmp qword ptr [g_Real_VerLanguageNameA]
Proxy_VerLanguageNameA endp

extern g_Real_VerLanguageNameW : qword
public Proxy_VerLanguageNameW
Proxy_VerLanguageNameW proc
    jmp qword ptr [g_Real_VerLanguageNameW]
Proxy_VerLanguageNameW endp

extern g_Real_VerQueryValueA : qword
public Proxy_VerQueryValueA
Proxy_VerQueryValueA proc
    jmp qword ptr [g_Real_VerQueryValueA]
Proxy_VerQueryValueA endp

extern g_Real_VerQueryValueW : qword
public Proxy_VerQueryValueW
Proxy_VerQueryValueW proc
    jmp qword ptr [g_Real_VerQueryValueW]
Proxy_VerQueryValueW endp

extern g_Real_ApplyCompatResolutionQuirking : qword
public Proxy_ApplyCompatResolutionQuirking
Proxy_ApplyCompatResolutionQuirking proc
    jmp qword ptr [g_Real_ApplyCompatResolutionQuirking]
Proxy_ApplyCompatResolutionQuirking endp

extern g_Real_CompatString : qword
public Proxy_CompatString
Proxy_CompatString proc
    jmp qword ptr [g_Real_CompatString]
Proxy_CompatString endp

extern g_Real_CompatValue : qword
public Proxy_CompatValue
Proxy_CompatValue proc
    jmp qword ptr [g_Real_CompatValue]
Proxy_CompatValue endp

extern g_Real_CreateDXGIFactory : qword
public Proxy_CreateDXGIFactory
Proxy_CreateDXGIFactory proc
    jmp qword ptr [g_Real_CreateDXGIFactory]
Proxy_CreateDXGIFactory endp

extern g_Real_CreateDXGIFactory1 : qword
public Proxy_CreateDXGIFactory1
Proxy_CreateDXGIFactory1 proc
    jmp qword ptr [g_Real_CreateDXGIFactory1]
Proxy_CreateDXGIFactory1 endp

extern g_Real_CreateDXGIFactory2 : qword
public Proxy_CreateDXGIFactory2
Proxy_CreateDXGIFactory2 proc
    jmp qword ptr [g_Real_CreateDXGIFactory2]
Proxy_CreateDXGIFactory2 endp

extern g_Real_DXGID3D10CreateDevice : qword
public Proxy_DXGID3D10CreateDevice
Proxy_DXGID3D10CreateDevice proc
    jmp qword ptr [g_Real_DXGID3D10CreateDevice]
Proxy_DXGID3D10CreateDevice endp

extern g_Real_DXGID3D10CreateLayeredDevice : qword
public Proxy_DXGID3D10CreateLayeredDevice
Proxy_DXGID3D10CreateLayeredDevice proc
    jmp qword ptr [g_Real_DXGID3D10CreateLayeredDevice]
Proxy_DXGID3D10CreateLayeredDevice endp

extern g_Real_DXGID3D10GetLayeredDeviceSize : qword
public Proxy_DXGID3D10GetLayeredDeviceSize
Proxy_DXGID3D10GetLayeredDeviceSize proc
    jmp qword ptr [g_Real_DXGID3D10GetLayeredDeviceSize]
Proxy_DXGID3D10GetLayeredDeviceSize endp

extern g_Real_DXGID3D10RegisterLayers : qword
public Proxy_DXGID3D10RegisterLayers
Proxy_DXGID3D10RegisterLayers proc
    jmp qword ptr [g_Real_DXGID3D10RegisterLayers]
Proxy_DXGID3D10RegisterLayers endp

extern g_Real_DXGIDeclareAdapterRemovalSupport : qword
public Proxy_DXGIDeclareAdapterRemovalSupport
Proxy_DXGIDeclareAdapterRemovalSupport proc
    jmp qword ptr [g_Real_DXGIDeclareAdapterRemovalSupport]
Proxy_DXGIDeclareAdapterRemovalSupport endp

extern g_Real_DXGIDisableVBlankVirtualization : qword
public Proxy_DXGIDisableVBlankVirtualization
Proxy_DXGIDisableVBlankVirtualization proc
    jmp qword ptr [g_Real_DXGIDisableVBlankVirtualization]
Proxy_DXGIDisableVBlankVirtualization endp

extern g_Real_DXGIDumpJournal : qword
public Proxy_DXGIDumpJournal
Proxy_DXGIDumpJournal proc
    jmp qword ptr [g_Real_DXGIDumpJournal]
Proxy_DXGIDumpJournal endp

extern g_Real_DXGIGetDebugInterface1 : qword
public Proxy_DXGIGetDebugInterface1
Proxy_DXGIGetDebugInterface1 proc
    jmp qword ptr [g_Real_DXGIGetDebugInterface1]
Proxy_DXGIGetDebugInterface1 endp

extern g_Real_DXGIReportAdapterConfiguration : qword
public Proxy_DXGIReportAdapterConfiguration
Proxy_DXGIReportAdapterConfiguration proc
    jmp qword ptr [g_Real_DXGIReportAdapterConfiguration]
Proxy_DXGIReportAdapterConfiguration endp

extern g_Real_PIXBeginCapture : qword
public Proxy_PIXBeginCapture
Proxy_PIXBeginCapture proc
    jmp qword ptr [g_Real_PIXBeginCapture]
Proxy_PIXBeginCapture endp

extern g_Real_PIXEndCapture : qword
public Proxy_PIXEndCapture
Proxy_PIXEndCapture proc
    jmp qword ptr [g_Real_PIXEndCapture]
Proxy_PIXEndCapture endp

extern g_Real_PIXGetCaptureState : qword
public Proxy_PIXGetCaptureState
Proxy_PIXGetCaptureState proc
    jmp qword ptr [g_Real_PIXGetCaptureState]
Proxy_PIXGetCaptureState endp

extern g_Real_SetAppCompatStringPointer : qword
public Proxy_SetAppCompatStringPointer
Proxy_SetAppCompatStringPointer proc
    jmp qword ptr [g_Real_SetAppCompatStringPointer]
Proxy_SetAppCompatStringPointer endp

extern g_Real_UpdateHMDEmulationStatus : qword
public Proxy_UpdateHMDEmulationStatus
Proxy_UpdateHMDEmulationStatus proc
    jmp qword ptr [g_Real_UpdateHMDEmulationStatus]
Proxy_UpdateHMDEmulationStatus endp

extern g_Real_CloseDriver : qword
public Proxy_CloseDriver
Proxy_CloseDriver proc
    jmp qword ptr [g_Real_CloseDriver]
Proxy_CloseDriver endp

extern g_Real_DefDriverProc : qword
public Proxy_DefDriverProc
Proxy_DefDriverProc proc
    jmp qword ptr [g_Real_DefDriverProc]
Proxy_DefDriverProc endp

extern g_Real_DriverCallback : qword
public Proxy_DriverCallback
Proxy_DriverCallback proc
    jmp qword ptr [g_Real_DriverCallback]
Proxy_DriverCallback endp

extern g_Real_DrvGetModuleHandle : qword
public Proxy_DrvGetModuleHandle
Proxy_DrvGetModuleHandle proc
    jmp qword ptr [g_Real_DrvGetModuleHandle]
Proxy_DrvGetModuleHandle endp

extern g_Real_GetDriverModuleHandle : qword
public Proxy_GetDriverModuleHandle
Proxy_GetDriverModuleHandle proc
    jmp qword ptr [g_Real_GetDriverModuleHandle]
Proxy_GetDriverModuleHandle endp

extern g_Real_OpenDriver : qword
public Proxy_OpenDriver
Proxy_OpenDriver proc
    jmp qword ptr [g_Real_OpenDriver]
Proxy_OpenDriver endp

extern g_Real_PlaySound : qword
public Proxy_PlaySound
Proxy_PlaySound proc
    jmp qword ptr [g_Real_PlaySound]
Proxy_PlaySound endp

extern g_Real_PlaySoundA : qword
public Proxy_PlaySoundA
Proxy_PlaySoundA proc
    jmp qword ptr [g_Real_PlaySoundA]
Proxy_PlaySoundA endp

extern g_Real_PlaySoundW : qword
public Proxy_PlaySoundW
Proxy_PlaySoundW proc
    jmp qword ptr [g_Real_PlaySoundW]
Proxy_PlaySoundW endp

extern g_Real_SendDriverMessage : qword
public Proxy_SendDriverMessage
Proxy_SendDriverMessage proc
    jmp qword ptr [g_Real_SendDriverMessage]
Proxy_SendDriverMessage endp

extern g_Real_WOWAppExit : qword
public Proxy_WOWAppExit
Proxy_WOWAppExit proc
    jmp qword ptr [g_Real_WOWAppExit]
Proxy_WOWAppExit endp

extern g_Real_auxGetDevCapsA : qword
public Proxy_auxGetDevCapsA
Proxy_auxGetDevCapsA proc
    jmp qword ptr [g_Real_auxGetDevCapsA]
Proxy_auxGetDevCapsA endp

extern g_Real_auxGetDevCapsW : qword
public Proxy_auxGetDevCapsW
Proxy_auxGetDevCapsW proc
    jmp qword ptr [g_Real_auxGetDevCapsW]
Proxy_auxGetDevCapsW endp

extern g_Real_auxGetNumDevs : qword
public Proxy_auxGetNumDevs
Proxy_auxGetNumDevs proc
    jmp qword ptr [g_Real_auxGetNumDevs]
Proxy_auxGetNumDevs endp

extern g_Real_auxGetVolume : qword
public Proxy_auxGetVolume
Proxy_auxGetVolume proc
    jmp qword ptr [g_Real_auxGetVolume]
Proxy_auxGetVolume endp

extern g_Real_auxOutMessage : qword
public Proxy_auxOutMessage
Proxy_auxOutMessage proc
    jmp qword ptr [g_Real_auxOutMessage]
Proxy_auxOutMessage endp

extern g_Real_auxSetVolume : qword
public Proxy_auxSetVolume
Proxy_auxSetVolume proc
    jmp qword ptr [g_Real_auxSetVolume]
Proxy_auxSetVolume endp

extern g_Real_joyConfigChanged : qword
public Proxy_joyConfigChanged
Proxy_joyConfigChanged proc
    jmp qword ptr [g_Real_joyConfigChanged]
Proxy_joyConfigChanged endp

extern g_Real_joyGetDevCapsA : qword
public Proxy_joyGetDevCapsA
Proxy_joyGetDevCapsA proc
    jmp qword ptr [g_Real_joyGetDevCapsA]
Proxy_joyGetDevCapsA endp

extern g_Real_joyGetDevCapsW : qword
public Proxy_joyGetDevCapsW
Proxy_joyGetDevCapsW proc
    jmp qword ptr [g_Real_joyGetDevCapsW]
Proxy_joyGetDevCapsW endp

extern g_Real_joyGetNumDevs : qword
public Proxy_joyGetNumDevs
Proxy_joyGetNumDevs proc
    jmp qword ptr [g_Real_joyGetNumDevs]
Proxy_joyGetNumDevs endp

extern g_Real_joyGetPos : qword
public Proxy_joyGetPos
Proxy_joyGetPos proc
    jmp qword ptr [g_Real_joyGetPos]
Proxy_joyGetPos endp

extern g_Real_joyGetPosEx : qword
public Proxy_joyGetPosEx
Proxy_joyGetPosEx proc
    jmp qword ptr [g_Real_joyGetPosEx]
Proxy_joyGetPosEx endp

extern g_Real_joyGetThreshold : qword
public Proxy_joyGetThreshold
Proxy_joyGetThreshold proc
    jmp qword ptr [g_Real_joyGetThreshold]
Proxy_joyGetThreshold endp

extern g_Real_joyReleaseCapture : qword
public Proxy_joyReleaseCapture
Proxy_joyReleaseCapture proc
    jmp qword ptr [g_Real_joyReleaseCapture]
Proxy_joyReleaseCapture endp

extern g_Real_joySetCapture : qword
public Proxy_joySetCapture
Proxy_joySetCapture proc
    jmp qword ptr [g_Real_joySetCapture]
Proxy_joySetCapture endp

extern g_Real_joySetThreshold : qword
public Proxy_joySetThreshold
Proxy_joySetThreshold proc
    jmp qword ptr [g_Real_joySetThreshold]
Proxy_joySetThreshold endp

extern g_Real_mciDriverNotify : qword
public Proxy_mciDriverNotify
Proxy_mciDriverNotify proc
    jmp qword ptr [g_Real_mciDriverNotify]
Proxy_mciDriverNotify endp

extern g_Real_mciDriverYield : qword
public Proxy_mciDriverYield
Proxy_mciDriverYield proc
    jmp qword ptr [g_Real_mciDriverYield]
Proxy_mciDriverYield endp

extern g_Real_mciExecute : qword
public Proxy_mciExecute
Proxy_mciExecute proc
    jmp qword ptr [g_Real_mciExecute]
Proxy_mciExecute endp

extern g_Real_mciFreeCommandResource : qword
public Proxy_mciFreeCommandResource
Proxy_mciFreeCommandResource proc
    jmp qword ptr [g_Real_mciFreeCommandResource]
Proxy_mciFreeCommandResource endp

extern g_Real_mciGetCreatorTask : qword
public Proxy_mciGetCreatorTask
Proxy_mciGetCreatorTask proc
    jmp qword ptr [g_Real_mciGetCreatorTask]
Proxy_mciGetCreatorTask endp

extern g_Real_mciGetDeviceIDA : qword
public Proxy_mciGetDeviceIDA
Proxy_mciGetDeviceIDA proc
    jmp qword ptr [g_Real_mciGetDeviceIDA]
Proxy_mciGetDeviceIDA endp

extern g_Real_mciGetDeviceIDFromElementIDA : qword
public Proxy_mciGetDeviceIDFromElementIDA
Proxy_mciGetDeviceIDFromElementIDA proc
    jmp qword ptr [g_Real_mciGetDeviceIDFromElementIDA]
Proxy_mciGetDeviceIDFromElementIDA endp

extern g_Real_mciGetDeviceIDFromElementIDW : qword
public Proxy_mciGetDeviceIDFromElementIDW
Proxy_mciGetDeviceIDFromElementIDW proc
    jmp qword ptr [g_Real_mciGetDeviceIDFromElementIDW]
Proxy_mciGetDeviceIDFromElementIDW endp

extern g_Real_mciGetDeviceIDW : qword
public Proxy_mciGetDeviceIDW
Proxy_mciGetDeviceIDW proc
    jmp qword ptr [g_Real_mciGetDeviceIDW]
Proxy_mciGetDeviceIDW endp

extern g_Real_mciGetDriverData : qword
public Proxy_mciGetDriverData
Proxy_mciGetDriverData proc
    jmp qword ptr [g_Real_mciGetDriverData]
Proxy_mciGetDriverData endp

extern g_Real_mciGetErrorStringA : qword
public Proxy_mciGetErrorStringA
Proxy_mciGetErrorStringA proc
    jmp qword ptr [g_Real_mciGetErrorStringA]
Proxy_mciGetErrorStringA endp

extern g_Real_mciGetErrorStringW : qword
public Proxy_mciGetErrorStringW
Proxy_mciGetErrorStringW proc
    jmp qword ptr [g_Real_mciGetErrorStringW]
Proxy_mciGetErrorStringW endp

extern g_Real_mciGetYieldProc : qword
public Proxy_mciGetYieldProc
Proxy_mciGetYieldProc proc
    jmp qword ptr [g_Real_mciGetYieldProc]
Proxy_mciGetYieldProc endp

extern g_Real_mciLoadCommandResource : qword
public Proxy_mciLoadCommandResource
Proxy_mciLoadCommandResource proc
    jmp qword ptr [g_Real_mciLoadCommandResource]
Proxy_mciLoadCommandResource endp

extern g_Real_mciSendCommandA : qword
public Proxy_mciSendCommandA
Proxy_mciSendCommandA proc
    jmp qword ptr [g_Real_mciSendCommandA]
Proxy_mciSendCommandA endp

extern g_Real_mciSendCommandW : qword
public Proxy_mciSendCommandW
Proxy_mciSendCommandW proc
    jmp qword ptr [g_Real_mciSendCommandW]
Proxy_mciSendCommandW endp

extern g_Real_mciSendStringA : qword
public Proxy_mciSendStringA
Proxy_mciSendStringA proc
    jmp qword ptr [g_Real_mciSendStringA]
Proxy_mciSendStringA endp

extern g_Real_mciSendStringW : qword
public Proxy_mciSendStringW
Proxy_mciSendStringW proc
    jmp qword ptr [g_Real_mciSendStringW]
Proxy_mciSendStringW endp

extern g_Real_mciSetDriverData : qword
public Proxy_mciSetDriverData
Proxy_mciSetDriverData proc
    jmp qword ptr [g_Real_mciSetDriverData]
Proxy_mciSetDriverData endp

extern g_Real_mciSetYieldProc : qword
public Proxy_mciSetYieldProc
Proxy_mciSetYieldProc proc
    jmp qword ptr [g_Real_mciSetYieldProc]
Proxy_mciSetYieldProc endp

extern g_Real_midiConnect : qword
public Proxy_midiConnect
Proxy_midiConnect proc
    jmp qword ptr [g_Real_midiConnect]
Proxy_midiConnect endp

extern g_Real_midiDisconnect : qword
public Proxy_midiDisconnect
Proxy_midiDisconnect proc
    jmp qword ptr [g_Real_midiDisconnect]
Proxy_midiDisconnect endp

extern g_Real_midiInAddBuffer : qword
public Proxy_midiInAddBuffer
Proxy_midiInAddBuffer proc
    jmp qword ptr [g_Real_midiInAddBuffer]
Proxy_midiInAddBuffer endp

extern g_Real_midiInClose : qword
public Proxy_midiInClose
Proxy_midiInClose proc
    jmp qword ptr [g_Real_midiInClose]
Proxy_midiInClose endp

extern g_Real_midiInGetDevCapsA : qword
public Proxy_midiInGetDevCapsA
Proxy_midiInGetDevCapsA proc
    jmp qword ptr [g_Real_midiInGetDevCapsA]
Proxy_midiInGetDevCapsA endp

extern g_Real_midiInGetDevCapsW : qword
public Proxy_midiInGetDevCapsW
Proxy_midiInGetDevCapsW proc
    jmp qword ptr [g_Real_midiInGetDevCapsW]
Proxy_midiInGetDevCapsW endp

extern g_Real_midiInGetErrorTextA : qword
public Proxy_midiInGetErrorTextA
Proxy_midiInGetErrorTextA proc
    jmp qword ptr [g_Real_midiInGetErrorTextA]
Proxy_midiInGetErrorTextA endp

extern g_Real_midiInGetErrorTextW : qword
public Proxy_midiInGetErrorTextW
Proxy_midiInGetErrorTextW proc
    jmp qword ptr [g_Real_midiInGetErrorTextW]
Proxy_midiInGetErrorTextW endp

extern g_Real_midiInGetID : qword
public Proxy_midiInGetID
Proxy_midiInGetID proc
    jmp qword ptr [g_Real_midiInGetID]
Proxy_midiInGetID endp

extern g_Real_midiInGetNumDevs : qword
public Proxy_midiInGetNumDevs
Proxy_midiInGetNumDevs proc
    jmp qword ptr [g_Real_midiInGetNumDevs]
Proxy_midiInGetNumDevs endp

extern g_Real_midiInMessage : qword
public Proxy_midiInMessage
Proxy_midiInMessage proc
    jmp qword ptr [g_Real_midiInMessage]
Proxy_midiInMessage endp

extern g_Real_midiInOpen : qword
public Proxy_midiInOpen
Proxy_midiInOpen proc
    jmp qword ptr [g_Real_midiInOpen]
Proxy_midiInOpen endp

extern g_Real_midiInPrepareHeader : qword
public Proxy_midiInPrepareHeader
Proxy_midiInPrepareHeader proc
    jmp qword ptr [g_Real_midiInPrepareHeader]
Proxy_midiInPrepareHeader endp

extern g_Real_midiInReset : qword
public Proxy_midiInReset
Proxy_midiInReset proc
    jmp qword ptr [g_Real_midiInReset]
Proxy_midiInReset endp

extern g_Real_midiInStart : qword
public Proxy_midiInStart
Proxy_midiInStart proc
    jmp qword ptr [g_Real_midiInStart]
Proxy_midiInStart endp

extern g_Real_midiInStop : qword
public Proxy_midiInStop
Proxy_midiInStop proc
    jmp qword ptr [g_Real_midiInStop]
Proxy_midiInStop endp

extern g_Real_midiInUnprepareHeader : qword
public Proxy_midiInUnprepareHeader
Proxy_midiInUnprepareHeader proc
    jmp qword ptr [g_Real_midiInUnprepareHeader]
Proxy_midiInUnprepareHeader endp

extern g_Real_midiOutCacheDrumPatches : qword
public Proxy_midiOutCacheDrumPatches
Proxy_midiOutCacheDrumPatches proc
    jmp qword ptr [g_Real_midiOutCacheDrumPatches]
Proxy_midiOutCacheDrumPatches endp

extern g_Real_midiOutCachePatches : qword
public Proxy_midiOutCachePatches
Proxy_midiOutCachePatches proc
    jmp qword ptr [g_Real_midiOutCachePatches]
Proxy_midiOutCachePatches endp

extern g_Real_midiOutClose : qword
public Proxy_midiOutClose
Proxy_midiOutClose proc
    jmp qword ptr [g_Real_midiOutClose]
Proxy_midiOutClose endp

extern g_Real_midiOutGetDevCapsA : qword
public Proxy_midiOutGetDevCapsA
Proxy_midiOutGetDevCapsA proc
    jmp qword ptr [g_Real_midiOutGetDevCapsA]
Proxy_midiOutGetDevCapsA endp

extern g_Real_midiOutGetDevCapsW : qword
public Proxy_midiOutGetDevCapsW
Proxy_midiOutGetDevCapsW proc
    jmp qword ptr [g_Real_midiOutGetDevCapsW]
Proxy_midiOutGetDevCapsW endp

extern g_Real_midiOutGetErrorTextA : qword
public Proxy_midiOutGetErrorTextA
Proxy_midiOutGetErrorTextA proc
    jmp qword ptr [g_Real_midiOutGetErrorTextA]
Proxy_midiOutGetErrorTextA endp

extern g_Real_midiOutGetErrorTextW : qword
public Proxy_midiOutGetErrorTextW
Proxy_midiOutGetErrorTextW proc
    jmp qword ptr [g_Real_midiOutGetErrorTextW]
Proxy_midiOutGetErrorTextW endp

extern g_Real_midiOutGetID : qword
public Proxy_midiOutGetID
Proxy_midiOutGetID proc
    jmp qword ptr [g_Real_midiOutGetID]
Proxy_midiOutGetID endp

extern g_Real_midiOutGetNumDevs : qword
public Proxy_midiOutGetNumDevs
Proxy_midiOutGetNumDevs proc
    jmp qword ptr [g_Real_midiOutGetNumDevs]
Proxy_midiOutGetNumDevs endp

extern g_Real_midiOutGetVolume : qword
public Proxy_midiOutGetVolume
Proxy_midiOutGetVolume proc
    jmp qword ptr [g_Real_midiOutGetVolume]
Proxy_midiOutGetVolume endp

extern g_Real_midiOutLongMsg : qword
public Proxy_midiOutLongMsg
Proxy_midiOutLongMsg proc
    jmp qword ptr [g_Real_midiOutLongMsg]
Proxy_midiOutLongMsg endp

extern g_Real_midiOutMessage : qword
public Proxy_midiOutMessage
Proxy_midiOutMessage proc
    jmp qword ptr [g_Real_midiOutMessage]
Proxy_midiOutMessage endp

extern g_Real_midiOutOpen : qword
public Proxy_midiOutOpen
Proxy_midiOutOpen proc
    jmp qword ptr [g_Real_midiOutOpen]
Proxy_midiOutOpen endp

extern g_Real_midiOutPrepareHeader : qword
public Proxy_midiOutPrepareHeader
Proxy_midiOutPrepareHeader proc
    jmp qword ptr [g_Real_midiOutPrepareHeader]
Proxy_midiOutPrepareHeader endp

extern g_Real_midiOutReset : qword
public Proxy_midiOutReset
Proxy_midiOutReset proc
    jmp qword ptr [g_Real_midiOutReset]
Proxy_midiOutReset endp

extern g_Real_midiOutSetVolume : qword
public Proxy_midiOutSetVolume
Proxy_midiOutSetVolume proc
    jmp qword ptr [g_Real_midiOutSetVolume]
Proxy_midiOutSetVolume endp

extern g_Real_midiOutShortMsg : qword
public Proxy_midiOutShortMsg
Proxy_midiOutShortMsg proc
    jmp qword ptr [g_Real_midiOutShortMsg]
Proxy_midiOutShortMsg endp

extern g_Real_midiOutUnprepareHeader : qword
public Proxy_midiOutUnprepareHeader
Proxy_midiOutUnprepareHeader proc
    jmp qword ptr [g_Real_midiOutUnprepareHeader]
Proxy_midiOutUnprepareHeader endp

extern g_Real_midiStreamClose : qword
public Proxy_midiStreamClose
Proxy_midiStreamClose proc
    jmp qword ptr [g_Real_midiStreamClose]
Proxy_midiStreamClose endp

extern g_Real_midiStreamOpen : qword
public Proxy_midiStreamOpen
Proxy_midiStreamOpen proc
    jmp qword ptr [g_Real_midiStreamOpen]
Proxy_midiStreamOpen endp

extern g_Real_midiStreamOut : qword
public Proxy_midiStreamOut
Proxy_midiStreamOut proc
    jmp qword ptr [g_Real_midiStreamOut]
Proxy_midiStreamOut endp

extern g_Real_midiStreamPause : qword
public Proxy_midiStreamPause
Proxy_midiStreamPause proc
    jmp qword ptr [g_Real_midiStreamPause]
Proxy_midiStreamPause endp

extern g_Real_midiStreamPosition : qword
public Proxy_midiStreamPosition
Proxy_midiStreamPosition proc
    jmp qword ptr [g_Real_midiStreamPosition]
Proxy_midiStreamPosition endp

extern g_Real_midiStreamProperty : qword
public Proxy_midiStreamProperty
Proxy_midiStreamProperty proc
    jmp qword ptr [g_Real_midiStreamProperty]
Proxy_midiStreamProperty endp

extern g_Real_midiStreamRestart : qword
public Proxy_midiStreamRestart
Proxy_midiStreamRestart proc
    jmp qword ptr [g_Real_midiStreamRestart]
Proxy_midiStreamRestart endp

extern g_Real_midiStreamStop : qword
public Proxy_midiStreamStop
Proxy_midiStreamStop proc
    jmp qword ptr [g_Real_midiStreamStop]
Proxy_midiStreamStop endp

extern g_Real_mixerClose : qword
public Proxy_mixerClose
Proxy_mixerClose proc
    jmp qword ptr [g_Real_mixerClose]
Proxy_mixerClose endp

extern g_Real_mixerGetControlDetailsA : qword
public Proxy_mixerGetControlDetailsA
Proxy_mixerGetControlDetailsA proc
    jmp qword ptr [g_Real_mixerGetControlDetailsA]
Proxy_mixerGetControlDetailsA endp

extern g_Real_mixerGetControlDetailsW : qword
public Proxy_mixerGetControlDetailsW
Proxy_mixerGetControlDetailsW proc
    jmp qword ptr [g_Real_mixerGetControlDetailsW]
Proxy_mixerGetControlDetailsW endp

extern g_Real_mixerGetDevCapsA : qword
public Proxy_mixerGetDevCapsA
Proxy_mixerGetDevCapsA proc
    jmp qword ptr [g_Real_mixerGetDevCapsA]
Proxy_mixerGetDevCapsA endp

extern g_Real_mixerGetDevCapsW : qword
public Proxy_mixerGetDevCapsW
Proxy_mixerGetDevCapsW proc
    jmp qword ptr [g_Real_mixerGetDevCapsW]
Proxy_mixerGetDevCapsW endp

extern g_Real_mixerGetID : qword
public Proxy_mixerGetID
Proxy_mixerGetID proc
    jmp qword ptr [g_Real_mixerGetID]
Proxy_mixerGetID endp

extern g_Real_mixerGetLineControlsA : qword
public Proxy_mixerGetLineControlsA
Proxy_mixerGetLineControlsA proc
    jmp qword ptr [g_Real_mixerGetLineControlsA]
Proxy_mixerGetLineControlsA endp

extern g_Real_mixerGetLineControlsW : qword
public Proxy_mixerGetLineControlsW
Proxy_mixerGetLineControlsW proc
    jmp qword ptr [g_Real_mixerGetLineControlsW]
Proxy_mixerGetLineControlsW endp

extern g_Real_mixerGetLineInfoA : qword
public Proxy_mixerGetLineInfoA
Proxy_mixerGetLineInfoA proc
    jmp qword ptr [g_Real_mixerGetLineInfoA]
Proxy_mixerGetLineInfoA endp

extern g_Real_mixerGetLineInfoW : qword
public Proxy_mixerGetLineInfoW
Proxy_mixerGetLineInfoW proc
    jmp qword ptr [g_Real_mixerGetLineInfoW]
Proxy_mixerGetLineInfoW endp

extern g_Real_mixerGetNumDevs : qword
public Proxy_mixerGetNumDevs
Proxy_mixerGetNumDevs proc
    jmp qword ptr [g_Real_mixerGetNumDevs]
Proxy_mixerGetNumDevs endp

extern g_Real_mixerMessage : qword
public Proxy_mixerMessage
Proxy_mixerMessage proc
    jmp qword ptr [g_Real_mixerMessage]
Proxy_mixerMessage endp

extern g_Real_mixerOpen : qword
public Proxy_mixerOpen
Proxy_mixerOpen proc
    jmp qword ptr [g_Real_mixerOpen]
Proxy_mixerOpen endp

extern g_Real_mixerSetControlDetails : qword
public Proxy_mixerSetControlDetails
Proxy_mixerSetControlDetails proc
    jmp qword ptr [g_Real_mixerSetControlDetails]
Proxy_mixerSetControlDetails endp

extern g_Real_mmDrvInstall : qword
public Proxy_mmDrvInstall
Proxy_mmDrvInstall proc
    jmp qword ptr [g_Real_mmDrvInstall]
Proxy_mmDrvInstall endp

extern g_Real_mmGetCurrentTask : qword
public Proxy_mmGetCurrentTask
Proxy_mmGetCurrentTask proc
    jmp qword ptr [g_Real_mmGetCurrentTask]
Proxy_mmGetCurrentTask endp

extern g_Real_mmTaskBlock : qword
public Proxy_mmTaskBlock
Proxy_mmTaskBlock proc
    jmp qword ptr [g_Real_mmTaskBlock]
Proxy_mmTaskBlock endp

extern g_Real_mmTaskCreate : qword
public Proxy_mmTaskCreate
Proxy_mmTaskCreate proc
    jmp qword ptr [g_Real_mmTaskCreate]
Proxy_mmTaskCreate endp

extern g_Real_mmTaskSignal : qword
public Proxy_mmTaskSignal
Proxy_mmTaskSignal proc
    jmp qword ptr [g_Real_mmTaskSignal]
Proxy_mmTaskSignal endp

extern g_Real_mmTaskYield : qword
public Proxy_mmTaskYield
Proxy_mmTaskYield proc
    jmp qword ptr [g_Real_mmTaskYield]
Proxy_mmTaskYield endp

extern g_Real_mmioAdvance : qword
public Proxy_mmioAdvance
Proxy_mmioAdvance proc
    jmp qword ptr [g_Real_mmioAdvance]
Proxy_mmioAdvance endp

extern g_Real_mmioAscend : qword
public Proxy_mmioAscend
Proxy_mmioAscend proc
    jmp qword ptr [g_Real_mmioAscend]
Proxy_mmioAscend endp

extern g_Real_mmioClose : qword
public Proxy_mmioClose
Proxy_mmioClose proc
    jmp qword ptr [g_Real_mmioClose]
Proxy_mmioClose endp

extern g_Real_mmioCreateChunk : qword
public Proxy_mmioCreateChunk
Proxy_mmioCreateChunk proc
    jmp qword ptr [g_Real_mmioCreateChunk]
Proxy_mmioCreateChunk endp

extern g_Real_mmioDescend : qword
public Proxy_mmioDescend
Proxy_mmioDescend proc
    jmp qword ptr [g_Real_mmioDescend]
Proxy_mmioDescend endp

extern g_Real_mmioFlush : qword
public Proxy_mmioFlush
Proxy_mmioFlush proc
    jmp qword ptr [g_Real_mmioFlush]
Proxy_mmioFlush endp

extern g_Real_mmioGetInfo : qword
public Proxy_mmioGetInfo
Proxy_mmioGetInfo proc
    jmp qword ptr [g_Real_mmioGetInfo]
Proxy_mmioGetInfo endp

extern g_Real_mmioInstallIOProcA : qword
public Proxy_mmioInstallIOProcA
Proxy_mmioInstallIOProcA proc
    jmp qword ptr [g_Real_mmioInstallIOProcA]
Proxy_mmioInstallIOProcA endp

extern g_Real_mmioInstallIOProcW : qword
public Proxy_mmioInstallIOProcW
Proxy_mmioInstallIOProcW proc
    jmp qword ptr [g_Real_mmioInstallIOProcW]
Proxy_mmioInstallIOProcW endp

extern g_Real_mmioOpenA : qword
public Proxy_mmioOpenA
Proxy_mmioOpenA proc
    jmp qword ptr [g_Real_mmioOpenA]
Proxy_mmioOpenA endp

extern g_Real_mmioOpenW : qword
public Proxy_mmioOpenW
Proxy_mmioOpenW proc
    jmp qword ptr [g_Real_mmioOpenW]
Proxy_mmioOpenW endp

extern g_Real_mmioRead : qword
public Proxy_mmioRead
Proxy_mmioRead proc
    jmp qword ptr [g_Real_mmioRead]
Proxy_mmioRead endp

extern g_Real_mmioRenameA : qword
public Proxy_mmioRenameA
Proxy_mmioRenameA proc
    jmp qword ptr [g_Real_mmioRenameA]
Proxy_mmioRenameA endp

extern g_Real_mmioRenameW : qword
public Proxy_mmioRenameW
Proxy_mmioRenameW proc
    jmp qword ptr [g_Real_mmioRenameW]
Proxy_mmioRenameW endp

extern g_Real_mmioSeek : qword
public Proxy_mmioSeek
Proxy_mmioSeek proc
    jmp qword ptr [g_Real_mmioSeek]
Proxy_mmioSeek endp

extern g_Real_mmioSendMessage : qword
public Proxy_mmioSendMessage
Proxy_mmioSendMessage proc
    jmp qword ptr [g_Real_mmioSendMessage]
Proxy_mmioSendMessage endp

extern g_Real_mmioSetBuffer : qword
public Proxy_mmioSetBuffer
Proxy_mmioSetBuffer proc
    jmp qword ptr [g_Real_mmioSetBuffer]
Proxy_mmioSetBuffer endp

extern g_Real_mmioSetInfo : qword
public Proxy_mmioSetInfo
Proxy_mmioSetInfo proc
    jmp qword ptr [g_Real_mmioSetInfo]
Proxy_mmioSetInfo endp

extern g_Real_mmioStringToFOURCCA : qword
public Proxy_mmioStringToFOURCCA
Proxy_mmioStringToFOURCCA proc
    jmp qword ptr [g_Real_mmioStringToFOURCCA]
Proxy_mmioStringToFOURCCA endp

extern g_Real_mmioStringToFOURCCW : qword
public Proxy_mmioStringToFOURCCW
Proxy_mmioStringToFOURCCW proc
    jmp qword ptr [g_Real_mmioStringToFOURCCW]
Proxy_mmioStringToFOURCCW endp

extern g_Real_mmioWrite : qword
public Proxy_mmioWrite
Proxy_mmioWrite proc
    jmp qword ptr [g_Real_mmioWrite]
Proxy_mmioWrite endp

extern g_Real_mmsystemGetVersion : qword
public Proxy_mmsystemGetVersion
Proxy_mmsystemGetVersion proc
    jmp qword ptr [g_Real_mmsystemGetVersion]
Proxy_mmsystemGetVersion endp

extern g_Real_sndPlaySoundA : qword
public Proxy_sndPlaySoundA
Proxy_sndPlaySoundA proc
    jmp qword ptr [g_Real_sndPlaySoundA]
Proxy_sndPlaySoundA endp

extern g_Real_sndPlaySoundW : qword
public Proxy_sndPlaySoundW
Proxy_sndPlaySoundW proc
    jmp qword ptr [g_Real_sndPlaySoundW]
Proxy_sndPlaySoundW endp

extern g_Real_timeBeginPeriod : qword
public Proxy_timeBeginPeriod
Proxy_timeBeginPeriod proc
    jmp qword ptr [g_Real_timeBeginPeriod]
Proxy_timeBeginPeriod endp

extern g_Real_timeEndPeriod : qword
public Proxy_timeEndPeriod
Proxy_timeEndPeriod proc
    jmp qword ptr [g_Real_timeEndPeriod]
Proxy_timeEndPeriod endp

extern g_Real_timeGetDevCaps : qword
public Proxy_timeGetDevCaps
Proxy_timeGetDevCaps proc
    jmp qword ptr [g_Real_timeGetDevCaps]
Proxy_timeGetDevCaps endp

extern g_Real_timeGetSystemTime : qword
public Proxy_timeGetSystemTime
Proxy_timeGetSystemTime proc
    jmp qword ptr [g_Real_timeGetSystemTime]
Proxy_timeGetSystemTime endp

extern g_Real_timeGetTime : qword
public Proxy_timeGetTime
Proxy_timeGetTime proc
    jmp qword ptr [g_Real_timeGetTime]
Proxy_timeGetTime endp

extern g_Real_timeKillEvent : qword
public Proxy_timeKillEvent
Proxy_timeKillEvent proc
    jmp qword ptr [g_Real_timeKillEvent]
Proxy_timeKillEvent endp

extern g_Real_timeSetEvent : qword
public Proxy_timeSetEvent
Proxy_timeSetEvent proc
    jmp qword ptr [g_Real_timeSetEvent]
Proxy_timeSetEvent endp

extern g_Real_waveInAddBuffer : qword
public Proxy_waveInAddBuffer
Proxy_waveInAddBuffer proc
    jmp qword ptr [g_Real_waveInAddBuffer]
Proxy_waveInAddBuffer endp

extern g_Real_waveInClose : qword
public Proxy_waveInClose
Proxy_waveInClose proc
    jmp qword ptr [g_Real_waveInClose]
Proxy_waveInClose endp

extern g_Real_waveInGetDevCapsA : qword
public Proxy_waveInGetDevCapsA
Proxy_waveInGetDevCapsA proc
    jmp qword ptr [g_Real_waveInGetDevCapsA]
Proxy_waveInGetDevCapsA endp

extern g_Real_waveInGetDevCapsW : qword
public Proxy_waveInGetDevCapsW
Proxy_waveInGetDevCapsW proc
    jmp qword ptr [g_Real_waveInGetDevCapsW]
Proxy_waveInGetDevCapsW endp

extern g_Real_waveInGetErrorTextA : qword
public Proxy_waveInGetErrorTextA
Proxy_waveInGetErrorTextA proc
    jmp qword ptr [g_Real_waveInGetErrorTextA]
Proxy_waveInGetErrorTextA endp

extern g_Real_waveInGetErrorTextW : qword
public Proxy_waveInGetErrorTextW
Proxy_waveInGetErrorTextW proc
    jmp qword ptr [g_Real_waveInGetErrorTextW]
Proxy_waveInGetErrorTextW endp

extern g_Real_waveInGetID : qword
public Proxy_waveInGetID
Proxy_waveInGetID proc
    jmp qword ptr [g_Real_waveInGetID]
Proxy_waveInGetID endp

extern g_Real_waveInGetNumDevs : qword
public Proxy_waveInGetNumDevs
Proxy_waveInGetNumDevs proc
    jmp qword ptr [g_Real_waveInGetNumDevs]
Proxy_waveInGetNumDevs endp

extern g_Real_waveInGetPosition : qword
public Proxy_waveInGetPosition
Proxy_waveInGetPosition proc
    jmp qword ptr [g_Real_waveInGetPosition]
Proxy_waveInGetPosition endp

extern g_Real_waveInMessage : qword
public Proxy_waveInMessage
Proxy_waveInMessage proc
    jmp qword ptr [g_Real_waveInMessage]
Proxy_waveInMessage endp

extern g_Real_waveInOpen : qword
public Proxy_waveInOpen
Proxy_waveInOpen proc
    jmp qword ptr [g_Real_waveInOpen]
Proxy_waveInOpen endp

extern g_Real_waveInPrepareHeader : qword
public Proxy_waveInPrepareHeader
Proxy_waveInPrepareHeader proc
    jmp qword ptr [g_Real_waveInPrepareHeader]
Proxy_waveInPrepareHeader endp

extern g_Real_waveInReset : qword
public Proxy_waveInReset
Proxy_waveInReset proc
    jmp qword ptr [g_Real_waveInReset]
Proxy_waveInReset endp

extern g_Real_waveInStart : qword
public Proxy_waveInStart
Proxy_waveInStart proc
    jmp qword ptr [g_Real_waveInStart]
Proxy_waveInStart endp

extern g_Real_waveInStop : qword
public Proxy_waveInStop
Proxy_waveInStop proc
    jmp qword ptr [g_Real_waveInStop]
Proxy_waveInStop endp

extern g_Real_waveInUnprepareHeader : qword
public Proxy_waveInUnprepareHeader
Proxy_waveInUnprepareHeader proc
    jmp qword ptr [g_Real_waveInUnprepareHeader]
Proxy_waveInUnprepareHeader endp

extern g_Real_waveOutBreakLoop : qword
public Proxy_waveOutBreakLoop
Proxy_waveOutBreakLoop proc
    jmp qword ptr [g_Real_waveOutBreakLoop]
Proxy_waveOutBreakLoop endp

extern g_Real_waveOutClose : qword
public Proxy_waveOutClose
Proxy_waveOutClose proc
    jmp qword ptr [g_Real_waveOutClose]
Proxy_waveOutClose endp

extern g_Real_waveOutGetDevCapsA : qword
public Proxy_waveOutGetDevCapsA
Proxy_waveOutGetDevCapsA proc
    jmp qword ptr [g_Real_waveOutGetDevCapsA]
Proxy_waveOutGetDevCapsA endp

extern g_Real_waveOutGetDevCapsW : qword
public Proxy_waveOutGetDevCapsW
Proxy_waveOutGetDevCapsW proc
    jmp qword ptr [g_Real_waveOutGetDevCapsW]
Proxy_waveOutGetDevCapsW endp

extern g_Real_waveOutGetErrorTextA : qword
public Proxy_waveOutGetErrorTextA
Proxy_waveOutGetErrorTextA proc
    jmp qword ptr [g_Real_waveOutGetErrorTextA]
Proxy_waveOutGetErrorTextA endp

extern g_Real_waveOutGetErrorTextW : qword
public Proxy_waveOutGetErrorTextW
Proxy_waveOutGetErrorTextW proc
    jmp qword ptr [g_Real_waveOutGetErrorTextW]
Proxy_waveOutGetErrorTextW endp

extern g_Real_waveOutGetID : qword
public Proxy_waveOutGetID
Proxy_waveOutGetID proc
    jmp qword ptr [g_Real_waveOutGetID]
Proxy_waveOutGetID endp

extern g_Real_waveOutGetNumDevs : qword
public Proxy_waveOutGetNumDevs
Proxy_waveOutGetNumDevs proc
    jmp qword ptr [g_Real_waveOutGetNumDevs]
Proxy_waveOutGetNumDevs endp

extern g_Real_waveOutGetPitch : qword
public Proxy_waveOutGetPitch
Proxy_waveOutGetPitch proc
    jmp qword ptr [g_Real_waveOutGetPitch]
Proxy_waveOutGetPitch endp

extern g_Real_waveOutGetPlaybackRate : qword
public Proxy_waveOutGetPlaybackRate
Proxy_waveOutGetPlaybackRate proc
    jmp qword ptr [g_Real_waveOutGetPlaybackRate]
Proxy_waveOutGetPlaybackRate endp

extern g_Real_waveOutGetPosition : qword
public Proxy_waveOutGetPosition
Proxy_waveOutGetPosition proc
    jmp qword ptr [g_Real_waveOutGetPosition]
Proxy_waveOutGetPosition endp

extern g_Real_waveOutGetVolume : qword
public Proxy_waveOutGetVolume
Proxy_waveOutGetVolume proc
    jmp qword ptr [g_Real_waveOutGetVolume]
Proxy_waveOutGetVolume endp

extern g_Real_waveOutMessage : qword
public Proxy_waveOutMessage
Proxy_waveOutMessage proc
    jmp qword ptr [g_Real_waveOutMessage]
Proxy_waveOutMessage endp

extern g_Real_waveOutOpen : qword
public Proxy_waveOutOpen
Proxy_waveOutOpen proc
    jmp qword ptr [g_Real_waveOutOpen]
Proxy_waveOutOpen endp

extern g_Real_waveOutPause : qword
public Proxy_waveOutPause
Proxy_waveOutPause proc
    jmp qword ptr [g_Real_waveOutPause]
Proxy_waveOutPause endp

extern g_Real_waveOutPrepareHeader : qword
public Proxy_waveOutPrepareHeader
Proxy_waveOutPrepareHeader proc
    jmp qword ptr [g_Real_waveOutPrepareHeader]
Proxy_waveOutPrepareHeader endp

extern g_Real_waveOutReset : qword
public Proxy_waveOutReset
Proxy_waveOutReset proc
    jmp qword ptr [g_Real_waveOutReset]
Proxy_waveOutReset endp

extern g_Real_waveOutRestart : qword
public Proxy_waveOutRestart
Proxy_waveOutRestart proc
    jmp qword ptr [g_Real_waveOutRestart]
Proxy_waveOutRestart endp

extern g_Real_waveOutSetPitch : qword
public Proxy_waveOutSetPitch
Proxy_waveOutSetPitch proc
    jmp qword ptr [g_Real_waveOutSetPitch]
Proxy_waveOutSetPitch endp

extern g_Real_waveOutSetPlaybackRate : qword
public Proxy_waveOutSetPlaybackRate
Proxy_waveOutSetPlaybackRate proc
    jmp qword ptr [g_Real_waveOutSetPlaybackRate]
Proxy_waveOutSetPlaybackRate endp

extern g_Real_waveOutSetVolume : qword
public Proxy_waveOutSetVolume
Proxy_waveOutSetVolume proc
    jmp qword ptr [g_Real_waveOutSetVolume]
Proxy_waveOutSetVolume endp

extern g_Real_waveOutUnprepareHeader : qword
public Proxy_waveOutUnprepareHeader
Proxy_waveOutUnprepareHeader proc
    jmp qword ptr [g_Real_waveOutUnprepareHeader]
Proxy_waveOutUnprepareHeader endp

extern g_Real_waveOutWrite : qword
public Proxy_waveOutWrite
Proxy_waveOutWrite proc
    jmp qword ptr [g_Real_waveOutWrite]
Proxy_waveOutWrite endp

extern g_Real_Ordinal2 : qword
public Proxy_Ordinal2
Proxy_Ordinal2 proc
    jmp qword ptr [g_Real_Ordinal2]
Proxy_Ordinal2 endp

end
