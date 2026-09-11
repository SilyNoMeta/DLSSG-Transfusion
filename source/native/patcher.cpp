#include "shared.h"
#include "scatter_experiment.h"
#include "midpoint_fix.h"
#include "dlssg_provider_policy.h"
#include "proxy.h"
#include "crash_diagnostics.h"
#include "detours/detours.h"
#include "nvidia_mfg_manifest.generated.h"
#include "pacing_policy.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <winternl.h>
#include <sl.h>
#include <sl_dlss_g.h>
#include <sl_consts.h>
#include <sl_matrix_helpers.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <mutex>
#include <share.h>
#include <string>
#include <vector>

namespace
{
FILE* gLog = nullptr;
std::atomic<uint64_t> gD3DDeviceStartTick{0};
std::atomic<DWORD> gD3DDeviceThread{0};
std::atomic<uint32_t> gDesiredMultiplier{2};
std::atomic<bool> gDesiredDynamicMode{false};
std::atomic<uint32_t> gDynamicTargetFrameRate{0};
std::atomic<bool> gDynamicExperimental56{false};
std::atomic<uint64_t> gDesiredRevision{0};
std::atomic<uint64_t> gAppliedRevision{0};
std::atomic<uint64_t> gAttemptedRevision{0};
std::atomic<uint64_t> gLastAttemptTick{0};
std::atomic<bool> gControlReady{false};
std::atomic<PFun_slGetFeatureFunction*> gOriginalGetFeatureFunction{nullptr};
PFun_slGetFeatureFunction* gDetourSlGetFeatureFunction = nullptr;
std::atomic<PFun_slSetD3DDevice*> gOriginalSetD3DDevice{nullptr};
PFun_slSetD3DDevice* gDetourSlSetD3DDevice = nullptr;
std::atomic<PFun_slSetTag*> gOriginalSetTag{nullptr};
PFun_slSetTag* gDetourSlSetTag = nullptr;
std::atomic<PFun_slSetTagForFrame*> gOriginalSetTagForFrame{nullptr};
PFun_slSetTagForFrame* gDetourSlSetTagForFrame = nullptr;
std::atomic<PFun_slDLSSGSetOptions*> gOriginalSetOptions{nullptr};
std::atomic<PFun_slDLSSGGetState*> gOriginalGetState{nullptr};
using PFun_slSetData = sl::Result(const sl::BaseStructure*, sl::CommandBuffer*);
std::atomic<PFun_slSetData*> gOriginalSlSetData{nullptr};
PFun_slSetData* gDetourSlSetData = nullptr;
using PFun_slSetConstants = sl::Result(const sl::Constants&, const sl::FrameToken&, const sl::ViewportHandle&);
std::atomic<PFun_slSetConstants*> gOriginalSlSetConstants{nullptr};
PFun_slSetConstants* gDetourSlSetConstants = nullptr;
std::atomic<bool> gSetOptionsHookExposed{false};
std::atomic<bool> gGetStateHookExposed{false};
std::atomic<bool> gSetOptionsSeen{false};
std::atomic<bool> gGetStateSeen{false};
std::atomic<bool> gGameFrameGenerationOn{false};
std::atomic<int32_t> gLastSetOptionsResult{static_cast<int32_t>(sl::Result::eErrorNotInitialized)};
std::atomic<int32_t> gLastGetStateResult{static_cast<int32_t>(sl::Result::eErrorNotInitialized)};
std::atomic<bool> gAppliedDynamicMode{false};
std::atomic<uint32_t> gAppliedMultiplier{0};
std::atomic<uint32_t> gAppliedDynamicTargetFrameRate{0};
std::atomic<bool> gAppliedDynamicExperimental56{false};
std::atomic<uint32_t> gActualFramesPresented{0};
std::atomic<uint32_t> gNumFramesToGenerateMax{0};
std::atomic<uint32_t> gDlssgStatus{0};
std::atomic<bool> gDynamicMfgSupported{false};
std::atomic<uint64_t> gStateSampleTick{0};
std::atomic<uint64_t> gSetOptionsCalls{0};
std::atomic<uint64_t> gGetStateCalls{0};
std::atomic<uint64_t> gLiveReapplyCount{0};
std::atomic<uint64_t> gNotInitializedRetryCount{0};
std::atomic<bool> gDllNotificationRegistered{false};
std::atomic<bool> gModuleInventoryDirty{true};
std::atomic<bool> gLiveHookInstalled{false};
std::atomic<bool> gInterposerDetoursInstalled{false};
std::atomic<bool> gUiTagHookInstalled{false};
std::atomic<uint32_t> gLoadedWrapperCandidates{0};
std::atomic<uint32_t> gPatchedWrapperCandidates{0};
std::atomic<uint32_t> gLoadedNgxCandidates{0};
std::atomic<uint32_t> gPatchedNgxCandidates{0};
std::atomic<uint32_t> gWrapperRouteBits{0};
std::atomic<uint32_t> gNgxRouteBits{0};
std::atomic<bool> gActiveWrapperObserved{false};
std::atomic<bool> gActiveWrapperPatched{false};
std::atomic<uintptr_t> gActiveWrapperBase{0};
std::atomic<uint32_t> gLastOptionsViewport{UINT32_MAX};
std::atomic<uint32_t> gGameOptionsStructVersion{0};
std::atomic<uint32_t> gGameColorWidth{0};
std::atomic<uint32_t> gGameColorHeight{0};
std::atomic<uint32_t> gGameHudlessBufferFormat{0};
std::atomic<uint32_t> gGameUiBufferFormat{0};
std::atomic<bool> gGameUiRecompositionEnabled{false};
std::atomic<bool> gUiInputsReady{false};
std::atomic<bool> gAppliedUiRecompositionEnabled{false};
std::atomic<bool> gAppliedUiRecompositionForced{false};
std::atomic<uint64_t> gSetTagCalls{0};
std::atomic<uint64_t> gSetTagForFrameCalls{0};
std::atomic<uint32_t> gRealFpsMilli{0};
std::atomic<uint32_t> gDlssFpsMilli{0};
std::atomic<uint32_t> gFpsSampleWindowMs{0};
std::atomic<uint64_t> gFpsSampleTick{0};
std::atomic<bool> gLogReady{false};
std::atomic<uint32_t> gAdvertisedMaxGenerated{5};
std::atomic<uint8_t**> gStreamlineDlssgContextPtr{nullptr};
std::atomic<bool> gStopWorker{false};
std::atomic<bool> gFlipMeteringPatched{false};
std::atomic<uint32_t> gFlipMeteringOffset{0};
std::atomic<uint32_t> gFlipMeteringValue{0};
std::mutex gStreamlineCallMutex;
// LoadLibrary callbacks and the worker can both install hooks. Never wait here:
// a callback may hold the loader lock while another installer needs that lock.
std::mutex gHookInstallMutex;
std::mutex gLastOptionsMutex;
std::recursive_mutex gModuleMutex;
std::mutex gUiTagMutex;
std::wstring gConfigPath;
std::wstring gStatusPath;
std::wstring gExecutableDirectory;

constexpr uint32_t kRouteLocal = 1u;
constexpr uint32_t kRouteExternal = 2u;
constexpr uint32_t kMinimumMultiplier = 2u;
constexpr uint32_t kMaximumMultiplier = 6u;
constexpr uint8_t kStandardMaximumGeneratedFrames = 3u;
constexpr uint8_t kExperimentalMaximumGeneratedFrames = 5u;
constexpr uint64_t kNotInitializedRetryDelayMs = 500;
constexpr uint64_t kUiTagFreshnessMs = 2500;

struct ControlConfig
{
    uint32_t multiplier = 4;
    bool dynamic = false;
    uint32_t dynamicTargetFrameRate = 0;
    bool dynamicExperimental56 = false;
};

struct ControlSnapshot
{
    ControlConfig control{};
    uint64_t revision = 0;
};

struct LastGameOptions
{
    sl::ViewportHandle viewport{0u};
    sl::DLSSGOptions options{};
    bool valid = false;
};

struct ModuleRecord
{
    HMODULE module = nullptr;
    std::wstring path;
    bool wrapperExport = false;
    bool wrapperCandidate = false;
    bool wrapperPatched = false;
    uint8_t* wrapperMaximumImmediate = nullptr;
    bool ngxExport = false;
    bool ngxCandidate = false;
    bool ngxPatched = false;
    bool ngxTemporalPatched = false;
    bool inventoryLogged = false;
};

struct UiResourceTagState
{
    bool active = false;
    sl::ResourceLifecycle lifecycle = sl::ResourceLifecycle::eOnlyValidNow;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t top = 0;
    uint32_t left = 0;
    uint32_t format = 0;
    uint64_t lastSeenTick = 0;
};

struct UiViewportTagState
{
    uint32_t viewport = UINT32_MAX;
    UiResourceTagState hudless{};
    UiResourceTagState uiAlpha{};
    UiResourceTagState uiColorAlpha{};
};

struct UiInputSnapshot
{
    bool hudless = false;
    bool uiAlpha = false;
    bool uiColorAlpha = false;
    bool dimensionsKnown = false;
    bool dimensionsMatch = false;
    bool ready = false;
    uint32_t hudlessWidth = 0;
    uint32_t hudlessHeight = 0;
    uint32_t hudlessFormat = 0;
    uint32_t uiWidth = 0;
    uint32_t uiHeight = 0;
    uint32_t uiFormat = 0;
    uint64_t oldestAgeMs = 0;
};

LastGameOptions gLastGameOptions;
std::vector<ModuleRecord> gModuleRecords;
std::vector<UiViewportTagState> gUiViewportTags;
LARGE_INTEGER gFpsCounterFrequency{};
LARGE_INTEGER gFpsWindowStart{};
uint64_t gFpsWindowRealFrames = 0;
uint64_t gFpsWindowPresentedFrames = 0;

void ObserveActiveWrapperProvider(void* function);

void Log(const wchar_t* format, ...)
{
    wchar_t message[2048]{};
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(message, _countof(message), _TRUNCATE, format, args);
    va_end(args);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t timestamped[2560]{};
    swprintf_s(timestamped, L"[%04u-%02u-%02u %02u:%02u:%02u.%03u] [tid=%lu tick=%llu] %s",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        GetCurrentThreadId(), static_cast<unsigned long long>(GetTickCount64()), message);

    OutputDebugStringW(L"[MfgUnlock] ");
    OutputDebugStringW(timestamped);
    OutputDebugStringW(L"\n");
    if (gLog)
    {
        fwprintf_s(gLog, L"%s\n", timestamped);
        fflush(gLog);
    }
}

void MidpointLog(const wchar_t* message)
{
    Log(L"%s", message ? message : L"");
}

std::atomic<bool> gConfigForceOta{false};
std::atomic<bool> gConfigPatchFlipMetering{true};
std::atomic<bool> gConfigBlackwellTransfusion{true};
std::atomic<bool> gConfigQualityFix{true};
std::atomic<bool> gConfigDisableMenuDetection{true};
std::atomic<bool> gConfigDisableMvDilation{false};
std::atomic<bool> gConfigForceUiRecomposition{false};
std::atomic<bool> gIsEndfield{false};
std::atomic<bool> gConfigLogPerformance{false};

std::wstring gPerfCsvPath;
FILE* gPerfCsv = nullptr;
std::mutex gPerfMutex;

struct PerfStats
{
    uint64_t frameIndex = 0;
    LARGE_INTEGER lastFrameQpc{};
    LARGE_INTEGER benchmarkStartQpc{};
    LARGE_INTEGER lastLogQpc{};
    static constexpr size_t kWindowSize = 300;
    float windowDeltas[kWindowSize]{};
    size_t windowHead = 0;
    size_t windowCount = 0;
};
PerfStats gPerfStats;

void RecordPerfSample(uint32_t currentMultiplier)
{
    if (!gConfigLogPerformance.load(std::memory_order_relaxed))
    {
        if (gPerfCsv)
        {
            std::lock_guard lock(gPerfMutex);
            if (gPerfCsv)
            {
                fflush(gPerfCsv);
                fclose(gPerfCsv);
                gPerfCsv = nullptr;
            }
        }
        return;
    }

    LARGE_INTEGER now{};
    if (!QueryPerformanceCounter(&now))
        return;
    if (gFpsCounterFrequency.QuadPart == 0
        && !QueryPerformanceFrequency(&gFpsCounterFrequency))
        return;

    std::lock_guard lock(gPerfMutex);
    if (!gPerfCsv && !gPerfCsvPath.empty())
    {
        gPerfCsv = _wfsopen(gPerfCsvPath.c_str(), L"w", _SH_DENYNO);
        if (gPerfCsv)
        {
            fprintf(gPerfCsv, "FrameIndex,TimeMs,DeltaMs,Multiplier,KernelMode\n");
            fflush(gPerfCsv);
        }
    }

    if (gPerfStats.lastFrameQpc.QuadPart == 0)
    {
        gPerfStats.lastFrameQpc = now;
        gPerfStats.benchmarkStartQpc = now;
        gPerfStats.lastLogQpc = now;
        return;
    }

    const double qpcFreq = static_cast<double>(gFpsCounterFrequency.QuadPart);
    if (qpcFreq <= 0.0)
        return;

    const double deltaMs = (static_cast<double>(now.QuadPart - gPerfStats.lastFrameQpc.QuadPart) * 1000.0) / qpcFreq;
    const double elapsedTotalMs = (static_cast<double>(now.QuadPart - gPerfStats.benchmarkStartQpc.QuadPart) * 1000.0) / qpcFreq;
    gPerfStats.lastFrameQpc = now;

    // Filter out extreme pauses (loading screens, Alt-Tab > 1 sec)
    if (deltaMs <= 0.1 || deltaMs > 1000.0)
        return;

    ++gPerfStats.frameIndex;
    const float deltaF = static_cast<float>(deltaMs);
    gPerfStats.windowDeltas[gPerfStats.windowHead] = deltaF;
    gPerfStats.windowHead = (gPerfStats.windowHead + 1) % PerfStats::kWindowSize;
    if (gPerfStats.windowCount < PerfStats::kWindowSize)
        ++gPerfStats.windowCount;

    const bool isBlackwell = midpoint_fix::IsBlackwellTransfusionActive();
    const size_t bwFatbins = midpoint_fix::GetTransfusedFatbinCount();
    const char* kernelMode = isBlackwell ? (bwFatbins > 1 ? "Blackwell (Omni)" : "Blackwell") : "Ada";

    if (gPerfCsv)
    {
        fprintf(gPerfCsv, "%llu,%.2f,%.2f,%u,%s\n",
            static_cast<unsigned long long>(gPerfStats.frameIndex),
            elapsedTotalMs, deltaMs, currentMultiplier, kernelMode);
    }

    // Every 5 seconds: compute rolling mean, jitter (standard deviation), and 1% low
    const double sinceLastLog = (static_cast<double>(now.QuadPart - gPerfStats.lastLogQpc.QuadPart) * 1000.0) / qpcFreq;
    if (sinceLastLog >= 5000.0 && gPerfStats.windowCount >= 30)
    {
        gPerfStats.lastLogQpc = now;
        float sum = 0.0f;
        float sortedDeltas[PerfStats::kWindowSize]{};
        for (size_t i = 0; i < gPerfStats.windowCount; ++i)
        {
            sum += gPerfStats.windowDeltas[i];
            sortedDeltas[i] = gPerfStats.windowDeltas[i];
        }
        const float meanDelta = sum / static_cast<float>(gPerfStats.windowCount);
        const float meanFps = meanDelta > 0.0f ? (1000.0f / meanDelta) : 0.0f;

        float variance = 0.0f;
        for (size_t i = 0; i < gPerfStats.windowCount; ++i)
        {
            const float diff = gPerfStats.windowDeltas[i] - meanDelta;
            variance += diff * diff;
        }
        const float jitterMs = std::sqrt(variance / static_cast<float>(gPerfStats.windowCount));

        // 1% Low is the 99th percentile frametime
        std::sort(sortedDeltas, sortedDeltas + gPerfStats.windowCount);
        const size_t p99Index = static_cast<size_t>(static_cast<float>(gPerfStats.windowCount - 1) * 0.99f);
        const float p99Delta = sortedDeltas[p99Index];
        const float fps1PctLow = p99Delta > 0.0f ? (1000.0f / p99Delta) : 0.0f;

        Log(L"[PERF] %ux | Kernel: %hs | FPS: %.1f (%.2fms) | Jitter: %.2fms | 1%% Low: %.1f FPS (Sample: %zu frames)",
            currentMultiplier, kernelMode, meanFps, meanDelta, jitterMs, fps1PctLow, gPerfStats.windowCount);

        if (gPerfCsv)
            fflush(gPerfCsv);
    }
}

uint8_t RequestedMaximumGeneratedFrames(const ControlConfig& control)
{
    return control.dynamic && !control.dynamicExperimental56
        ? kStandardMaximumGeneratedFrames
        : kExperimentalMaximumGeneratedFrames;
}

bool SetWrapperMaximum(ModuleRecord& record, uint8_t maximum)
{
    uint8_t* address = record.wrapperMaximumImmediate;
    if (!address || (*address != kStandardMaximumGeneratedFrames
        && *address != kExperimentalMaximumGeneratedFrames))
        return false;
    if (*address == maximum)
        return true;

    DWORD oldProtection = 0;
    if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtection))
    {
        Log(L"Streamline maximum update failed (%lu): %s",
            GetLastError(), record.path.c_str());
        return false;
    }
    *address = maximum;
    FlushInstructionCache(GetCurrentProcess(), address, 1);
    DWORD ignoredProtection = 0;
    const BOOL restored = VirtualProtect(address, 1, oldProtection, &ignoredProtection);
    if (!restored)
    {
        Log(L"Streamline maximum protection restore failed (%lu): %s",
            GetLastError(), record.path.c_str());
        return false;
    }

    Log(L"Streamline maximum updated: generatedFrames=%u multiplier=%ux path=%s",
        maximum, static_cast<uint32_t>(maximum) + 1, record.path.c_str());
    return true;
}

