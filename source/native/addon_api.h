#pragma once
// C ABI shared by the engine (DLSSG-Transfusion.dll / .asi) and the optional
// ReShade add-on. The add-on finds the engine by these export names, whatever
// the proxy file is called, and never links against it.
#include <stdint.h>

#define DLSSGT_ADDON_API_VERSION 6u

// Writes the full path of the DLSSG-Transfusion.json in use (NUL terminated).
// Returns the length without the terminator, or 0 if the path is not known yet
// or does not fit in `capacity` characters.
#define DLSSGT_EXPORT_GET_CONFIG_PATH "DLSSGTransfusion_GetConfigPath"
typedef uint32_t(__stdcall* DLSSGTGetConfigPathFn)(wchar_t* buffer, uint32_t capacity);

#define DLSSGT_EXPORT_GET_STATUS "DLSSGTransfusion_GetStatus"

struct DLSSGTStatus
{
    uint32_t size;                  // in: sizeof(DLSSGTStatus)
    uint32_t version;               // in: DLSSGT_ADDON_API_VERSION
    uint32_t bridgeReady;           // Streamline wrapper and NGX provider patched
    uint32_t frameGenerationOn;     // the game currently requests DLSS-G on
    uint32_t pending;               // latest request not yet accepted by the game
    uint32_t setOptionsSeen;        // the game has submitted DLSS-G options
    int32_t setOptionsResult;       // last sl::Result of the game's options call
    uint32_t appliedMultiplier;     // multiplier last submitted to Streamline
    uint32_t actualFramesPresented; // frames presented per rendered frame (DLSS-G state)
    uint32_t stateSampleAgeMs;      // age of actualFramesPresented
    uint32_t realFpsMilli;          // rendered frames per second x1000
    uint32_t dlssFpsMilli;          // presented frames per second x1000
    uint32_t fpsSampleAgeMs;        // age of the FPS sample
    char route[16];                 // patch route: "local", "ota", "external", ...
    uint32_t uiRecomposition;       // 0 off, 1 on, 2 on (forced by DLSSG-Transfusion)
    uint32_t hudlessSource;         // 0 none, 1 game, 2 UI assist capture
    uint32_t uiAlphaSource;         // 0 none, 1 game, 2 UI assist injection
    char dlssVersion[24];           // nvngx_dlss.dll (Super Resolution), "" if not loaded
    char dlssgVersion[24];          // nvngx_dlssg.dll (Frame Generation)
    char streamlineVersion[24];     // sl.interposer.dll
    uint32_t pacingValid;           // displayed-frame pacing below is fresh (FG on)
    uint32_t pacingAverageUs;       // average interval between displayed frames
    uint32_t pacingP99Us;           // 99th percentile interval
    uint32_t pacingJitterUs;        // standard deviation of the interval
    uint32_t gpuValid;              // NVML sample below is available
    uint32_t gpuUtilization;        // %
    uint32_t gpuTemperatureC;
    uint32_t gpuPowerMilliwatts;
    uint32_t gpuClockMhz;
    uint32_t gpuMemoryClockMhz;
    uint32_t vramUsedMb;            // whole GPU, all processes
    uint32_t vramTotalMb;
    char debugLine[48];             // same text as the overlay debug line
    uint32_t srHooked;              // DLSS Super Resolution (NGX D3D12) hooks installed
    uint32_t srScale;               // requested render scale, percent x1000 (0 = game)
    uint32_t srObserved;            // a DLSS SR evaluation was seen in the last 2 s
    uint32_t srVerified;            // 8 consecutive frames rendered at the requested scale
    uint32_t srInputWidth, srInputHeight;    // last observed render resolution
    uint32_t srOutputWidth, srOutputHeight;  // last observed output resolution
    uint32_t unrealState;           // r.ScreenPercentage live control: 0 searching, 1 not found, 2 found
    uint32_t unrealScreenPercentageMilli;    // its current value, percent x1000
};

typedef int(__stdcall* DLSSGTGetStatusFn)(DLSSGTStatus* status);

// Suspends the engine's keyboard shortcuts while the add-on records one (1),
// resumes them (0).
#define DLSSGT_EXPORT_SET_HOTKEY_CAPTURE "DLSSGTransfusion_SetHotkeyCapture"
typedef void(__stdcall* DLSSGTSetHotkeyCaptureFn)(int active);

// The multiplier overlay's text, for the add-on to draw where the engine's own
// overlay cannot (it draws through DXGI only; Vulkan games have none). Same
// lines and settings as the engine's overlay. Returns 0 if `size` is too small.
// Optional export: DLSSGTStatus and the API version are unchanged.
#define DLSSGT_EXPORT_GET_OVERLAY "DLSSGTransfusion_GetOverlay"

#define DLSSGT_OVERLAY_MAX_LINES 9u
#define DLSSGT_OVERLAY_LINE_LENGTH 48u

struct DLSSGTOverlay
{
    uint32_t size;                  // in: sizeof(DLSSGTOverlay)
    uint32_t visible;               // "showOverlay" / Ctrl+Alt+O
    uint32_t position;              // 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left
    uint32_t nativeDrawing;         // the engine's DXGI overlay drew in the last second
    uint32_t lineCount;             // 0: nothing to show (FG off or suspended, no sample yet)
    char lines[DLSSGT_OVERLAY_MAX_LINES][DLSSGT_OVERLAY_LINE_LENGTH];
};

typedef int(__stdcall* DLSSGTGetOverlayFn)(DLSSGTOverlay* overlay);
