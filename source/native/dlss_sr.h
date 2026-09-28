#pragma once
/*
 * DLSS Super Resolution render-scale override (D3D12). Ported from
 * dlssg_for_sm86 src/companion/super_resolution.hpp and ngx_hook.hpp.
 *
 * RENDER SCALE
 * Games ask NGX for the recommended render size through the parameter block's
 * DLSSOptimalSettingsCallback. We wrap that callback and answer with the size
 * for the configured scale (and widen the dynamic-resolution bounds to allow
 * it). It never resizes or relabels GPU resources: a game that caches the
 * recommendation, or computes its own size, ignores it. Evaluations are
 * observed so the panel can show whether the requested size is really used
 * (8 consecutive matching frames = verified).
 */
#include <Windows.h>
#include <TlHelp32.h>

#include <nvsdk_ngx_params.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "detours/detours.h"

namespace dlss_sr
{
inline void (*g_log)(const char* text) = nullptr;
inline void Message(const char* text) { if (g_log) g_log(text); }

// Scale in percent * 1000: 0 follows the game, 33333 (Ultra Performance) or
// 50000..100000.
inline bool ValidScale(unsigned scale) { return !scale || scale == 33333 || (scale >= 50000 && scale <= 100000); }
inline unsigned Dimension(unsigned output, unsigned scale) { return static_cast<unsigned>((uint64_t{output} * scale + 50000) / 100000); }

struct Status
{
    unsigned scale = 0, revision = 1, verified = 0, consecutive = 0;
    unsigned inputWidth = 0, inputHeight = 0, outputWidth = 0, outputHeight = 0;
    ULONGLONG observed = 0;
    bool successful = false;
};
inline SRWLOCK g_lock = SRWLOCK_INIT;
inline Status g_status;
inline std::atomic_bool g_installed{false};

inline Status Snapshot()
{
    AcquireSRWLockShared(&g_lock);
    const Status status = g_status;
    ReleaseSRWLockShared(&g_lock);
    return status;
}
inline bool Fresh(const Status& s, ULONGLONG now) { return s.successful && s.observed && now - s.observed < 2000; }
inline bool Verified(const Status& s, ULONGLONG now) { return s.scale && s.verified == s.revision && Fresh(s, now); }

// Called when DLSSG-Transfusion.json changes.
inline void SetScale(unsigned scale)
{
    if (!ValidScale(scale))
        scale = 0;
    AcquireSRWLockExclusive(&g_lock);
    if (scale != g_status.scale)
    {
        g_status.scale = scale;
        ++g_status.revision;
        g_status.consecutive = g_status.verified = 0;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

namespace internal
{
using Params = NVSDK_NGX_Parameter;
using Result = NVSDK_NGX_Result;

inline void Observe(Status& s, unsigned iw, unsigned ih, unsigned ow, unsigned oh, bool success, ULONGLONG now)
{
    if (!Fresh(s, now)) s.consecutive = s.verified = 0;
    s.successful = success;
    const bool match = success && s.scale && iw && ih && ow && oh
        && std::abs(static_cast<int>(iw) - static_cast<int>(Dimension(ow, s.scale))) <= 2
        && std::abs(static_cast<int>(ih) - static_cast<int>(Dimension(oh, s.scale))) <= 2;
    if (match) { if (s.consecutive < 8) ++s.consecutive; if (s.consecutive == 8) s.verified = s.revision; }
    else s.consecutive = s.verified = 0;
    if (success) { s.inputWidth = iw; s.inputHeight = ih; s.outputWidth = ow; s.outputHeight = oh; s.observed = now; }
}

// Wrappers for up to 8 distinct optimal-settings callbacks.
using OptimalFn = Result(__cdecl*)(Params*);
inline std::array<OptimalFn, 8> g_optimal{};
template <unsigned N>
Result __cdecl Optimal(Params* p)
{
    const auto result = g_optimal[N](p);
    const Status s = Snapshot();
    unsigned w = 0, h = 0;
    if (result != NVSDK_NGX_Result_Success || !p || !s.scale || p->Get("Width", &w) != NVSDK_NGX_Result_Success
        || p->Get("Height", &h) != NVSDK_NGX_Result_Success || !w || !h || w > 32768 || h > 32768)
        return result;
    const unsigned rw = Dimension(w, s.scale), rh = Dimension(h, s.scale);
    p->Set("OutWidth", rw);
    p->Set("OutHeight", rh);
    const char* keys[] = {"DLSS.Get.Dynamic.Min.Render.Width", "DLSS.Get.Dynamic.Max.Render.Width",
        "DLSS.Get.Dynamic.Min.Render.Height", "DLSS.Get.Dynamic.Max.Render.Height"};
    for (unsigned i = 0; i < 4; ++i)
    {
        unsigned old = 0;
        p->Get(keys[i], &old);
        const unsigned requested = i < 2 ? rw : rh;
        p->Set(keys[i], !old ? requested : i % 2 ? (std::max)(old, requested) : (std::min)(old, requested));
    }
    return result;
}
inline constexpr OptimalFn kOptimal[] = {Optimal<0>, Optimal<1>, Optimal<2>, Optimal<3>,
    Optimal<4>, Optimal<5>, Optimal<6>, Optimal<7>};

inline void Attach(Params* p)
{
    void* callback = nullptr;
    if (!p || p->Get("DLSSOptimalSettingsCallback", &callback) != NVSDK_NGX_Result_Success || !callback)
        return;
    AcquireSRWLockExclusive(&g_lock);
    for (auto wrapper : kOptimal)
        if (callback == reinterpret_cast<void*>(wrapper)) { ReleaseSRWLockExclusive(&g_lock); return; }
    for (unsigned i = 0; i < 8; ++i)
    {
        if (g_optimal[i] == callback || !g_optimal[i])
        {
            HMODULE owner = nullptr;
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                    reinterpret_cast<LPCWSTR>(callback), &owner))
            {
                g_optimal[i] = reinterpret_cast<OptimalFn>(callback);
                p->Set("DLSSOptimalSettingsCallback", reinterpret_cast<void*>(kOptimal[i]));
            }
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
}

using ParamsFn = Result(__cdecl*)(Params**);
using CreateFn = Result(__cdecl*)(void*, NVSDK_NGX_Feature, Params*, void**);
using EvalFn = Result(__cdecl*)(void*, const void*, const Params*, void*);
using ReleaseFn = Result(__cdecl*)(void*);
inline ParamsFn g_params = nullptr, g_caps = nullptr;
inline CreateFn g_create = nullptr;
inline EvalFn g_eval = nullptr;
inline ReleaseFn g_release = nullptr;

struct Feature { const void* handle = nullptr; unsigned iw = 0, ih = 0, ow = 0, oh = 0; };
inline std::array<Feature, 32> g_features{};

inline Result __cdecl Parameters(Params** out)
{
    const auto result = g_params(out);
    if (result == NVSDK_NGX_Result_Success && out) Attach(*out);
    return result;
}
inline Result __cdecl Capabilities(Params** out)
{
    const auto result = g_caps(out);
    if (result == NVSDK_NGX_Result_Success && out) Attach(*out);
    return result;
}
inline Result __cdecl Create(void* list, NVSDK_NGX_Feature type, Params* p, void** out)
{
    const auto result = g_create(list, type, p, out);
    if (result == NVSDK_NGX_Result_Success && type == NVSDK_NGX_Feature_SuperSampling && p && out && *out)
    {
        Feature f{};
        f.handle = *out;
        p->Get("Width", &f.iw); p->Get("Height", &f.ih);
        p->Get("OutWidth", &f.ow); p->Get("OutHeight", &f.oh);
        AcquireSRWLockExclusive(&g_lock);
        for (auto& entry : g_features)
            if (!entry.handle || entry.handle == f.handle) { entry = f; break; }
        ReleaseSRWLockExclusive(&g_lock);
        char text[160];
        sprintf_s(text, "[DLSS-SR] Super Resolution feature created: %ux%u -> %ux%u", f.iw, f.ih, f.ow, f.oh);
        Message(text);
    }
    return result;
}
inline Result __cdecl Evaluate(void* list, const void* handle, const Params* p, void* callback)
{
    const auto result = g_eval(list, handle, p, callback);
    Feature f{};
    AcquireSRWLockShared(&g_lock);
    for (const auto& entry : g_features)
        if (entry.handle == handle) { f = entry; break; }
    ReleaseSRWLockShared(&g_lock);
    if (f.handle && p)
    {
        unsigned iw = 0, ih = 0;
        p->Get("DLSS.Render.Subrect.Dimensions.Width", &iw);
        p->Get("DLSS.Render.Subrect.Dimensions.Height", &ih);
        if (!iw || !ih) { iw = f.iw; ih = f.ih; }
        AcquireSRWLockExclusive(&g_lock);
        Observe(g_status, iw, ih, f.ow, f.oh, result == NVSDK_NGX_Result_Success, GetTickCount64());
        ReleaseSRWLockExclusive(&g_lock);
    }
    return result;
}
inline Result __cdecl Release(void* handle)
{
    const auto result = g_release(handle);
    if (result == NVSDK_NGX_Result_Success)
    {
        AcquireSRWLockExclusive(&g_lock);
        for (auto& f : g_features)
            if (f.handle == handle) f = {};
        g_status.verified = g_status.consecutive = 0;
        ReleaseSRWLockExclusive(&g_lock);
    }
    return result;
}

// _nvngx.dll is busy on the render thread while we patch it: every thread must
// be registered with the Detours transaction, or one resuming inside the
// rewritten prologue hangs the game.
inline bool AttachAll(const std::vector<std::pair<void**, void*>>& hooks)
{
    if (DetourTransactionBegin() != NO_ERROR)
        return false;
    std::vector<HANDLE> threads;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry))
        {
            if (entry.th32OwnerProcessID != GetCurrentProcessId() || entry.th32ThreadID == GetCurrentThreadId())
                continue;
            if (HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | SYNCHRONIZE,
                    FALSE, entry.th32ThreadID))
                threads.push_back(thread);
        }
        CloseHandle(snapshot);
    }
    bool ok = true;
    for (HANDLE thread : threads)
        if (DetourUpdateThread(thread) != NO_ERROR && WaitForSingleObject(thread, 0) != WAIT_OBJECT_0)
            ok = false;
    ok = ok && DetourUpdateThread(GetCurrentThread()) == NO_ERROR;
    for (size_t i = 0; ok && i < hooks.size(); ++i)
        ok = DetourAttach(hooks[i].first, hooks[i].second) == NO_ERROR;
    if (!ok || DetourTransactionCommit() != NO_ERROR)
    {
        DetourTransactionAbort();
        ok = false;
    }
    for (HANDLE thread : threads)
        CloseHandle(thread);
    return ok;
}
} // namespace internal