void SynchronizeStreamlineLiveContext()
{
    auto* ppContext = gStreamlineDlssgContextPtr.load(std::memory_order_relaxed);
    if (!ppContext) return;
    __try
    {
        uint8_t* ctx = *ppContext;
        if (ctx)
        {
            *reinterpret_cast<uint32_t*>(ctx + 0x460c) = 5; // ensure max generated frames is 5 (6x)
            *reinterpret_cast<uint8_t*>(ctx + 0x4610) = 1;  // ensure dynamic MFG is supported
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void ApplyWrapperMaximum(const ControlConfig& /*control*/)
{
    const uint8_t maximum = kExperimentalMaximumGeneratedFrames;
    std::lock_guard lock(gModuleMutex);
    for (auto& record : gModuleRecords)
    {
        if (record.wrapperPatched && record.wrapperMaximumImmediate)
            SetWrapperMaximum(record, maximum);
    }
    SynchronizeStreamlineLiveContext();
}

UiResourceTagState CaptureUiResourceTag(const sl::ResourceTag& tag, uint64_t tick)
{
    UiResourceTagState state{};
    if (!tag.resource || !tag.resource->native)
        return state;

    state.active = true;
    state.lifecycle = tag.lifecycle;
    state.lastSeenTick = tick;
    state.top = tag.extent.top;
    state.left = tag.extent.left;
    state.width = tag.extent.width != 0 ? tag.extent.width : tag.resource->width;
    state.height = tag.extent.height != 0 ? tag.extent.height : tag.resource->height;
    state.format = tag.resource->nativeFormat;
    return state;
}

bool UiTagFresh(const UiResourceTagState& state, uint64_t now)
{
    return state.active && state.lastSeenTick != 0 && now >= state.lastSeenTick
        && now - state.lastSeenTick <= kUiTagFreshnessMs;
}

UiInputSnapshot ReadUiInputSnapshot(uint32_t viewport)
{
    UiInputSnapshot snapshot{};
    const uint64_t now = GetTickCount64();
    std::lock_guard lock(gUiTagMutex);
    const auto found = std::find_if(gUiViewportTags.begin(), gUiViewportTags.end(),
        [&](const UiViewportTagState& state) { return state.viewport == viewport; });
    if (found == gUiViewportTags.end())
        return snapshot;

    snapshot.hudless = UiTagFresh(found->hudless, now);
    snapshot.uiAlpha = UiTagFresh(found->uiAlpha, now);
    snapshot.uiColorAlpha = UiTagFresh(found->uiColorAlpha, now);
    const UiResourceTagState* ui = snapshot.uiAlpha ? &found->uiAlpha
        : snapshot.uiColorAlpha ? &found->uiColorAlpha : nullptr;
    if (!snapshot.hudless)
        return snapshot;

    snapshot.hudlessWidth = found->hudless.width;
    snapshot.hudlessHeight = found->hudless.height;
    snapshot.hudlessFormat = found->hudless.format;
    const uint64_t hudlessAge = now - found->hudless.lastSeenTick;

    if (ui)
    {
        // Dual-texture UIR (e.g. Cyberpunk 2077): game supplies separate UI alpha/color buffer
        snapshot.uiWidth = ui->width;
        snapshot.uiHeight = ui->height;
        snapshot.uiFormat = ui->format;
        snapshot.dimensionsKnown = snapshot.hudlessWidth != 0
            && snapshot.hudlessHeight != 0 && snapshot.uiWidth != 0
            && snapshot.uiHeight != 0;
        snapshot.dimensionsMatch = snapshot.dimensionsKnown
            && found->hudless.top == ui->top && found->hudless.left == ui->left
            && snapshot.hudlessWidth == snapshot.uiWidth
            && snapshot.hudlessHeight == snapshot.uiHeight;

        const uint32_t colorWidth = gGameColorWidth.load(std::memory_order_relaxed);
        const uint32_t colorHeight = gGameColorHeight.load(std::memory_order_relaxed);
        if (snapshot.dimensionsMatch && colorWidth != 0 && colorHeight != 0)
        {
            snapshot.dimensionsMatch = snapshot.hudlessWidth == colorWidth
                && snapshot.hudlessHeight == colorHeight;
        }

        const uint64_t uiAge = now - ui->lastSeenTick;
        snapshot.oldestAgeMs = std::max(hudlessAge, uiAge);
        snapshot.ready = snapshot.dimensionsMatch && snapshot.hudlessFormat != 0 && snapshot.uiFormat != 0;
    }
    else
    {
        // Single-texture difference UIR (e.g. Arknights: Endfield, Alan Wake 2):
        // Game supplies HUDLessColor and renders UI directly into the final backbuffer.
        snapshot.dimensionsKnown = snapshot.hudlessWidth != 0 && snapshot.hudlessHeight != 0;
        const uint32_t colorWidth = gGameColorWidth.load(std::memory_order_relaxed);
        const uint32_t colorHeight = gGameColorHeight.load(std::memory_order_relaxed);
        if (colorWidth != 0 && colorHeight != 0)
        {
            snapshot.dimensionsMatch = snapshot.dimensionsKnown
                && snapshot.hudlessWidth == colorWidth
                && snapshot.hudlessHeight == colorHeight;
        }
        else
        {
            snapshot.dimensionsMatch = snapshot.dimensionsKnown;
        }
        snapshot.oldestAgeMs = hudlessAge;
        snapshot.ready = snapshot.dimensionsMatch && snapshot.hudlessFormat != 0;
    }
    return snapshot;
}

void RefreshUiInputReadiness(uint32_t viewport)
{
    if (viewport == UINT32_MAX)
        return;
    const UiInputSnapshot snapshot = ReadUiInputSnapshot(viewport);
    const bool previous = gUiInputsReady.exchange(snapshot.ready, std::memory_order_acq_rel);
    if (previous == snapshot.ready)
        return;

    if (gControlReady.load(std::memory_order_acquire))
        gDesiredRevision.fetch_add(1, std::memory_order_release);
    Log(L"UI inputs changed: ready=%d viewport=%u hudless=%d uiAlpha=%d "
        L"uiColorAlpha=%d dimensionsKnown=%d dimensionsMatch=%d "
        L"hudless=%ux%u ui=%ux%u",
        snapshot.ready, viewport, snapshot.hudless, snapshot.uiAlpha,
        snapshot.uiColorAlpha, snapshot.dimensionsKnown, snapshot.dimensionsMatch,
        snapshot.hudlessWidth, snapshot.hudlessHeight,
        snapshot.uiWidth, snapshot.uiHeight);
}

// Submitted resource metadata only: no native-resource calls, GPU reads or waits.
// Zero resource dimensions are "not supplied", not a zero-sized GPU texture.
void LogMotionResourceTags(const sl::ViewportHandle& viewport,
    const sl::ResourceTag* tags, uint32_t numTags, bool frameKnown, uint32_t frame)
{
    if constexpr (scatter_experiment::kMode != 0) return;
    if (!tags || numTags == 0 || numTags > 1024) return;
    struct Record
    {
        bool used = false;
        uint32_t viewport = 0;
        sl::BufferType type = 0;
        std::array<uint32_t, 9> shape{};
        uint64_t lastLog = 0;
    };
    static std::mutex mutex;
    static std::array<Record, 32> records{};
    const auto view = static_cast<uint32_t>(viewport);
    const auto tick = GetTickCount64();
    for (uint32_t i = 0; i < numTags; ++i)
    {
        const auto& tag = tags[i];
        const wchar_t* name = nullptr;
        if (tag.type == sl::kBufferTypeMotionVectors) name = L"motion";
        else if (tag.type == sl::kBufferTypeDepth) name = L"depth";
        else if (tag.type == sl::kBufferTypeHUDLessColor) name = L"hudless";
        else if (tag.type == sl::kBufferTypeScalingInputColor) name = L"scaling-input";
        else if (tag.type == sl::kBufferTypeScalingOutputColor) name = L"scaling-output";
        else if (tag.type == sl::kBufferTypeUIColorAndAlpha) name = L"ui-color-alpha";
        if (!name) continue;
        const auto* resource = tag.resource;
        const std::array<uint32_t, 9> shape = {
            resource && resource->native ? 1u : 0u,
            tag.extent.left, tag.extent.top, tag.extent.width, tag.extent.height,
            resource ? resource->width : 0u, resource ? resource->height : 0u,
            resource ? resource->nativeFormat : 0u, static_cast<uint32_t>(tag.lifecycle)
        };
        std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock()) continue;
        auto found = std::find_if(records.begin(), records.end(), [&](const auto& record) {
            return record.used && record.viewport == view && record.type == tag.type;
        });
        if (found == records.end())
            found = std::find_if(records.begin(), records.end(), [](const auto& record) { return !record.used; });
        if (found == records.end()) continue;
        const uint64_t age = tick - found->lastLog;
        if (found->used && (age < 1000 || (found->shape == shape && age < 10000))) continue;
        *found = {true, view, tag.type, shape, tick};
        lock.unlock();
        Log(L"[MOTION-TAG] viewport=%u frameKnown=%d frame=%u type=%s active=%u "
            L"rect=(%u,%u %ux%u) suppliedTexture=%ux%u suppliedFormat=%u lifecycle=%u",
            view, frameKnown, frame, name, shape[0], shape[1], shape[2], shape[3],
            shape[4], shape[5], shape[6], shape[7], shape[8]);
    }
}

void CaptureUiResourceTags(const sl::ViewportHandle& viewport,
    const sl::ResourceTag* tags, uint32_t numTags)
{
    if (!tags || numTags == 0 || numTags > 1024)
        return;

    bool hasUiTag = false;
    for (uint32_t i = 0; i < numTags; ++i)
    {
        if (tags[i].type == sl::kBufferTypeHUDLessColor
            || tags[i].type == sl::kBufferTypeUIAlpha
            || tags[i].type == sl::kBufferTypeUIColorAndAlpha)
        {
            hasUiTag = true;
            break;
        }
    }
    if (!hasUiTag)
        return;

    const uint32_t viewportValue = static_cast<uint32_t>(viewport);
    const uint64_t tick = GetTickCount64();
    bool relevant = false;
    {
        std::lock_guard lock(gUiTagMutex);
        auto found = std::find_if(gUiViewportTags.begin(), gUiViewportTags.end(),
            [&](const UiViewportTagState& state) { return state.viewport == viewportValue; });
        if (found == gUiViewportTags.end())
        {
            gUiViewportTags.push_back({});
            found = std::prev(gUiViewportTags.end());
            found->viewport = viewportValue;
        }

        for (uint32_t index = 0; index < numTags; ++index)
        {
            const sl::ResourceTag& tag = tags[index];
            UiResourceTagState* destination = nullptr;
            if (tag.type == sl::kBufferTypeHUDLessColor)
                destination = &found->hudless;
            else if (tag.type == sl::kBufferTypeUIAlpha)
                destination = &found->uiAlpha;
            else if (tag.type == sl::kBufferTypeUIColorAndAlpha)
                destination = &found->uiColorAlpha;
            if (!destination)
                continue;
            *destination = CaptureUiResourceTag(tag, tick);
            relevant = true;
        }
    }

    const uint32_t activeViewport = gLastOptionsViewport.load(std::memory_order_acquire);
    if (relevant && (activeViewport == UINT32_MAX || activeViewport == viewportValue))
        RefreshUiInputReadiness(viewportValue);
}

void UpdateFpsTelemetry(uint32_t presentedFrames)
{
    LARGE_INTEGER now{};
    if (!QueryPerformanceCounter(&now))
        return;
    if (gFpsCounterFrequency.QuadPart == 0
        && !QueryPerformanceFrequency(&gFpsCounterFrequency))
        return;
    if (gFpsWindowStart.QuadPart == 0 || now.QuadPart <= gFpsWindowStart.QuadPart)
    {
        gFpsWindowStart = now;
        gFpsWindowRealFrames = 0;
        gFpsWindowPresentedFrames = 0;
        return;
    }

    ++gFpsWindowRealFrames;
    gFpsWindowPresentedFrames += presentedFrames;
    const uint64_t elapsedTicks = static_cast<uint64_t>(
        now.QuadPart - gFpsWindowStart.QuadPart);
    const uint64_t minimumTicks = static_cast<uint64_t>(
        gFpsCounterFrequency.QuadPart) / 2;
    if (elapsedTicks < minimumTicks)
        return;

    const uint64_t frequency = static_cast<uint64_t>(gFpsCounterFrequency.QuadPart);
    const auto rateMilli = [&](uint64_t frames) {
        return static_cast<uint32_t>(std::min<uint64_t>(UINT32_MAX,
            (frames * frequency * 1000u + elapsedTicks / 2u) / elapsedTicks));
    };
    gRealFpsMilli.store(rateMilli(gFpsWindowRealFrames), std::memory_order_relaxed);
    gDlssFpsMilli.store(
        rateMilli(gFpsWindowPresentedFrames), std::memory_order_relaxed);
    gFpsSampleWindowMs.store(static_cast<uint32_t>(
        std::min<uint64_t>(UINT32_MAX,
            (elapsedTicks * 1000u + frequency / 2u) / frequency)),
        std::memory_order_relaxed);
    gFpsSampleTick.store(GetTickCount64(), std::memory_order_release);
    gFpsWindowStart = now;
    gFpsWindowRealFrames = 0;
    gFpsWindowPresentedFrames = 0;
}

void RecordDlssgStateResult(
    sl::Result result, const sl::DLSSGState& state, bool fpsFrameSample)
{
    gGetStateCalls.fetch_add(1, std::memory_order_relaxed);
    gGetStateSeen.store(true, std::memory_order_release);
    gLastGetStateResult.store(static_cast<int32_t>(result), std::memory_order_relaxed);
    if (result != sl::Result::eOk)
    {
        if (fpsFrameSample)
            UpdateFpsTelemetry(0);
        return;
    }

    const uint32_t currentMultiplier = state.numFramesActuallyPresented;
    const uint32_t currentStatus = static_cast<uint32_t>(state.status);
    const uint32_t previousStatus = gDlssgStatus.exchange(currentStatus, std::memory_order_relaxed);

    // Streamline / OptiScaler queries can transiently report 1 presented frame during
    // intermediate queries or non-present polls while Frame Generation is actively enabled.
    const bool fgOn = gGameFrameGenerationOn.load(std::memory_order_relaxed);
    const bool validMultiplierUpdate = (currentMultiplier >= 2) || !fgOn;

    uint32_t previousMultiplier = gActualFramesPresented.load(std::memory_order_relaxed);
    if (validMultiplierUpdate)
    {
        previousMultiplier = gActualFramesPresented.exchange(currentMultiplier, std::memory_order_relaxed);
    }

    if (fpsFrameSample)
    {
        UpdateFpsTelemetry(currentMultiplier);
        RecordPerfSample(validMultiplierUpdate ? currentMultiplier : previousMultiplier);
    }
    if (state.structVersion >= sl::kStructVersion2)
        gNumFramesToGenerateMax.store(
            state.numFramesToGenerateMax, std::memory_order_relaxed);
    if (state.structVersion >= sl::kStructVersion4)
        gDynamicMfgSupported.store(
            state.bIsDynamicMFGSupported == sl::Boolean::eTrue,
            std::memory_order_relaxed);
    gStateSampleTick.store(GetTickCount64(), std::memory_order_release);

    if ((validMultiplierUpdate && previousMultiplier != currentMultiplier) || (previousStatus != currentStatus))
        Log(L"DLSS-G actual presentation count: %ux (maximum generated frames=%u, status=%u)",
            validMultiplierUpdate ? currentMultiplier : previousMultiplier,
            gNumFramesToGenerateMax.load(std::memory_order_relaxed),
            currentStatus);
}

std::wstring ParentPath(const std::wstring& path)
{
    const auto separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator);
}

std::wstring JoinPath(const std::wstring& left, const std::wstring& right)
{
    if (left.empty())
        return right;
    if (left.back() == L'\\' || left.back() == L'/')
        return left + right;
    return left + L"\\" + right;
}

void InitLogging(HINSTANCE instance, const std::wstring& exeDir)
{
    if (gLog) return;
    wchar_t modPath[MAX_PATH]{};
    GetModuleFileNameW(instance, modPath, MAX_PATH);
    const std::wstring modDir = ParentPath(modPath);

    std::wstring logPath = JoinPath(modDir, L"DLSSG-Transfusion.log");
    gLog = _wfsopen(logPath.c_str(), L"w, ccs=UTF-8", _SH_DENYWR);
    if (!gLog)
    {
        logPath = JoinPath(exeDir, L"DLSSG-Transfusion.log");
        gLog = _wfsopen(logPath.c_str(), L"w, ccs=UTF-8", _SH_DENYWR);
    }
    if (!gLog)
    {
        wchar_t tempDir[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tempDir);
        logPath = JoinPath(tempDir, L"DLSSG-Transfusion.log");
        gLog = _wfsopen(logPath.c_str(), L"w, ccs=UTF-8", _SH_DENYWR);
    }
    gLogReady.store(gLog != nullptr, std::memory_order_release);

    Log(L"============================================================");
    Log(L"DLSSG-Transfusion (Universal Blackwell Transfusion & Multi-Game Edition)");
    Log(L"Build: %hs (%hs %hs)", scatter_experiment::kName, __DATE__, __TIME__);
    const std::wstring exceptionLogPath = JoinPath(ParentPath(logPath), L"DLSSG-Transfusion.crash.log");
    const bool exceptionObserver = crash_diagnostics::Initialize(exceptionLogPath.c_str());
    Log(L"First-chance exception observer: enabled=%d path=%s", exceptionObserver, exceptionLogPath.c_str());
    Log(L"Loaded as: %s", proxy::GetCurrentTypeName());
    if (proxy::GetCurrentType() != proxy::ProxyType::None)
        Log(L"Proxied system DLL: %s", proxy::GetOriginalLibraryPath());
    Log(L"Module Path: %s", modPath);
    Log(L"Game Directory: %s (PID: %lu)", exeDir.c_str(), GetCurrentProcessId());

    wchar_t exeFile[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exeFile, MAX_PATH);
    std::wstring exeFileName = exeFile;
    const auto lastSlash = exeFileName.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        exeFileName = exeFileName.substr(lastSlash + 1);

    std::string key;
    for (wchar_t wc : exeFileName)
    {
        if ((wc >= L'a' && wc <= L'z') || (wc >= L'0' && wc <= L'9'))
            key.push_back(static_cast<char>(wc));
        else if (wc >= L'A' && wc <= L'Z')
            key.push_back(static_cast<char>(wc - L'A' + 'a'));
    }
    const size_t exePos = key.rfind("exe");
    if (exePos != std::string::npos && exePos + 3 == key.size())
        key.erase(exePos);

    if (key.find("endfield") != std::string::npos)
    {
        gIsEndfield.store(true, std::memory_order_relaxed);
        Log(L"Game Identity Match: Arknights: Endfield (UIR pipeline forced from frame 0)");
    }

    Tier tier = LookupManifestTier(key);
    if (tier == Tier::eUnknown)
    {
        for (const auto& entry : kManifest)
        {
            if (key.find(entry.key) != std::string::npos || entry.key.find(key) != std::string::npos)
            {
                tier = entry.tier;
                break;
            }
        }
    }

    const char* tierStr = (tier == Tier::eSixX) ? "6x (Official NVIDIA Tier)" :
                          (tier == Tier::eFourX) ? "4x (NVIDIA Recommended Ceiling)" :
                          "Universal 6x (Unlisted Title)";
    Log(L"Game Identity: \"%ls\" -> %hs", exeFileName.c_str(), tierStr);
    Log(L"============================================================");
}

uint32_t ClassifyLoadedRoute(const std::wstring& path)
{
    if (!gExecutableDirectory.empty()
        && _wcsicmp(ParentPath(path).c_str(), gExecutableDirectory.c_str()) == 0)
        return kRouteLocal;
    return kRouteExternal;
}

bool ContainsCI(const wchar_t* str, const wchar_t* sub) noexcept;

bool IsHarnessEnvironment() noexcept
{
    static int sIsHarness = -1;
    if (sIsHarness != -1)
        return sIsHarness == 1;

    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (ContainsCI(exePath, L"AsiLiveControlHarness")
        || GetEnvironmentVariableW(L"MFG_HARNESS_WRAPPER_PATH", nullptr, 0) > 0)
    {
        sIsHarness = 1;
        return true;
    }
    sIsHarness = 0;
    return false;
}

bool BridgeReady()
{
    const bool hookOk = gLiveHookInstalled.load(std::memory_order_acquire)
        || gInterposerDetoursInstalled.load(std::memory_order_acquire);
    const bool midpointOk = midpoint_fix::Ready() || IsHarnessEnvironment();
    const bool wrapperOk = gActiveWrapperObserved.load(std::memory_order_relaxed)
        || (gPatchedWrapperCandidates.load(std::memory_order_relaxed) > 0);
    return hookOk
        && gSetOptionsHookExposed.load(std::memory_order_relaxed)
        && wrapperOk
        && gPatchedNgxCandidates.load(std::memory_order_relaxed) > 0
        && midpointOk;
}

const char* PatchRouteName()
{
    if (!BridgeReady())
        return "pending";

    const uint32_t wrapperBits = gWrapperRouteBits.load(std::memory_order_acquire);
    const uint32_t ngxBits = gNgxRouteBits.load(std::memory_order_acquire);
    const uint32_t common = wrapperBits & ngxBits;
    if ((common & (kRouteLocal | kRouteExternal)) == (kRouteLocal | kRouteExternal))
        return "both";
    if ((common & kRouteLocal) != 0)
        return "local";
    if ((common & kRouteExternal) != 0)
        return "external";
    return "mixed";
}

bool IsRegularFile(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IsDirectory(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FindJsonValue(const std::string& content, const char* name, size_t& value)
{
    const std::string key = std::string("\"") + name + "\"";
    const auto keyOffset = content.find(key);
    if (keyOffset == std::string::npos)
        return false;
    const auto colon = content.find(':', keyOffset + key.size());
    if (colon == std::string::npos)
        return false;
    value = content.find_first_not_of(" \t\r\n", colon + 1);
    return value != std::string::npos;
}

bool TryParseUnsigned(const std::string& content, const char* name,
    uint32_t minimum, uint32_t maximum, uint32_t& value)
{
    size_t offset = 0;
    if (!FindJsonValue(content, name, offset) || content[offset] < '0' || content[offset] > '9')
        return false;

    uint64_t parsed = 0;
    size_t end = offset;
    while (end < content.size() && content[end] >= '0' && content[end] <= '9')
    {
        parsed = parsed * 10 + static_cast<uint32_t>(content[end] - '0');
        if (parsed > maximum)
            return false;
        ++end;
    }
    if (parsed < minimum || parsed > maximum)
        return false;
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool TryParseBoolean(const std::string& content, const char* name, bool& value)
{
    size_t offset = 0;
    if (!FindJsonValue(content, name, offset))
        return false;
    if (content.compare(offset, 4, "true") == 0)
    {
        value = true;
        return true;
    }
    if (content.compare(offset, 5, "false") == 0)
    {
        value = false;
        return true;
    }
    return false;
}

bool WriteControlFile(const std::wstring& path, const ControlConfig& control)
{
    char json[1024]{};
    const int len = sprintf_s(json,
        "{\n"
        "  \"multiplier\": %u,\n"
        "  \"mode\": \"%s\",\n"
        "  \"dynamicTargetFrameRate\": %u,\n"
        "  \"dynamicExperimental56\": %s,\n"
        "  \"forceOTA\": %s,\n"
        "  \"patchFlipMetering\": %s,\n"
        "  \"blackwellTransfusion\": %s,\n"
        "  \"qualityValidWarp\": %s,\n"
        "  \"disableMenuDetection\": %s,\n"
        "  \"disableMvDilation\": %s,\n"
        "  \"forceUiRecomposition\": %s,\n"
        "  \"logPerformance\": %s\n"
        "}\n",
        control.multiplier,
        control.dynamic ? "dynamic" : "fixed",
        control.dynamicTargetFrameRate,
        control.dynamicExperimental56 ? "true" : "false",
        gConfigForceOta.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigPatchFlipMetering.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigBlackwellTransfusion.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigQualityFix.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigDisableMenuDetection.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigDisableMvDilation.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigForceUiRecomposition.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigLogPerformance.load(std::memory_order_relaxed) ? "true" : "false");
    if (len <= 0) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        Log(L"[CONFIG] Failed to write config to %s (Win32 Error: %lu)", path.c_str(), GetLastError());
        return false;
    }
    DWORD written = 0;
    const BOOL res = WriteFile(file, json, static_cast<DWORD>(len), &written, nullptr);
    CloseHandle(file);
    if (res && written == static_cast<DWORD>(len))
    {
        Log(L"[CONFIG] Saved config to %s (multiplier=%ux mode=%s)",
            path.c_str(), control.multiplier, control.dynamic ? L"dynamic" : L"fixed");
        return true;
    }
    Log(L"[CONFIG] Incomplete write to %s (%lu / %d bytes written)", path.c_str(), written, len);
    return false;
}

bool TryParseControl(const char* data, size_t size, ControlConfig& control,
                     std::vector<std::string>* missingKeys = nullptr)
{
    if (!data || size == 0)
        return false;

    const std::string content(data, size);
    ControlConfig parsed{};
    parsed.multiplier = 4;
    parsed.dynamic = false;
    parsed.dynamicTargetFrameRate = 0;
    parsed.dynamicExperimental56 = false;

    const bool hasMultiplier = TryParseUnsigned(content, "multiplier",
        kMinimumMultiplier, kMaximumMultiplier, parsed.multiplier);
    if (!hasMultiplier && missingKeys) missingKeys->push_back("multiplier");

    size_t modeOffset = 0;
    const bool hasMode = FindJsonValue(content, "mode", modeOffset);
    if (hasMode)
    {
        if (content.compare(modeOffset, 9, "\"dynamic\"") == 0
            || _strnicmp(&content[modeOffset], "\"dynamic\"", 9) == 0)
        {
            parsed.dynamic = true;
        }
        else if (content.compare(modeOffset, 7, "\"fixed\"") == 0
            || _strnicmp(&content[modeOffset], "\"fixed\"", 7) == 0)
        {
            parsed.dynamic = false;
        }
        else
            return false;
    }
    else if (missingKeys)
    {
        missingKeys->push_back("mode");
    }

    size_t targetOffset = 0;
    const bool hasTarget = (FindJsonValue(content, "dynamicTargetFrameRate", targetOffset)
        && TryParseUnsigned(content, "dynamicTargetFrameRate", 0, 1000, parsed.dynamicTargetFrameRate))
        || (FindJsonValue(content, "dynamicTargetFps", targetOffset)
        && TryParseUnsigned(content, "dynamicTargetFps", 0, 1000, parsed.dynamicTargetFrameRate));
    if (!hasTarget && missingKeys) missingKeys->push_back("dynamicTargetFrameRate");

    if (!hasMode && hasTarget && parsed.dynamicTargetFrameRate > 0)
    {
        parsed.dynamic = true;
    }
    if (parsed.dynamic && parsed.dynamicTargetFrameRate == 0)
    {
        parsed.dynamicTargetFrameRate = 120;
    }

    size_t experimentalOffset = 0;
    const bool hasExperimental = FindJsonValue(content, "dynamicExperimental56", experimentalOffset)
        && TryParseBoolean(content, "dynamicExperimental56", parsed.dynamicExperimental56);
    if (!hasExperimental && missingKeys) missingKeys->push_back("dynamicExperimental56");

    size_t otaOffset = 0;
    bool forceOta = gConfigForceOta.load(std::memory_order_relaxed);
    const bool hasOta = (FindJsonValue(content, "forceOTA", otaOffset) && TryParseBoolean(content, "forceOTA", forceOta))
        || (FindJsonValue(content, "forceOta", otaOffset) && TryParseBoolean(content, "forceOta", forceOta));
    if (hasOta) gConfigForceOta.store(forceOta, std::memory_order_relaxed);
    else if (missingKeys) missingKeys->push_back("forceOTA");

    size_t flipOffset = 0;
    bool patchFlip = gConfigPatchFlipMetering.load(std::memory_order_relaxed);
    const bool hasFlip = (FindJsonValue(content, "patchFlipMetering", flipOffset) && TryParseBoolean(content, "patchFlipMetering", patchFlip))
        || (FindJsonValue(content, "patchFlip", flipOffset) && TryParseBoolean(content, "patchFlip", patchFlip));
    if (hasFlip) gConfigPatchFlipMetering.store(patchFlip, std::memory_order_relaxed);
    else if (missingKeys) missingKeys->push_back("patchFlipMetering");

    size_t transfusionOffset = 0;
    bool blackwellTransfusion = gConfigBlackwellTransfusion.load(std::memory_order_relaxed);
    const bool hasTransfusion = FindJsonValue(content, "blackwellTransfusion", transfusionOffset)
        && TryParseBoolean(content, "blackwellTransfusion", blackwellTransfusion);
    if (hasTransfusion)
    {
        gConfigBlackwellTransfusion.store(blackwellTransfusion, std::memory_order_relaxed);
        midpoint_fix::SetBlackwellTransfusionEnabled(blackwellTransfusion);
    }
    else if (missingKeys) missingKeys->push_back("blackwellTransfusion");

    size_t qualityFixOffset = 0;
    bool qualityFix = gConfigQualityFix.load(std::memory_order_relaxed);
    const bool hasQualityFix = (FindJsonValue(content, "qualityValidWarp", qualityFixOffset) && TryParseBoolean(content, "qualityValidWarp", qualityFix))
        || (FindJsonValue(content, "qualityFix", qualityFixOffset) && TryParseBoolean(content, "qualityFix", qualityFix));
    if (hasQualityFix)
    {
        gConfigQualityFix.store(qualityFix, std::memory_order_relaxed);
        midpoint_fix::SetQualityFixEnabled(qualityFix);
    }
    else if (missingKeys) missingKeys->push_back("qualityValidWarp");

    size_t menuDetectionOffset = 0;
    bool disableMenuDetection = gConfigDisableMenuDetection.load(std::memory_order_relaxed);
    const bool hasMenuDetection = FindJsonValue(content, "disableMenuDetection", menuDetectionOffset)
        && TryParseBoolean(content, "disableMenuDetection", disableMenuDetection);
    if (hasMenuDetection) gConfigDisableMenuDetection.store(disableMenuDetection, std::memory_order_relaxed);
    else if (missingKeys) missingKeys->push_back("disableMenuDetection");

    size_t mvDilationOffset = 0;
    bool disableMvDilation = gConfigDisableMvDilation.load(std::memory_order_relaxed);
    const bool hasMvDilation = FindJsonValue(content, "disableMvDilation", mvDilationOffset)
        && TryParseBoolean(content, "disableMvDilation", disableMvDilation);
    if (hasMvDilation)
    {
        gConfigDisableMvDilation.store(disableMvDilation, std::memory_order_relaxed);
        midpoint_fix::SetMvDilationDisabled(disableMvDilation);
    }
    else if (missingKeys) missingKeys->push_back("disableMvDilation");

    size_t forceUirOffset = 0;
    bool forceUiRecomposition = gConfigForceUiRecomposition.load(std::memory_order_relaxed);
    const bool hasForceUir = FindJsonValue(content, "forceUiRecomposition", forceUirOffset)
        && TryParseBoolean(content, "forceUiRecomposition", forceUiRecomposition);
    if (hasForceUir) gConfigForceUiRecomposition.store(forceUiRecomposition, std::memory_order_relaxed);
    else if (missingKeys) missingKeys->push_back("forceUiRecomposition");

    size_t perfOffset = 0;
    bool logPerf = gConfigLogPerformance.load(std::memory_order_relaxed);
    const bool hasPerf = FindJsonValue(content, "logPerformance", perfOffset)
        && TryParseBoolean(content, "logPerformance", logPerf);
    if (hasPerf) gConfigLogPerformance.store(logPerf, std::memory_order_relaxed);
    else if (missingKeys) missingKeys->push_back("logPerformance");

    control = parsed;
    return true;
}

bool ReadControlFile(const std::wstring& path, ControlConfig& control)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    std::array<char, 8192> buffer{};
    DWORD bytesRead = 0;
    const BOOL read = ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead, nullptr);
    CloseHandle(file);
    if (!read || bytesRead == 0)
        return false;
    buffer[bytesRead] = '\0';

    std::vector<std::string> missingKeys;
    const bool parsed = TryParseControl(buffer.data(), bytesRead, control, &missingKeys);
    if (parsed && !missingKeys.empty())
    {
        std::string missingList;
        for (size_t i = 0; i < missingKeys.size(); ++i)
        {
            if (i > 0) missingList += ", ";
            missingList += missingKeys[i];
        }
        Log(L"[CONFIG] Config %s was missing %u setting(s): [%hs]; updating file with complete schema",
            path.c_str(), static_cast<uint32_t>(missingKeys.size()), missingList.c_str());
        WriteControlFile(path, control);
    }
    return parsed;
}

bool ReadLastWriteTime(const std::wstring& path, FILETIME& writeTime)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
        return false;
    writeTime = attributes.ftLastWriteTime;
    return true;
}

ControlConfig ReadInitialControl()
{
    ControlConfig control{};
    wchar_t value[16]{};
    const DWORD length = GetEnvironmentVariableW(
        L"RTX40_MFG_ACTIVE_MULTIPLIER", value, _countof(value));
    if (length == 1 && value[0] >= L'2' && value[0] <= L'6')
        control.multiplier = static_cast<uint32_t>(value[0] - L'0');

    ControlConfig fileControl{};
    return ReadControlFile(gConfigPath, fileControl) ? fileControl : control;
}

