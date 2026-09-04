#include "shared.h"
#include "midpoint_fix.h"
#include "dlssg_provider_policy.h"
#include "proxy.h"
#include "detours/detours.h"
#include "nvidia_mfg_manifest.generated.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <winternl.h>
#include <sl.h>
#include <sl_dlss_g.h>

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
std::atomic<PFun_slSetD3DDevice*> gOriginalSetD3DDevice{nullptr};
std::atomic<PFun_slSetTag*> gOriginalSetTag{nullptr};
std::atomic<PFun_slSetTagForFrame*> gOriginalSetTagForFrame{nullptr};
std::atomic<PFun_slDLSSGSetOptions*> gOriginalSetOptions{nullptr};
std::atomic<PFun_slDLSSGGetState*> gOriginalGetState{nullptr};
using PFun_slSetData = sl::Result(const sl::BaseStructure*, sl::CommandBuffer*);
std::atomic<PFun_slSetData*> gOriginalSlSetData{nullptr};
PFun_slSetData* gDetourSlSetData = nullptr;
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
std::atomic<bool> gStopWorker{false};
std::atomic<bool> gFlipMeteringPatched{false};
std::atomic<uint32_t> gFlipMeteringOffset{0};
std::atomic<uint32_t> gFlipMeteringValue{0};
std::mutex gStreamlineCallMutex;
std::mutex gLastOptionsMutex;
std::mutex gModuleMutex;
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
    uint32_t multiplier = 2;
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
    swprintf_s(timestamped, L"[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);

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
std::atomic<bool> gConfigLogPerformance{true};

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
    if (control.dynamic && !control.dynamicExperimental56)
        return kStandardMaximumGeneratedFrames;
    return kExperimentalMaximumGeneratedFrames;
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

void ApplyWrapperMaximum(const ControlConfig& control)
{
    const uint8_t maximum = RequestedMaximumGeneratedFrames(control);
    std::lock_guard lock(gModuleMutex);
    for (auto& record : gModuleRecords)
    {
        if (record.wrapperPatched && record.wrapperMaximumImmediate)
            SetWrapperMaximum(record, maximum);
    }
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
    if (!snapshot.hudless || !ui)
        return snapshot;

    snapshot.hudlessWidth = found->hudless.width;
    snapshot.hudlessHeight = found->hudless.height;
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

    const uint64_t hudlessAge = now - found->hudless.lastSeenTick;
    const uint64_t uiAge = now - ui->lastSeenTick;
    snapshot.oldestAgeMs = std::max(hudlessAge, uiAge);
    snapshot.ready = snapshot.dimensionsMatch;
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

void CaptureUiResourceTags(const sl::ViewportHandle& viewport,
    const sl::ResourceTag* tags, uint32_t numTags)
{
    if (!tags || numTags == 0 || numTags > 1024)
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

bool BridgeReady()
{
    const bool hookOk = gLiveHookInstalled.load(std::memory_order_acquire)
        || gInterposerDetoursInstalled.load(std::memory_order_acquire);
    const bool midpointOk = midpoint_fix::Ready();
    return hookOk
        && gSetOptionsHookExposed.load(std::memory_order_relaxed)
        && gActiveWrapperObserved.load(std::memory_order_relaxed)
        && gActiveWrapperPatched.load(std::memory_order_relaxed)
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

bool TryParseControl(const char* data, size_t size, ControlConfig& control)
{
    if (!data || size == 0)
        return false;

    const std::string content(data, size);
    ControlConfig parsed{};
    if (!TryParseUnsigned(content, "multiplier",
        kMinimumMultiplier, kMaximumMultiplier, parsed.multiplier))
        return false;

    size_t modeOffset = 0;
    if (FindJsonValue(content, "mode", modeOffset))
    {
        if (content.compare(modeOffset, 9, "\"dynamic\"") == 0)
            parsed.dynamic = true;
        else if (content.compare(modeOffset, 7, "\"fixed\"") != 0)
            return false;
    }

    size_t targetOffset = 0;
    if (FindJsonValue(content, "dynamicTargetFrameRate", targetOffset)
        && !TryParseUnsigned(content, "dynamicTargetFrameRate", 0, 1000,
            parsed.dynamicTargetFrameRate))
        return false;

    size_t experimentalOffset = 0;
    if (FindJsonValue(content, "dynamicExperimental56", experimentalOffset)
        && !TryParseBoolean(content, "dynamicExperimental56",
            parsed.dynamicExperimental56))
        return false;

    size_t otaOffset = 0;
    if (FindJsonValue(content, "forceOTA", otaOffset))
    {
        bool forceOta = false;
        if (TryParseBoolean(content, "forceOTA", forceOta))
            gConfigForceOta.store(forceOta, std::memory_order_relaxed);
    }

    size_t flipOffset = 0;
    if (FindJsonValue(content, "patchFlipMetering", flipOffset))
    {
        bool patchFlip = false;
        if (TryParseBoolean(content, "patchFlipMetering", patchFlip))
            gConfigPatchFlipMetering.store(patchFlip, std::memory_order_relaxed);
    }

    size_t transfusionOffset = 0;
    if (FindJsonValue(content, "blackwellTransfusion", transfusionOffset))
    {
        bool blackwellTransfusion = true;
        if (TryParseBoolean(content, "blackwellTransfusion", blackwellTransfusion))
        {
            gConfigBlackwellTransfusion.store(blackwellTransfusion, std::memory_order_relaxed);
            midpoint_fix::SetBlackwellTransfusionEnabled(blackwellTransfusion);
        }
    }

    size_t perfOffset = 0;
    if (FindJsonValue(content, "logPerformance", perfOffset))
    {
        bool logPerf = true;
        if (TryParseBoolean(content, "logPerformance", logPerf))
            gConfigLogPerformance.store(logPerf, std::memory_order_relaxed);
    }

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

    std::array<char, 4096> buffer{};
    DWORD bytesRead = 0;
    const BOOL read = ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);
    CloseHandle(file);
    return read && TryParseControl(buffer.data(), bytesRead, control);
}

bool WriteControlFile(const std::wstring& path, const ControlConfig& control)
{
    char json[512]{};
    const int len = sprintf_s(json,
        "{\n"
        "  \"multiplier\": %u,\n"
        "  \"mode\": \"%s\",\n"
        "  \"dynamicTargetFrameRate\": %u,\n"
        "  \"dynamicExperimental56\": %s,\n"
        "  \"forceOTA\": %s,\n"
        "  \"patchFlipMetering\": %s,\n"
        "  \"blackwellTransfusion\": %s,\n"
        "  \"logPerformance\": %s\n"
        "}\n",
        control.multiplier,
        control.dynamic ? "dynamic" : "fixed",
        control.dynamicTargetFrameRate,
        control.dynamicExperimental56 ? "true" : "false",
        gConfigForceOta.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigPatchFlipMetering.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigBlackwellTransfusion.load(std::memory_order_relaxed) ? "true" : "false",
        gConfigLogPerformance.load(std::memory_order_relaxed) ? "true" : "false");
    if (len <= 0) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const BOOL res = WriteFile(file, json, static_cast<DWORD>(len), &written, nullptr);
    CloseHandle(file);
    return res && written == static_cast<DWORD>(len);
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

    // List of candidate config filenames in order of preference
    const wchar_t* candidateNames[] = {
        L"DLSSG-Transfusion.json",
        L"RTX40MFG.json",
        L"RTX40MFG_config.json",
        L"config.json" // backwards compatibility fallback
    };

    // 1. Next to proxy/ASI module
    for (const wchar_t* name : candidateNames)
    {
        const std::wstring p = JoinPath(moduleDir, name);
        if (IsRegularFile(p))
            return p;
    }

    // 2. Next to executable
    for (const wchar_t* name : candidateNames)
    {
        const std::wstring p = JoinPath(executableDirectory, name);
        if (IsRegularFile(p))
            return p;
    }

    // 3. Cyberpunk CET mod path if it exists
    const std::wstring cetDir = JoinPath(executableDirectory,
        L"plugins\\cyber_engine_tweaks\\mods\\RTX40MFG");
    if (IsDirectory(cetDir))
    {
        for (const wchar_t* name : candidateNames)
        {
            const std::wstring p = JoinPath(cetDir, name);
            if (IsRegularFile(p))
                return p;
        }
        return JoinPath(cetDir, L"DLSSG-Transfusion.json");
    }

    // 4. Default: DLSSG-Transfusion.json next to module or executable
    return moduleDir.empty() ? JoinPath(executableDirectory, L"DLSSG-Transfusion.json")
                             : JoinPath(moduleDir, L"DLSSG-Transfusion.json");
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
    bool preserveNext, bool enableUiRecomposition)
{
    sl::DLSSGOptions adjusted = CopyKnownOptions(source, preserveNext);
    if (snapshot.control.dynamic)
    {
        // The injected object is a complete v5 structure even when Cyberpunk supplied
        // an older prefix, so the active wrapper can consume the dynamic target
        // without reading beyond the game's allocation.
        adjusted.structVersion = sl::kStructVersion5;
        adjusted.mode = sl::DLSSGMode::eDynamic;
        adjusted.dynamicTargetFrameRate =
            static_cast<float>(snapshot.control.dynamicTargetFrameRate);
    }
    else
    {
        adjusted.mode = sl::DLSSGMode::eOn;
        adjusted.numFramesToGenerate =
            std::clamp(snapshot.control.multiplier,
            kMinimumMultiplier, kMaximumMultiplier) - 1;
    }
    if (enableUiRecomposition)
    {
        adjusted.structVersion = std::max<size_t>(
            adjusted.structVersion, sl::kStructVersion4);
        adjusted.enableUserInterfaceRecomposition = sl::Boolean::eTrue;
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
    gGameUiRecompositionEnabled.store(options.structVersion >= sl::kStructVersion4
        && options.enableUserInterfaceRecomposition == sl::Boolean::eTrue,
        std::memory_order_relaxed);
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
        *outViewport = gLastGameOptions.viewport;
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
        Log(L"%s dynamic MFG: target=%u FPS experimental56=%d max=%ux result=%d",
            liveReapply ? L"Live-reapplied" : L"Applied",
            snapshot.control.dynamicTargetFrameRate,
            snapshot.control.dynamicExperimental56,
            static_cast<uint32_t>(RequestedMaximumGeneratedFrames(snapshot.control)) + 1,
            static_cast<int>(result));
    else
        Log(L"%s fixed multiplier: %ux, result=%d",
            liveReapply ? L"Live-reapplied" : L"Applied",
            snapshot.control.multiplier, static_cast<int>(result));
}

sl::Result SubmitAdjustedOptions(
    PFun_slDLSSGSetOptions* original, const sl::ViewportHandle& viewport,
    const sl::DLSSGOptions& source, const ControlSnapshot& snapshot, bool liveReapply)
{
    const UiInputSnapshot uiInputs = ReadUiInputSnapshot(
        static_cast<uint32_t>(viewport));
    const bool gameUiRecomposition = source.structVersion >= sl::kStructVersion4
        && source.enableUserInterfaceRecomposition == sl::Boolean::eTrue;
    const bool forceUiRecomposition = uiInputs.ready && !gameUiRecomposition;
    const sl::DLSSGOptions adjusted = BuildAdjustedOptions(
        source, snapshot, !liveReapply, uiInputs.ready);
    const bool uiRecompositionEnabled = adjusted.structVersion >= sl::kStructVersion4
        && adjusted.enableUserInterfaceRecomposition == sl::Boolean::eTrue;
    const sl::Result result = original(viewport, adjusted);
    RecordAppliedControl(snapshot, result, liveReapply,
        uiRecompositionEnabled, forceUiRecomposition);
    return result == sl::Result::eWarnOutOfVRAM ? sl::Result::eOk : result;
}

void ReapplyPendingControl(const sl::ViewportHandle& viewport)
{
    if (!gControlReady.load(std::memory_order_acquire)
        || !gGameFrameGenerationOn.load(std::memory_order_acquire))
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
    if (!originalSetData && gDetourSlSetData)
        originalSetData = gDetourSlSetData;
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
            Log(L"Live reapply failed for request revision %llu: result=%d",
                static_cast<unsigned long long>(snapshot.revision), static_cast<int>(result));
    }
    else if (originalSetData)
    {
        sl::DLSSGOptions adjusted = BuildAdjustedOptions(source, snapshot, false, false);
        sl::ViewportHandle vpCopy = targetViewport;
        vpCopy.next = &adjusted;
        adjusted.next = nullptr;
        const sl::Result result = originalSetData(&vpCopy, nullptr);
        RecordAppliedControl(snapshot, result, true, false, false);
        if (result != sl::Result::eOk && result != sl::Result::eWarnOutOfVRAM)
            Log(L"Live reapply via setData failed for request revision %llu: result=%d",
                static_cast<unsigned long long>(snapshot.revision), static_cast<int>(result));
    }
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
    if (!gControlReady.load(std::memory_order_acquire))
    {
        const sl::Result result = original(viewport, options);
        gSetOptionsSeen.store(true, std::memory_order_release);
        gLastSetOptionsResult.store(static_cast<int32_t>(result), std::memory_order_relaxed);
        return result;
    }

    if (!BridgeReady())
    {
        const sl::Result result = original(viewport, options);
        gSetOptionsSeen.store(true, std::memory_order_release);
        gLastSetOptionsResult.store(static_cast<int32_t>(result), std::memory_order_relaxed);
        if (result != sl::Result::eOk || !BridgeReady())
            return result;
        Log(L"Active DLSS-G modules became ready during the native options call; applying saved control");
    }

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
        const uint32_t advertised = gNumFramesToGenerateMax.load(std::memory_order_relaxed);
        if (advertised >= 2 && state.numFramesToGenerateMax < advertised)
        {
            state.numFramesToGenerateMax = advertised;
        }
    }
    RecordDlssgStateResult(result, state, true);
    return result;
}

sl::Result HookSlGetFeatureFunction(
    sl::Feature feature, const char* functionName, void*& function)
{
    auto* original = gOriginalGetFeatureFunction.load(std::memory_order_acquire);
    if (!original)
        return sl::Result::eErrorNotInitialized;

    const sl::Result result = original(feature, functionName, function);
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
    return result;
}

sl::Result HookSlSetTag(const sl::ViewportHandle& viewport,
    const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer)
{
    auto* original = gOriginalSetTag.load(std::memory_order_acquire);
    if (!original)
        return sl::Result::eErrorNotInitialized;
    const sl::Result result = original(viewport, tags, numTags, cmdBuffer);
    gSetTagCalls.fetch_add(1, std::memory_order_relaxed);
    if (result == sl::Result::eOk)
        CaptureUiResourceTags(viewport, tags, numTags);
    return result;
}

sl::Result HookSlSetTagForFrame(const sl::FrameToken& frame,
    const sl::ViewportHandle& viewport, const sl::ResourceTag* tags,
    uint32_t numTags, sl::CommandBuffer* cmdBuffer)
{
    auto* original = gOriginalSetTagForFrame.load(std::memory_order_acquire);
    if (!original)
        return sl::Result::eErrorNotInitialized;
    const sl::Result result = original(frame, viewport, tags, numTags, cmdBuffer);
    gSetTagForFrameCalls.fetch_add(1, std::memory_order_relaxed);
    if (result == sl::Result::eOk)
        CaptureUiResourceTags(viewport, tags, numTags);
    return result;
}

sl::Result HookSlSetD3DDevice(void* device)
{
    auto* original = gOriginalSetD3DDevice.load(std::memory_order_acquire);
    if (!original)
        return sl::Result::eErrorNotInitialized;
    if (midpoint_fix::ObserveD3D12Device(device))
        gModuleInventoryDirty.store(true, std::memory_order_release);
    return original(device);
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
    if (!original && gDetourSlSetVulkanInfo)
        original = gDetourSlSetVulkanInfo;
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

    if (physicalDevice)
    {
        midpoint_fix::ObserveVulkanPhysicalDevice(physicalDevice);
        gModuleInventoryDirty.store(true, std::memory_order_release);
    }
    return original(info);
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

sl::Result HookSlSetData(const sl::BaseStructure* inputs, sl::CommandBuffer* cmdBuffer)
{
    auto* original = gOriginalSlSetData.load(std::memory_order_acquire);
    if (!original && gDetourSlSetData)
        original = gDetourSlSetData;
    if (!original)
        return sl::Result::eErrorNotInitialized;

    const auto* options = FindStructInChain<sl::DLSSGOptions>(inputs);
    const auto* viewport = FindStructInChain<sl::ViewportHandle>(inputs);

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
            if (gLastGameOptions.valid)
                gLastGameOptions.options = CopyKnownOptions(*options, false);
        }

        if (enabled && gControlReady.load(std::memory_order_acquire) && BridgeReady())
        {
            const ControlSnapshot snapshot = ReadControlSnapshot();
            sl::DLSSGOptions adjusted = BuildAdjustedOptions(*options, snapshot, false, false);

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

    return original(inputs, cmdBuffer);
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
    void* original = nullptr;
    const bool installed = HookMainExecutableImport("sl.interposer.dll",
        "slGetFeatureFunction", reinterpret_cast<void*>(&HookSlGetFeatureFunction), original);
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
    void* original = nullptr;
    const bool installed = HookMainExecutableImport("sl.interposer.dll",
        "slSetD3DDevice", reinterpret_cast<void*>(&HookSlSetD3DDevice), original);
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
    if (gDetourSlSetVulkanInfo)
        return true;
    void* original = nullptr;
    const bool installed = HookMainExecutableImport("sl.interposer.dll",
        "slSetVulkanInfo", reinterpret_cast<void*>(&HookSlSetVulkanInfo), original);
    if (original)
    {
        gOriginalSetVulkanInfo.store(
            reinterpret_cast<PFun_slSetVulkanInfo*>(original),
            std::memory_order_release);
    }
    return installed || (gDetourSlSetVulkanInfo != nullptr);
}

bool InstallSetDataHook()
{
    if (gDetourSlSetData)
        return true;
    void* original = nullptr;
    const bool installed = HookMainExecutableImport("sl.interposer.dll",
        "slSetData", reinterpret_cast<void*>(&HookSlSetData), original);
    if (original)
    {
        gOriginalSlSetData.store(
            reinterpret_cast<PFun_slSetData*>(original),
            std::memory_order_release);
    }
    return installed || (gDetourSlSetData != nullptr);
}

bool InstallUiTagHooks()
{
    void* legacyOriginal = nullptr;
    const bool legacyInstalled = HookMainExecutableImport("sl.interposer.dll",
        "slSetTag", reinterpret_cast<void*>(&HookSlSetTag), legacyOriginal);
    if (legacyOriginal)
    {
        gOriginalSetTag.store(reinterpret_cast<PFun_slSetTag*>(legacyOriginal),
            std::memory_order_release);
    }

    void* frameOriginal = nullptr;
    const bool frameInstalled = HookMainExecutableImport("sl.interposer.dll",
        "slSetTagForFrame", reinterpret_cast<void*>(&HookSlSetTagForFrame), frameOriginal);
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
            || ContainsCI(moduleName, L"nvapi"))
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
        const bool temporalOk = record.ngxTemporalPatched || midpoint_fix::Ready();
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
    Log(L"Loaded module: wrapperExport=%d wrapperCandidate=%d wrapperPatched=%d "
        L"ngxExport=%d ngxCandidate=%d ngxPatched=%d midpointPatched=%d path=%s",
        record.wrapperExport, record.wrapperCandidate, record.wrapperPatched,
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
                    gAdvertisedMaxGenerated.store(ceiling, std::memory_order_release);
                    return true;
                }

                if (lastByte == 0xD1)
                {
                    DWORD oldProtect = 0;
                    if (VirtualProtect(start + off, 10, PAGE_EXECUTE_READWRITE, &oldProtect))
                    {
                        start[off + 9] = 0xD2;
                        DWORD ignored = 0;
                        VirtualProtect(start + off, 10, oldProtect, &ignored);
                        FlushInstructionCache(GetCurrentProcess(), start + off, 10);
                        gAdvertisedMaxGenerated.store(ceiling, std::memory_order_release);
                        Log(L"Streamline ceiling bypass applied in %s (compiled max=%ux, cmovb bypassed)",
                            path ? path : L"", ceiling + 1);
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

using PFun_slInit = sl::Result(const sl::Preferences&, uint64_t);
PFun_slInit* gOriginalSlInit = nullptr;
PFun_slGetFeatureFunction* gDetourSlGetFeatureFunction = nullptr;

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
    const sl::Result result = gOriginalSlInit(forceOta ? localPref : pref, sdkVersion);
    Log(L"slInit intercepted: flags 0x%llX%s result=%d",
        before, forceOta ? L" (forced OTA)" : L"", static_cast<int>(result));
    return result;
}

void InstallInterposerDetours()
{
    if (gInterposerDetoursInstalled.load(std::memory_order_acquire))
        return;

    HMODULE interposer = GetModuleHandleW(L"sl.interposer.dll");
    if (!interposer)
        return;

    auto* getFeatureFn = reinterpret_cast<PFun_slGetFeatureFunction*>(
        GetProcAddress(interposer, "slGetFeatureFunction"));
    auto* initFn = reinterpret_cast<PFun_slInit*>(
        GetProcAddress(interposer, "slInit"));
    auto* vulkanInfoFn = reinterpret_cast<PFun_slSetVulkanInfo*>(
        GetProcAddress(interposer, "slSetVulkanInfo"));
    auto* setDataFn = reinterpret_cast<PFun_slSetData*>(
        GetProcAddress(interposer, "slSetData"));

    if (!getFeatureFn && !initFn && !vulkanInfoFn && !setDataFn)
        return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (getFeatureFn)
    {
        gDetourSlGetFeatureFunction = getFeatureFn;
        DetourAttach(reinterpret_cast<void**>(&gDetourSlGetFeatureFunction),
                     reinterpret_cast<void*>(&HookSlGetFeatureFunction));
    }
    if (initFn)
    {
        gOriginalSlInit = initFn;
        DetourAttach(reinterpret_cast<void**>(&gOriginalSlInit),
                     reinterpret_cast<void*>(&HookSlInit));
    }
    if (vulkanInfoFn)
    {
        gDetourSlSetVulkanInfo = vulkanInfoFn;
        DetourAttach(reinterpret_cast<void**>(&gDetourSlSetVulkanInfo),
                     reinterpret_cast<void*>(&HookSlSetVulkanInfo));
    }
    if (setDataFn)
    {
        gDetourSlSetData = setDataFn;
        DetourAttach(reinterpret_cast<void**>(&gDetourSlSetData),
                     reinterpret_cast<void*>(&HookSlSetData));
    }
    const LONG status = DetourTransactionCommit();
    if (status == NO_ERROR)
    {
        gInterposerDetoursInstalled.store(true, std::memory_order_release);
        gLiveHookInstalled.store(true, std::memory_order_release);
        if (gDetourSlGetFeatureFunction)
            gOriginalGetFeatureFunction.store(gDetourSlGetFeatureFunction, std::memory_order_release);
        if (gDetourSlSetVulkanInfo)
            gOriginalSetVulkanInfo.store(gDetourSlSetVulkanInfo, std::memory_order_release);
        if (gDetourSlSetData)
            gOriginalSlSetData.store(gDetourSlSetData, std::memory_order_release);
        Log(L"Direct Detours on sl.interposer.dll installed successfully (slGetFeatureFunction, slInit, slSetVulkanInfo, slSetData)");
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
    if (gDetourSlSetVulkanInfo)
        DetourDetach(reinterpret_cast<void**>(&gDetourSlSetVulkanInfo),
                     reinterpret_cast<void*>(&HookSlSetVulkanInfo));
    if (gDetourSlSetData)
        DetourDetach(reinterpret_cast<void**>(&gDetourSlSetData),
                     reinterpret_cast<void*>(&HookSlSetData));
    DetourTransactionCommit();
    gInterposerDetoursInstalled.store(false, std::memory_order_release);
}

using PFun_NvAPI_QueryInterface = void*(__stdcall*)(unsigned int InterfaceId);
PFun_NvAPI_QueryInterface gRealNvAPI_QueryInterface = nullptr;
std::atomic<bool> gNvApiHookInstalled{false};

void* __stdcall HookNvAPI_QueryInterface(unsigned int interfaceId)
{
    constexpr unsigned int kNvAPI_D3D12_SetFlipConfig = 0xf3148c42;
    if (interfaceId == kNvAPI_D3D12_SetFlipConfig && gConfigPatchFlipMetering.load(std::memory_order_relaxed))
    {
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true))
            Log(L"NVAPI: NvAPI_D3D12_SetFlipConfig (0xF3148C42) query intercepted -> returning nullptr (OptiScaler FlipMetering bypass)");
        return nullptr;
    }
    if (gRealNvAPI_QueryInterface)
        return gRealNvAPI_QueryInterface(interfaceId);
    return nullptr;
}

void InstallNvApiHook()
{
    if (gNvApiHookInstalled.load(std::memory_order_acquire))
        return;

    HMODULE nvapi = GetModuleHandleW(L"nvapi64.dll");
    if (!nvapi)
        return;

    auto* queryInterface = reinterpret_cast<PFun_NvAPI_QueryInterface>(
        GetProcAddress(nvapi, "nvapi_QueryInterface"));
    if (!queryInterface)
        return;

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
    if (!gNvApiHookInstalled.load(std::memory_order_acquire))
        return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (gRealNvAPI_QueryInterface)
        DetourDetach(reinterpret_cast<void**>(&gRealNvAPI_QueryInterface),
                     reinterpret_cast<void*>(&HookNvAPI_QueryInterface));
    DetourTransactionCommit();
    gNvApiHookInstalled.store(false, std::memory_order_release);
}

using PFun_LoadLibraryW = HMODULE(WINAPI*)(LPCWSTR);
using PFun_LoadLibraryExW = HMODULE(WINAPI*)(LPCWSTR, HANDLE, DWORD);

PFun_LoadLibraryW gRealLoadLibraryW = nullptr;
PFun_LoadLibraryExW gRealLoadLibraryExW = nullptr;
std::atomic<bool> gLoadHooksInstalled{false};

ModuleRecord InspectLoadedModule(HMODULE module, const std::wstring& suppliedPath);
void OnPotentialModuleLoaded(HMODULE module, LPCWSTR name);

HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpLibFileName)
{
    HMODULE mod = gRealLoadLibraryW(lpLibFileName);
    if (mod && lpLibFileName && reinterpret_cast<uintptr_t>(lpLibFileName) >= 0x10000)
    {
        if (IsTargetModule(lpLibFileName, lpLibFileName))
            OnPotentialModuleLoaded(mod, lpLibFileName);
    }
    return mod;
}

HMODULE WINAPI HookLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HMODULE mod = gRealLoadLibraryExW(lpLibFileName, hFile, dwFlags);
    constexpr DWORD kDataOnly = LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;
    if (mod && lpLibFileName && reinterpret_cast<uintptr_t>(lpLibFileName) >= 0x10000 && (dwFlags & kDataOnly) == 0)
    {
        if (IsTargetModule(lpLibFileName, lpLibFileName))
            OnPotentialModuleLoaded(mod, lpLibFileName);
    }
    return mod;
}

void InstallLoadLibraryHooks()
{
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
    }
}