inline void TryInstallOnce();

// Called right after the NGX core loads (LoadLibrary hook) and, as a fallback,
// from the worker thread. Only one caller installs.
inline void TryInstall()
{
    static std::atomic_bool installing{false};
    if (g_installed.load(std::memory_order_acquire) || installing.exchange(true))
        return;
    TryInstallOnce();
    installing.store(false);
}

inline void TryInstallOnce()
{
    using namespace internal;
    HMODULE ngx = GetModuleHandleW(L"_nvngx.dll");
    if (!ngx)
        ngx = GetModuleHandleW(L"nvngx.dll");
    if (!ngx || !GetProcAddress(ngx, "NVSDK_NGX_D3D12_GetCapabilityParameters"))
        return;
    HMODULE pin = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(ngx), &pin);
    const struct { const char* name; void** real; void* replacement; } exports[] = {
        {"NVSDK_NGX_D3D12_GetParameters", reinterpret_cast<void**>(&g_params), reinterpret_cast<void*>(&Parameters)},
        {"NVSDK_NGX_D3D12_GetCapabilityParameters", reinterpret_cast<void**>(&g_caps), reinterpret_cast<void*>(&Capabilities)},
        {"NVSDK_NGX_D3D12_CreateFeature", reinterpret_cast<void**>(&g_create), reinterpret_cast<void*>(&Create)},
        {"NVSDK_NGX_D3D12_EvaluateFeature", reinterpret_cast<void**>(&g_eval), reinterpret_cast<void*>(&Evaluate)},
        {"NVSDK_NGX_D3D12_ReleaseFeature", reinterpret_cast<void**>(&g_release), reinterpret_cast<void*>(&Release)},
    };
    std::vector<std::pair<void**, void*>> hooks;
    for (const auto& item : exports)
    {
        void* address = reinterpret_cast<void*>(GetProcAddress(ngx, item.name));
        if (!address)
        {
            Message("[DLSS-SR] NGX core is missing an export; render-scale override unavailable.");
            g_installed.store(true);  // do not retry every tick
            return;
        }
        *item.real = address;
        hooks.emplace_back(item.real, item.replacement);
    }
    const bool ok = AttachAll(hooks);
    if (!ok)
        for (const auto& item : exports) *item.real = nullptr;
    g_installed.store(true);
    Message(ok ? "[DLSS-SR] NGX Super Resolution hooks installed (render-scale override available)."
               : "[DLSS-SR] NGX Super Resolution hooks could not be installed.");
}
inline bool Hooked() { return internal::g_eval != nullptr; }

} // namespace dlss_sr