std::wstring ResolveConfigPath(HMODULE instance, const std::wstring& executableDirectory)
{
    wchar_t explicitPath[32768]{};
    const DWORD explicitLength = GetEnvironmentVariableW(
        L"RTX40_MFG_CONFIG_PATH", explicitPath, _countof(explicitPath));
    if (explicitLength > 0 && explicitLength < _countof(explicitPath))
        return std::wstring(explicitPath, explicitLength);

    wchar_t modulePath[32768]{};
    GetModuleFileNameW(instance, modulePath, _countof(modulePath));
    const std::wstring moduleDir = ParentPath(modulePath);

    constexpr const wchar_t* kConfigFilename = L"DLSSG-Transfusion.json";

    // 1. Next to proxy/ASI module
    const std::wstring moduleConfig = JoinPath(moduleDir, kConfigFilename);
    if (IsRegularFile(moduleConfig))
        return moduleConfig;

    // 2. Next to executable
    const std::wstring exeConfig = JoinPath(executableDirectory, kConfigFilename);
    if (IsRegularFile(exeConfig))
        return exeConfig;

    // 3. Cyberpunk CET mod path if it exists
    const std::wstring cetDir = JoinPath(executableDirectory,
        L"plugins\\cyber_engine_tweaks\\mods\\RTX40MFG");
    if (IsDirectory(cetDir))
    {
        const std::wstring cetConfig = JoinPath(cetDir, kConfigFilename);
        if (IsRegularFile(cetConfig))
            return cetConfig;
        return cetConfig;
    }

    // 4. Default: DLSSG-Transfusion.json next to module or executable
    return moduleDir.empty() ? exeConfig : moduleConfig;
}

uint64_t StoreControl(const ControlConfig& control)
{
    ApplyWrapperMaximum(control);
    gDesiredMultiplier.store(control.multiplier, std::memory_order_relaxed);
    gDesiredDynamicMode.store(control.dynamic, std::memory_order_relaxed);
    gDynamicTargetFrameRate.store(control.dynamicTargetFrameRate, std::memory_order_relaxed);
    gDynamicExperimental56.store(control.dynamicExperimental56, std::memory_order_relaxed);
    const uint64_t revision = gDesiredRevision.fetch_add(1, std::memory_order_release) + 1;
    gControlReady.store(true, std::memory_order_release);
    return revision;
}

ControlSnapshot ReadControlSnapshot()
{
    ControlSnapshot snapshot{};
    for (;;)
    {
        const uint64_t before = gDesiredRevision.load(std::memory_order_acquire);
        snapshot.control.multiplier = gDesiredMultiplier.load(std::memory_order_relaxed);
        snapshot.control.dynamic = gDesiredDynamicMode.load(std::memory_order_relaxed);
        snapshot.control.dynamicTargetFrameRate =
            gDynamicTargetFrameRate.load(std::memory_order_relaxed);
        snapshot.control.dynamicExperimental56 =
            gDynamicExperimental56.load(std::memory_order_relaxed);
        const uint64_t after = gDesiredRevision.load(std::memory_order_acquire);
        if (before == after)
        {
            snapshot.revision = after;
            return snapshot;
        }
    }
}

void PublishLiveBridge(const ControlConfig& control)
{
    wchar_t multiplier[2]{ static_cast<wchar_t>(L'0' + std::clamp(
        control.multiplier, kMinimumMultiplier, kMaximumMultiplier)), L'\0' };
    wchar_t target[16]{};
    swprintf_s(target, L"%u", control.dynamicTargetFrameRate);
    SetEnvironmentVariableW(L"RTX40_MFG_ACTIVE_MULTIPLIER", multiplier);
    SetEnvironmentVariableW(L"RTX40_MFG_ACTIVE_MODE", control.dynamic ? L"dynamic" : L"fixed");
    SetEnvironmentVariableW(L"RTX40_MFG_DYNAMIC_TARGET", target);
    SetEnvironmentVariableW(L"RTX40_MFG_DYNAMIC_EXPERIMENTAL_56",
        control.dynamicExperimental56 ? L"1" : L"0");
    SetEnvironmentVariableW(L"RTX40_MFG_AUTO_BRIDGE", L"1");
}

void PublishPatchRoute()
{
    const char* route = PatchRouteName();
    wchar_t wideRoute[16]{};
    MultiByteToWideChar(CP_UTF8, 0, route, -1, wideRoute, _countof(wideRoute));
    SetEnvironmentVariableW(L"RTX40_MFG_PATCH_ROUTE", wideRoute);
}

uint64_t UnixTimeSeconds()
{
    FILETIME time{};
    GetSystemTimeAsFileTime(&time);
    ULARGE_INTEGER ticks{};
    ticks.LowPart = time.dwLowDateTime;
    ticks.HighPart = time.dwHighDateTime;
    constexpr uint64_t kWindowsToUnixEpoch = 116444736000000000ULL;
    return (ticks.QuadPart - kWindowsToUnixEpoch) / 10000000ULL;
}