ModuleRecord InspectLoadedModule(HMODULE module, const std::wstring& suppliedPath)
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
                return record.module == module
                    && _wcsicmp(record.path.c_str(), path.c_str()) == 0;
            });
        if (existing != gModuleRecords.end())
        {
            if (existing->ngxPatched && !existing->ngxTemporalPatched)
            {
                existing->ngxTemporalPatched = midpoint_fix::PatchProvider(
                    module, path.c_str());
                if (existing->ngxTemporalPatched)
                    RecomputeModuleStateLocked();
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
            record.wrapperExport = ModuleExportsFunction(module, "slGetPluginFunction");
            record.ngxExport =
                dlssg_provider_policy::IsDlssgImplementationModule(module)
                && (ModuleExportsFunction(module, "NVSDK_NGX_D3D12_CreateFeature")
                    || ModuleExportsFunction(module, "NVSDK_NGX_VULKAN_CreateFeature"))
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
                    SetWrapperMaximum(record,
                        RequestedMaximumGeneratedFrames(ReadControlSnapshot().control));
                }
                const bool ceilingPatched = PatchStreamlineCeilingClamp(module, path.c_str());
                record.wrapperCandidate = record.wrapperCandidate || ceilingPatched;
                record.wrapperPatched = record.wrapperPatched || ceilingPatched;
            }
            if (record.ngxExport)
            {
                const bool archGatesPatched = PatchDlssgArchGates(module, path.c_str());
                const PatternPatchResult result =
                    PatchUniqueExecutablePattern(module, path, kNgxPatch);
                record.ngxCandidate = result.candidate || archGatesPatched;
                record.ngxPatched = result.patched || archGatesPatched;
                if (record.ngxPatched || archGatesPatched)
                {
                    record.ngxTemporalPatched =
                        midpoint_fix::PatchProvider(module, path.c_str());
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
        gActiveWrapperPatched.store(false, std::memory_order_release);
        gActiveWrapperObserved.store(false, std::memory_order_release);
        gActiveWrapperBase.store(0, std::memory_order_release);
    }
}