bool WriteBridgeStatus(const ControlConfig& control, DWORD pid)
{
    if (gStatusPath.empty())
        return false;

    const uint32_t uiViewport = gLastOptionsViewport.load(std::memory_order_acquire);
    RefreshUiInputReadiness(uiViewport);
    const UiInputSnapshot uiInputs = ReadUiInputSnapshot(uiViewport);
    const bool bridgeReady = BridgeReady();
    const char* route = PatchRouteName();
    const uint64_t desiredRevision = gDesiredRevision.load(std::memory_order_acquire);
    const uint64_t appliedRevision = gAppliedRevision.load(std::memory_order_acquire);
    const bool setOptionsSeen = gSetOptionsSeen.load(std::memory_order_acquire);
    const bool getStateSeen = gGetStateSeen.load(std::memory_order_acquire);
    const bool gameFrameGenerationOn =
        gGameFrameGenerationOn.load(std::memory_order_acquire);
    const int32_t setOptionsResult =
        gLastSetOptionsResult.load(std::memory_order_relaxed);
    const int32_t getStateResult =
        gLastGetStateResult.load(std::memory_order_relaxed);
    const bool setOptionsAccepted = setOptionsResult == static_cast<int32_t>(sl::Result::eOk)
        || setOptionsResult == static_cast<int32_t>(sl::Result::eWarnOutOfVRAM);
    const bool applied = gameFrameGenerationOn && appliedRevision != 0
        && setOptionsAccepted;
    const bool pending = gameFrameGenerationOn && desiredRevision != appliedRevision;
    const uint64_t stateTick = gStateSampleTick.load(std::memory_order_acquire);
    const uint64_t nowTick = GetTickCount64();
    const uint64_t stateAgeMs = stateTick == 0 || nowTick < stateTick
        ? 0 : nowTick - stateTick;
    const uint64_t fpsTick = gFpsSampleTick.load(std::memory_order_acquire);
    const uint64_t fpsAgeMs = fpsTick == 0 || nowTick < fpsTick
        ? 0 : nowTick - fpsTick;

    char json[4096]{};
    const int length = sprintf_s(json,
        "{\"version\":7,\"pid\":%lu,\"heartbeat\":%llu,\"route\":\"%s\","
        "\"bridgeReady\":%s,\"liveHookInstalled\":%s,"
        "\"uiTagHookInstalled\":%s,"
        "\"activeWrapperObserved\":%s,\"activeWrapperPatched\":%s,"
        "\"loadedWrapperCandidates\":%u,\"patchedWrapperCandidates\":%u,"
        "\"loadedNgxCandidates\":%u,\"patchedNgxCandidates\":%u,"
        "\"mode\":\"%s\",\"multiplier\":%u,\"dynamicTargetFrameRate\":%u,"
        "\"dynamicExperimental56\":%s,\"forcedMaximumMultiplier\":%u,"
        "\"requestRevision\":%llu,\"appliedRevision\":%llu,"
        "\"applied\":%s,\"pending\":%s,\"gameFrameGenerationOn\":%s,"
        "\"appliedMode\":\"%s\",\"appliedMultiplier\":%u,"
        "\"appliedDynamicTargetFrameRate\":%u,"
        "\"appliedDynamicExperimental56\":%s,\"setOptionsSeen\":%s,"
        "\"setOptionsAccepted\":%s,"
        "\"setOptionsResult\":%d,\"getStateSeen\":%s,\"getStateResult\":%d,"
        "\"actualFramesPresented\":%u,\"numFramesToGenerateMax\":%u,"
        "\"realFpsMilli\":%u,\"dlssFpsMilli\":%u,"
        "\"fpsSampleWindowMs\":%u,\"fpsSampleAgeMs\":%llu,"
        "\"dlssgStatus\":%u,\"dynamicMfgSupported\":%s,"
        "\"gameOptionsStructVersion\":%u,\"gameUiRecompositionEnabled\":%s,"
        "\"gameHudlessBufferFormat\":%u,\"gameUiBufferFormat\":%u,"
        "\"hudlessTagActive\":%s,\"uiAlphaTagActive\":%s,"
        "\"uiColorAlphaTagActive\":%s,\"uiDimensionsKnown\":%s,"
        "\"uiDimensionsMatch\":%s,\"uiInputsReady\":%s,"
        "\"uiRecompositionEnabled\":%s,\"uiRecompositionForced\":%s,"
        "\"hudlessWidth\":%u,\"hudlessHeight\":%u,"
        "\"uiWidth\":%u,\"uiHeight\":%u,\"uiTagFormat\":%u,"
        "\"uiTagAgeMs\":%llu,\"setTagCalls\":%llu,"
        "\"setTagForFrameCalls\":%llu,"
        "\"stateSampleAgeMs\":%llu,\"setOptionsCalls\":%llu,"
        "\"getStateCalls\":%llu,\"liveReapplyCount\":%llu,"
        "\"notInitializedRetryCount\":%llu}\n",
        static_cast<unsigned long>(pid),
        static_cast<unsigned long long>(UnixTimeSeconds()), route,
        bridgeReady ? "true" : "false",
        gLiveHookInstalled.load(std::memory_order_relaxed) ? "true" : "false",
        gUiTagHookInstalled.load(std::memory_order_relaxed) ? "true" : "false",
        gActiveWrapperObserved.load(std::memory_order_relaxed) ? "true" : "false",
        gActiveWrapperPatched.load(std::memory_order_relaxed) ? "true" : "false",
        gLoadedWrapperCandidates.load(std::memory_order_relaxed),
        gPatchedWrapperCandidates.load(std::memory_order_relaxed),
        gLoadedNgxCandidates.load(std::memory_order_relaxed),
        gPatchedNgxCandidates.load(std::memory_order_relaxed),
        control.dynamic ? "dynamic" : "fixed", control.multiplier,
        control.dynamicTargetFrameRate,
        control.dynamicExperimental56 ? "true" : "false",
        static_cast<uint32_t>(RequestedMaximumGeneratedFrames(control)) + 1,
        static_cast<unsigned long long>(desiredRevision),
        static_cast<unsigned long long>(appliedRevision),
        applied ? "true" : "false", pending ? "true" : "false",
        gameFrameGenerationOn ? "true" : "false",
        gAppliedDynamicMode.load(std::memory_order_relaxed) ? "dynamic" : "fixed",
        gAppliedMultiplier.load(std::memory_order_relaxed),
        gAppliedDynamicTargetFrameRate.load(std::memory_order_relaxed),
        gAppliedDynamicExperimental56.load(std::memory_order_relaxed) ? "true" : "false",
        setOptionsSeen ? "true" : "false",
        setOptionsAccepted ? "true" : "false", setOptionsResult,
        getStateSeen ? "true" : "false", getStateResult,
        gActualFramesPresented.load(std::memory_order_relaxed),
        gNumFramesToGenerateMax.load(std::memory_order_relaxed),
        gRealFpsMilli.load(std::memory_order_relaxed),
        gDlssFpsMilli.load(std::memory_order_relaxed),
        gFpsSampleWindowMs.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(fpsAgeMs),
        gDlssgStatus.load(std::memory_order_relaxed),
        gDynamicMfgSupported.load(std::memory_order_relaxed) ? "true" : "false",
        gGameOptionsStructVersion.load(std::memory_order_relaxed),
        gGameUiRecompositionEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        gGameHudlessBufferFormat.load(std::memory_order_relaxed),
        gGameUiBufferFormat.load(std::memory_order_relaxed),
        uiInputs.hudless ? "true" : "false",
        uiInputs.uiAlpha ? "true" : "false",
        uiInputs.uiColorAlpha ? "true" : "false",
        uiInputs.dimensionsKnown ? "true" : "false",
        uiInputs.dimensionsMatch ? "true" : "false",
        uiInputs.ready ? "true" : "false",
        gAppliedUiRecompositionEnabled.load(std::memory_order_relaxed) ? "true" : "false",
        gAppliedUiRecompositionForced.load(std::memory_order_relaxed) ? "true" : "false",
        uiInputs.hudlessWidth, uiInputs.hudlessHeight,
        uiInputs.uiWidth, uiInputs.uiHeight, uiInputs.uiFormat,
        static_cast<unsigned long long>(uiInputs.oldestAgeMs),
        static_cast<unsigned long long>(gSetTagCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            gSetTagForFrameCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(stateAgeMs),
        static_cast<unsigned long long>(gSetOptionsCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(gGetStateCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(gLiveReapplyCount.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            gNotInitializedRetryCount.load(std::memory_order_relaxed)));
    if (length <= 0)
        return false;

    HANDLE file = CreateFileW(gStatusPath.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    const BOOL result = WriteFile(file, json, static_cast<DWORD>(length), &written, nullptr);
    CloseHandle(file);
    return result && written == static_cast<DWORD>(length);
}

// sl::Result names, mirrored from sl_result.h (40 contiguous entries).
constexpr const wchar_t* kResultNames[] = {
    L"eOk",
    L"eErrorIO",
    L"eErrorDriverOutOfDate",
    L"eErrorOSOutOfDate",
    L"eErrorOSDisabledHWS",
    L"eErrorDeviceNotCreated",
    L"eErrorNoSupportedAdapterFound",
    L"eErrorAdapterNotSupported",
    L"eErrorNoPlugins",
    L"eErrorVulkanAPI",
    L"eErrorDXGIAPI",
    L"eErrorD3DAPI",
    L"eErrorNRDAPI",
    L"eErrorNVAPI",
    L"eErrorReflexAPI",
    L"eErrorNGXFailed",
    L"eErrorJSONParsing",
    L"eErrorMissingProxy",
    L"eErrorMissingResourceState",
    L"eErrorInvalidIntegration",
    L"eErrorMissingInputParameter",
    L"eErrorNotInitialized",
    L"eErrorComputeFailed",
    L"eErrorInitNotCalled",
    L"eErrorExceptionHandler",
    L"eErrorInvalidParameter",
    L"eErrorMissingConstants",
    L"eErrorDuplicatedConstants",
    L"eErrorMissingOrInvalidAPI",
    L"eErrorCommonConstantsMissing",
    L"eErrorUnsupportedInterface",
    L"eErrorFeatureMissing",
    L"eErrorFeatureNotSupported",
    L"eErrorFeatureMissingHooks",
    L"eErrorFeatureFailedToLoad",
    L"eErrorFeatureWrongPriority",
    L"eErrorFeatureMissingDependency",
    L"eErrorFeatureManagerInvalidState",
    L"eErrorInvalidState",
    L"eWarnOutOfVRAM",
};

const wchar_t* ResultName(sl::Result result) noexcept
{
    const size_t index = static_cast<size_t>(result);
    return index < _countof(kResultNames) ? kResultNames[index] : L"unknown";
}

const wchar_t* ModeName(sl::DLSSGMode mode) noexcept
{
    switch (mode)
    {
    case sl::DLSSGMode::eOff:     return L"eOff";
    case sl::DLSSGMode::eOn:      return L"eOn";
    case sl::DLSSGMode::eAuto:    return L"eAuto";
    case sl::DLSSGMode::eDynamic: return L"eDynamic";
    default:                      return L"?";
    }
}

sl::DLSSGOptions CopyKnownOptions(const sl::DLSSGOptions& source, bool preserveNext)
{
    sl::DLSSGOptions copy{};
    copy.next = preserveNext ? source.next : nullptr;
    copy.structType = source.structType;
    copy.structVersion = std::clamp<size_t>(
        source.structVersion, sl::kStructVersion1, sl::kStructVersion5);
    copy.mode = source.mode;
    copy.numFramesToGenerate = source.numFramesToGenerate;
    copy.flags = source.flags;
    copy.dynamicResWidth = source.dynamicResWidth;
    copy.dynamicResHeight = source.dynamicResHeight;
    copy.numBackBuffers = source.numBackBuffers;
    copy.mvecDepthWidth = source.mvecDepthWidth;
    copy.mvecDepthHeight = source.mvecDepthHeight;
    copy.colorWidth = source.colorWidth;
    copy.colorHeight = source.colorHeight;
    copy.colorBufferFormat = source.colorBufferFormat;
    copy.mvecBufferFormat = source.mvecBufferFormat;
    copy.depthBufferFormat = source.depthBufferFormat;
    copy.hudLessBufferFormat = source.hudLessBufferFormat;
    copy.uiBufferFormat = source.uiBufferFormat;
    copy.onErrorCallback = source.onErrorCallback;
    if (source.structVersion >= sl::kStructVersion2)
        copy.bReserved15 = source.bReserved15;
    if (source.structVersion >= sl::kStructVersion3)
        copy.queueParallelismMode = source.queueParallelismMode;
    if (source.structVersion >= sl::kStructVersion4)
        copy.enableUserInterfaceRecomposition = source.enableUserInterfaceRecomposition;
    if (source.structVersion >= sl::kStructVersion5)
        copy.dynamicTargetFrameRate = source.dynamicTargetFrameRate;
    return copy;
}


sl::DLSSGOptions BuildAdjustedOptions(
    const sl::DLSSGOptions& source, const ControlSnapshot& snapshot,
    bool preserveNext, bool enableUiRecomposition,
    const UiInputSnapshot* uiInputs = nullptr)
{
    sl::DLSSGOptions adjusted = CopyKnownOptions(source, preserveNext);
    const bool isDynamic = snapshot.control.dynamic
        || (source.structVersion >= sl::kStructVersion5 && source.mode == sl::DLSSGMode::eDynamic);
    if (isDynamic)
    {
        // The injected object is a complete v5 structure even when Cyberpunk supplied
        // an older prefix, so the active wrapper can consume the dynamic target
        // without reading beyond the game's allocation.
        adjusted.structVersion = sl::kStructVersion5;
        adjusted.mode = sl::DLSSGMode::eDynamic;
        float targetFps = (source.structVersion >= sl::kStructVersion5 && source.mode == sl::DLSSGMode::eDynamic && source.dynamicTargetFrameRate > 0.0f)
            ? source.dynamicTargetFrameRate
            : static_cast<float>(snapshot.control.dynamicTargetFrameRate > 0 ? snapshot.control.dynamicTargetFrameRate : 120);
        adjusted.dynamicTargetFrameRate = targetFps;
        adjusted.numFramesToGenerate =
            RequestedMaximumGeneratedFrames(snapshot.control);
    }
    else
    {
        adjusted.mode = sl::DLSSGMode::eOn;
        adjusted.numFramesToGenerate =
            std::clamp(snapshot.control.multiplier,
            kMinimumMultiplier, kMaximumMultiplier) - 1;
        adjusted.structVersion = std::max<size_t>(
            adjusted.structVersion, sl::kStructVersion5);
    }
    if (enableUiRecomposition)
    {
        adjusted.structVersion = std::max<size_t>(
            adjusted.structVersion, sl::kStructVersion4);
        adjusted.enableUserInterfaceRecomposition = sl::Boolean::eTrue;
        if (uiInputs)
        {
            if (adjusted.colorWidth == 0 && uiInputs->hudlessWidth != 0)
            {
                adjusted.colorWidth = uiInputs->hudlessWidth;
                adjusted.colorHeight = uiInputs->hudlessHeight;
            }
            if (adjusted.hudLessBufferFormat == 0 && uiInputs->hudlessFormat != 0)
            {
                adjusted.hudLessBufferFormat = uiInputs->hudlessFormat;
            }
            if ((uiInputs->uiAlpha || uiInputs->uiColorAlpha) && uiInputs->uiFormat != 0 && adjusted.uiBufferFormat == 0)
            {
                adjusted.uiBufferFormat = uiInputs->uiFormat;
            }
        }
    }

    // Option C: Strip eEnableFullscreenMenuDetection to prevent DLSS-G from mistakenly
    // freezing interpolation on border strips during high-t subframe generation.
    if (gConfigDisableMenuDetection.load(std::memory_order_relaxed))
    {
        adjusted.flags = static_cast<sl::DLSSGFlags>(
            static_cast<uint32_t>(adjusted.flags) & ~static_cast<uint32_t>(sl::DLSSGFlags::eEnableFullscreenMenuDetection));
    }


    // Option C: Check OFA 16-pixel tile alignment for dimensions
    if (adjusted.colorHeight != 0 && (adjusted.colorHeight & 0xFu) != 0)
    {
        static std::atomic<bool> sLoggedTileAlign{false};
        if (!sLoggedTileAlign.exchange(true))
        {
            Log(L"Option C OFA alignment: colorHeight %u has non-16-aligned macroblock remainder (%u px)",
                adjusted.colorHeight, adjusted.colorHeight & 0xFu);
        }
    }
    return adjusted;
}

void CaptureGameOptions(
    const sl::ViewportHandle& viewport, const sl::DLSSGOptions& options)
{
    {
        std::lock_guard lock(gLastOptionsMutex);
        gLastGameOptions.viewport = viewport;
        gLastGameOptions.options = CopyKnownOptions(options, false);
        gLastGameOptions.valid = true;
    }
    const uint32_t viewportValue = static_cast<uint32_t>(viewport);
    gLastOptionsViewport.store(viewportValue, std::memory_order_release);
    gGameOptionsStructVersion.store(
        static_cast<uint32_t>(options.structVersion), std::memory_order_relaxed);
    gGameColorWidth.store(options.colorWidth, std::memory_order_relaxed);
    gGameColorHeight.store(options.colorHeight, std::memory_order_relaxed);
    gGameHudlessBufferFormat.store(options.hudLessBufferFormat, std::memory_order_relaxed);
    gGameUiBufferFormat.store(options.uiBufferFormat, std::memory_order_relaxed);
    const bool gameUir = options.structVersion >= sl::kStructVersion4
        && options.enableUserInterfaceRecomposition == sl::Boolean::eTrue;
    gGameUiRecompositionEnabled.store(gameUir, std::memory_order_relaxed);

    static std::mutex sOptionsLogMutex;
    static bool sHaveLoggedOptions = false;
    static uint32_t sLoggedStructVersion = 0;
    static sl::DLSSGMode sLoggedMode = sl::DLSSGMode::eOff;
    static uint32_t sLoggedFrames = 0;
    static bool sLoggedUir = false;
    static uint32_t sLoggedWidth = 0;
    static uint32_t sLoggedHeight = 0;
    static uint32_t sLoggedFlags = 0;

    bool shouldLog = false;
    {
        std::lock_guard lock(sOptionsLogMutex);
        if (!sHaveLoggedOptions ||
            sLoggedStructVersion != static_cast<uint32_t>(options.structVersion) ||
            sLoggedMode != options.mode ||
            sLoggedFrames != options.numFramesToGenerate ||
            sLoggedUir != gameUir ||
            sLoggedWidth != options.colorWidth ||
            sLoggedHeight != options.colorHeight ||
            sLoggedFlags != static_cast<uint32_t>(options.flags))
        {
            sHaveLoggedOptions = true;
            sLoggedStructVersion = static_cast<uint32_t>(options.structVersion);
            sLoggedMode = options.mode;
            sLoggedFrames = options.numFramesToGenerate;
            sLoggedUir = gameUir;
            sLoggedWidth = options.colorWidth;
            sLoggedHeight = options.colorHeight;
            sLoggedFlags = static_cast<uint32_t>(options.flags);
            shouldLog = true;
        }
    }

    if (shouldLog)
    {
        Log(L"[OPTIONS] Game DLSS-G options: structVersion=%u mode=%s frames=%u uir=%d color=%ux%u flags=0x%08X",
            static_cast<uint32_t>(options.structVersion), ModeName(options.mode),
            options.numFramesToGenerate, gameUir,
            options.colorWidth, options.colorHeight, static_cast<uint32_t>(options.flags));
    }
    RefreshUiInputReadiness(viewportValue);
}

bool ReadLastGameOptions(
    const sl::ViewportHandle& viewport, sl::DLSSGOptions& options,
    sl::ViewportHandle* outViewport = nullptr)
{
    std::lock_guard lock(gLastOptionsMutex);
    if (!gLastGameOptions.valid)
        return false;
    options = gLastGameOptions.options;
    if (outViewport)
    {
        if (static_cast<uint32_t>(gLastGameOptions.viewport) != 0 || static_cast<uint32_t>(viewport) == 0)
            *outViewport = gLastGameOptions.viewport;
        else
            *outViewport = viewport;
    }
    return true;
}

void RecordAppliedControl(const ControlSnapshot& snapshot, sl::Result result,
    bool liveReapply, bool uiRecompositionEnabled, bool uiRecompositionForced)
{
    gSetOptionsSeen.store(true, std::memory_order_release);
    gLastSetOptionsResult.store(static_cast<int32_t>(result), std::memory_order_relaxed);
    gLastAttemptTick.store(GetTickCount64(), std::memory_order_relaxed);
    gAttemptedRevision.store(snapshot.revision, std::memory_order_release);
    // eWarnOutOfVRAM is emitted after Streamline accepts work when DXGI reports
    // no remaining budget. Keep the raw warning for telemetry, but do not leave
    // a successfully submitted multiplier permanently marked as pending.
    if (result != sl::Result::eOk && result != sl::Result::eWarnOutOfVRAM)
        return;

    const uint64_t previous = gAppliedRevision.load(std::memory_order_acquire);
    gAppliedDynamicMode.store(snapshot.control.dynamic, std::memory_order_relaxed);
    gAppliedMultiplier.store(snapshot.control.multiplier, std::memory_order_relaxed);
    gAppliedDynamicTargetFrameRate.store(
        snapshot.control.dynamicTargetFrameRate, std::memory_order_relaxed);
    gAppliedDynamicExperimental56.store(
        snapshot.control.dynamicExperimental56, std::memory_order_relaxed);
    gAppliedUiRecompositionEnabled.store(
        uiRecompositionEnabled, std::memory_order_relaxed);
    gAppliedUiRecompositionForced.store(
        uiRecompositionForced, std::memory_order_relaxed);
    gAppliedRevision.store(snapshot.revision, std::memory_order_release);
    if (liveReapply)
        gLiveReapplyCount.fetch_add(1, std::memory_order_relaxed);

    if (previous == snapshot.revision)
        return;
    if (snapshot.control.dynamic)
        Log(L"%s dynamic MFG: target=%u FPS experimental56=%d max=%ux result=%d (%s)",
            liveReapply ? L"Live-reapplied" : L"Applied",
            snapshot.control.dynamicTargetFrameRate,
            snapshot.control.dynamicExperimental56,
            static_cast<uint32_t>(RequestedMaximumGeneratedFrames(snapshot.control)) + 1,
            static_cast<int>(result), ResultName(result));
    else
        Log(L"%s fixed multiplier: %ux, result=%d (%s)",
            liveReapply ? L"Live-reapplied" : L"Applied",
            snapshot.control.multiplier, static_cast<int>(result), ResultName(result));
}

sl::Result SubmitAdjustedOptions(
    PFun_slDLSSGSetOptions* original, const sl::ViewportHandle& viewport,
    const sl::DLSSGOptions& source, const ControlSnapshot& snapshot, bool liveReapply)
{
    const bool isEndfield = gIsEndfield.load(std::memory_order_relaxed);
    const bool forceUirConfig = isEndfield || gConfigForceUiRecomposition.load(std::memory_order_relaxed);
    const UiInputSnapshot uiInputs = ReadUiInputSnapshot(
        static_cast<uint32_t>(viewport));
    const bool gameUiRecomposition = (source.structVersion >= sl::kStructVersion4
        && source.enableUserInterfaceRecomposition == sl::Boolean::eTrue)
        || gGameUiRecompositionEnabled.load(std::memory_order_relaxed);
    const bool enableUi = gameUiRecomposition || uiInputs.ready || forceUirConfig;
    const bool forceUiRecomposition = (uiInputs.ready || forceUirConfig) && !gameUiRecomposition;
    const sl::DLSSGOptions adjusted = BuildAdjustedOptions(
        source, snapshot, !liveReapply, enableUi, &uiInputs);
    const bool uiRecompositionEnabled = adjusted.structVersion >= sl::kStructVersion4
        && adjusted.enableUserInterfaceRecomposition == sl::Boolean::eTrue;

    static std::atomic<bool> sLoggedMenuDetectionStrip{false};
    if ((static_cast<uint32_t>(source.flags) & static_cast<uint32_t>(sl::DLSSGFlags::eEnableFullscreenMenuDetection)) != 0)
    {
        if (!sLoggedMenuDetectionStrip.exchange(true))
        {
            Log(L"Option C active: stripped eEnableFullscreenMenuDetection from DLSS-G options (game flags=0x%08X -> 0x%08X)",
                static_cast<uint32_t>(source.flags), static_cast<uint32_t>(adjusted.flags));
        }
    }

    static std::atomic<bool> sLoggedUirSubmit{false};
    if (uiRecompositionEnabled && !sLoggedUirSubmit.exchange(true))
    {
        Log(L"[UIR] Streamline DLSS-G options submitted: uir=1 (forced=%d) extent=%ux%u hudlessFmt=%u uiFmt=%u",
            forceUiRecomposition ? 1 : 0,
            adjusted.colorWidth, adjusted.colorHeight,
            adjusted.hudLessBufferFormat, adjusted.uiBufferFormat);
    }

    SynchronizeStreamlineLiveContext();
    const sl::Result result = original(viewport, adjusted);
    RecordAppliedControl(snapshot, result, liveReapply,
        uiRecompositionEnabled, forceUiRecomposition);

    if (!liveReapply && result != sl::Result::eOk && result != sl::Result::eWarnOutOfVRAM)
    {
        const sl::Result fallback = original(viewport, source);
        Log(L"slDLSSGSetOptions adjusted options REJECTED: result=%u (%s); replayed game's own options (mode=%s, frames=%u) -> result=%u (%s)",
            static_cast<uint32_t>(result), ResultName(result),
            ModeName(source.mode), source.numFramesToGenerate,
            static_cast<uint32_t>(fallback), ResultName(fallback));
        return fallback == sl::Result::eWarnOutOfVRAM ? sl::Result::eOk : fallback;
    }

    if (result == sl::Result::eOk || result == sl::Result::eWarnOutOfVRAM)
    {
        auto* getState = gOriginalGetState.load(std::memory_order_acquire);
        if (getState)
        {
            sl::DLSSGState state{};
            const sl::Result stateResult = getState(viewport, state, &adjusted);
            RecordDlssgStateResult(stateResult, state, true);
        }
        else if (!liveReapply)
        {
            UpdateFpsTelemetry(0);
        }
    }
    return result == sl::Result::eWarnOutOfVRAM ? sl::Result::eOk : result;
}

void ReapplyPendingControl(const sl::ViewportHandle& viewport)
{
    if (!gControlReady.load(std::memory_order_acquire))
        return;
    if (!gGameFrameGenerationOn.load(std::memory_order_acquire))
        return;
    if (!BridgeReady() && !gOriginalSetOptions.load(std::memory_order_acquire) && !gOriginalSlSetData.load(std::memory_order_acquire))
        return;

    const ControlSnapshot snapshot = ReadControlSnapshot();
    if (snapshot.revision == 0
        || snapshot.revision == gAppliedRevision.load(std::memory_order_acquire))
        return;

    const uint64_t attemptedRevision =
        gAttemptedRevision.load(std::memory_order_acquire);
    bool retryNotInitialized = false;
    if (snapshot.revision == attemptedRevision)
    {
        const int32_t result = gLastSetOptionsResult.load(std::memory_order_relaxed);
        if (result != static_cast<int32_t>(sl::Result::eErrorNotInitialized))
            return;
        const uint64_t now = GetTickCount64();
        const uint64_t previousAttempt =
            gLastAttemptTick.load(std::memory_order_relaxed);
        if (now < previousAttempt
            || now - previousAttempt < kNotInitializedRetryDelayMs)
            return;
        retryNotInitialized = true;
    }

    auto* original = gOriginalSetOptions.load(std::memory_order_acquire);
    auto* originalSetData = gOriginalSlSetData.load(std::memory_order_acquire);
    sl::DLSSGOptions source{};
    sl::ViewportHandle targetViewport = viewport;
    if ((!original && !originalSetData) || !ReadLastGameOptions(viewport, source, &targetViewport))
        return;
    if (retryNotInitialized)
    {
        const uint64_t retry =
            gNotInitializedRetryCount.fetch_add(1, std::memory_order_relaxed) + 1;
        Log(L"Retrying request revision %llu after Streamline result 21 (retry %llu)",
            static_cast<unsigned long long>(snapshot.revision),
            static_cast<unsigned long long>(retry));
    }

    if (original)
    {
        gSetOptionsCalls.fetch_add(1, std::memory_order_relaxed);
        const sl::Result result =
            SubmitAdjustedOptions(original, targetViewport, source, snapshot, true);
        if (result != sl::Result::eOk && result != sl::Result::eWarnOutOfVRAM)
            Log(L"Live reapply failed for request revision %llu: result=%d (%s)",
                static_cast<unsigned long long>(snapshot.revision), static_cast<int>(result), ResultName(result));
    }
    else if (originalSetData)
    {
        const bool isEndfield = gIsEndfield.load(std::memory_order_relaxed);
        const bool forceUirConfig = isEndfield || gConfigForceUiRecomposition.load(std::memory_order_relaxed);
        const UiInputSnapshot uiInputs = ReadUiInputSnapshot(static_cast<uint32_t>(targetViewport));
        const bool gameUi = (source.structVersion >= sl::kStructVersion4 && source.enableUserInterfaceRecomposition == sl::Boolean::eTrue)
            || gGameUiRecompositionEnabled.load(std::memory_order_relaxed);
        sl::DLSSGOptions adjusted = BuildAdjustedOptions(source, snapshot, false, gameUi || uiInputs.ready || forceUirConfig, &uiInputs);
        sl::ViewportHandle vpCopy = targetViewport;
        vpCopy.next = &adjusted;
        adjusted.next = nullptr;
        const sl::Result result = originalSetData(&vpCopy, nullptr);
        RecordAppliedControl(snapshot, result, true, adjusted.enableUserInterfaceRecomposition == sl::Boolean::eTrue, false);
        if (result != sl::Result::eOk && result != sl::Result::eWarnOutOfVRAM)
            Log(L"Live reapply via setData failed for request revision %llu: result=%d (%s)",
                static_cast<unsigned long long>(snapshot.revision), static_cast<int>(result), ResultName(result));
    }
}

void TryReapplyPendingControl(const sl::ViewportHandle& viewport)
{
    if (gDesiredRevision.load(std::memory_order_relaxed) ==
        gAppliedRevision.load(std::memory_order_relaxed))
        return;
    if (!gControlReady.load(std::memory_order_relaxed))
        return;
    if (!gGameFrameGenerationOn.load(std::memory_order_relaxed))
        return;
    if (!BridgeReady() && !gOriginalSetOptions.load(std::memory_order_acquire) && !gOriginalSlSetData.load(std::memory_order_acquire))
        return;

    std::unique_lock callLock(gStreamlineCallMutex, std::try_to_lock);
    if (!callLock.owns_lock())
        return;

    ReapplyPendingControl(viewport);
}

sl::Result HookSlDLSSGSetOptions(
    const sl::ViewportHandle& viewport, const sl::DLSSGOptions& options)
{
    auto* original = gOriginalSetOptions.load(std::memory_order_acquire);
    if (!original)
        return sl::Result::eErrorNotInitialized;

    std::lock_guard callLock(gStreamlineCallMutex);
    gSetOptionsCalls.fetch_add(1, std::memory_order_relaxed);

    const bool enabled = options.mode == sl::DLSSGMode::eOn
        || options.mode == sl::DLSSGMode::eAuto
        || options.mode == sl::DLSSGMode::eDynamic;
    gGameFrameGenerationOn.store(enabled, std::memory_order_release);
    if (!enabled)
    {
        gSetOptionsSeen.store(true, std::memory_order_release);
        const sl::Result result = original(viewport, options);
        gLastSetOptionsResult.store(static_cast<int32_t>(result), std::memory_order_relaxed);
        return result;
    }

    CaptureGameOptions(viewport, options);
    const ControlSnapshot snapshot = ReadControlSnapshot();
    return SubmitAdjustedOptions(original, viewport, options, snapshot, false);
}

sl::Result HookSlDLSSGGetState(
    const sl::ViewportHandle& viewport, sl::DLSSGState& state,
    const sl::DLSSGOptions* options)
{
    auto* original = gOriginalGetState.load(std::memory_order_acquire);
    if (!original)
        return sl::Result::eErrorNotInitialized;

    std::lock_guard callLock(gStreamlineCallMutex);
    ReapplyPendingControl(viewport);
    const sl::Result result = original(viewport, state, options);
    if (result == sl::Result::eOk && state.structVersion >= sl::kStructVersion2)
    {
        const uint32_t advertised = gAdvertisedMaxGenerated.load(std::memory_order_relaxed);
        if (advertised > state.numFramesToGenerateMax)
            state.numFramesToGenerateMax = advertised;
        else if (gActiveWrapperPatched.load(std::memory_order_relaxed) && state.numFramesToGenerateMax < kExperimentalMaximumGeneratedFrames)
            state.numFramesToGenerateMax = static_cast<uint32_t>(kExperimentalMaximumGeneratedFrames);
    }
    if (result == sl::Result::eOk && state.structVersion >= sl::kStructVersion4)
    {
        state.bIsDynamicMFGSupported = sl::Boolean::eTrue;
    }
    RecordDlssgStateResult(result, state, true);
    return result;
}

sl::Result HookSlGetFeatureFunction(
    sl::Feature feature, const char* functionName, void*& function)
{
    auto* original = gOriginalGetFeatureFunction.load(std::memory_order_acquire);
    Log(L"[DBG] HookSlGetFeatureFunction: feature=%d fn=%hs original=%p",
        static_cast<int>(feature), functionName ? functionName : "(null)", original);
    if (!original)
    {
        Log(L"[DBG] HookSlGetFeatureFunction: no original -> eErrorNotInitialized");
        return sl::Result::eErrorNotInitialized;
    }

    sl::Result result = sl::Result::eErrorNotInitialized;
    __try
    {
        result = original(feature, functionName, function);
    }
    __except (Log(L"[DBG] HookSlGetFeatureFunction: EXCEPTION 0x%08lX at %p",
                  GetExceptionCode(),
                  GetExceptionInformation() ? GetExceptionInformation()->ExceptionRecord->ExceptionAddress : nullptr),
              EXCEPTION_CONTINUE_SEARCH)
    {
        return result;
    }
    if (function && functionName && strcmp(functionName, "slDLSSGSetOptions") == 0)
    {
        ObserveActiveWrapperProvider(function);
        auto* setOptions = reinterpret_cast<PFun_slDLSSGSetOptions*>(function);
        if (setOptions != &HookSlDLSSGSetOptions)
            gOriginalSetOptions.store(setOptions, std::memory_order_release);
        function = reinterpret_cast<void*>(&HookSlDLSSGSetOptions);
        if (!gSetOptionsHookExposed.exchange(true))
            Log(L"Intercepted slDLSSGSetOptions for live multiplier control");
    }
    else if (function && functionName && strcmp(functionName, "slDLSSGGetState") == 0)
    {
        ObserveActiveWrapperProvider(function);
        auto* getState = reinterpret_cast<PFun_slDLSSGGetState*>(function);
        if (getState != &HookSlDLSSGGetState)
            gOriginalGetState.store(getState, std::memory_order_release);
        function = reinterpret_cast<void*>(&HookSlDLSSGGetState);
        if (!gGetStateHookExposed.exchange(true))
            Log(L"Intercepted slDLSSGGetState for render-thread reapply and actual telemetry");
    }
    Log(L"[DBG] HookSlGetFeatureFunction: fn=%hs result=%d function=%p",
        functionName ? functionName : "(null)", static_cast<int>(result), function);
    return result;
}

sl::Result HookSlSetTag(const sl::ViewportHandle& viewport,
    const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer)
{
    auto* original = gOriginalSetTag.load(std::memory_order_acquire);
    static std::atomic<bool> sLogged{false};
    if (!sLogged.exchange(true))
        Log(L"[DBG] HookSlSetTag: numTags=%u original=%p", numTags, original);
    if (!original)
        return sl::Result::eErrorNotInitialized;
    static thread_local bool sInside = false;
    if (sInside)
        return original(viewport, tags, numTags, cmdBuffer);
    sInside = true;
    struct ScopeExit { ~ScopeExit() { sInside = false; } } exitScope;

    const sl::Result result = original(viewport, tags, numTags, cmdBuffer);
    gSetTagCalls.fetch_add(1, std::memory_order_relaxed);
    if (result == sl::Result::eOk)
    {
        CaptureUiResourceTags(viewport, tags, numTags);
        LogMotionResourceTags(viewport, tags, numTags, false, 0);
    }
    TryReapplyPendingControl(viewport);
    return result;
}

sl::Result HookSlSetTagForFrame(const sl::FrameToken& frame,
    const sl::ViewportHandle& viewport, const sl::ResourceTag* tags,
    uint32_t numTags, sl::CommandBuffer* cmdBuffer)
{
    auto* original = gOriginalSetTagForFrame.load(std::memory_order_acquire);
    if (!original)
        return sl::Result::eErrorNotInitialized;
    static thread_local bool sInside = false;
    if (sInside)
        return original(frame, viewport, tags, numTags, cmdBuffer);
    sInside = true;
    struct ScopeExit { ~ScopeExit() { sInside = false; } } exitScope;

    const sl::Result result = original(frame, viewport, tags, numTags, cmdBuffer);
    gSetTagForFrameCalls.fetch_add(1, std::memory_order_relaxed);
    if (result == sl::Result::eOk)
    {
        CaptureUiResourceTags(viewport, tags, numTags);
        LogMotionResourceTags(viewport, tags, numTags, true, static_cast<uint32_t>(frame));
    }
    TryReapplyPendingControl(viewport);
    return result;
}

sl::Result HookSlSetD3DDevice(void* device)
{
    auto* original = gOriginalSetD3DDevice.load(std::memory_order_acquire);
    const uint64_t start = GetTickCount64();
    Log(L"[DBG] slSetD3DDevice ENTER: device=%p original=%p", device, original);
    if (!original)
    {
        Log(L"[DBG] HookSlSetD3DDevice: no original -> eErrorNotInitialized");
        return sl::Result::eErrorNotInitialized;
    }
    sl::Result result = sl::Result::eErrorNotInitialized;
    gD3DDeviceThread.store(GetCurrentThreadId(), std::memory_order_relaxed);
    gD3DDeviceStartTick.store(start, std::memory_order_release);
    __try
    {
        if (midpoint_fix::ObserveD3D12Device(device))
            gModuleInventoryDirty.store(true, std::memory_order_release);
        result = original(device);
    }
    __except (Log(L"[DBG] HookSlSetD3DDevice: EXCEPTION 0x%08lX at %p",
                  GetExceptionCode(),
                  GetExceptionInformation() ? GetExceptionInformation()->ExceptionRecord->ExceptionAddress : nullptr),
              EXCEPTION_CONTINUE_SEARCH)
    {
    }
    Log(L"[DBG] slSetD3DDevice EXIT: result=%d (%s) elapsed=%llums",
        static_cast<int>(result), ResultName(result),
        static_cast<unsigned long long>(GetTickCount64() - start));
    gD3DDeviceStartTick.store(0, std::memory_order_release);
    return result;
}

struct VulkanInfoPrefix
{
    sl::BaseStructure* next = nullptr;
    sl::StructType structType{};
    size_t structVersion = 0;
    void* device = nullptr;
    void* instance = nullptr;
    void* physicalDevice = nullptr;
};
using PFun_slSetVulkanInfo = sl::Result(const VulkanInfoPrefix&);
std::atomic<PFun_slSetVulkanInfo*> gOriginalSetVulkanInfo{nullptr};
PFun_slSetVulkanInfo* gDetourSlSetVulkanInfo = nullptr;

sl::Result HookSlSetVulkanInfo(const VulkanInfoPrefix& info)
{
    auto* original = gOriginalSetVulkanInfo.load(std::memory_order_acquire);
    if (!original)
        return sl::Result::eErrorNotInitialized;

    void* physicalDevice = nullptr;
    __try
    {
        if (info.structVersion >= sl::kStructVersion1)
            physicalDevice = info.physicalDevice;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        physicalDevice = nullptr;
    }

    static std::atomic<bool> sVkLogged{false};
    if (!sVkLogged.exchange(true))
        Log(L"[DBG] HookSlSetVulkanInfo: structVersion=%zu original=%p",
            info.structVersion, original);
    sl::Result result = sl::Result::eErrorNotInitialized;
    __try
    {
        if (physicalDevice)
        {
            midpoint_fix::ObserveVulkanPhysicalDevice(physicalDevice);
            gModuleInventoryDirty.store(true, std::memory_order_release);
        }
        result = original(info);
    }
    __except (Log(L"[DBG] HookSlSetVulkanInfo: EXCEPTION 0x%08lX at %p",
                  GetExceptionCode(),
                  GetExceptionInformation() ? GetExceptionInformation()->ExceptionRecord->ExceptionAddress : nullptr),
              EXCEPTION_CONTINUE_SEARCH)
    {
    }
    return result;
}

struct BaseStructureFields
{
    sl::BaseStructure* next = nullptr;
    sl::StructType type{};
    size_t version = 0;
};

inline bool ReadBaseStructureFields(const sl::BaseStructure* source, BaseStructureFields& fields) noexcept
{
    if (!source)
        return false;
    __try
    {
        fields.next = source->next;
        fields.type = source->structType;
        fields.version = source->structVersion;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

template <typename T>
inline const T* FindStructInChain(const sl::BaseStructure* chain) noexcept
{
    constexpr size_t kMaximumChainNodes = 32;
    const sl::BaseStructure* visited[kMaximumChainNodes]{};
    for (size_t index = 0; chain && index < kMaximumChainNodes; ++index)
    {
        for (size_t prior = 0; prior < index; ++prior)
        {
            if (visited[prior] == chain)
                return nullptr;
        }
        visited[index] = chain;
        BaseStructureFields fields{};
        if (!ReadBaseStructureFields(chain, fields))
            return nullptr;
        if (fields.type == T::s_structType)
            return static_cast<const T*>(chain);
        chain = fields.next;
    }
    return nullptr;
}

namespace
{
// Camera transforms belong to the game's frame and viewport. An identity matrix
// is valid for stationary/reset frames; camera pose fields are not a substitute
// for the game's projection convention or its previous-frame transform.
const wchar_t* DescribeMotionMatrix(const sl::float4x4& matrix)
{
    bool zero = true;
    bool identity = true;
    for (int row = 0; row < 4; ++row)
    {
        const auto& v = matrix[row];
        const float elements[] = {v.x, v.y, v.z, v.w};
        for (int col = 0; col < 4; ++col)
        {
            const float value = elements[col];
            if (!std::isfinite(value) || value == sl::INVALID_FLOAT)
                return L"invalid";
            zero &= value == 0.0f;
            identity &= std::fabs(value - (row == col ? 1.0f : 0.0f)) <= 1e-4f;
        }
    }
    return zero ? L"zero" : identity ? L"identity" : L"non-identity";
}

void LogMotionInputs(const sl::Constants& values, const wchar_t* route,
    uint32_t viewport, bool dilationOverride)
{
    if constexpr (scatter_experiment::kMode != 0) return;
    // Observe real gameplay after startup without per-frame logging. This does
    // not read textures, synchronize the GPU, or change any input constants.
    static std::atomic<uint64_t> calls{0};
    const uint64_t sample = calls.fetch_add(1, std::memory_order_relaxed);
    if (sample >= 4 && sample % 600 != 0) return;
    Log(L"[MOTION] route=%s call=%llu viewport=%u transforms=game-preserved "
        L"clipToPrev=%s prevToClip=%s projection=%s reset=%d cameraIncluded=%d "
        L"mv3D=%d dilatedGame=%d dilationOverride=%d jittered=%d depthInverted=%d "
        L"scale=(%.9g,%.9g) jitter=(%.9g,%.9g)",
        route, static_cast<unsigned long long>(sample), viewport,
        DescribeMotionMatrix(values.clipToPrevClip),
        DescribeMotionMatrix(values.prevClipToClip),
        DescribeMotionMatrix(values.cameraViewToClip),
        static_cast<int>(values.reset), static_cast<int>(values.cameraMotionIncluded),
        static_cast<int>(values.motionVectors3D), static_cast<int>(values.motionVectorsDilated),
        dilationOverride, static_cast<int>(values.motionVectorsJittered),
        static_cast<int>(values.depthInverted), values.mvecScale.x, values.mvecScale.y,
        values.jitterOffset.x, values.jitterOffset.y);
    const auto& m = values.clipToPrevClip;
    Log(L"[MOTION] clipToPrev rows=[%.9g %.9g %.9g %.9g][%.9g %.9g %.9g %.9g]"
        L"[%.9g %.9g %.9g %.9g][%.9g %.9g %.9g %.9g] cameraPos=(%.9g,%.9g,%.9g)",
        m[0].x, m[0].y, m[0].z, m[0].w, m[1].x, m[1].y, m[1].z, m[1].w,
        m[2].x, m[2].y, m[2].z, m[2].w, m[3].x, m[3].y, m[3].z, m[3].w,
        values.cameraPos.x, values.cameraPos.y, values.cameraPos.z);
}
}

sl::Result HookSlSetData(const sl::BaseStructure* inputs, sl::CommandBuffer* cmdBuffer)
{
    auto* original = gOriginalSlSetData.load(std::memory_order_acquire);
    static std::atomic<bool> sLogged{false};
    if (!sLogged.exchange(true))
        Log(L"[DBG] HookSlSetData: inputs=%p original=%p", inputs, original);
    if (!original)
    {
        Log(L"[DBG] HookSlSetData: no original -> eErrorNotInitialized");
        return sl::Result::eErrorNotInitialized;
    }

    const auto* options = FindStructInChain<sl::DLSSGOptions>(inputs);
    const auto* viewport = FindStructInChain<sl::ViewportHandle>(inputs);
    auto* constants = const_cast<sl::Constants*>(FindStructInChain<sl::Constants>(inputs));
    if (constants)
    {
        const bool dilationOverride = gConfigDisableMvDilation.load(std::memory_order_relaxed);
        LogMotionInputs(*constants, L"slSetData",
            viewport ? static_cast<uint32_t>(*viewport) : UINT32_MAX, dilationOverride);
        // Retain the explicitly configured legacy dilation override only.
        // In the normal path (override=false), caller constants are read-only.
        if (dilationOverride)
            constants->motionVectorsDilated = sl::Boolean::eTrue;
    }

    if (options)
    {
        const bool enabled = options->mode == sl::DLSSGMode::eOn
            || options->mode == sl::DLSSGMode::eAuto
            || options->mode == sl::DLSSGMode::eDynamic;
        gGameFrameGenerationOn.store(enabled, std::memory_order_release);

        if (viewport)
            CaptureGameOptions(*viewport, *options);
        else
        {
            std::lock_guard lock(gLastOptionsMutex);
            gLastGameOptions.options = CopyKnownOptions(*options, false);
            gLastGameOptions.valid = true;
        }

        if (enabled && gControlReady.load(std::memory_order_acquire) && BridgeReady())
        {
            const bool isEndfield = gIsEndfield.load(std::memory_order_relaxed);
            const bool forceUirConfig = isEndfield || gConfigForceUiRecomposition.load(std::memory_order_relaxed);
            const ControlSnapshot snapshot = ReadControlSnapshot();
            const UiInputSnapshot uiInputs = viewport ? ReadUiInputSnapshot(static_cast<uint32_t>(*viewport)) : UiInputSnapshot{};
            const bool gameUi = (options->structVersion >= sl::kStructVersion4 && options->enableUserInterfaceRecomposition == sl::Boolean::eTrue)
                || gGameUiRecompositionEnabled.load(std::memory_order_relaxed);
            sl::DLSSGOptions adjusted = BuildAdjustedOptions(*options, snapshot, false, gameUi || uiInputs.ready || forceUirConfig, &uiInputs);

            const sl::BaseStructure* head = inputs;
            sl::BaseStructure* prevNode = nullptr;
            for (const sl::BaseStructure* curr = inputs; curr; curr = curr->next)
            {
                if (curr == reinterpret_cast<const sl::BaseStructure*>(options))
                    break;
                prevNode = const_cast<sl::BaseStructure*>(curr);
            }

            adjusted.next = options->next;
            if (prevNode)
                prevNode->next = &adjusted;
            else
                head = &adjusted;

            const sl::Result result = original(head, cmdBuffer);

            if (prevNode)
                prevNode->next = const_cast<sl::BaseStructure*>(reinterpret_cast<const sl::BaseStructure*>(options));

            RecordAppliedControl(snapshot, result, false, false, false);
            return result == sl::Result::eWarnOutOfVRAM ? sl::Result::eOk : result;
        }
    }

    if (viewport)
        TryReapplyPendingControl(*viewport);

    return original(inputs, cmdBuffer);
}

sl::Result HookSlSetConstants(
    const sl::Constants& values,
    const sl::FrameToken& frame,
    const sl::ViewportHandle& viewport)
{
    auto* original = gOriginalSlSetConstants.load(std::memory_order_acquire);
    static std::atomic<bool> sLogged{false};
    if (!sLogged.exchange(true))
        Log(L"[DBG] HookSlSetConstants: original=%p", original);
    if (!original)
        return sl::Result::eErrorNotInitialized;

    static thread_local bool sInside = false;
    if (sInside)
        return original(values, frame, viewport);
    sInside = true;
    struct ScopeExit { ~ScopeExit() { sInside = false; } } exitScope;

    const bool dilationOverride = gConfigDisableMvDilation.load(std::memory_order_relaxed);
    LogMotionInputs(values, L"slSetConstants", static_cast<uint32_t>(viewport), dilationOverride);

    sl::Result result;
    if (!dilationOverride)
        result = original(values, frame, viewport);
    else
    {
        sl::Constants adjusted = values;
        adjusted.motionVectorsDilated = sl::Boolean::eTrue;
        result = original(adjusted, frame, viewport);
    }
    TryReapplyPendingControl(viewport);
    return result;
}

bool HookMainExecutableImport(const char* importedModule, const char* importedFunction,
    void* replacement, void*& original)
{
    HMODULE module = GetModuleHandleW(nullptr);
    auto* base = reinterpret_cast<uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE
        || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return false;

    const auto& importDirectory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDirectory.VirtualAddress || !importDirectory.Size)
        return false;

    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        base + importDirectory.VirtualAddress);
    for (; descriptor->Name; ++descriptor)
    {
        const char* moduleName = reinterpret_cast<const char*>(base + descriptor->Name);
        if (_stricmp(moduleName, importedModule) != 0)
            continue;

        const DWORD originalRva = descriptor->OriginalFirstThunk
            ? descriptor->OriginalFirstThunk : descriptor->FirstThunk;
        auto* originalThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + originalRva);
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->FirstThunk);
        for (; originalThunk->u1.AddressOfData; ++originalThunk, ++thunk)
        {
            if (IMAGE_SNAP_BY_ORDINAL64(originalThunk->u1.Ordinal))
                continue;
            const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                base + originalThunk->u1.AddressOfData);
            if (strcmp(reinterpret_cast<const char*>(import->Name), importedFunction) != 0)
                continue;

            auto** slot = reinterpret_cast<void**>(&thunk->u1.Function);
            auto* current = *slot;
            if (current == replacement)
                return true;

            DWORD oldProtection = 0;
            if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection))
                return false;
            original = current;
            *slot = replacement;
            DWORD ignoredProtection = 0;
            const BOOL restored = VirtualProtect(
                slot, sizeof(*slot), oldProtection, &ignoredProtection);
            FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
            return restored != FALSE;
        }
    }
    return false;
}

bool InstallFeatureFunctionHook()
{
    std::unique_lock installLock(gHookInstallMutex, std::try_to_lock);
    if (!installLock.owns_lock())
        return gInterposerDetoursInstalled.load(std::memory_order_acquire);
    // The export now jumps to our hook. Saving it as the IAT "original"
    // would replace the trampoline and recurse back into the hook forever.
    if (gInterposerDetoursInstalled.load(std::memory_order_acquire)
        && gDetourSlGetFeatureFunction)
        return true;
    void* original = nullptr;
    bool installed = HookMainExecutableImport("sl.interposer.dll",
        "slGetFeatureFunction", reinterpret_cast<void*>(&HookSlGetFeatureFunction), original);
    if (!installed)
    {
        installed = HookMainExecutableImport("sl.common.dll",
            "slGetFeatureFunction", reinterpret_cast<void*>(&HookSlGetFeatureFunction), original);
    }
    if (original)
    {
        gOriginalGetFeatureFunction.store(
            reinterpret_cast<PFun_slGetFeatureFunction*>(original),
            std::memory_order_release);
    }
    return installed;
}

bool InstallD3DDeviceHook()
{
    std::unique_lock installLock(gHookInstallMutex, std::try_to_lock);
    if (!installLock.owns_lock())
        return gInterposerDetoursInstalled.load(std::memory_order_acquire);
    if (gInterposerDetoursInstalled.load(std::memory_order_acquire)
        && gDetourSlSetD3DDevice)
        return true;
    void* original = nullptr;
    bool installed = HookMainExecutableImport("sl.interposer.dll",
        "slSetD3DDevice", reinterpret_cast<void*>(&HookSlSetD3DDevice), original);
    if (!installed)
    {
        installed = HookMainExecutableImport("sl.common.dll",
            "slSetD3DDevice", reinterpret_cast<void*>(&HookSlSetD3DDevice), original);
    }
    if (original)
    {
        gOriginalSetD3DDevice.store(
            reinterpret_cast<PFun_slSetD3DDevice*>(original),
            std::memory_order_release);
    }
    return installed;
}

bool InstallVulkanInfoHook()
{
    std::unique_lock installLock(gHookInstallMutex, std::try_to_lock);
    if (!installLock.owns_lock())
        return gInterposerDetoursInstalled.load(std::memory_order_acquire);
    if (gInterposerDetoursInstalled.load(std::memory_order_acquire)
        && gDetourSlSetVulkanInfo)
        return true;
    void* original = nullptr;
    bool installed = HookMainExecutableImport("sl.interposer.dll",
        "slSetVulkanInfo", reinterpret_cast<void*>(&HookSlSetVulkanInfo), original);
    if (!installed)
    {
        installed = HookMainExecutableImport("sl.common.dll",
            "slSetVulkanInfo", reinterpret_cast<void*>(&HookSlSetVulkanInfo), original);
    }
    if (original)
    {
        gOriginalSetVulkanInfo.store(
            reinterpret_cast<PFun_slSetVulkanInfo*>(original),
            std::memory_order_release);
    }
    return installed;
}

bool InstallSetDataHook()
{
    std::unique_lock installLock(gHookInstallMutex, std::try_to_lock);
    if (!installLock.owns_lock())
        return gInterposerDetoursInstalled.load(std::memory_order_acquire);
    if (gInterposerDetoursInstalled.load(std::memory_order_acquire)
        && gDetourSlSetData)
        return true;
    void* original = nullptr;
    bool installed = HookMainExecutableImport("sl.interposer.dll",
        "slSetData", reinterpret_cast<void*>(&HookSlSetData), original);
    if (!installed)
    {
        installed = HookMainExecutableImport("sl.common.dll",
            "slSetData", reinterpret_cast<void*>(&HookSlSetData), original);
    }
    if (original)
    {
        gOriginalSlSetData.store(
            reinterpret_cast<PFun_slSetData*>(original),
            std::memory_order_release);
    }
    return installed;
}



bool InstallUiTagHooks()
{
    std::unique_lock installLock(gHookInstallMutex, std::try_to_lock);
    if (!installLock.owns_lock())
        return gUiTagHookInstalled.load(std::memory_order_acquire);
    void* legacyOriginal = nullptr;
    bool legacyInstalled = gInterposerDetoursInstalled.load(std::memory_order_acquire)
        && gDetourSlSetTag;
    if (!legacyInstalled)
        legacyInstalled = HookMainExecutableImport("sl.interposer.dll",
        "slSetTag", reinterpret_cast<void*>(&HookSlSetTag), legacyOriginal);
    if (!legacyInstalled)
    {
        legacyInstalled = HookMainExecutableImport("sl.common.dll",
            "slSetTag", reinterpret_cast<void*>(&HookSlSetTag), legacyOriginal);
    }
    if (legacyOriginal)
    {
        gOriginalSetTag.store(reinterpret_cast<PFun_slSetTag*>(legacyOriginal),
            std::memory_order_release);
    }

    void* frameOriginal = nullptr;
    bool frameInstalled = gInterposerDetoursInstalled.load(std::memory_order_acquire)
        && gDetourSlSetTagForFrame;
    if (!frameInstalled)
        frameInstalled = HookMainExecutableImport("sl.interposer.dll",
        "slSetTagForFrame", reinterpret_cast<void*>(&HookSlSetTagForFrame), frameOriginal);
    if (!frameInstalled)
    {
        frameInstalled = HookMainExecutableImport("sl.common.dll",
            "slSetTagForFrame", reinterpret_cast<void*>(&HookSlSetTagForFrame), frameOriginal);
    }
    if (frameOriginal)
    {
        gOriginalSetTagForFrame.store(
            reinterpret_cast<PFun_slSetTagForFrame*>(frameOriginal),
            std::memory_order_release);
    }

    const bool installed = legacyInstalled || frameInstalled;
    gUiTagHookInstalled.store(installed, std::memory_order_release);
    return installed;
}

struct PatternPatch
{
    const wchar_t* label;
    const uint8_t* pattern;
    size_t patternSize;
    size_t patchOffset;
    const uint8_t* original;
    const uint8_t* replacement;
    size_t patchSize;
};

static constexpr std::array<uint8_t, 10> kWrapperPattern{
    0xBA, 0x05, 0x00, 0x00, 0x00, 0x3B, 0xCA, 0x0F, 0x42, 0xD1
};
static constexpr std::array<uint8_t, 3> kWrapperOriginal{ 0x0F, 0x42, 0xD1 };
static constexpr std::array<uint8_t, 3> kWrapperReplacement{ 0x90, 0x90, 0x90 };
static const PatternPatch kWrapperPatch{
    L"Streamline maximum", kWrapperPattern.data(), kWrapperPattern.size(), 7,
    kWrapperOriginal.data(), kWrapperReplacement.data(), kWrapperOriginal.size()
};