void InspectAlreadyLoadedModules()
{
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
            if (std::find(loadedModules.begin(), loadedModules.end(),
                    record.module) == loadedModules.end())
                removedModules.push_back(record.module);
        }
    }
    for (HMODULE module : removedModules)
        RemoveLoadedModule(module);
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
    const ModuleRecord record = InspectLoadedModule(module, LoadedModulePath(module));
    if (!record.wrapperExport)
        return;

    const uintptr_t base = reinterpret_cast<uintptr_t>(module);
    const uintptr_t previous = gActiveWrapperBase.exchange(base, std::memory_order_acq_rel);
    gActiveWrapperPatched.store(record.wrapperPatched, std::memory_order_release);
    gActiveWrapperObserved.store(true, std::memory_order_release);
    if (previous != base)
        Log(L"Active DLSS-G wrapper provider: patched=%d path=%s",
            record.wrapperPatched, record.path.c_str());
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
    gStatusPath = JoinPath(ParentPath(gConfigPath), L"bridge_status.json");
    DeleteFileW(gStatusPath.c_str());
    gPerfCsvPath = JoinPath(ParentPath(gConfigPath), L"DLSSG-Transfusion_perf.csv");
    const ControlConfig initialControl = ReadInitialControl();
    StoreControl(initialControl);
    midpoint_fix::SetBlackwellTransfusionEnabled(gConfigBlackwellTransfusion.load(std::memory_order_relaxed));
    FILETIME configWriteTime{};
    ReadLastWriteTime(gConfigPath, configWriteTime);
    Log(L"Initial control: mode=%s multiplier=%ux dynamicTarget=%u FPS "
        L"dynamicExperimental56=%d blackwellTransfusion=%d logPerformance=%d; config: %s",
        initialControl.dynamic ? L"dynamic" : L"fixed", initialControl.multiplier,
        initialControl.dynamicTargetFrameRate, initialControl.dynamicExperimental56,
        gConfigBlackwellTransfusion.load(std::memory_order_relaxed),
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
    if (!WriteBridgeStatus(activeControl, pid))
        Log(L"Could not publish CET bridge status file: %s", gStatusPath.c_str());

    // CET writes config.json when the user changes the mode. Watch it off
    // the presenting thread and atomically publish changes for the SetOptions hook.
    uint32_t heartbeatTicks = 0;
    uint32_t inventoryTicks = 0;
    bool previousReady = BridgeReady();
    std::string previousRoute = PatchRouteName();
    for (;;)
    {
        if (gStopWorker.load(std::memory_order_relaxed))
            break;
        Sleep(50);
        if (ProcessStandaloneHotkeys(activeControl))
        {
            StoreControl(activeControl);
            PublishLiveBridge(activeControl);
            WriteControlFile(gConfigPath, activeControl);
            ReadLastWriteTime(gConfigPath, configWriteTime);
            WriteBridgeStatus(activeControl, pid);
            sl::ViewportHandle vp{};
            ReapplyPendingControl(vp);
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
        DisableThreadLibraryCalls(instance);
        proxy::Initialize(instance);
        midpoint_fix::SetLogCallback(&MidpointLog);
        wchar_t executablePath[32768]{};
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