static constexpr std::array<uint8_t, 13> kNgxPattern{
    0x84, 0xD2, 0x0F, 0x84, 0x03, 0x01, 0x00, 0x00, 0xBE, 0x05, 0x00, 0x00, 0x00
};
static constexpr std::array<uint8_t, 6> kNgxOriginal{ 0x0F, 0x84, 0x03, 0x01, 0x00, 0x00 };
static constexpr std::array<uint8_t, 6> kNgxReplacement{ 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
static const PatternPatch kNgxPatch{
    L"NGX device support", kNgxPattern.data(), kNgxPattern.size(), 2,
    kNgxOriginal.data(), kNgxReplacement.data(), kNgxOriginal.size()
};

struct PatternPatchResult
{
    bool candidate = false;
    bool patched = false;
    uint8_t* match = nullptr;
};

bool ContainsCI(const wchar_t* str, const wchar_t* sub) noexcept
{
    if (!str || !sub || !*sub) return false;
    const size_t subLen = wcslen(sub);
    const size_t strLen = wcslen(str);
    if (strLen < subLen) return false;
    for (size_t i = 0; i <= strLen - subLen; ++i)
    {
        if (_wcsnicmp(str + i, sub, subLen) == 0)
            return true;
    }
    return false;
}

bool IsTargetModule(const wchar_t* moduleName, const wchar_t* fullPath)
{
    if (moduleName)
    {
        if (ContainsCI(moduleName, L"dlss")
            || ContainsCI(moduleName, L"nvngx")
            || ContainsCI(moduleName, L"interposer")
            || ContainsCI(moduleName, L"sl.")
            || ContainsCI(moduleName, L"sl_")
            || ContainsCI(moduleName, L"nvapi")
            || ContainsCI(moduleName, L"d3d12")
            || ContainsCI(moduleName, L"dxgi"))
            return true;
    }

    if (fullPath)
    {
        if (ContainsCI(fullPath, L"\\models\\")
            || ContainsCI(fullPath, L"\\dlssg\\")
            || ContainsCI(fullPath, L"sl_dlss_g")
            || ContainsCI(fullPath, L"nvngx_dlssg"))
            return true;
    }
    return false;
}

const IMAGE_NT_HEADERS64* ImageHeaders(HMODULE module)
{
    if (!module)
        return nullptr;
    __try
    {
        const auto* base = reinterpret_cast<const uint8_t*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0
            || static_cast<size_t>(dos->e_lfanew) > 1024 * 1024)
            return nullptr;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE
            || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            return nullptr;
        return nt;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

bool RvaRangeIsValid(const IMAGE_NT_HEADERS64* nt, DWORD rva, size_t size)
{
    return nt && rva < nt->OptionalHeader.SizeOfImage
        && size <= static_cast<size_t>(nt->OptionalHeader.SizeOfImage - rva);
}

bool ModuleExportsFunction(HMODULE module, const char* expected)
{
    const auto* nt = ImageHeaders(module);
    if (!nt || !expected)
        return false;

    __try
    {
        const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!directory.VirtualAddress
            || !RvaRangeIsValid(nt, directory.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY)))
            return false;

        const auto* base = reinterpret_cast<const uint8_t*>(module);
        const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
            base + directory.VirtualAddress);
        const size_t namesSize = static_cast<size_t>(exports->NumberOfNames) * sizeof(DWORD);
        if (!exports->AddressOfNames
            || !RvaRangeIsValid(nt, exports->AddressOfNames, namesSize))
            return false;

        const auto* names = reinterpret_cast<const DWORD*>(base + exports->AddressOfNames);
        for (DWORD index = 0; index < exports->NumberOfNames; ++index)
        {
            const DWORD nameRva = names[index];
            if (!RvaRangeIsValid(nt, nameRva, 1))
                continue;
            const char* name = reinterpret_cast<const char*>(base + nameRva);
            const size_t remaining = nt->OptionalHeader.SizeOfImage - nameRva;
            const size_t length = strnlen_s(name, remaining);
            if (length < remaining && strcmp(name, expected) == 0)
                return true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return false;
}

PatternPatchResult PatchUniqueExecutablePattern(
    HMODULE module, const std::wstring& path, const PatternPatch& patch)
{
    const auto* base = reinterpret_cast<const uint8_t*>(module);
    const auto* nt = ImageHeaders(module);
    if (!nt)
        return {};

    uint8_t* match = nullptr;
    size_t matchCount = 0;
    __try
    {
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
                continue;
            auto* begin = const_cast<uint8_t*>(base + section->VirtualAddress);
            if (section->VirtualAddress >= nt->OptionalHeader.SizeOfImage)
                continue;
            const size_t available = nt->OptionalHeader.SizeOfImage - section->VirtualAddress;
            const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));
            if (size < patch.patternSize)
                continue;
            const size_t suffixOffset = patch.patchOffset + patch.patchSize;
            for (size_t offset = 0; offset + patch.patternSize <= size; ++offset)
            {
                const bool prefixMatches = patch.patchOffset == 0
                    || memcmp(begin + offset, patch.pattern, patch.patchOffset) == 0;
                const bool suffixMatches = suffixOffset == patch.patternSize
                    || memcmp(begin + offset + suffixOffset, patch.pattern + suffixOffset,
                        patch.patternSize - suffixOffset) == 0;
                const auto* candidate = begin + offset + patch.patchOffset;
                const bool patchBytesMatch = memcmp(candidate, patch.original, patch.patchSize) == 0
                    || memcmp(candidate, patch.replacement, patch.patchSize) == 0;
                if (prefixMatches && suffixMatches && patchBytesMatch)
                {
                    match = begin + offset;
                    ++matchCount;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return {};
    }

    if (matchCount == 0)
        return {};
    if (matchCount != 1 || !match)
    {
        Log(L"%s: expected one code pattern, found %zu: %s", patch.label, matchCount, path.c_str());
        return {true, false, nullptr};
    }

    uint8_t* address = match + patch.patchOffset;
    if (memcmp(address, patch.replacement, patch.patchSize) == 0)
    {
        Log(L"%s: already patched at RVA 0x%zX: %s", patch.label,
            static_cast<size_t>(address - const_cast<uint8_t*>(base)), path.c_str());
        return {true, true, match};
    }
    if (memcmp(address, patch.original, patch.patchSize) != 0)
    {
        Log(L"%s: matched context but original bytes differ: %s", patch.label, path.c_str());
        return {true, false, match};
    }

    DWORD oldProtection = 0;
    if (!VirtualProtect(address, patch.patchSize, PAGE_EXECUTE_READWRITE, &oldProtection))
    {
        Log(L"%s: VirtualProtect failed (%lu): %s", patch.label, GetLastError(), path.c_str());
        return {true, false, match};
    }
    memcpy(address, patch.replacement, patch.patchSize);
    FlushInstructionCache(GetCurrentProcess(), address, patch.patchSize);
    DWORD ignoredProtection = 0;
    const BOOL restored = VirtualProtect(address, patch.patchSize, oldProtection, &ignoredProtection);
    if (!restored)
    {
        Log(L"%s: protection restore failed (%lu): %s", patch.label, GetLastError(), path.c_str());
        return {true, false, match};
    }

    Log(L"%s: patched RVA 0x%zX: %s", patch.label,
        static_cast<size_t>(address - const_cast<uint8_t*>(base)), path.c_str());
    return {true, true, match};
}

std::wstring LoadedModulePath(HMODULE module)
{
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(module, path, _countof(path));
    return length > 0 && length < _countof(path)
        ? std::wstring(path, length) : std::wstring{};
}

void RecomputeModuleStateLocked()
{
    uint32_t wrapperCandidates = 0;
    uint32_t patchedWrappers = 0;
    uint32_t ngxCandidates = 0;
    uint32_t patchedNgx = 0;
    uint32_t wrapperRouteBits = 0;
    uint32_t ngxRouteBits = 0;
    for (const auto& record : gModuleRecords)
    {
        if (record.wrapperCandidate)
            ++wrapperCandidates;
        if (record.wrapperPatched)
        {
            ++patchedWrappers;
            wrapperRouteBits |= ClassifyLoadedRoute(record.path);
        }
        if (record.ngxCandidate)
            ++ngxCandidates;
        const bool temporalOk = record.ngxTemporalPatched
            || midpoint_fix::Ready()
            || IsHarnessEnvironment();
        if (record.ngxPatched && temporalOk)
        {
            ++patchedNgx;
            ngxRouteBits |= ClassifyLoadedRoute(record.path);
        }
    }
    gLoadedWrapperCandidates.store(wrapperCandidates, std::memory_order_release);
    gPatchedWrapperCandidates.store(patchedWrappers, std::memory_order_release);
    gLoadedNgxCandidates.store(ngxCandidates, std::memory_order_release);
    gPatchedNgxCandidates.store(patchedNgx, std::memory_order_release);
    gWrapperRouteBits.store(wrapperRouteBits, std::memory_order_release);
    gNgxRouteBits.store(ngxRouteBits, std::memory_order_release);
}

void LogModuleInventory(const ModuleRecord& record)
{
    if (!record.wrapperExport && !record.ngxExport)
        return;
    Log(L"Loaded module: base=%p wrapperExport=%d wrapperCandidate=%d wrapperPatched=%d "
        L"ngxExport=%d ngxCandidate=%d ngxPatched=%d midpointPatched=%d path=%s",
        record.module, record.wrapperExport, record.wrapperCandidate, record.wrapperPatched,
        record.ngxExport, record.ngxCandidate, record.ngxPatched,
        record.ngxTemporalPatched,
        record.path.c_str());
}

bool PatchStreamlineFlipMetering(HMODULE module, const wchar_t* path)
{
    if (!module) return false;
    const auto* nt = ImageHeaders(module);
    if (!nt) return false;

    constexpr char kFlipMarker[] = "FG1 DLL has been detected";
    constexpr size_t kMarkerLen = sizeof(kFlipMarker) - 1;
    auto* base = reinterpret_cast<uint8_t*>(module);

    const uint8_t* marker = nullptr;
    unsigned int wantOffset = 0;
    int wantValue = -1;
    size_t sitesPatched = 0;

    __try
    {
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections && !marker; ++i, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_READ) == 0) continue;
            uint8_t* start = base + section->VirtualAddress;
            if (section->VirtualAddress >= nt->OptionalHeader.SizeOfImage) continue;
            const size_t available = nt->OptionalHeader.SizeOfImage - section->VirtualAddress;
            const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));
            if (size < kMarkerLen) continue;
            for (size_t off = 0; off + kMarkerLen <= size; ++off)
            {
                if (memcmp(start + off, kFlipMarker, kMarkerLen) == 0)
                {
                    marker = start + off;
                    break;
                }
            }
        }
        if (!marker) return false;

        section = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections && wantValue < 0; ++i, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
            uint8_t* start = base + section->VirtualAddress;
            if (section->VirtualAddress >= nt->OptionalHeader.SizeOfImage) continue;
            const size_t available = nt->OptionalHeader.SizeOfImage - section->VirtualAddress;
            const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));
            if (size < 8) continue;
            for (size_t off = 0; off + 8 <= size && wantValue < 0; ++off)
            {
                if (!(start[off] == 0x48 || start[off] == 0x4C)) continue;
                if (start[off + 1] != 0x8D) continue;
                if ((start[off + 2] & 0xC7) != 0x05) continue;
                int disp = 0;
                memcpy(&disp, start + off + 3, sizeof(disp));
                if (start + off + 7 + disp != marker) continue;

                const size_t window = 0x200;
                const size_t limit = (off + window < size) ? (off + window) : size;
                for (size_t w = off; w + 7 <= limit; ++w)
                {
                    if (start[w] != 0xC6) continue;
                    if (start[w + 1] < 0x80 || start[w + 1] > 0xBF) continue;
                    unsigned int field = 0;
                    memcpy(&field, start + w + 2, sizeof(field));
                    const uint8_t imm = start[w + 6];
                    if (field <= 0x100 || field >= 0x20000) continue;
                    if (imm > 1) continue;
                    wantOffset = field;
                    wantValue = imm;
                    break;
                }
            }
        }

        if (wantValue < 0)
        {
            Log(L"Located DLSS-G plugin (%s) but could not extract flip metering state", path ? path : L"");
            return false;
        }

        const uint8_t opposite = static_cast<uint8_t>(1 - wantValue);
        section = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
            uint8_t* start = base + section->VirtualAddress;
            if (section->VirtualAddress >= nt->OptionalHeader.SizeOfImage) continue;
            const size_t available = nt->OptionalHeader.SizeOfImage - section->VirtualAddress;
            const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));
            if (size < 7) continue;
            for (size_t off = 0; off + 7 <= size; ++off)
            {
                // C6 /0 disp32 imm8 form
                if (start[off] == 0xC6)
                {
                    if (start[off + 1] < 0x80 || start[off + 1] > 0xBF) continue;
                    unsigned int field = 0;
                    memcpy(&field, start + off + 2, sizeof(field));
                    if (field != wantOffset) continue;
                    if (start[off + 6] != opposite) continue;

                    DWORD oldProtect = 0;
                    if (VirtualProtect(start + off + 6, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
                    {
                        start[off + 6] = static_cast<uint8_t>(wantValue);
                        DWORD ignored = 0;
                        VirtualProtect(start + off + 6, 1, oldProtect, &ignored);
                        FlushInstructionCache(GetCurrentProcess(), start + off + 6, 1);
                        ++sitesPatched;
                    }
                    continue;
                }

                // 40 88 /r disp32 form
                if (start[off] == 0x40 && start[off + 1] == 0x88)
                {
                    const uint8_t modrm = start[off + 2];
                    if (modrm < 0x80 || modrm > 0xBF) continue;
                    const uint8_t rm = static_cast<uint8_t>(modrm & 7);
                    if (rm == 4) continue;
                    unsigned int field = 0;
                    memcpy(&field, start + off + 3, sizeof(field));
                    if (field != wantOffset) continue;

                    uint8_t replacement[7] = {
                        0xC6, static_cast<uint8_t>(0x80 | rm), 0, 0, 0, 0, static_cast<uint8_t>(wantValue)
                    };
                    memcpy(replacement + 2, &wantOffset, sizeof(wantOffset));

                    DWORD oldProtect = 0;
                    if (VirtualProtect(start + off, 7, PAGE_EXECUTE_READWRITE, &oldProtect))
                    {
                        memcpy(start + off, replacement, 7);
                        DWORD ignored = 0;
                        VirtualProtect(start + off, 7, oldProtect, &ignored);
                        FlushInstructionCache(GetCurrentProcess(), start + off, 7);
                        ++sitesPatched;
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (sitesPatched > 0)
    {
        gFlipMeteringOffset.store(wantOffset, std::memory_order_relaxed);
        gFlipMeteringValue.store(static_cast<uint32_t>(wantValue), std::memory_order_relaxed);
        gFlipMeteringPatched.store(true, std::memory_order_release);
        Log(L"Streamline FlipMetering: forced software pacing (RSYNC) in %s (+0x%X pinned to %d at %zu sites)",
            path ? path : L"", wantOffset, wantValue, sitesPatched);
        return true;
    }
    return gFlipMeteringPatched.load(std::memory_order_relaxed);
}

bool PatchStreamlineCeilingClamp(HMODULE module, const wchar_t* path)
{
    if (!module) return false;
    const auto* nt = ImageHeaders(module);
    if (!nt) return false;
    auto* base = reinterpret_cast<uint8_t*>(module);

    const uint8_t prefix[] = {0x3B, 0xCA, 0x0F, 0x42};
    __try
    {
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
            uint8_t* start = base + section->VirtualAddress;
            if (section->VirtualAddress >= nt->OptionalHeader.SizeOfImage) continue;
            const size_t available = nt->OptionalHeader.SizeOfImage - section->VirtualAddress;
            const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));
            if (size < 10) continue;
            for (size_t off = 0; off + 10 <= size; ++off)
            {
                if (start[off] != 0xBA) continue;
                if (start[off + 2] != 0 || start[off + 3] != 0 || start[off + 4] != 0) continue;
                if (memcmp(start + off + 5, prefix, sizeof(prefix)) != 0) continue;
                const uint8_t ceiling = start[off + 1];
                if (ceiling == 0 || ceiling > 8) continue;

                const uint8_t lastByte = start[off + 9];
                if (lastByte == 0xD2 || lastByte == 0x90)
                {
                    gAdvertisedMaxGenerated.store(std::max<uint32_t>(ceiling, static_cast<uint32_t>(kExperimentalMaximumGeneratedFrames)), std::memory_order_release);
                    return true;
                }

                if (lastByte == 0xD1)
                {
                    DWORD oldProtect = 0;
                    if (VirtualProtect(start + off, 10, PAGE_EXECUTE_READWRITE, &oldProtect))
                    {
                        start[off + 1] = static_cast<uint8_t>(kExperimentalMaximumGeneratedFrames);
                        start[off + 9] = 0xD2;
                        DWORD ignored = 0;
                        VirtualProtect(start + off, 10, oldProtect, &ignored);
                        FlushInstructionCache(GetCurrentProcess(), start + off, 10);
                        gAdvertisedMaxGenerated.store(kExperimentalMaximumGeneratedFrames, std::memory_order_release);
                        Log(L"Streamline ceiling bypass applied in %s (compiled max=%ux, cmovb bypassed, ceiling raised to %ux)",
                            path ? path : L"", ceiling + 1, kExperimentalMaximumGeneratedFrames + 1);
                        return true;
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return false;
}

bool PatchStreamlineUiRecomposition(HMODULE module, const wchar_t* path)
{
    if (!module) return false;
    const auto* nt = ImageHeaders(module);
    if (!nt) return false;
    auto* base = reinterpret_cast<uint8_t*>(module);

    const uint8_t pattern[] = {
        0x80, 0xB8, 0xCB, 0x46, 0x00, 0x00, 0x00,
        0x74, 0x0D,
        0x80, 0xB8, 0xCA, 0x46, 0x00, 0x00, 0x00,
        0x0F, 0x95, 0xC0,
        0x88, 0x47, 0x70
    };

    const uint8_t replacement[22] = {
        0x8A, 0x42, 0x6C,
        0x88, 0x47, 0x70,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };

    bool patchedAny = false;
    __try
    {
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
            uint8_t* start = base + section->VirtualAddress;
            if (section->VirtualAddress >= nt->OptionalHeader.SizeOfImage) continue;
            const size_t available = nt->OptionalHeader.SizeOfImage - section->VirtualAddress;
            const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));

            if (size >= sizeof(pattern))
            {
                for (size_t off = 0; off + sizeof(pattern) <= size; ++off)
                {
                    if (memcmp(start + off, replacement, 6) == 0)
                    {
                        patchedAny = true;
                        break;
                    }
                    if (memcmp(start + off, pattern, sizeof(pattern)) == 0)
                    {
                        DWORD oldProtect = 0;
                        if (VirtualProtect(start + off, sizeof(replacement), PAGE_EXECUTE_READWRITE, &oldProtect))
                        {
                            memcpy(start + off, replacement, sizeof(replacement));
                            DWORD ignored = 0;
                            VirtualProtect(start + off, sizeof(replacement), oldProtect, &ignored);
                            FlushInstructionCache(GetCurrentProcess(), start + off, sizeof(replacement));
                            Log(L"Streamline UIR synchronization patch applied in %s (syncing [rdi+0x70] with options.uir)",
                                path ? path : L"");
                            patchedAny = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return patchedAny;
}

#pragma pack(push, 1)
struct StreamlineDynamicMfgParams
{
    uint32_t numFramesToGenerate;       // 0x00
    uint32_t numFramesToGenerate2;      // 0x04
    uint32_t multiplier;                // 0x08
    uint32_t reserved0C;                // 0x0C
    uint64_t reserved10;                // 0x10
    uint32_t reflexParam;               // 0x18
    uint8_t  flag1C;                    // 0x1C
    uint8_t  reserved1D[3];             // 0x1D
    uint64_t reserved20;                // 0x20
    uint64_t reserved28;                // 0x28
    float    dynamicTargetFrameRate;    // 0x30
    uint32_t reserved34;                // 0x34
    uint64_t frameTimeUs;               // 0x38
    uint32_t reserved40;                // 0x40
    uint32_t maxFramesToGenerate;       // 0x44
    uint64_t reserved48;                // 0x48
    uint8_t  flag50;                    // 0x50
};
#pragma pack(pop)

typedef void (*PFun_StreamlineDynamicMfgCalculator)(void* context, StreamlineDynamicMfgParams* params);
static PFun_StreamlineDynamicMfgCalculator gOriginalDynamicMfgCalculator = nullptr;
static std::atomic<bool> gCalcHooked{ false };

void HookedDynamicMfgCalculator(void* context, StreamlineDynamicMfgParams* params)
{
    if (!params)
    {
        if (gOriginalDynamicMfgCalculator)
            gOriginalDynamicMfgCalculator(context, params);
        return;
    }

    if (gOriginalDynamicMfgCalculator)
        gOriginalDynamicMfgCalculator(context, params);

    // If Reflex 2 driver SDK is present and functioning (flag50 != 0), let driver pacing handle it
    if (params->flag50 != 0)
        return;

    // High-precision smooth pacing engine for software / Reflex 1 fallback
    static LARGE_INTEGER sLastQpc{};
    static LARGE_INTEGER sQpcFreq{};
    LARGE_INTEGER nowQpc{};
    QueryPerformanceCounter(&nowQpc);
    if (sQpcFreq.QuadPart == 0)
        QueryPerformanceFrequency(&sQpcFreq);

    uint64_t qpcDeltaUs = 0;
    if (sLastQpc.QuadPart != 0 && nowQpc.QuadPart > sLastQpc.QuadPart && sQpcFreq.QuadPart > 0)
    {
        qpcDeltaUs = static_cast<uint64_t>(
            (nowQpc.QuadPart - sLastQpc.QuadPart) * 1000000 / sQpcFreq.QuadPart);
    }
    sLastQpc = nowQpc;

    uint64_t sampleUs = params->frameTimeUs;
    // Fall back to render-thread QPC interval if Reflex didn't provide delta or delta is anomalous
    if (sampleUs < 1000 || sampleUs > 500000)
    {
        sampleUs = qpcDeltaUs;
    }
    if (sampleUs < 1000 || sampleUs > 500000)
    {
        sampleUs = 16666; // 60 FPS fallback
    }

    static double sSmoothedFrameTimeUs = 0.0;
    static uint32_t sCurrentStableMultiplier = 0;
    static uint32_t sFramesSinceLastSwitch = 0;
    constexpr uint32_t kMinSwitchCooldownFrames = 40;

    if (sSmoothedFrameTimeUs <= 0.0)
    {
        sSmoothedFrameTimeUs = static_cast<double>(sampleUs);
    }
    else
    {
        // Smooth exponential moving average (alpha = 0.06, ~16 frames half-life)
        constexpr double kAlpha = 0.06;
        sSmoothedFrameTimeUs = (1.0 - kAlpha) * sSmoothedFrameTimeUs + kAlpha * static_cast<double>(sampleUs);
    }

    float targetFps = params->dynamicTargetFrameRate;
    if (targetFps <= 0.0f)
    {
        const auto snapshot = ReadControlSnapshot();
        if (snapshot.control.dynamicTargetFrameRate > 0)
            targetFps = static_cast<float>(snapshot.control.dynamicTargetFrameRate);
        else
        {
            DEVMODEW dm{};
            dm.dmSize = sizeof(dm);
            if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 30)
                targetFps = static_cast<float>(dm.dmDisplayFrequency);
            else
                targetFps = 120.0f;
        }
    }

    const double baseFps = 1000000.0 / sSmoothedFrameTimeUs;
    const double idealMultiplier = static_cast<double>(targetFps) / baseFps;

    uint32_t maxAllowedFrames = params->maxFramesToGenerate;
    if (maxAllowedFrames < 1) maxAllowedFrames = 1;
    const auto snapshot = ReadControlSnapshot();
    if (!snapshot.control.dynamicExperimental56 && maxAllowedFrames > 3)
    {
        maxAllowedFrames = 3;
    }
    const uint32_t maxAllowedMultiplier = maxAllowedFrames + 1;

    uint32_t desiredMultiplier = sCurrentStableMultiplier == 0
        ? static_cast<uint32_t>(std::round(idealMultiplier))
        : sCurrentStableMultiplier;

    // Hysteresis deadband (+- 0.35) to prevent oscillation around rounding thresholds
    if (idealMultiplier > static_cast<double>(desiredMultiplier) + 0.35)
    {
        desiredMultiplier = static_cast<uint32_t>(std::round(idealMultiplier));
    }
    else if (idealMultiplier < static_cast<double>(desiredMultiplier) - 0.35)
    {
        desiredMultiplier = static_cast<uint32_t>(std::round(idealMultiplier));
    }

    desiredMultiplier = std::clamp(desiredMultiplier, 2u, maxAllowedMultiplier);

    ++sFramesSinceLastSwitch;
    if (sCurrentStableMultiplier == 0)
    {
        sCurrentStableMultiplier = desiredMultiplier;
        sFramesSinceLastSwitch = kMinSwitchCooldownFrames;
    }
    else if (desiredMultiplier != sCurrentStableMultiplier)
    {
        const bool emergencyScaleUp = idealMultiplier > static_cast<double>(sCurrentStableMultiplier) + 1.25;
        if (sFramesSinceLastSwitch >= kMinSwitchCooldownFrames || emergencyScaleUp)
        {
            Log(L"[DMFG-PACER] Stable transition: %ux -> %ux (baseFps=%.1f, smoothedTime=%.1fms, targetFps=%.0f, emergency=%d)",
                sCurrentStableMultiplier, desiredMultiplier,
                baseFps, sSmoothedFrameTimeUs / 1000.0, targetFps, emergencyScaleUp ? 1 : 0);
            sCurrentStableMultiplier = desiredMultiplier;
            sFramesSinceLastSwitch = 0;
        }
    }

    const uint32_t finalFrames = sCurrentStableMultiplier - 1;
    params->numFramesToGenerate = finalFrames;
    params->numFramesToGenerate2 = finalFrames;
    params->multiplier = sCurrentStableMultiplier;
    params->flag50 = 0;
    if (context)
    {
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(context) + 0x4078) = finalFrames;
    }
}

bool PatchStreamlineDynamicMfgSupported(HMODULE module, const wchar_t* path)
{
    if (!module) return false;
    const auto* nt = ImageHeaders(module);
    if (!nt) return false;
    auto* base = reinterpret_cast<uint8_t*>(module);

    struct DynamicMfgSite
    {
        const wchar_t* name;
        const uint8_t* pattern;
        const uint8_t* replacement;
        size_t size;
        bool patched = false;
        bool newlyPatched = false;
    };

    // Site 1: checkDynamicMFGSupport capability check (RVA 0x3c6e0)
    // 48 83 EC 58 80 B9 0C 45 00 00 00 -> sub rsp, 0x58; cmp byte ptr [rcx+0x450c], 0
    // Replaced with: B0 01 C3 90 90 90 90 90 90 90 90 -> mov al, 1; ret; nop...
    static const uint8_t kCapPattern[] = {
        0x48, 0x83, 0xEC, 0x58, 0x80, 0xB9, 0x0C, 0x45, 0x00, 0x00, 0x00
    };
    static const uint8_t kCapReplacement[] = {
        0xB0, 0x01, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };

    // Site 2: Runtime Dispatch Gate in dlfg evaluate loop (RVA 0x4725c)
    // 41 80 BE 10 46 00 00 00 74 0F 48 8D -> cmp [r14+0x4610], 0; je +0x0F (skips 0x464a0 calculator); lea rdx, [rbp+0x6d0]
    // Replaced with: 41 80 BE 10 46 00 00 00 90 90 48 8D -> nop nop instead of je +0x0F
    static const uint8_t kDispatchPattern[] = {
        0x41, 0x80, 0xBE, 0x10, 0x46, 0x00, 0x00, 0x00, 0x74, 0x0F, 0x48, 0x8D
    };
    static const uint8_t kDispatchReplacement[] = {
        0x41, 0x80, 0xBE, 0x10, 0x46, 0x00, 0x00, 0x00, 0x90, 0x90, 0x48, 0x8D
    };

    // Site 3: Mode Validator Gate in validateOptions (RVA 0x4bc17)
    // 40 38 B1 10 46 00 00 0F 85 6F 01 00 00 -> cmp [rcx+0x4610], sil; jne +0x16F (success)
    // Replaced with: 40 38 B1 10 46 00 00 E9 70 01 00 00 90 -> jmp +0x170 (unconditional success); nop
    static const uint8_t kValidatorPattern[] = {
        0x40, 0x38, 0xB1, 0x10, 0x46, 0x00, 0x00, 0x0F, 0x85, 0x6F, 0x01, 0x00, 0x00
    };
    static const uint8_t kValidatorReplacement[] = {
        0x40, 0x38, 0xB1, 0x10, 0x46, 0x00, 0x00, 0xE9, 0x70, 0x01, 0x00, 0x00, 0x90
    };

    // Site 4: State Advertising Gate in slDLSSGGetState (RVA 0x57c5a)
    // 45 38 BE 10 46 00 00 0F 95 C0 88 47 50 -> cmp [r14+0x4610], r15b; setne al; mov [rdi+0x50], al
    // Replaced with: 45 38 BE 10 46 00 00 B0 01 90 88 47 50 -> mov al, 1; nop
    static const uint8_t kStatePattern[] = {
        0x45, 0x38, 0xBE, 0x10, 0x46, 0x00, 0x00, 0x0F, 0x95, 0xC0, 0x88, 0x47, 0x50
    };
    static const uint8_t kStateReplacement[] = {
        0x45, 0x38, 0xBE, 0x10, 0x46, 0x00, 0x00, 0xB0, 0x01, 0x90, 0x88, 0x47, 0x50
    };

    // Site 5: Max Frames Validation Gate in slSetData (RVA 0x5839b)
    // 45 8B 8F 0C 46 00 00 45 3B C1 0F 86 BA 00 00 00 -> mov r9d, [r15+0x460c]; cmp r8d, r9d; jbe +0xBA
    // Replaced with: 45 8B 8F 0C 46 00 00 45 3B C1 E9 BB 00 00 00 90 -> unconditional jmp +0xBB; nop (never reject 5x/6x with 0x26)
    static const uint8_t kMaxFramesPattern[] = {
        0x45, 0x8B, 0x8F, 0x0C, 0x46, 0x00, 0x00,
        0x45, 0x3B, 0xC1,
        0x0F, 0x86, 0xBA, 0x00, 0x00, 0x00
    };
    static const uint8_t kMaxFramesReplacement[] = {
        0x45, 0x8B, 0x8F, 0x0C, 0x46, 0x00, 0x00,
        0x45, 0x3B, 0xC1,
        0xE9, 0xBB, 0x00, 0x00, 0x00, 0x90
    };

    DynamicMfgSite sites[] = {
        { L"Capability Gate (checkDynamicMFGSupport -> true)", kCapPattern, kCapReplacement, sizeof(kCapPattern) },
        { L"Runtime Dispatch Gate (unblocked dynamic calculation)", kDispatchPattern, kDispatchReplacement, sizeof(kDispatchPattern) },
        { L"Mode Validator Gate (unblocked eDynamic validation)", kValidatorPattern, kValidatorReplacement, sizeof(kValidatorPattern) },
        { L"State Advertising Gate (forced state.bIsDynamicMFGSupported = 1)", kStatePattern, kStateReplacement, sizeof(kStatePattern) },
        { L"Max Frames Validation Gate (unblocked 5x/6x in slSetData)", kMaxFramesPattern, kMaxFramesReplacement, sizeof(kMaxFramesPattern) }
    };

    bool anyPatched = false;
    size_t newlyPatchedCount = 0;

    __try
    {
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
            uint8_t* start = base + section->VirtualAddress;
            if (section->VirtualAddress >= nt->OptionalHeader.SizeOfImage) continue;
            const size_t available = nt->OptionalHeader.SizeOfImage - section->VirtualAddress;
            const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));

            for (auto& site : sites)
            {
                if (site.patched || size < site.size) continue;
                for (size_t off = 0; off + site.size <= size; ++off)
                {
                    if (memcmp(start + off, site.replacement, site.size) == 0)
                    {
                        site.patched = true;
                        anyPatched = true;
                        break;
                    }
                    if (memcmp(start + off, site.pattern, site.size) == 0)
                    {
                        DWORD oldProtect = 0;
                        if (VirtualProtect(start + off, site.size, PAGE_EXECUTE_READWRITE, &oldProtect))
                        {
                            memcpy(start + off, site.replacement, site.size);
                            DWORD ignored = 0;
                            VirtualProtect(start + off, site.size, oldProtect, &ignored);
                            FlushInstructionCache(GetCurrentProcess(), start + off, site.size);
                            site.patched = true;
                            site.newlyPatched = true;
                            anyPatched = true;
                            newlyPatchedCount++;
                            Log(L"Streamline Dynamic MFG unlock: %s applied in %s",
                                site.name, path ? path : L"");
                            break;
                        }
                    }
                }
            }

            for (size_t off = 0; off + 15 <= size; ++off)
            {
                if (start[off] == 0x4C && start[off + 1] == 0x8B && start[off + 2] == 0x3D)
                {
                    static const uint8_t kContextTail[] = { 0x44, 0x8B, 0x47, 0x24, 0x41, 0x83, 0xF8, 0x01 };
                    if (memcmp(start + off + 7, kContextTail, sizeof(kContextTail)) == 0)
                    {
                        const int32_t disp = *reinterpret_cast<const int32_t*>(start + off + 3);
                        uint8_t** ppContext = reinterpret_cast<uint8_t**>(start + off + 7 + disp);
                        gStreamlineDlssgContextPtr.store(ppContext, std::memory_order_release);
                        SynchronizeStreamlineLiveContext();
                    }
                }
            }

            for (size_t off = 0; off + 16 <= size; ++off)
            {
                static const uint8_t kCalcPattern[] = {
                    0x48, 0x8B, 0xC4, 0x53, 0x57, 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x48, 0x89, 0x70, 0xE8
                };
                if (!gCalcHooked.load(std::memory_order_relaxed) &&
                    memcmp(start + off, kCalcPattern, sizeof(kCalcPattern)) == 0)
                {
                    void* target = start + off;
                    gOriginalDynamicMfgCalculator = reinterpret_cast<PFun_StreamlineDynamicMfgCalculator>(target);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    LONG status = DetourAttach(reinterpret_cast<void**>(&gOriginalDynamicMfgCalculator),
                                               reinterpret_cast<void*>(&HookedDynamicMfgCalculator));
                    if (status == NO_ERROR && DetourTransactionCommit() == NO_ERROR)
                    {
                        gCalcHooked.store(true, std::memory_order_release);
                        Log(L"Streamline Dynamic MFG smooth pacer hook installed at 0x%p in %s",
                            target, path ? path : L"");
                    }
                    else
                    {
                        DetourTransactionAbort();
                        Log(L"Streamline Dynamic MFG smooth pacer hook FAILED (status=%ld) at 0x%p in %s",
                            status, target, path ? path : L"");
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (newlyPatchedCount > 0)
    {
        size_t activeCount = 0;
        for (const auto& s : sites)
            if (s.patched) activeCount++;
        Log(L"Streamline Dynamic MFG unlock: %zu of %zu site(s) successfully unlocked in %s",
            activeCount, sizeof(sites) / sizeof(sites[0]), path ? path : L"");
    }

    return anyPatched;
}

static bool SafeScanDlssgArchSites(const uint8_t* base, const IMAGE_NT_HEADERS64* nt,
    uint8_t** outSites, size_t maxSites, size_t* outFound, size_t* outAlreadyPatched)
{
    if (!base || !nt || !outSites || !outFound || !outAlreadyPatched)
        return false;
    __try
    {
        constexpr uint8_t kArchOld = 0xB0;
        constexpr uint8_t kArchNew = 0x90;
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
            const uint8_t* start = base + section->VirtualAddress;
            if (section->VirtualAddress >= nt->OptionalHeader.SizeOfImage) continue;
            const size_t available = nt->OptionalHeader.SizeOfImage - section->VirtualAddress;
            const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));
            if (size < 6) continue;
            for (size_t off = 0; off + 6 <= size; ++off)
            {
                if (start[off] == 0x3D && start[off + 2] == 0x01 && start[off + 3] == 0x00 && start[off + 4] == 0x00)
                {
                    if (start[off + 1] == kArchOld && *outFound < maxSites)
                        outSites[(*outFound)++] = const_cast<uint8_t*>(start + off + 1);
                    else if (start[off + 1] == kArchNew)
                        ++(*outAlreadyPatched);
                    continue;
                }
                if (start[off] == 0x81 && start[off + 1] >= 0xF8 && start[off + 1] <= 0xFF
                    && start[off + 3] == 0x01 && start[off + 4] == 0x00 && start[off + 5] == 0x00)
                {
                    if (start[off + 2] == kArchOld && *outFound < maxSites)
                        outSites[(*outFound)++] = const_cast<uint8_t*>(start + off + 2);
                    else if (start[off + 2] == kArchNew)
                        ++(*outAlreadyPatched);
                    continue;
                }
                if (off + 7 <= size && (start[off] >= 0x40 && start[off] <= 0x4F)
                    && start[off + 1] == 0x81 && start[off + 2] >= 0xF8 && start[off + 2] <= 0xFF
                    && start[off + 4] == 0x01 && start[off + 5] == 0x00 && start[off + 6] == 0x00)
                {
                    if (start[off + 3] == kArchOld && *outFound < maxSites)
                        outSites[(*outFound)++] = const_cast<uint8_t*>(start + off + 3);
                    else if (start[off + 3] == kArchNew)
                        ++(*outAlreadyPatched);
                }
            }
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool PatchDlssgArchGates(HMODULE module, const wchar_t* path)
{
    if (!module) return false;
    const auto* nt = ImageHeaders(module);
    if (!nt) return false;
    auto* base = reinterpret_cast<uint8_t*>(module);

    constexpr uint8_t kArchNew = 0x90; // 0x190 AD10x (Ada)

    uint8_t* sites[64]{};
    size_t sitesFound = 0;
    size_t alreadyPatched = 0;
    if (!SafeScanDlssgArchSites(base, nt, sites, _countof(sites), &sitesFound, &alreadyPatched))
        return false;

    size_t written = 0;
    for (size_t i = 0; i < sitesFound; ++i)
    {
        uint8_t* site = sites[i];
        DWORD oldProtect = 0;
        if (VirtualProtect(site, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            *site = kArchNew;
            DWORD ignored = 0;
            VirtualProtect(site, 1, oldProtect, &ignored);
            FlushInstructionCache(GetCurrentProcess(), site, 1);
            ++written;
        }
    }

    if (written > 0)
    {
        Log(L"DLSS-G arch gates patched in %s: %zu site(s) rewrote 0x1b0 -> 0x190 (Ada)",
            path ? path : L"", written);
        return true;
    }
    return alreadyPatched > 0;
}

bool PatchDlssgHudlessUiRecomposition(HMODULE module, const wchar_t* path)
{
    if (!module) return false;
    const auto* nt = ImageHeaders(module);
    if (!nt) return false;
    auto* base = reinterpret_cast<uint8_t*>(module);

    // nvngx_dlssg.dll runtime UIR check at RVA 0x6180B:
    // 80 7D 32 00 4C 8B 74 24 48 48 8B 74 24 38 48 8B 5C 24 30 88 45 33 74 19
    // Followed by:
    // 84 C0 74 15 (test al, al; je 0x6183c - checks if separate UI/UIAlpha texture exists)
    // followed by:
    // 80 7D 37 00 74 0F (cmp byte ptr [rbp + 0x37], 0; je 0x6183c - checks if Preset supports UIR)
    // NOP out ONLY the first 4 bytes (test al, al; je 0x6183c) so HUDless-only games fall through,
    // but KEEP the Preset guard (cmp [rbp+0x37], 0; je) intact so Preset A cleanly turns UIR OFF!
    static const uint8_t kRuntimePrefix[24] = {
        0x80, 0x7D, 0x32, 0x00,
        0x4C, 0x8B, 0x74, 0x24, 0x48,
        0x48, 0x8B, 0x74, 0x24, 0x38,
        0x48, 0x8B, 0x5C, 0x24, 0x30,
        0x88, 0x45, 0x33,
        0x74, 0x19
    };
    static const uint8_t kRuntimeOriginal[4] = {
        0x84, 0xC0, 0x74, 0x15
    };
    static const uint8_t kRuntimeNops[4] = {
        0x90, 0x90, 0x90, 0x90
    };
    static const uint8_t kPresetGuard[6] = {
        0x80, 0x7D, 0x37, 0x00, 0x74, 0x0F
    };

    // nvngx_dlssg.dll create-time UIR check at RVA 0x37633:
    // 84 C0 74 02 B0 01 88 43 59 (test al, al; je +2; mov al, 1; mov [rbx+0x59], al)
    // NOP out the je +2 so [rbx+0x59] is unconditionally initialized to 1.
    static const uint8_t kCreateOriginal[9] = {
        0x84, 0xC0, 0x74, 0x02, 0xB0, 0x01, 0x88, 0x43, 0x59
    };

    bool patchedAny = false;
    __try
    {
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
            uint8_t* start = base + section->VirtualAddress;
            if (section->VirtualAddress >= nt->OptionalHeader.SizeOfImage) continue;
            const size_t available = nt->OptionalHeader.SizeOfImage - section->VirtualAddress;
            const size_t size = std::min<size_t>(available, static_cast<size_t>(section->Misc.VirtualSize));

            // Runtime UIR patch
            constexpr size_t kRuntimeTotal = sizeof(kRuntimePrefix) + sizeof(kRuntimeOriginal) + sizeof(kPresetGuard);
            if (size >= kRuntimeTotal)
            {
                for (size_t off = 0; off + kRuntimeTotal <= size; ++off)
                {
                    if (memcmp(start + off, kRuntimePrefix, sizeof(kRuntimePrefix)) == 0)
                    {
                        uint8_t* patchSite = start + off + sizeof(kRuntimePrefix);
                        // If already patched with 4 NOPs and preset guard is intact:
                        if (memcmp(patchSite, kRuntimeNops, sizeof(kRuntimeNops)) == 0
                            && memcmp(patchSite + sizeof(kRuntimeNops), kPresetGuard, sizeof(kPresetGuard)) == 0)
                        {
                            patchedAny = true;
                            break;
                        }
                        // If previously patched with 10 NOPs: restore the preset guard!
                        static const uint8_t kTenNops[10] = { 0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90 };
                        if (memcmp(patchSite, kTenNops, sizeof(kTenNops)) == 0)
                        {
                            DWORD oldProtect = 0;
                            if (VirtualProtect(patchSite + 4, sizeof(kPresetGuard), PAGE_EXECUTE_READWRITE, &oldProtect))
                            {
                                memcpy(patchSite + 4, kPresetGuard, sizeof(kPresetGuard));
                                DWORD ignored = 0;
                                VirtualProtect(patchSite + 4, sizeof(kPresetGuard), oldProtect, &ignored);
                                FlushInstructionCache(GetCurrentProcess(), patchSite + 4, sizeof(kPresetGuard));
                                Log(L"DLSS-G NGX runtime UIR preset guard restored in %s (Preset A compatibility active)",
                                    path ? path : L"");
                                patchedAny = true;
                                break;
                            }
                        }
                        if (memcmp(patchSite, kRuntimeOriginal, sizeof(kRuntimeOriginal)) == 0)
                        {
                            DWORD oldProtect = 0;
                            if (VirtualProtect(patchSite, sizeof(kRuntimeNops), PAGE_EXECUTE_READWRITE, &oldProtect))
                            {
                                memcpy(patchSite, kRuntimeNops, sizeof(kRuntimeNops));
                                DWORD ignored = 0;
                                VirtualProtect(patchSite, sizeof(kRuntimeNops), oldProtect, &ignored);
                                FlushInstructionCache(GetCurrentProcess(), patchSite, sizeof(kRuntimeNops));
                                Log(L"DLSS-G NGX runtime HUDless UIR patch applied in %s (unblocking UIR for HUDless while preserving Preset A guard)",
                                    path ? path : L"");
                                patchedAny = true;
                                break;
                            }
                        }
                    }
                }
            }

            // Create-time UIR patch
            if (size >= sizeof(kCreateOriginal))
            {
                for (size_t off = 0; off + sizeof(kCreateOriginal) <= size; ++off)
                {
                    if (start[off] == 0x84 && start[off + 1] == 0xC0
                        && start[off + 4] == 0xB0 && start[off + 5] == 0x01
                        && start[off + 6] == 0x88 && start[off + 7] == 0x43 && start[off + 8] == 0x59)
                    {
                        uint8_t* jumpSite = start + off + 2;
                        if (jumpSite[0] == 0x90 && jumpSite[1] == 0x90)
                        {
                            patchedAny = true;
                            break;
                        }
                        if (jumpSite[0] == 0x74 && jumpSite[1] == 0x02)
                        {
                            DWORD oldProtect = 0;
                            if (VirtualProtect(jumpSite, 2, PAGE_EXECUTE_READWRITE, &oldProtect))
                            {
                                jumpSite[0] = 0x90;
                                jumpSite[1] = 0x90;
                                DWORD ignored = 0;
                                VirtualProtect(jumpSite, 2, oldProtect, &ignored);
                                FlushInstructionCache(GetCurrentProcess(), jumpSite, 2);
                                Log(L"DLSS-G NGX create-time HUDless UIR patch applied in %s",
                                    path ? path : L"");
                                patchedAny = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return patchedAny;
}

using PFun_slInit = sl::Result(const sl::Preferences&, uint64_t);
PFun_slInit* gOriginalSlInit = nullptr;

sl::Result HookSlInit(const sl::Preferences& pref, uint64_t sdkVersion)
{
    if (!gOriginalSlInit)
        return sl::Result::eErrorNotInitialized;

    const bool forceOta = gConfigForceOta.load(std::memory_order_relaxed);
    sl::Preferences localPref = pref;
    const uint64_t before = static_cast<uint64_t>(pref.flags);
    constexpr uint64_t kOta = static_cast<uint64_t>(sl::PreferenceFlags::eAllowOTA) |
                              static_cast<uint64_t>(sl::PreferenceFlags::eLoadDownloadedPlugins);
    if (forceOta)
    {
        localPref.flags = static_cast<sl::PreferenceFlags>(before | kOta);
    }
    Log(L"[DBG] HookSlInit: calling original=%p forceOta=%d flags=0x%llX",
        gOriginalSlInit, (int)forceOta, before);
    sl::Result result = sl::Result::eErrorNotInitialized;
    __try
    {
        result = gOriginalSlInit(forceOta ? localPref : pref, sdkVersion);
    }
    __except (Log(L"[DBG] HookSlInit: EXCEPTION 0x%08lX at %p inside original slInit",
                  GetExceptionCode(),
                  GetExceptionInformation() ? GetExceptionInformation()->ExceptionRecord->ExceptionAddress : nullptr),
              EXCEPTION_CONTINUE_SEARCH)
    {
    }
    Log(L"slInit intercepted: flags 0x%llX%s result=%d",
        before, forceOta ? L" (forced OTA)" : L"", static_cast<int>(result));
    return result;
}

void InstallInterposerDetours()
{
    std::unique_lock installLock(gHookInstallMutex, std::try_to_lock);
    if (!installLock.owns_lock())
        return;
    if (gInterposerDetoursInstalled.load(std::memory_order_acquire))
        return;

    HMODULE interposer = GetModuleHandleW(L"sl.interposer.dll");
    if (!interposer)
        return;

    auto* getFeatureFn = reinterpret_cast<PFun_slGetFeatureFunction*>(
        GetProcAddress(interposer, "slGetFeatureFunction"));
    auto* initFn = reinterpret_cast<PFun_slInit*>(
        GetProcAddress(interposer, "slInit"));
    auto* setD3DDeviceFn = reinterpret_cast<PFun_slSetD3DDevice*>(
        GetProcAddress(interposer, "slSetD3DDevice"));
    auto* vulkanInfoFn = reinterpret_cast<PFun_slSetVulkanInfo*>(
        GetProcAddress(interposer, "slSetVulkanInfo"));
    auto* setDataFn = reinterpret_cast<PFun_slSetData*>(
        GetProcAddress(interposer, "slSetData"));
    auto* setConstantsFn = reinterpret_cast<PFun_slSetConstants*>(
        GetProcAddress(interposer, "slSetConstants"));
    auto* setTagFn = reinterpret_cast<PFun_slSetTag*>(
        GetProcAddress(interposer, "slSetTag"));
    auto* setTagForFrameFn = reinterpret_cast<PFun_slSetTagForFrame*>(
        GetProcAddress(interposer, "slSetTagForFrame"));

    if (!getFeatureFn && !initFn && !setD3DDeviceFn && !vulkanInfoFn && !setDataFn && !setConstantsFn && !setTagFn && !setTagForFrameFn)
        return;

    const LONG beginStatus = DetourTransactionBegin();
    if (beginStatus != NO_ERROR)
    {
        Log(L"Deferred Streamline hook installation: transaction unavailable (%ld)", beginStatus);
        return;
    }
    LONG attachStatus = DetourUpdateThread(GetCurrentThread());
    const auto attach = [&](void** target, void* hook) {
        if (attachStatus == NO_ERROR)
            attachStatus = DetourAttach(target, hook);
    };
    if (getFeatureFn)
    {
        gDetourSlGetFeatureFunction = getFeatureFn;
        attach(reinterpret_cast<void**>(&gDetourSlGetFeatureFunction),
                     reinterpret_cast<void*>(&HookSlGetFeatureFunction));
    }
    if (initFn)
    {
        gOriginalSlInit = initFn;
        attach(reinterpret_cast<void**>(&gOriginalSlInit),
                     reinterpret_cast<void*>(&HookSlInit));
    }
    if (setD3DDeviceFn)
    {
        gDetourSlSetD3DDevice = setD3DDeviceFn;
        attach(reinterpret_cast<void**>(&gDetourSlSetD3DDevice),
                     reinterpret_cast<void*>(&HookSlSetD3DDevice));
    }
    if (vulkanInfoFn)
    {
        gDetourSlSetVulkanInfo = vulkanInfoFn;
        attach(reinterpret_cast<void**>(&gDetourSlSetVulkanInfo),
                     reinterpret_cast<void*>(&HookSlSetVulkanInfo));
    }
    if (setDataFn)
    {
        gDetourSlSetData = setDataFn;
        attach(reinterpret_cast<void**>(&gDetourSlSetData),
                     reinterpret_cast<void*>(&HookSlSetData));
    }
    if (setConstantsFn)
    {
        gDetourSlSetConstants = setConstantsFn;
        attach(reinterpret_cast<void**>(&gDetourSlSetConstants),
                     reinterpret_cast<void*>(&HookSlSetConstants));
    }
    if (setTagFn)
    {
        gDetourSlSetTag = setTagFn;
        attach(reinterpret_cast<void**>(&gDetourSlSetTag),
                     reinterpret_cast<void*>(&HookSlSetTag));
    }
    if (setTagForFrameFn)
    {
        gDetourSlSetTagForFrame = setTagForFrameFn;
        attach(reinterpret_cast<void**>(&gDetourSlSetTagForFrame),
                     reinterpret_cast<void*>(&HookSlSetTagForFrame));
    }
    LONG status = attachStatus;
    if (status == NO_ERROR)
        status = DetourTransactionCommit();
    else
        DetourTransactionAbort();
    if (status == NO_ERROR)
    {
        if (gDetourSlGetFeatureFunction)
            gOriginalGetFeatureFunction.store(gDetourSlGetFeatureFunction, std::memory_order_release);
        if (gDetourSlSetD3DDevice)
            gOriginalSetD3DDevice.store(gDetourSlSetD3DDevice, std::memory_order_release);
        if (gDetourSlSetVulkanInfo)
            gOriginalSetVulkanInfo.store(gDetourSlSetVulkanInfo, std::memory_order_release);
        if (gDetourSlSetData)
            gOriginalSlSetData.store(gDetourSlSetData, std::memory_order_release);
        if (gDetourSlSetConstants)
            gOriginalSlSetConstants.store(gDetourSlSetConstants, std::memory_order_release);
        if (gDetourSlSetTag)
        {
            gOriginalSetTag.store(gDetourSlSetTag, std::memory_order_release);
            gUiTagHookInstalled.store(true, std::memory_order_release);
        }
        if (gDetourSlSetTagForFrame)
        {
            gOriginalSetTagForFrame.store(gDetourSlSetTagForFrame, std::memory_order_release);
            gUiTagHookInstalled.store(true, std::memory_order_release);
        }
        // Publish success only after every original points at its trampoline.
        gLiveHookInstalled.store(true, std::memory_order_release);
        gInterposerDetoursInstalled.store(true, std::memory_order_release);
        Log(L"Direct Detours on sl.interposer.dll installed successfully (slGetFeatureFunction, slInit, slSetD3DDevice, slSetVulkanInfo, slSetData, slSetConstants, slSetTag, slSetTagForFrame)");
    }
    else
    {
        Log(L"Failed to install direct Detours on sl.interposer.dll (status=%ld)", status);
    }
}

void UninstallInterposerDetours()
{
    if (!gInterposerDetoursInstalled.load(std::memory_order_acquire))
        return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (gDetourSlGetFeatureFunction)
        DetourDetach(reinterpret_cast<void**>(&gDetourSlGetFeatureFunction),
                     reinterpret_cast<void*>(&HookSlGetFeatureFunction));
    if (gOriginalSlInit)
        DetourDetach(reinterpret_cast<void**>(&gOriginalSlInit),
                     reinterpret_cast<void*>(&HookSlInit));
    if (gDetourSlSetD3DDevice)
        DetourDetach(reinterpret_cast<void**>(&gDetourSlSetD3DDevice),
                     reinterpret_cast<void*>(&HookSlSetD3DDevice));
    if (gDetourSlSetVulkanInfo)
        DetourDetach(reinterpret_cast<void**>(&gDetourSlSetVulkanInfo),
                     reinterpret_cast<void*>(&HookSlSetVulkanInfo));
    if (gDetourSlSetData)
        DetourDetach(reinterpret_cast<void**>(&gDetourSlSetData),
                     reinterpret_cast<void*>(&HookSlSetData));
    if (gDetourSlSetConstants)
        DetourDetach(reinterpret_cast<void**>(&gDetourSlSetConstants),
                     reinterpret_cast<void*>(&HookSlSetConstants));
    if (gDetourSlSetTag)
        DetourDetach(reinterpret_cast<void**>(&gDetourSlSetTag),
                     reinterpret_cast<void*>(&HookSlSetTag));
    if (gDetourSlSetTagForFrame)
        DetourDetach(reinterpret_cast<void**>(&gDetourSlSetTagForFrame),
                     reinterpret_cast<void*>(&HookSlSetTagForFrame));
    DetourTransactionCommit();
    gInterposerDetoursInstalled.store(false, std::memory_order_release);
}

#include "nvapi_motion_trace.h"
#include "provider_dispatch_trace.h"

using PFun_NvAPI_QueryInterface = void*(__stdcall*)(unsigned int InterfaceId);
PFun_NvAPI_QueryInterface gRealNvAPI_QueryInterface = nullptr;
PFun_NvAPI_QueryInterface gRealNvAPIImpl_QueryInterface = nullptr;
std::atomic<bool> gNvApiImplHookInstalled{false};
std::atomic<bool> gNvApiHookInstalled{false};

void* __stdcall HookNvAPI_QueryInterface(unsigned int interfaceId)
{
    static std::atomic<bool> seen{false};
    if (!seen.exchange(true)) Log(L"[KERNEL-TRACE] public resolver reached");
    constexpr unsigned int kNvAPI_D3D12_SetFlipConfig = 0xf3148c42;
    if (interfaceId == kNvAPI_D3D12_SetFlipConfig && gConfigPatchFlipMetering.load(std::memory_order_relaxed))
    {
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true))
            Log(L"NVAPI: NvAPI_D3D12_SetFlipConfig (0xF3148C42) query intercepted -> returning nullptr (OptiScaler FlipMetering bypass)");
        return nullptr;
    }
    if (gRealNvAPI_QueryInterface)
        return scatter_experiment::kMode == 0
            ? nvapi_motion_trace::Intercept(interfaceId, gRealNvAPI_QueryInterface(interfaceId))
            : gRealNvAPI_QueryInterface(interfaceId);
    return nullptr;
}

void* __stdcall HookNvAPIImpl_QueryInterface(unsigned int interfaceId)
{
    static std::atomic<bool> seen{false};
    if (!seen.exchange(true)) Log(L"[KERNEL-TRACE] implementation resolver reached");
    if (!gRealNvAPIImpl_QueryInterface) return nullptr;
    return nvapi_motion_trace::Intercept(interfaceId, gRealNvAPIImpl_QueryInterface(interfaceId));
}

void InstallNvApiHook()
{
    std::unique_lock installLock(gHookInstallMutex, std::try_to_lock);
    if (!installLock.owns_lock())
        return;
    HMODULE nvapi = GetModuleHandleW(L"nvapi64.dll");
    if (!nvapi)
        return;

    auto* queryInterface = reinterpret_cast<PFun_NvAPI_QueryInterface>(
        GetProcAddress(nvapi, "nvapi_QueryInterface"));
    if (!queryInterface)
        return;

    // The public runtime can forward to a separately exported implementation
    // resolver. Each detour needs its own original; sharing a trampoline here
    // would recurse or forward a call through the wrong entry point.
    const HMODULE impl = GetModuleHandleW(L"nvapi64_impl.dll");
    const auto implQuery = impl ? reinterpret_cast<PFun_NvAPI_QueryInterface>(
        GetProcAddress(impl, "nvapi_QueryInterface")) : nullptr;
    if (scatter_experiment::kMode == 0 && implQuery && implQuery != queryInterface
        && !gNvApiImplHookInstalled.load(std::memory_order_acquire))
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        gRealNvAPIImpl_QueryInterface = implQuery;
        DetourAttach(reinterpret_cast<void**>(&gRealNvAPIImpl_QueryInterface),
            reinterpret_cast<void*>(&HookNvAPIImpl_QueryInterface));
        const LONG result = DetourTransactionCommit();
        if (result == NO_ERROR)
            gNvApiImplHookInstalled.store(true, std::memory_order_release);
        else
            gRealNvAPIImpl_QueryInterface = nullptr;
        Log(L"[KERNEL-TRACE] implementation resolver hook result=%ld entry=%p", result, implQuery);
    }
    if (gNvApiHookInstalled.load(std::memory_order_acquire)) return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    gRealNvAPI_QueryInterface = queryInterface;
    DetourAttach(reinterpret_cast<void**>(&gRealNvAPI_QueryInterface),
                 reinterpret_cast<void*>(&HookNvAPI_QueryInterface));
    if (DetourTransactionCommit() == NO_ERROR)
    {
        gNvApiHookInstalled.store(true, std::memory_order_release);
        Log(L"NVAPI: Hooked nvapi_QueryInterface (OptiScaler FlipMetering bypass: %s)",
            gConfigPatchFlipMetering.load(std::memory_order_relaxed) ? L"ENABLED" : L"DISABLED");
    }
}

void UninstallNvApiHook()
{
    if (!gNvApiHookInstalled.load(std::memory_order_acquire)
        && !gNvApiImplHookInstalled.load(std::memory_order_acquire)
        && !provider_dispatch_trace::original) return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (gNvApiHookInstalled.load(std::memory_order_acquire) && gRealNvAPI_QueryInterface)
        DetourDetach(reinterpret_cast<void**>(&gRealNvAPI_QueryInterface),
                     reinterpret_cast<void*>(&HookNvAPI_QueryInterface));
    if (gNvApiImplHookInstalled.load(std::memory_order_acquire) && gRealNvAPIImpl_QueryInterface)
        DetourDetach(reinterpret_cast<void**>(&gRealNvAPIImpl_QueryInterface),
            reinterpret_cast<void*>(&HookNvAPIImpl_QueryInterface));
    if (provider_dispatch_trace::original)
        DetourDetach(reinterpret_cast<void**>(&provider_dispatch_trace::original),
            reinterpret_cast<void*>(&provider_dispatch_trace::Hook));
    if (DetourTransactionCommit() == NO_ERROR)
    {
        gNvApiHookInstalled.store(false, std::memory_order_release);
        gNvApiImplHookInstalled.store(false, std::memory_order_release);
    }
}

using PFun_LoadLibraryW = HMODULE(WINAPI*)(LPCWSTR);
using PFun_LoadLibraryExW = HMODULE(WINAPI*)(LPCWSTR, HANDLE, DWORD);

PFun_LoadLibraryW gRealLoadLibraryW = nullptr;
PFun_LoadLibraryExW gRealLoadLibraryExW = nullptr;
std::atomic<bool> gLoadHooksInstalled{false};

ModuleRecord InspectLoadedModule(HMODULE module, const std::wstring& suppliedPath, bool forceWrapper = false);
void OnPotentialModuleLoaded(HMODULE module, LPCWSTR name);

HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpLibFileName)
{
    const DWORD incomingError = GetLastError();
    const bool trace = lpLibFileName && reinterpret_cast<uintptr_t>(lpLibFileName) >= 0x10000
        && IsTargetModule(lpLibFileName, lpLibFileName);
    if (trace) Log(L"[LOAD] LoadLibraryW ENTER: %s", lpLibFileName);
    SetLastError(incomingError);
    HMODULE mod = gRealLoadLibraryW(lpLibFileName);
    const DWORD loadError = GetLastError();
    if (trace) Log(L"[LOAD] LoadLibraryW RETURN: base=%p error=%lu path=%s",
        mod, mod ? 0 : loadError, lpLibFileName);
    if (mod && lpLibFileName && reinterpret_cast<uintptr_t>(lpLibFileName) >= 0x10000)
    {
        if (IsTargetModule(lpLibFileName, lpLibFileName))
        {
            __try
            {
                OnPotentialModuleLoaded(mod, lpLibFileName);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                Log(L"[LOAD] SEH exception 0x%08lX during OnPotentialModuleLoaded for %s",
                    GetExceptionCode(), lpLibFileName);
            }
        }
    }
    if (trace) Log(L"[LOAD] LoadLibraryW inspection complete: base=%p", mod);
    SetLastError(loadError);
    return mod;
}

HMODULE WINAPI HookLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    const DWORD incomingError = GetLastError();
    const bool trace = lpLibFileName && reinterpret_cast<uintptr_t>(lpLibFileName) >= 0x10000
        && IsTargetModule(lpLibFileName, lpLibFileName);
    if (trace) Log(L"[LOAD] LoadLibraryExW ENTER: flags=0x%lX path=%s", dwFlags, lpLibFileName);
    SetLastError(incomingError);
    HMODULE mod = gRealLoadLibraryExW(lpLibFileName, hFile, dwFlags);
    const DWORD loadError = GetLastError();
    if (trace) Log(L"[LOAD] LoadLibraryExW RETURN: base=%p error=%lu path=%s",
        mod, mod ? 0 : loadError, lpLibFileName);
    constexpr DWORD kDataOnly = LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;
    if (mod && lpLibFileName && reinterpret_cast<uintptr_t>(lpLibFileName) >= 0x10000 && (dwFlags & kDataOnly) == 0)
    {
        if (IsTargetModule(lpLibFileName, lpLibFileName))
        {
            __try
            {
                OnPotentialModuleLoaded(mod, lpLibFileName);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                Log(L"[LOAD] SEH exception 0x%08lX during OnPotentialModuleLoaded for %s",
                    GetExceptionCode(), lpLibFileName);
            }
        }
    }
    if (trace) Log(L"[LOAD] LoadLibraryExW inspection complete: base=%p", mod);
    SetLastError(loadError);
    return mod;
}

void InstallLoadLibraryHooks()
{
    std::unique_lock installLock(gHookInstallMutex, std::try_to_lock);
    if (!installLock.owns_lock())
        return;
    if (gLoadHooksInstalled.load(std::memory_order_acquire))
        return;

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32)
        return;

    gRealLoadLibraryW = reinterpret_cast<PFun_LoadLibraryW>(GetProcAddress(kernel32, "LoadLibraryW"));
    gRealLoadLibraryExW = reinterpret_cast<PFun_LoadLibraryExW>(GetProcAddress(kernel32, "LoadLibraryExW"));
    if (!gRealLoadLibraryW || !gRealLoadLibraryExW)
        return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<void**>(&gRealLoadLibraryW), reinterpret_cast<void*>(&HookLoadLibraryW));
    DetourAttach(reinterpret_cast<void**>(&gRealLoadLibraryExW), reinterpret_cast<void*>(&HookLoadLibraryExW));
    if (DetourTransactionCommit() == NO_ERROR)
    {
        gLoadHooksInstalled.store(true, std::memory_order_release);
        Log(L"Early LoadLibraryW/ExW hooks installed on kernel32.dll");
    }
}

void UninstallLoadLibraryHooks()
{
    if (!gLoadHooksInstalled.load(std::memory_order_acquire))
        return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (gRealLoadLibraryW)
        DetourDetach(reinterpret_cast<void**>(&gRealLoadLibraryW), reinterpret_cast<void*>(&HookLoadLibraryW));
    if (gRealLoadLibraryExW)
        DetourDetach(reinterpret_cast<void**>(&gRealLoadLibraryExW), reinterpret_cast<void*>(&HookLoadLibraryExW));
    DetourTransactionCommit();
    gLoadHooksInstalled.store(false, std::memory_order_release);
}

void OnPotentialModuleLoaded(HMODULE module, LPCWSTR name)
{
    if (!module || !name || reinterpret_cast<uintptr_t>(name) < 0x10000) return;
    if (!IsTargetModule(name, name)) return;
    if (ContainsCI(name, L"nvapi"))
    {
        InstallNvApiHook();
    }
    if (ContainsCI(name, L"sl.interposer"))
    {
        InstallInterposerDetours();
        InstallFeatureFunctionHook();
        InstallD3DDeviceHook();
        InstallVulkanInfoHook();
        InstallSetDataHook();
        InstallUiTagHooks();
    }
    if (ContainsCI(name, L"nvngx_dlssg")
        || ContainsCI(name, L"\\models\\dlssg\\")
        || ContainsCI(name, L"sl.dlss_g")
        || ContainsCI(name, L"sl_dlss_g_"))
    {
        gModuleInventoryDirty.store(true, std::memory_order_release);
        InspectLoadedModule(module, name);
    }
}

static bool SafePatchProvider(HMODULE module, const wchar_t* path) noexcept
{
    __try
    {
        return midpoint_fix::PatchProvider(module, path);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log(L"D157 midpoint fix: SEH exception 0x%08lX during PatchProvider in %s",
            GetExceptionCode(), path ? path : L"");
        return false;
    }
}

ModuleRecord InspectLoadedModule(HMODULE module, const std::wstring& suppliedPath, bool forceWrapper)
{
    if (!module)
        return {};
    const std::wstring path = suppliedPath.empty() ? LoadedModulePath(module) : suppliedPath;
    ModuleRecord snapshot{};
    bool logInventory = false;
    {
        std::lock_guard lock(gModuleMutex);
        const auto existing = std::find_if(gModuleRecords.begin(), gModuleRecords.end(),
            [&](const ModuleRecord& record) {
                return record.module == module;
            });
        if (existing != gModuleRecords.end())
        {
            if (forceWrapper && !existing->wrapperExport)
            {
                existing->wrapperExport = true;
                const PatternPatchResult result =
                    PatchUniqueExecutablePattern(module, path, kWrapperPatch);
                existing->wrapperCandidate = result.candidate;
                existing->wrapperPatched = result.patched;
                if (result.patched && result.match)
                {
                    existing->wrapperMaximumImmediate = result.match + 1;
                    SetWrapperMaximum(*existing, kExperimentalMaximumGeneratedFrames);
                }
                const bool ceilingPatched = PatchStreamlineCeilingClamp(module, path.c_str());
                PatchStreamlineUiRecomposition(module, path.c_str());
                const bool dmfgPatched = PatchStreamlineDynamicMfgSupported(module, path.c_str());
                existing->wrapperCandidate = existing->wrapperCandidate || ceilingPatched || dmfgPatched;
                existing->wrapperPatched = existing->wrapperPatched || ceilingPatched || dmfgPatched;
                RecomputeModuleStateLocked();
            }
            if (existing->ngxPatched && !existing->ngxTemporalPatched)
            {
                existing->ngxTemporalPatched = SafePatchProvider(module, path.c_str());
                if (existing->ngxTemporalPatched)
                    RecomputeModuleStateLocked();
            }
            if (existing->ngxExport || existing->ngxPatched)
            {
                PatchDlssgHudlessUiRecomposition(module, path.c_str());
            }
            if (gLogReady.load(std::memory_order_acquire) && !existing->inventoryLogged)
            {
                existing->inventoryLogged = true;
                logInventory = true;
            }
            snapshot = *existing;
        }
        else
        {
            ModuleRecord record{};
            record.module = module;
            record.path = path;
            const bool hasDlssgName = ContainsCI(path.c_str(), L"dlss_g") || ContainsCI(path.c_str(), L"sl.dlss_g");
            record.wrapperExport =
                forceWrapper
                || (hasDlssgName && (ModuleExportsFunction(module, "slGetPluginFunction") || ModuleExportsFunction(module, "slGetFeatureFunction")))
                || ModuleExportsFunction(module, "slDLSSGGetState")
                || ((ModuleExportsFunction(module, "slGetFeatureFunction")
                        || ModuleExportsFunction(module, "slSetData")
                        || ModuleExportsFunction(module, "slSetConstants"))
                    && ModuleExportsFunction(module, "slDLSSGGetState"));
            record.ngxExport =
                dlssg_provider_policy::IsDlssgImplementationModule(module)
                && (ModuleExportsFunction(module, "NVSDK_NGX_D3D12_CreateFeature")
                    || ModuleExportsFunction(module, "NVSDK_NGX_VULKAN_CreateFeature")
                    || ModuleExportsFunction(module, "NVSDK_NGX_VULKAN_CreateFeature1"))
                && ModuleExportsFunction(module, "NVSDK_NGX_GetGPUArchitecture");
            if (!record.wrapperExport && !record.ngxExport)
                return record;
            if (record.wrapperExport)
            {


                const PatternPatchResult result =
                    PatchUniqueExecutablePattern(module, path, kWrapperPatch);
                record.wrapperCandidate = result.candidate;
                record.wrapperPatched = result.patched;
                if (result.patched && result.match)
                {
                    record.wrapperMaximumImmediate = result.match + 1;
                    SetWrapperMaximum(record, kExperimentalMaximumGeneratedFrames);
                }
                const bool ceilingPatched = PatchStreamlineCeilingClamp(module, path.c_str());
                PatchStreamlineUiRecomposition(module, path.c_str());
                const bool dmfgPatched = PatchStreamlineDynamicMfgSupported(module, path.c_str());
                record.wrapperCandidate = record.wrapperCandidate || ceilingPatched || dmfgPatched;
                record.wrapperPatched = record.wrapperPatched || ceilingPatched || dmfgPatched;
            }
            if (record.ngxExport)
            {
                const bool archGatesPatched = PatchDlssgArchGates(module, path.c_str());
                PatchDlssgHudlessUiRecomposition(module, path.c_str());
                const PatternPatchResult result =
                    PatchUniqueExecutablePattern(module, path, kNgxPatch);
                record.ngxCandidate = result.candidate || archGatesPatched;
                record.ngxPatched = result.patched || archGatesPatched;
                if (record.ngxPatched || archGatesPatched)
                {
                    record.ngxTemporalPatched =
                        SafePatchProvider(module, path.c_str());
                }
            }
            record.inventoryLogged = gLogReady.load(std::memory_order_acquire);
            logInventory = record.inventoryLogged;
            gModuleRecords.push_back(record);
            RecomputeModuleStateLocked();
            snapshot = record;
        }
    }
    if (logInventory)
        LogModuleInventory(snapshot);
    if (snapshot.ngxTemporalPatched && scatter_experiment::kMode == 0) provider_dispatch_trace::Install(module);
    return snapshot;
}

void FlushModuleInventoryToLog()
{
    std::vector<ModuleRecord> records;
    {
        std::lock_guard lock(gModuleMutex);
        for (auto& record : gModuleRecords)
        {
            if (!record.inventoryLogged)
            {
                record.inventoryLogged = true;
                records.push_back(record);
            }
        }
    }
    for (const auto& record : records)
        LogModuleInventory(record);
}

void RemoveLoadedModule(HMODULE module)
{
    if (!module)
        return;
    {
        std::lock_guard lock(gModuleMutex);
        gModuleRecords.erase(std::remove_if(gModuleRecords.begin(), gModuleRecords.end(),
            [&](const ModuleRecord& record) { return record.module == module; }),
            gModuleRecords.end());
        RecomputeModuleStateLocked();
    }
    const uintptr_t base = reinterpret_cast<uintptr_t>(module);
    if (gActiveWrapperBase.load(std::memory_order_acquire) == base)
    {
        const bool haveOtherWrapper = gPatchedWrapperCandidates.load(std::memory_order_relaxed) > 0;
        gActiveWrapperPatched.store(haveOtherWrapper, std::memory_order_release);
        gActiveWrapperObserved.store(haveOtherWrapper, std::memory_order_release);
        gActiveWrapperBase.store(0, std::memory_order_release);
    }
}

void InspectAlreadyLoadedModules()
{
    static std::vector<HMODULE> previousDiagnosticModules;
    std::vector<HMODULE> diagnosticModules;
    if (!gInterposerDetoursInstalled.load(std::memory_order_acquire) && GetModuleHandleW(L"sl.interposer.dll"))
    {
        InstallInterposerDetours();
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        Log(L"Could not enumerate loaded modules (%lu)", GetLastError());
        return;
    }

    std::vector<HMODULE> loadedModules;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry))
    {
        do
        {
            HMODULE observed = reinterpret_cast<HMODULE>(entry.modBaseAddr);
            diagnosticModules.push_back(observed);
            if (std::find(previousDiagnosticModules.begin(), previousDiagnosticModules.end(), observed)
                == previousDiagnosticModules.end())
                crash_diagnostics::RecordModule(observed, entry.modBaseSize, entry.szExePath);
            if (!IsTargetModule(entry.szModule, entry.szExePath))
            {
                entry.dwSize = sizeof(entry);
                continue;
            }
            HMODULE module = reinterpret_cast<HMODULE>(entry.modBaseAddr);
            loadedModules.push_back(module);
            InspectLoadedModule(module, entry.szExePath);
            entry.dwSize = sizeof(entry);
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    std::vector<HMODULE> removedModules;
    {
        std::lock_guard lock(gModuleMutex);
        for (const ModuleRecord& record : gModuleRecords)
        {
            HMODULE test = nullptr;
            if (!GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(record.module), &test) || test != record.module)
            {
                removedModules.push_back(record.module);
            }
        }
    }
    for (HMODULE module : removedModules)
        RemoveLoadedModule(module);

    previousDiagnosticModules = std::move(diagnosticModules);
}

void ObserveActiveWrapperProvider(void* function)
{
    if (!function)
        return;
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(function, &memory, sizeof(memory)) != sizeof(memory)
        || !memory.AllocationBase)
        return;

    HMODULE module = static_cast<HMODULE>(memory.AllocationBase);
    const ModuleRecord record = InspectLoadedModule(module, LoadedModulePath(module), true);
    if (!record.wrapperExport)
        return;

    const uintptr_t base = reinterpret_cast<uintptr_t>(module);
    const uintptr_t previous = gActiveWrapperBase.exchange(base, std::memory_order_acq_rel);
    const bool isPatched = record.wrapperPatched || (gPatchedWrapperCandidates.load(std::memory_order_relaxed) > 0);
    gActiveWrapperPatched.store(isPatched, std::memory_order_release);
    gActiveWrapperObserved.store(true, std::memory_order_release);
    if (previous != base)
        Log(L"Active DLSS-G wrapper provider: patched=%d (effective=%d) path=%s",
            record.wrapperPatched, isPatched ? 1 : 0, record.path.c_str());
}

struct MfgLdrDllLoadedNotificationData
{
    ULONG flags;
    const UNICODE_STRING* fullDllName;
    const UNICODE_STRING* baseDllName;
    PVOID dllBase;
    ULONG sizeOfImage;
};

union MfgLdrDllNotificationData
{
    MfgLdrDllLoadedNotificationData loaded;
    MfgLdrDllLoadedNotificationData unloaded;
};

using MfgLdrDllNotificationFunction = void (CALLBACK*)(
    ULONG reason, const MfgLdrDllNotificationData* data, void* context);
using LdrRegisterDllNotificationFn = NTSTATUS (NTAPI*)(
    ULONG flags, MfgLdrDllNotificationFunction callback, void* context, void** cookie);

void CALLBACK OnDllNotification(
    ULONG reason, const MfgLdrDllNotificationData* data, void*)
{
    static constexpr ULONG kDllLoaded = 1;
    static constexpr ULONG kDllUnloaded = 2;
    if (data && (reason == kDllLoaded || reason == kDllUnloaded))
        gModuleInventoryDirty.store(true, std::memory_order_release);
}

bool RegisterDllNotification()
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto* registerNotification = ntdll ? reinterpret_cast<LdrRegisterDllNotificationFn>(
        GetProcAddress(ntdll, "LdrRegisterDllNotification")) : nullptr;
    if (!registerNotification)
        return false;

    void* cookie = nullptr;
    const NTSTATUS status = registerNotification(0, &OnDllNotification, nullptr, &cookie);
    const bool registered = status >= 0 && cookie != nullptr;
    gDllNotificationRegistered.store(registered, std::memory_order_release);
    return registered;
}

bool ProcessStandaloneHotkeys(ControlConfig& control)
{
    const bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    if (!ctrlPressed || !altPressed)
        return false;

    static uint64_t sLastHotkeyTick = 0;
    const uint64_t now = GetTickCount64();
    if (now - sLastHotkeyTick < 200)
        return false;

    bool changed = false;
    for (uint32_t mult = 2; mult <= 6; ++mult)
    {
        const int keyChar = '0' + mult;
        const int keyNumpad = VK_NUMPAD0 + mult;
        if ((GetAsyncKeyState(keyChar) & 0x8000) != 0 || (GetAsyncKeyState(keyNumpad) & 0x8000) != 0)
        {
            control.multiplier = mult;
            control.dynamic = false;
            changed = true;
            Log(L"[HOTKEY] Multiplier set to %ux (fixed mode)", mult);
            break;
        }
    }

    if (!changed && (GetAsyncKeyState(VK_PRIOR) & 0x8000) != 0) // PageUp
    {
        if (control.multiplier < 6)
            control.multiplier++;
        control.dynamic = false;
        changed = true;
        Log(L"[HOTKEY] Multiplier increased to %ux (fixed mode)", control.multiplier);
    }
    else if (!changed && (GetAsyncKeyState(VK_NEXT) & 0x8000) != 0) // PageDown
    {
        if (control.multiplier > 2)
            control.multiplier--;
        control.dynamic = false;
        changed = true;
        Log(L"[HOTKEY] Multiplier decreased to %ux (fixed mode)", control.multiplier);
    }
    else if (!changed && (GetAsyncKeyState('D') & 0x8000) != 0)
    {
        control.dynamic = !control.dynamic;
        if (control.dynamic && control.dynamicTargetFrameRate == 0)
            control.dynamicTargetFrameRate = 120;
        changed = true;
        if (control.dynamic)
        {
            Log(L"[HOTKEY] Mode toggled: DYNAMIC (target=%u FPS)",
                control.dynamicTargetFrameRate);
        }
        else
        {
            Log(L"[HOTKEY] Mode toggled: FIXED (multiplier=%ux)",
                control.multiplier);
        }
    }
    else if (!changed)
    {
        const bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        const uint32_t step = shiftPressed ? 1 : 5;

        const bool upPressed = (GetAsyncKeyState(VK_UP) & 0x8000) != 0
            || (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) != 0
            || (GetAsyncKeyState(VK_ADD) & 0x8000) != 0;
        const bool downPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0
            || (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) != 0
            || (GetAsyncKeyState(VK_SUBTRACT) & 0x8000) != 0;

        if (upPressed)
        {
            uint32_t newTarget = control.dynamicTargetFrameRate;
            if (newTarget == 0)
                newTarget = 60;
            else if (newTarget + step <= 1000)
                newTarget += step;
            else
                newTarget = 1000;

            if (newTarget != control.dynamicTargetFrameRate || !control.dynamic)
            {
                control.dynamicTargetFrameRate = newTarget;
                control.dynamic = true;
                changed = true;
                Log(L"[HOTKEY] Dynamic target FPS increased to %u FPS (dynamic mode enabled)",
                    control.dynamicTargetFrameRate);
            }
        }
        else if (downPressed)
        {
            uint32_t newTarget = control.dynamicTargetFrameRate;
            if (newTarget == 0)
                newTarget = 60;
            else if (newTarget > 30 + step)
                newTarget -= step;
            else
                newTarget = 30;

            if (newTarget != control.dynamicTargetFrameRate || !control.dynamic)
            {
                control.dynamicTargetFrameRate = newTarget;
                control.dynamic = true;
                changed = true;
                Log(L"[HOTKEY] Dynamic target FPS decreased to %u FPS (dynamic mode enabled)",
                    control.dynamicTargetFrameRate);
            }
        }
    }

    if (changed)
        sLastHotkeyTick = now;

    return changed;
}

DWORD WINAPI PatchWorker(void* context)
{
    const DWORD pid = GetCurrentProcessId();
    std::wstring logPath;
    if (!gLog)
    {
        wchar_t tempDirectory[MAX_PATH]{};
        DWORD tempLength = GetTempPathW(_countof(tempDirectory), tempDirectory);
        if (tempLength > 0 && tempLength < _countof(tempDirectory))
        {
            wchar_t logName[64]{};
            swprintf_s(logName, L"DLSSG-Transfusion-%lu.log", static_cast<unsigned long>(pid));
            logPath = JoinPath(tempDirectory, logName);
            gLog = _wfsopen(logPath.c_str(), L"w, ccs=UTF-8", _SH_DENYWR);
        }
    }
    gLogReady.store(gLog != nullptr, std::memory_order_release);

    const std::wstring mappingName = MfgUnlockObjectName(L"Status", pid);
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mappingName.c_str());
    auto* shared = mapping ? static_cast<MfgUnlockStatus*>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MfgUnlockStatus))) : nullptr;

    const std::wstring eventName = MfgUnlockObjectName(L"Ready", pid);
    HANDLE readyEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str());

    wchar_t executablePath[32768]{};
    GetModuleFileNameW(nullptr, executablePath, _countof(executablePath));
    const std::wstring executableDirectory = ParentPath(executablePath);
    gConfigPath = ResolveConfigPath(static_cast<HMODULE>(context), executableDirectory);

    const std::wstring cetDir = JoinPath(executableDirectory,
        L"plugins\\cyber_engine_tweaks\\mods\\RTX40MFG");
    if (IsDirectory(cetDir))
    {
        gStatusPath = JoinPath(cetDir, L"bridge_status.json");
        DeleteFileW(gStatusPath.c_str());
    }
    else
    {
        gStatusPath.clear();
        // Clean up any stale bridge_status.json in the config or game directory
        const std::wstring staleStatus = JoinPath(ParentPath(gConfigPath), L"bridge_status.json");
        if (IsRegularFile(staleStatus))
            DeleteFileW(staleStatus.c_str());
        const std::wstring staleExeStatus = JoinPath(executableDirectory, L"bridge_status.json");
        if (IsRegularFile(staleExeStatus))
            DeleteFileW(staleExeStatus.c_str());
    }
    gPerfCsvPath = JoinPath(ParentPath(gConfigPath), L"DLSSG-Transfusion_perf.csv");
    const ControlConfig initialControl = ReadInitialControl();
    StoreControl(initialControl);
    midpoint_fix::SetBlackwellTransfusionEnabled(gConfigBlackwellTransfusion.load(std::memory_order_relaxed));
    midpoint_fix::SetQualityFixEnabled(gConfigQualityFix.load(std::memory_order_relaxed));
    Log(L"Quality valid-warp fix requested=%d (provider-load setting; restart required to change)",
        gConfigQualityFix.load(std::memory_order_relaxed));
    midpoint_fix::SetMvDilationDisabled(gConfigDisableMvDilation.load(std::memory_order_relaxed));
    if (!IsRegularFile(gConfigPath))
    {
        if (WriteControlFile(gConfigPath, initialControl))
            Log(L"[CONFIG] Created default configuration file at: %s", gConfigPath.c_str());
    }
    FILETIME configWriteTime{};
    ReadLastWriteTime(gConfigPath, configWriteTime);
    Log(L"Initial control: mode=%s multiplier=%ux dynamicTarget=%u FPS "
        L"dynamicExperimental56=%d blackwellTransfusion=%d disableMenuDetection=%d disableMvDilation=%d forceUiRecomposition=%d logPerformance=%d; config: %s",
        initialControl.dynamic ? L"dynamic" : L"fixed", initialControl.multiplier,
        initialControl.dynamicTargetFrameRate, initialControl.dynamicExperimental56,
        gConfigBlackwellTransfusion.load(std::memory_order_relaxed),
        gConfigDisableMenuDetection.load(std::memory_order_relaxed),
        gConfigDisableMvDilation.load(std::memory_order_relaxed),
        gConfigForceUiRecomposition.load(std::memory_order_relaxed),
        gConfigLogPerformance.load(std::memory_order_relaxed),
        gConfigPath.c_str());

    Log(L"Patch worker started for PID %lu", static_cast<unsigned long>(pid));
    Log(L"Early DLL notification registered: %d",
        gDllNotificationRegistered.load(std::memory_order_acquire));
    const bool liveHookInstalled = InstallFeatureFunctionHook();
    gLiveHookInstalled.store(liveHookInstalled, std::memory_order_release);
    Log(L"Streamline feature-function interception installed: %d", liveHookInstalled);
    Log(L"Streamline D3D device interception installed: %d",
        InstallD3DDeviceHook());
    Log(L"Streamline Vulkan info interception installed: %d",
        InstallVulkanInfoHook());
    Log(L"Streamline SetData interception installed: %d",
        InstallSetDataHook());

    const bool uiTagHookInstalled = InstallUiTagHooks();
    Log(L"Streamline UI tag interception installed: %d", uiTagHookInstalled);
    InstallInterposerDetours();
    InstallNvApiHook();
    InspectAlreadyLoadedModules();
    FlushModuleInventoryToLog();
    Log(L"Loaded-module discovery initialized: ready=%d route=%hs wrappers=%u/%u ngx=%u/%u",
        BridgeReady(), PatchRouteName(),
        gPatchedWrapperCandidates.load(std::memory_order_relaxed),
        gLoadedWrapperCandidates.load(std::memory_order_relaxed),
        gPatchedNgxCandidates.load(std::memory_order_relaxed),
        gLoadedNgxCandidates.load(std::memory_order_relaxed));

    if (shared)
    {
        shared->magic = kMfgUnlockStatusMagic;
        shared->win32Error = liveHookInstalled ? ERROR_SUCCESS : ERROR_PROC_NOT_FOUND;
        shared->wrapperPatchCount = static_cast<LONG>(
            gPatchedWrapperCandidates.load(std::memory_order_relaxed));
        shared->ngxPatchCount = static_cast<LONG>(
            gPatchedNgxCandidates.load(std::memory_order_relaxed));
        if (!logPath.empty())
            wcsncpy_s(shared->logPath, logPath.c_str(), _TRUNCATE);
        InterlockedExchange(&shared->state,
            BridgeReady() ? 1 : liveHookInstalled ? 0 : -1);
    }
    if (readyEvent)
        SetEvent(readyEvent);

    if (shared)
        UnmapViewOfFile(shared);
    if (mapping)
        CloseHandle(mapping);
    if (readyEvent)
        CloseHandle(readyEvent);
    PublishPatchRoute();
    PublishLiveBridge(initialControl);
    ControlConfig activeControl = initialControl;
    if (!gStatusPath.empty())
    {
        if (!WriteBridgeStatus(activeControl, pid))
            Log(L"Could not publish CET bridge status file: %s", gStatusPath.c_str());
        else
            Log(L"[BRIDGE] CET status bridge active at: %s", gStatusPath.c_str());
    }
    else
    {
        Log(L"[BRIDGE] Standalone mode (CET IPC inactive; bridge diagnostics integrated into log)");
    }

    // CET writes config.json when the user changes the mode. Watch it off
    // the presenting thread and atomically publish changes for the SetOptions hook.
    uint32_t heartbeatTicks = 0;
    uint32_t diagnosticTicks = 0;
    const uint64_t diagnosticStart = GetTickCount64();
    uint32_t inventoryTicks = 0;
    bool previousReady = BridgeReady();
    std::string previousRoute = PatchRouteName();
    for (;;)
    {
        if (gStopWorker.load(std::memory_order_relaxed))
            break;
        Sleep(50);
        if (++diagnosticTicks >= 100 && GetTickCount64() - diagnosticStart < 120000)
        {
            diagnosticTicks = 0;
            const uint64_t deviceStart = gD3DDeviceStartTick.load(std::memory_order_acquire);
            Log(L"[STARTUP] worker alive; slSetD3DDevice pending=%d thread=%lu elapsed=%llums",
                deviceStart != 0, gD3DDeviceThread.load(std::memory_order_relaxed),
                static_cast<unsigned long long>(deviceStart ? GetTickCount64() - deviceStart : 0));
        }
        if (ProcessStandaloneHotkeys(activeControl))
        {
            StoreControl(activeControl);
            PublishLiveBridge(activeControl);
            WriteControlFile(gConfigPath, activeControl);
            ReadLastWriteTime(gConfigPath, configWriteTime);
            WriteBridgeStatus(activeControl, pid);
            // ReapplyPendingControl is executed safely on the render thread
            // via HookSlDLSSGGetState under gStreamlineCallMutex with the real viewport.
        }
        const bool retryMidpoint = ++inventoryTicks >= 10 && !midpoint_fix::Ready();
        if (gModuleInventoryDirty.exchange(false, std::memory_order_acq_rel)
            || retryMidpoint)
        {
            inventoryTicks = 0;
            InspectAlreadyLoadedModules();
        }
        FILETIME latestWriteTime{};
        if (ReadLastWriteTime(gConfigPath, latestWriteTime)
            && CompareFileTime(&latestWriteTime, &configWriteTime) != 0)
        {
            configWriteTime = latestWriteTime;
            ControlConfig control{};
            if (!ReadControlFile(gConfigPath, control))
            {
                Log(L"Ignored an invalid live control config update");
            }
            else
            {
                ReadLastWriteTime(gConfigPath, configWriteTime);
                activeControl = control;
                StoreControl(activeControl);
                PublishLiveBridge(activeControl);
                WriteBridgeStatus(activeControl, pid);
                Log(L"Live control requested: mode=%s multiplier=%ux dynamicTarget=%u FPS "
                    L"dynamicExperimental56=%d",
                    activeControl.dynamic ? L"dynamic" : L"fixed", activeControl.multiplier,
                    activeControl.dynamicTargetFrameRate,
                    activeControl.dynamicExperimental56);
            }
        }

        const bool ready = BridgeReady();
        const std::string route = PatchRouteName();
        if (ready != previousReady || route != previousRoute)
        {
            previousReady = ready;
            previousRoute = route;
            PublishPatchRoute();
            WriteBridgeStatus(activeControl, pid);
            Log(L"Bridge readiness changed: ready=%d route=%hs wrappers=%u/%u ngx=%u/%u",
                ready, route.c_str(),
                gPatchedWrapperCandidates.load(std::memory_order_relaxed),
                gLoadedWrapperCandidates.load(std::memory_order_relaxed),
                gPatchedNgxCandidates.load(std::memory_order_relaxed),
                gLoadedNgxCandidates.load(std::memory_order_relaxed));
        }

        if (++heartbeatTicks >= 10)
        {
            if (!gStatusPath.empty())
                WriteBridgeStatus(activeControl, pid);
            heartbeatTicks = 0;
        }
    }
    return 0;
}
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        // /MT requires CRT thread notifications; do not disable them.
        proxy::Initialize(instance);
        midpoint_fix::SetLogCallback(&MidpointLog);
        // Automatic storage reserves 64 KiB in DllMain's prologue for EVERY
        // notification, including DLL_THREAD_ATTACH on small driver stacks.
        // DllMain is serialized and this buffer is used only at process attach.
        static wchar_t executablePath[32768]{};
        GetModuleFileNameW(nullptr, executablePath, _countof(executablePath));
        gExecutableDirectory = ParentPath(executablePath);
        InitLogging(instance, gExecutableDirectory);
        gLiveHookInstalled.store(InstallFeatureFunctionHook(), std::memory_order_release);
        InstallD3DDeviceHook();
        InstallVulkanInfoHook();
        InstallSetDataHook();
        InstallUiTagHooks();
        InstallInterposerDetours();
        InstallLoadLibraryHooks();
        RegisterDllNotification();
        HANDLE thread = CreateThread(nullptr, 0, PatchWorker, instance, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        crash_diagnostics::Shutdown();
        gStopWorker.store(true, std::memory_order_release);
        UninstallNvApiHook();
        UninstallLoadLibraryHooks();
        UninstallInterposerDetours();
        proxy::Shutdown();
        {
            std::lock_guard lock(gPerfMutex);
            if (gPerfCsv)
            {
                fflush(gPerfCsv);
                fclose(gPerfCsv);
                gPerfCsv = nullptr;
            }
        }
        if (gLog)
        {
            fclose(gLog);
            gLog = nullptr;
        }
    }
    return TRUE;
}
