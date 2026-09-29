/*
 * Automatic HUD-less capture and UI layer synthesis for Streamline DLSS-G on
 * D3D12.
 * SPDX-License-Identifier: MIT
 *
 * Ported from dlssg_for_sm86 src/companion/hudless_capture.hpp (feat/0.3.5,
 * 2bff790). Changes: Transfusion logging, Detours installation, plain buffer
 * names, UIAlpha (not Alpha) counts as a game UI layer. Feeds Transfusion's
 * own UIR readiness (Preset B) through the tags it submits.
 *
 * HUD-LESS CAPTURE
 * Some games give FSR/XeFG a scene-without-UI image but tag no HUDLessColor
 * for DLSS-G. With DLSS-G active, Crimson Desert composites its final scene
 * straight into Streamline's fake swapchain buffer and then draws the UI into
 * that same buffer; with FSR/XeFG the same pass writes the declared HUD-less
 * texture instead. The buffer between those two steps is the HUD-less scene.
 * This module copies it at that boundary and tags the copy as HUDLessColor.
 *
 * Composite rule, validated in Crimson Desert with tools/hudless_probe. On one
 * command list:
 *   1. a full-size texture becomes readable (render target/UAV -> shader
 *      resource): the finished scene the composite will sample;
 *   2. the bound fake swapchain buffer becomes a render target;
 *   3. one or two full-screen draws (<= 6 vertices, one instance) follow,
 *      without a target change in between;
 *   4. the list binds other targets, moves the buffer out of RENDER_TARGET or
 *      closes: the buffer, necessarily in RENDER_TARGET, is the HUD-less scene.
 * Anything else captures nothing: a scene drawn directly into the buffer, UI
 * drawn first, or a transition before the bind (descriptor handles do not
 * identify resources, so a target change before any draw disarms). Once a
 * game matched the rule recently, a frame that does not match (menu, map,
 * cutscene) tags its finished image instead, so Streamline never reuses a
 * stale scene: HUD-less equal to final means "no UI to extract".
 *
 * UI LAYER SYNTHESIS
 * With a HUD-less scene but no UI alpha, DLSS-G extracts the UI as final minus
 * HUD-less. Behind translucent UI that difference still contains the real
 * frame's background, which DLSS-G then adds over the interpolated scene: a
 * double image behind panels and minimaps (Cyberpunk 2077, Crimson Desert).
 * Once the frame's final image is complete (a presentable buffer leaves
 * RENDER_TARGET for PRESENT), a compute pass on our own command list, submitted
 * right after the game's, builds a premultiplied UI layer
 *     alpha = where final and HUD-less differ (above dithering, dilated 3x3)
 *     UI    = final - (1 - alpha) * HUD-less
 * and tags it as UIColorAndAlpha. Real frames are reproduced exactly; covered
 * pixels keep the finished image on generated frames instead of mixing two
 * backgrounds. The HUD-less source is our capture or a copy of the game's own
 * tag. A frame whose estimated UI covers more than 40% of the screen (post
 * effects after the UI) pauses the synthesis until coverage falls below 30%.
 *
 * Presentable buffers are learned from Streamline itself: at present, DLSS-G
 * copies the final image into its own "nv.sl.dlss_g.clone..." textures
 * (observed with Streamline 2.7 in Cyberpunk and 2.14 in Crimson Desert), so
 * the source of such a copy is the final image whatever the game names it.
 * Learned buffers expire when Streamline stops copying them (resize).
 * Streamline's fake-buffer name and a "Swapchain" name prefix are recognized
 * from the first frame.
 *
 * Both follow "uiAssist" (on by default), on D3D12 only, never with frame-based
 * tagging. The capture never runs while the game tags its own HUD-less scene;
 * the synthesis never runs while the game tags its own UI layer or alpha.
 * Hooks install once DLSS-G tags arrived for a while.
 */
#pragma once

#include <windows.h>
#include <d3d12.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <sstream>
#include <vector>

#include <sl.h>

#include "detours/detours.h"
#include "ui_synthesis_cs.h"  // compiled from shaders/ui_synthesis.hlsl

namespace hud_assist {

// Set by Transfusion to its log; the codes below are listed in docs/HUD-ASSIST.fr.md.
inline void (*g_log)(const char* text) = nullptr;
inline void Message(const char* text) { if (g_log) g_log(text); }
inline std::atomic_bool g_log_hud_ui{false};
inline std::atomic<uint32_t> g_game_hudless_trace_count{0};
inline void SetLogHudUi(bool enabled) {
  if (enabled && !g_log_hud_ui.load(std::memory_order_relaxed))
    g_game_hudless_trace_count.store(0, std::memory_order_relaxed);
  g_log_hud_ui.store(enabled, std::memory_order_relaxed);
}

// Set once by the runtime: false for Vulkan, where a tag's native handle is a
// VkImage rather than an ID3D12Resource.
inline std::atomic_bool g_d3d12{false};
// Updated with every game tag batch from the UI recomposition mode.
inline std::atomic_bool g_allowed{false};
inline std::atomic_bool g_install_started{false};
inline std::atomic_bool g_installed{false};
inline std::atomic_bool g_frame_based_tags{false};
inline std::atomic<uint32_t> g_viewport{0};
inline std::atomic<uint32_t> g_dlssg_batches{0};
inline std::atomic<ULONGLONG> g_game_hudless_tick{0};
inline std::atomic<ULONGLONG> g_game_ui_tick{0};
inline std::atomic<ULONGLONG> g_scene_capture_tick{0};
inline std::atomic<ULONGLONG> g_hudless_copy_tick{0};
inline std::atomic<uint64_t> g_captures{0};
inline std::atomic<uint64_t> g_scene_captures{0};
inline std::atomic<uint64_t> g_ui_layers{0};
inline std::atomic<ULONGLONG> g_ui_layer_tick{0};  // last synthesized UI layer submitted
inline std::atomic<uint64_t> g_tag_failures{0};
inline std::atomic_bool g_ui_paused{false};
// Set by framecount: submits one tag straight to Streamline and records it as
// a game UI input. Returns true when Streamline accepted it.
inline bool (*g_submit)(uint32_t viewport, const sl::ResourceTag& tag, sl::CommandBuffer* list) = nullptr;

constexpr uint32_t kBatchesBeforeInstall = 300;  // about five seconds of DLSS-G frames
constexpr ULONGLONG kGameInputHoldMs = 2000;
constexpr ULONGLONG kFallbackWindowMs = 5000;
constexpr ULONGLONG kHudlessFreshMs = 250;
constexpr uint32_t kMaxCompositeDraws = 2, kMaxCompositeVertices = 6;
constexpr double kPauseCoverage = 0.40, kResumeCoverage = 0.30;

inline bool Recent(const std::atomic<ULONGLONG>& tick, ULONGLONG window, ULONGLONG now = GetTickCount64()) {
  const ULONGLONG seen = tick.load(std::memory_order_relaxed);
  return seen && now - seen < window;
}
inline bool GameProvidesHudless() { return Recent(g_game_hudless_tick, kGameInputHoldMs); }
inline bool GameProvidesUi() { return Recent(g_game_ui_tick, kGameInputHoldMs); }

namespace internal {

using BarrierFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_RESOURCE_BARRIER*);
using SetTargetsFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*,
                                              BOOL, const D3D12_CPU_DESCRIPTOR_HANDLE*);
using DrawFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, UINT);
using DrawIndexedFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, INT, UINT);
using CloseFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*);
using EnhancedBarrierFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList7*, UINT32, const D3D12_BARRIER_GROUP*);
using ExecuteFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using CopyRegionFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, const D3D12_TEXTURE_COPY_LOCATION*, UINT, UINT,
                                              UINT, const D3D12_TEXTURE_COPY_LOCATION*, const D3D12_BOX*);
using CopyResourceFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*);

// ID3D12GraphicsCommandList7 and ID3D12CommandQueue vtable slots (d3d12.h
// declaration order, checked with CINTERFACE offsetof on SDK 10.0.28000).
constexpr size_t kCloseSlot = 9, kDrawSlot = 12, kDrawIndexedSlot = 13, kCopyRegionSlot = 16, kCopyResourceSlot = 17,
                 kBarrierSlot = 26, kSetTargetsSlot = 46, kEnhancedBarrierSlot = 80, kExecuteSlot = 10;

inline BarrierFn g_real_barrier = nullptr;
inline SetTargetsFn g_real_set_targets = nullptr;
inline DrawFn g_real_draw = nullptr;
inline DrawIndexedFn g_real_draw_indexed = nullptr;
inline CloseFn g_real_close = nullptr;
inline EnhancedBarrierFn g_real_enhanced_barrier = nullptr;
inline ExecuteFn g_real_execute = nullptr;
inline CopyRegionFn g_real_copy_region = nullptr;
inline CopyResourceFn g_real_copy_resource = nullptr;

// A list is recorded by one thread at a time, so its state lives with the
// recording thread. A period is one RENDER_TARGET span of a swapchain buffer.
struct Period {
  ID3D12GraphicsCommandList* list = nullptr;
  ID3D12Resource* target = nullptr;
  UINT64 width = 0;
  UINT height = 0;
  bool enhanced = false;   // the game moves this buffer with enhanced barriers
  bool armed = false;      // the composite may still be in progress
  bool source = false;     // a full-size texture became readable first
  bool composite = true;   // every draw so far looked like a full-screen pass
  bool captured = false;   // this period already produced a tag
  uint32_t draws = 0;
};
struct Readable {
  ID3D12GraphicsCommandList* list = nullptr;
  UINT64 width = 0;
  UINT height = 0;
};
inline thread_local Period t_period;
inline thread_local Readable t_readable;  // last full-size texture made readable
inline thread_local bool t_inside = false;  // our own and Streamline's commands

// One-time diagnostics: which step of the rules a game reaches.
inline std::atomic_bool g_logged_live{false}, g_logged_legacy_buffer{false}, g_logged_enhanced_buffer{false},
    g_logged_armed{false}, g_logged_rejected{false}, g_logged_fallback{false}, g_logged_game_hudless{false},
    g_logged_game_ui{false}, g_logged_final{false}, g_logged_learned{false};
inline void LogOnce(std::atomic_bool& flag, const char* text) {
  if (!flag.exchange(true)) Message(text);
}

// WKPDID_D3DDebugObjectNameW / WKPDID_D3DDebugObjectName (d3dcommon.h),
// defined here to avoid linking dxguid.lib.
inline constexpr GUID kDebugObjectNameW = {0x4cca5fd8, 0x921f, 0x42c8, {0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41}};
inline constexpr GUID kDebugObjectName = {0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}};

// Presentable buffers are recognized by debug name: Streamline names its fake
// swapchain buffers; some games rename them (Cyberpunk: "Swapchain[n].Buffer[i]").
enum class Presentable : uint8_t { none, streamline, game, clone };

inline Presentable ClassifyName(ID3D12Resource* resource) {
  char name[128] = {};
  wchar_t wide[128] = {};
  UINT size = sizeof(wide) - sizeof(wchar_t);
  if (SUCCEEDED(resource->GetPrivateData(kDebugObjectNameW, &size, wide))) {
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, name, sizeof(name) - 1, nullptr, nullptr);
  } else {
    size = sizeof(name) - 1;
    if (FAILED(resource->GetPrivateData(kDebugObjectName, &size, name))) return Presentable::none;
  }
  static constexpr char kFake[] = "fake-swapchain-buffer", kClone[] = "nv.sl.dlss_g.clone", kGame[] = "Swapchain";
  if (strstr(name, kFake)) return Presentable::streamline;
  if (strncmp(name, kClone, sizeof(kClone) - 1) == 0) return Presentable::clone;
  if (strncmp(name, kGame, sizeof(kGame) - 1) == 0) return Presentable::game;
  return Presentable::none;
}

// Classifications are stored on the resource itself (private data under our
// GUID), never keyed by address: a new resource reusing a freed address carries
// no mark. A negative result is rechecked after two seconds, in case the name
// is set after the first barrier.
inline constexpr GUID kNameMark = {0x6c2d1f0e, 0x93a4, 0x4b7e, {0x8d, 0x51, 0x0f, 0x3a, 0x77, 0xe2, 0x19, 0xc4}};
inline constexpr GUID kLearnedMark = {0x2b9e7a41, 0x5d0c, 0x4e63, {0xa1, 0x8f, 0x64, 0x0b, 0xd3, 0x52, 0xce, 0x07}};
struct Mark {
  uint32_t kind = 0;
  uint32_t reserved = 0;
  ULONGLONG tick = 0;
};
inline bool ReadMark(ID3D12Resource* resource, const GUID& key, Mark& mark) {
  UINT size = sizeof(mark);
  return SUCCEEDED(resource->GetPrivateData(key, &size, &mark)) && size == sizeof(mark);
}
inline Presentable ClassifyBuffer(ID3D12Resource* resource) {
  if (!resource) return Presentable::none;
  const ULONGLONG now = GetTickCount64();
  Mark mark;
  if (ReadMark(resource, kNameMark, mark) &&
      (mark.kind != static_cast<uint32_t>(Presentable::none) || now - mark.tick < 2000))
    return static_cast<Presentable>(mark.kind);
  const Presentable kind = ClassifyName(resource);
  mark = {static_cast<uint32_t>(kind), 0, now};
  resource->SetPrivateData(kNameMark, sizeof(mark), &mark);
  return kind;
}
// The capture rule is validated on Streamline's own buffers only.
inline bool IsFakeSwapchainBuffer(ID3D12Resource* resource) {
  return ClassifyBuffer(resource) == Presentable::streamline;
}

// Buffers Streamline copied into its clones recently. The mark lives on the
// resource and expires when Streamline stops copying it (resize).
constexpr ULONGLONG kLearnedExpiryMs = 2000;

inline void LearnPresentable(ID3D12Resource* source) {
  Mark mark;
  const bool known = ReadMark(source, kLearnedMark, mark);
  mark = {1, 0, GetTickCount64()};
  source->SetPrivateData(kLearnedMark, sizeof(mark), &mark);
  if (!known) LogOnce(g_logged_learned, "UI assist D2.");
}

inline bool IsLearnedPresentable(ID3D12Resource* resource) {
  Mark mark;
  return resource && ReadMark(resource, kLearnedMark, mark) && GetTickCount64() - mark.tick <= kLearnedExpiryMs;
}

inline bool IsPresentableBuffer(ID3D12Resource* resource) {
  const Presentable kind = ClassifyBuffer(resource);
  return kind == Presentable::streamline || kind == Presentable::game || IsLearnedPresentable(resource);
}

// A copy from a game texture into Streamline's clone of the same size is its
// present-time copy of the final image. Small copies (uploads) are skipped.
inline UINT64 g_learn_min_width = 640;
inline UINT g_learn_min_height = 360;
inline void ObserveCopy(ID3D12Resource* destination, ID3D12Resource* source) {
  if (t_inside || !destination || !source || !g_allowed.load(std::memory_order_relaxed)) return;
  const D3D12_RESOURCE_DESC to = destination->GetDesc(), from = source->GetDesc();
  if (to.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || from.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
      to.Width != from.Width || to.Height != from.Height || to.Width < g_learn_min_width || to.Height < g_learn_min_height)
    return;
  if (ClassifyBuffer(destination) != Presentable::clone) return;
  const Presentable kind = ClassifyBuffer(source);
  if (kind == Presentable::clone) return;
  LearnPresentable(source);
}

inline void Barrier(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                    D3D12_RESOURCE_STATES after) {
  D3D12_RESOURCE_BARRIER b{};
  b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition.pResource = resource;
  b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  b.Transition.StateBefore = before;
  b.Transition.StateAfter = after;
  g_real_barrier(list, 1, &b);
}

inline void TextureBarrier(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_BARRIER_SYNC sync_before,
                           D3D12_BARRIER_SYNC sync_after, D3D12_BARRIER_ACCESS access_before,
                           D3D12_BARRIER_ACCESS access_after, D3D12_BARRIER_LAYOUT before,
                           D3D12_BARRIER_LAYOUT after) {
  D3D12_TEXTURE_BARRIER b{};
  b.SyncBefore = sync_before;
  b.SyncAfter = sync_after;
  b.AccessBefore = access_before;
  b.AccessAfter = access_after;
  b.LayoutBefore = before;
  b.LayoutAfter = after;
  b.pResource = resource;
  b.Subresources.IndexOrFirstMipLevel = 0xffffffff;  // all subresources
  D3D12_BARRIER_GROUP group{D3D12_BARRIER_TYPE_TEXTURE, 1};
  group.pTextureBarriers = &b;
  g_real_enhanced_barrier(reinterpret_cast<ID3D12GraphicsCommandList7*>(list), 1, &group);
}

inline void EnhancedBarrier(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, bool to_copy) {
  if (to_copy)
    TextureBarrier(list, resource, D3D12_BARRIER_SYNC_RENDER_TARGET, D3D12_BARRIER_SYNC_COPY,
                   D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_ACCESS_COPY_SOURCE,
                   D3D12_BARRIER_LAYOUT_RENDER_TARGET, D3D12_BARRIER_LAYOUT_COPY_SOURCE);
  else
    TextureBarrier(list, resource, D3D12_BARRIER_SYNC_COPY, D3D12_BARRIER_SYNC_RENDER_TARGET,
                   D3D12_BARRIER_ACCESS_COPY_SOURCE, D3D12_BARRIER_ACCESS_RENDER_TARGET,
                   D3D12_BARRIER_LAYOUT_COPY_SOURCE, D3D12_BARRIER_LAYOUT_RENDER_TARGET);
}

// Our HUD-less copy stays in COPY_DEST: Streamline copies an eOnlyValidNow tag
// on the same list, the UI synthesis reads it, and the next frame writes it
// again. A replaced copy is released only after later captures.
inline SRWLOCK g_copy_lock = SRWLOCK_INIT;
inline ID3D12Resource* g_copy = nullptr;
inline D3D12_RESOURCE_DESC g_copy_desc{};
inline std::vector<std::pair<ID3D12Resource*, uint64_t>> g_retired;

inline void TraceGameHudless(uint32_t attempt, const char* step) {
  std::stringstream s;
  s << "UI assist TRACE game-hudless #" << attempt << ' ' << step;
  Message(s.str().c_str());
}

inline ID3D12Resource* CopyTarget(ID3D12Resource* source, const D3D12_RESOURCE_DESC& desc) {
  if (g_copy && g_copy_desc.Width == desc.Width && g_copy_desc.Height == desc.Height &&
      g_copy_desc.Format == desc.Format)
    return g_copy;
  ID3D12Device* device = nullptr;
  if (FAILED(source->GetDevice(IID_PPV_ARGS(&device)))) return nullptr;
  D3D12_RESOURCE_DESC copy = desc;
  copy.Flags = D3D12_RESOURCE_FLAG_NONE;
  const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
  ID3D12Resource* created = nullptr;
  const HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &copy,
                                                     D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                     IID_PPV_ARGS(&created));
  device->Release();
  if (FAILED(hr)) return nullptr;
  created->SetName(L"dlssg-transfusion.hudless");
  if (g_copy) g_retired.emplace_back(g_copy, g_captures.load() + 16);
  g_copy = created;
  g_copy_desc = copy;
  std::stringstream s;
  s << "UI assist C1 " << desc.Width << 'x' << desc.Height << " "
    << static_cast<unsigned>(desc.Format) << " created.";
  Message(s.str().c_str());
  return created;
}

inline void ReleaseRetired(uint64_t count) {
  for (size_t i = 0; i < g_retired.size();) {
    if (g_retired[i].second <= count) {
      g_retired[i].first->Release();
      g_retired.erase(g_retired.begin() + i);
    } else {
      ++i;
    }
  }
}

inline bool SingleSubresource2D(const D3D12_RESOURCE_DESC& desc) {
  return desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.SampleDesc.Count == 1 &&
         desc.MipLevels == 1 && desc.DepthOrArraySize == 1;
}

// Copies the buffer (in RENDER_TARGET) and tags the copy. `scene` is false for
// the finished-image fallback. Returns true when Streamline accepted the tag.
inline bool Capture(ID3D12GraphicsCommandList* list, ID3D12Resource* target, bool enhanced, bool scene) {
  if (!g_submit || !g_allowed.load(std::memory_order_relaxed) || GameProvidesHudless()) return false;
  if (enhanced && !g_real_enhanced_barrier) return false;
  const D3D12_RESOURCE_DESC desc = target->GetDesc();
  // CopyResource needs identical subresource layouts; swapchain buffers have one.
  if (!SingleSubresource2D(desc)) return false;
  bool accepted = false;
  t_inside = true;
  AcquireSRWLockExclusive(&g_copy_lock);
  if (ID3D12Resource* copy = CopyTarget(target, desc)) {
    // Move the buffer with the same barrier model the game uses for it.
    if (enhanced) EnhancedBarrier(list, target, true);
    else Barrier(list, target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    list->CopyResource(copy, target);
    if (enhanced) EnhancedBarrier(list, target, false);
    else Barrier(list, target, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    sl::Resource resource(sl::ResourceType::eTex2d, copy, D3D12_RESOURCE_STATE_COPY_DEST);
    resource.width = static_cast<uint32_t>(desc.Width);
    resource.height = desc.Height;
    resource.nativeFormat = static_cast<uint32_t>(desc.Format);
    resource.mipLevels = 1;
    resource.arrayLayers = 1;
    resource.flags = 0;
    const sl::Extent extent{0, 0, static_cast<uint32_t>(desc.Width), desc.Height};
    const sl::ResourceTag tag(&resource, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eOnlyValidNow, &extent);
    accepted = g_submit(g_viewport.load(std::memory_order_relaxed), tag, reinterpret_cast<sl::CommandBuffer*>(list));
    const uint64_t count = accepted ? g_captures.fetch_add(1) + 1 : g_captures.load();
    if (!accepted && g_tag_failures.fetch_add(1) == 0)
      Message("UI assist E1.");
    if (accepted && scene && g_scene_captures.fetch_add(1) == 0)
      Message("UI assist C2.");
    if (accepted) g_hudless_copy_tick.store(GetTickCount64(), std::memory_order_relaxed);
    ReleaseRetired(count);
  }
  ReleaseSRWLockExclusive(&g_copy_lock);
  t_inside = false;
  if (accepted && scene) g_scene_capture_tick.store(GetTickCount64(), std::memory_order_relaxed);
  return accepted;
}

// Copies the game's own HUD-less tag, on the list and in the state it declared,
// so the UI synthesis reads the scene of the same frame.
inline void CopyGameHudless(ID3D12GraphicsCommandList* list, ID3D12Resource* source, uint32_t state) {
  if (!g_installed.load(std::memory_order_acquire) || !list || !source || state == UINT_MAX) return;
  const D3D12_RESOURCE_DESC desc = source->GetDesc();
  if (!SingleSubresource2D(desc)) return;
  const auto declared = static_cast<D3D12_RESOURCE_STATES>(state);
  const uint32_t attempt = g_log_hud_ui.load(std::memory_order_relaxed)
      ? g_game_hudless_trace_count.fetch_add(1, std::memory_order_relaxed) + 1 : 0;
  const bool trace = attempt != 0 && attempt <= 3;
  if (trace) {
    std::stringstream s;
    s << "UI assist TRACE game-hudless #" << attempt << " G0 enter list=" << list
      << " source=" << source << " declaredState=0x" << std::hex << state
      << std::dec << " size=" << desc.Width << 'x' << desc.Height
      << " format=" << static_cast<unsigned>(desc.Format)
      << " flags=0x" << std::hex << static_cast<unsigned>(desc.Flags);
    Message(s.str().c_str());
  }
  t_inside = true;
  AcquireSRWLockExclusive(&g_copy_lock);
  if (trace) TraceGameHudless(attempt, "G1 lock acquired; before CopyTarget");
  if (ID3D12Resource* copy = CopyTarget(source, desc)) {
    if (trace) TraceGameHudless(attempt, "G2 CopyTarget returned; before source transition");
    if (declared != D3D12_RESOURCE_STATE_COPY_SOURCE) {
      if (trace) TraceGameHudless(attempt, "G3 before source barrier to COPY_SOURCE");
      Barrier(list, source, declared, D3D12_RESOURCE_STATE_COPY_SOURCE);
      if (trace) TraceGameHudless(attempt, "G4 source barrier to COPY_SOURCE returned");
    }
    if (trace) TraceGameHudless(attempt, "G5 before CopyResource");
    list->CopyResource(copy, source);
    if (trace) TraceGameHudless(attempt, "G6 CopyResource returned");
    if (declared != D3D12_RESOURCE_STATE_COPY_SOURCE) {
      if (trace) TraceGameHudless(attempt, "G7 before source barrier restore");
      Barrier(list, source, D3D12_RESOURCE_STATE_COPY_SOURCE, declared);
      if (trace) TraceGameHudless(attempt, "G8 source barrier restore returned");
    }
    g_hudless_copy_tick.store(GetTickCount64(), std::memory_order_relaxed);
    if (trace) TraceGameHudless(attempt, "G9 before retired-resource release");
    ReleaseRetired(g_captures.fetch_add(1) + 1);
    if (trace) TraceGameHudless(attempt, "G10 retired-resource release returned");
  }
  else if (trace) TraceGameHudless(attempt, "G2 CopyTarget failed");
  ReleaseSRWLockExclusive(&g_copy_lock);
  t_inside = false;
}

// ---- UI layer synthesis ----------------------------------------------------


// Shader views need a typed format; the copies keep the game's (possibly
// typeless) one. Both images are read as stored, so their encodings match.
inline DXGI_FORMAT ViewFormat(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default: return format;
  }
}

// A presentable buffer that just left RENDER_TARGET for present on a list:
// its final image is complete once that list has executed.
struct Final {
  ID3D12GraphicsCommandList* list = nullptr;
  ID3D12Resource* buffer = nullptr;
  bool enhanced = false;
  D3D12_BARRIER_LAYOUT layout = D3D12_BARRIER_LAYOUT_PRESENT;  // after the game's barrier
};
inline std::mutex g_final_lock;
inline std::vector<Final> g_finals;

inline void OnFinal(ID3D12GraphicsCommandList* list, ID3D12Resource* buffer, bool enhanced,
                    D3D12_BARRIER_LAYOUT layout) {
  if (!g_allowed.load(std::memory_order_relaxed) || GameProvidesUi()) return;
  LogOnce(g_logged_final, "UI assist D1.");
  std::lock_guard guard(g_final_lock);
  for (auto& f : g_finals)
    if (f.list == list) {
      f = {list, buffer, enhanced, layout};
      return;
    }
  if (g_finals.size() >= 16) g_finals.erase(g_finals.begin());
  g_finals.push_back({list, buffer, enhanced, layout});
}

struct Synthesis {
  bool initialized = false, failed = false;
  ID3D12Device* device = nullptr;
  ID3D12RootSignature* root = nullptr;
  ID3D12PipelineState* pso = nullptr;
  ID3D12DescriptorHeap* heap = nullptr;
  UINT increment = 0;
  ID3D12Fence* fence = nullptr;
  uint64_t fence_value = 0;
  ID3D12Resource* counter = nullptr;  // UAV, cleared from `zero` each frame
  ID3D12Resource* zero = nullptr;
  ID3D12Resource* ui = nullptr;
  D3D12_RESOURCE_DESC ui_desc{};
  struct Slot {
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Resource* readback = nullptr;
    uint64_t fence = 0;
    uint64_t pixels = 0;  // pixels of the frame whose coverage is in `readback`
  } slots[3];
  uint32_t next = 0;
};
inline std::mutex g_synthesis_lock;
inline Synthesis g_synthesis;

inline ID3D12Resource* Buffer(ID3D12Device* device, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state,
                              D3D12_RESOURCE_FLAGS flags) {
  D3D12_RESOURCE_DESC d{};
  d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  d.Width = 256;
  d.Height = d.DepthOrArraySize = d.MipLevels = 1;
  d.SampleDesc.Count = 1;
  d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  d.Flags = flags;
  const D3D12_HEAP_PROPERTIES heap{type};
  ID3D12Resource* r = nullptr;
  device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&r));
  return r;
}

inline bool InitializeSynthesis(Synthesis& s, ID3D12Device* device) {
  s.initialized = true;
  auto serialize = reinterpret_cast<decltype(&D3D12SerializeRootSignature)>(
      GetProcAddress(GetModuleHandleW(L"d3d12.dll"), "D3D12SerializeRootSignature"));
  ID3DBlob* root = nullptr;
  if (!serialize) return false;
  const D3D12_DESCRIPTOR_RANGE ranges[2] = {
      {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, 0},
      {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 2}};
  D3D12_ROOT_PARAMETER params[3]{};
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[0].DescriptorTable = {2, ranges};
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
  params[1].Descriptor = {1, 0};
  params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  params[2].Constants = {0, 0, 2};
  const D3D12_ROOT_SIGNATURE_DESC rs{3, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
  bool ok = SUCCEEDED(serialize(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &root, nullptr)) &&
            SUCCEEDED(device->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                                  IID_PPV_ARGS(&s.root)));
  if (ok) {
    D3D12_COMPUTE_PIPELINE_STATE_DESC p{};
    p.pRootSignature = s.root;
    p.CS = {kUiSynthesisCs, sizeof(kUiSynthesisCs)};
    ok = SUCCEEDED(device->CreateComputePipelineState(&p, IID_PPV_ARGS(&s.pso)));
  }
  if (root) root->Release();
  const D3D12_DESCRIPTOR_HEAP_DESC h{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 9,
                                     D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
  ok = ok && SUCCEEDED(device->CreateDescriptorHeap(&h, IID_PPV_ARGS(&s.heap))) &&
       SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&s.fence)));
  s.increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  s.counter = ok ? Buffer(device, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
                 : nullptr;
  s.zero = ok ? Buffer(device, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE)
              : nullptr;
  ok = ok && s.counter && s.zero;
  if (ok) {
    void* data = nullptr;
    ok = SUCCEEDED(s.zero->Map(0, nullptr, &data));
    if (ok) {
      std::memset(data, 0, 256);
      s.zero->Unmap(0, nullptr);
    }
  }
  for (auto& slot : s.slots) {
    ok = ok && SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&slot.allocator))) &&
         SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, slot.allocator, nullptr,
                                             IID_PPV_ARGS(&slot.list))) &&
         SUCCEEDED(slot.list->Close()) &&
         (slot.readback = Buffer(device, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST,
                                 D3D12_RESOURCE_FLAG_NONE)) != nullptr;
  }
  s.device = device;
  device->AddRef();
  return ok;
}

// The layer uses the final image's format so HDR values are not clipped;
// BGRA8 has no guaranteed typed UAV store, so it becomes RGBA8.
inline DXGI_FORMAT UiFormat(DXGI_FORMAT final_format) {
  const DXGI_FORMAT view = ViewFormat(final_format);
  switch (view) {
    case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R10G10B10A2_UNORM: return view;
    default: return DXGI_FORMAT_R8G8B8A8_UNORM;
  }
}

inline bool EnsureUiTexture(Synthesis& s, UINT64 width, UINT height, DXGI_FORMAT format) {
  if (s.ui && s.ui_desc.Width == width && s.ui_desc.Height == height && s.ui_desc.Format == format) return true;
  D3D12_RESOURCE_DESC d{};
  d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  d.Width = width;
  d.Height = height;
  d.DepthOrArraySize = d.MipLevels = 1;
  d.Format = format;
  d.SampleDesc.Count = 1;
  d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
  ID3D12Resource* created = nullptr;
  if (FAILED(s.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d,
                                               D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&created))))
    return false;
  created->SetName(L"dlssg-transfusion.ui");
  // Every slot that may still use the old layer has completed: all waited below.
  if (s.ui) s.ui->Release();
  s.ui = created;
  s.ui_desc = d;
  return true;
}

// Reads a slot's coverage count once its work completed and updates the pause.
inline void ReadCoverage(Synthesis::Slot& slot) {
  if (!slot.pixels) return;
  uint32_t* data = nullptr;
  const D3D12_RANGE range{0, 4};
  if (SUCCEEDED(slot.readback->Map(0, &range, reinterpret_cast<void**>(&data)))) {
    const double coverage = static_cast<double>(*data) / static_cast<double>(slot.pixels);
    const D3D12_RANGE none{0, 0};
    slot.readback->Unmap(0, &none);
    const bool paused = g_ui_paused.load(std::memory_order_relaxed);
    if (!paused && coverage > kPauseCoverage) {
      g_ui_paused = true;
      std::stringstream s;
      s << "UI assist D4 " << static_cast<int>(coverage * 100) << "%.";
      Message(s.str().c_str());
    } else if (paused && coverage < kResumeCoverage) {
      g_ui_paused = false;
      Message("UI assist D5.");
    }
  }
  slot.pixels = 0;
}

inline void Synthesize(ID3D12CommandQueue* queue, const Final& final) {
  if (!g_submit || !g_allowed.load(std::memory_order_relaxed) || GameProvidesUi() ||
      !Recent(g_hudless_copy_tick, kHudlessFreshMs))
    return;
  std::lock_guard guard(g_synthesis_lock);
  auto& s = g_synthesis;
  if (s.failed) return;
  if (!s.initialized) {
    ID3D12Device* device = nullptr;
    if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device)))) return;
    const bool ok = InitializeSynthesis(s, device);
    device->Release();
    if (!ok) {
      s.failed = true;
      Message("UI assist E2.");
      return;
    }
  }
  if (final.enhanced && !g_real_enhanced_barrier) return;
  const D3D12_RESOURCE_DESC final_desc = final.buffer->GetDesc();
  AcquireSRWLockShared(&g_copy_lock);
  ID3D12Resource* hudless = g_copy;
  const D3D12_RESOURCE_DESC hudless_desc = g_copy_desc;
  if (hudless) hudless->AddRef();
  ReleaseSRWLockShared(&g_copy_lock);
  if (!hudless) return;
  if (!SingleSubresource2D(final_desc) || hudless_desc.Width != final_desc.Width ||
      hudless_desc.Height != final_desc.Height) {
    hudless->Release();
    return;
  }

  auto& slot = s.slots[s.next];
  const uint32_t index = s.next;
  s.next = (s.next + 1) % 3;
  if (s.fence->GetCompletedValue() < slot.fence) {
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    s.fence->SetEventOnCompletion(slot.fence, event);
    WaitForSingleObject(event, 1000);
    CloseHandle(event);
  }
  ReadCoverage(slot);
  // The UI layer is recreated only when no slot can still be using it.
  const DXGI_FORMAT ui_format = UiFormat(final_desc.Format);
  if (!s.ui || s.ui_desc.Width != final_desc.Width || s.ui_desc.Height != final_desc.Height ||
      s.ui_desc.Format != ui_format) {
    if (s.fence->GetCompletedValue() < s.fence_value) {
      HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
      s.fence->SetEventOnCompletion(s.fence_value, event);
      WaitForSingleObject(event, 1000);
      CloseHandle(event);
    }
    if (!EnsureUiTexture(s, final_desc.Width, final_desc.Height, ui_format)) {
      hudless->Release();
      return;
    }
  }

  t_inside = true;
  auto* list = slot.list;
  slot.allocator->Reset();
  list->Reset(slot.allocator, nullptr);
  auto cpu = s.heap->GetCPUDescriptorHandleForHeapStart();
  auto gpu = s.heap->GetGPUDescriptorHandleForHeapStart();
  cpu.ptr += static_cast<SIZE_T>(index) * 3 * s.increment;
  gpu.ptr += static_cast<UINT64>(index) * 3 * s.increment;
  D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
  srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv.Texture2D.MipLevels = 1;
  srv.Format = ViewFormat(final_desc.Format);
  s.device->CreateShaderResourceView(final.buffer, &srv, cpu);
  cpu.ptr += s.increment;
  srv.Format = ViewFormat(hudless_desc.Format);
  s.device->CreateShaderResourceView(hudless, &srv, cpu);
  cpu.ptr += s.increment;
  D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
  uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
  uav.Format = ui_format;
  s.device->CreateUnorderedAccessView(s.ui, nullptr, &uav, cpu);

  // Read the finished image with the barrier model the game uses for it.
  if (final.enhanced)
    TextureBarrier(list, final.buffer, D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_SYNC_COMPUTE_SHADING,
                   D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_ACCESS_SHADER_RESOURCE, final.layout,
                   D3D12_BARRIER_LAYOUT_SHADER_RESOURCE);
  else
    Barrier(list, final.buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  Barrier(list, hudless, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  Barrier(list, s.counter, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
  list->CopyBufferRegion(s.counter, 0, s.zero, 0, 4);
  Barrier(list, s.counter, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

  ID3D12DescriptorHeap* heaps[] = {s.heap};
  list->SetDescriptorHeaps(1, heaps);
  list->SetComputeRootSignature(s.root);
  list->SetPipelineState(s.pso);
  list->SetComputeRootDescriptorTable(0, gpu);
  list->SetComputeRootUnorderedAccessView(1, s.counter->GetGPUVirtualAddress());
  const UINT size[2] = {static_cast<UINT>(final_desc.Width), final_desc.Height};
  list->SetComputeRoot32BitConstants(2, 2, size, 0);
  list->Dispatch((size[0] + 7) / 8, (size[1] + 7) / 8, 1);

  if (final.enhanced)
    TextureBarrier(list, final.buffer, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_NONE,
                   D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_ACCESS_NO_ACCESS,
                   D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, final.layout);
  else
    Barrier(list, final.buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT);
  Barrier(list, hudless, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
  D3D12_RESOURCE_BARRIER written{};
  written.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  written.UAV.pResource = s.ui;
  g_real_barrier(list, 1, &written);
  Barrier(list, s.counter, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  list->CopyBufferRegion(slot.readback, 0, s.counter, 0, 4);
  Barrier(list, s.counter, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  slot.pixels = static_cast<uint64_t>(size[0]) * size[1];

  bool accepted = false;
  if (!g_ui_paused.load(std::memory_order_relaxed)) {
    sl::Resource resource(sl::ResourceType::eTex2d, s.ui, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    resource.width = size[0];
    resource.height = size[1];
    resource.nativeFormat = ui_format;
    resource.mipLevels = 1;
    resource.arrayLayers = 1;
    resource.flags = 0;
    const sl::Extent extent{0, 0, size[0], size[1]};
    const sl::ResourceTag tag(&resource, sl::kBufferTypeUIColorAndAlpha, sl::ResourceLifecycle::eOnlyValidNow,
                              &extent);
    accepted = g_submit(g_viewport.load(std::memory_order_relaxed), tag, reinterpret_cast<sl::CommandBuffer*>(list));
    if (accepted) g_ui_layer_tick.store(GetTickCount64(), std::memory_order_relaxed);
    if (accepted && g_ui_layers.fetch_add(1) == 0)
      Message("UI assist D3.");
    if (!accepted && g_tag_failures.fetch_add(1) == 0)
      Message("UI assist E3.");
  }
  list->Close();
  ID3D12CommandList* lists[] = {list};
  g_real_execute(queue, 1, lists);
  slot.fence = ++s.fence_value;
  queue->Signal(s.fence, slot.fence);
  t_inside = false;
  hudless->Release();
}

// ---- Command-list hooks ----------------------------------------------------

// Ends the composite window: captures the scene if the rule matched. The
// period stays open until the buffer leaves RENDER_TARGET.
inline void EndComposite(ID3D12GraphicsCommandList* list) {
  auto& p = t_period;
  if (t_inside || p.list != list || !p.armed) return;
  p.armed = false;
  if (p.draws >= 1 && p.draws <= kMaxCompositeDraws && p.composite && p.source) {
    p.captured = Capture(list, p.target, p.enhanced, true);
  } else if (p.draws) {
    LogOnce(g_logged_rejected,
            "UI assist B4.");
  }
}

// Ends the period: a game that matched recently gets its finished image
// tagged instead, so Streamline never reuses a stale scene.
inline void EndPeriod(ID3D12GraphicsCommandList* list) {
  auto& p = t_period;
  if (t_inside || p.list != list) return;
  EndComposite(list);
  const Period ended = p;
  p = {};
  if (ended.captured) return;
  if (!Recent(g_scene_capture_tick, kFallbackWindowMs)) return;
  if (Capture(list, ended.target, ended.enhanced, false))
    LogOnce(g_logged_fallback,
            "UI assist C3.");
}

inline void BeginPeriod(ID3D12GraphicsCommandList* list, ID3D12Resource* target, bool enhanced) {
  if (t_period.list && t_period.list != list) t_period = {};  // a list left open on this thread
  const D3D12_RESOURCE_DESC desc = target->GetDesc();
  t_period = {list, target, desc.Width, desc.Height, enhanced, true, false, true, false, 0};
  t_period.source = t_readable.list == list && t_readable.width == desc.Width && t_readable.height == desc.Height;
  LogOnce(g_logged_armed, "UI assist B3.");
}

// A texture that stops being written and becomes shader-readable. A full-size
// one on this list, before the composite draws, is the scene it samples.
inline void BecameReadable(ID3D12GraphicsCommandList* list, ID3D12Resource* resource) {
  if (!resource) return;
  const D3D12_RESOURCE_DESC desc = resource->GetDesc();
  if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) return;
  t_readable = {list, desc.Width, desc.Height};
  auto& p = t_period;
  if (p.list == list && p.armed && !p.draws && desc.Width == p.width && desc.Height == p.height) p.source = true;
}

inline void CountDraw(ID3D12GraphicsCommandList* list, UINT vertices, UINT instances) {
  auto& p = t_period;
  if (p.list != list || !p.armed) return;
  ++p.draws;
  if (vertices > kMaxCompositeVertices || instances != 1) p.composite = false;
}

inline void STDMETHODCALLTYPE HookedBarrier(ID3D12GraphicsCommandList* list, UINT count,
                                            const D3D12_RESOURCE_BARRIER* barriers) {
  if (!t_inside && barriers) {
    LogOnce(g_logged_live, "UI assist A3.");
    constexpr auto written = D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    constexpr auto readable =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    for (UINT i = 0; i < count; ++i) {
      if (barriers[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) continue;
      const auto& t = barriers[i].Transition;
      const bool from_target = t.StateBefore & D3D12_RESOURCE_STATE_RENDER_TARGET;
      const bool to_target = t.StateAfter & D3D12_RESOURCE_STATE_RENDER_TARGET;
      if (t_period.list == list && t.pResource == t_period.target) {
        if (from_target && !to_target) EndPeriod(list);  // still RENDER_TARGET until this barrier runs
      } else if (to_target && !from_target && IsFakeSwapchainBuffer(t.pResource)) {
        LogOnce(g_logged_legacy_buffer, "UI assist B2.");
        BeginPeriod(list, t.pResource, false);
      } else if ((t.StateBefore & written) && (t.StateAfter & readable) && !(t.StateAfter & written)) {
        BecameReadable(list, t.pResource);
      }
      if (from_target && t.StateAfter == D3D12_RESOURCE_STATE_PRESENT && IsPresentableBuffer(t.pResource))
        OnFinal(list, t.pResource, false, D3D12_BARRIER_LAYOUT_PRESENT);
    }
  }
  g_real_barrier(list, count, barriers);
}

inline bool ReadableLayout(D3D12_BARRIER_LAYOUT layout) {
  return layout == D3D12_BARRIER_LAYOUT_SHADER_RESOURCE || layout == D3D12_BARRIER_LAYOUT_GENERIC_READ ||
         layout == D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE ||
         layout == D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ ||
         layout == D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_SHADER_RESOURCE ||
         layout == D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_GENERIC_READ;
}
inline bool WrittenLayout(D3D12_BARRIER_LAYOUT layout) {
  return layout == D3D12_BARRIER_LAYOUT_RENDER_TARGET || layout == D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS ||
         layout == D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS ||
         layout == D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_UNORDERED_ACCESS;
}
inline bool PresentLayout(D3D12_BARRIER_LAYOUT layout) {
  return layout == D3D12_BARRIER_LAYOUT_PRESENT || layout == D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON;
}

inline void STDMETHODCALLTYPE HookedEnhancedBarrier(ID3D12GraphicsCommandList7* list7, UINT32 count,
                                                    const D3D12_BARRIER_GROUP* groups) {
  auto* list = reinterpret_cast<ID3D12GraphicsCommandList*>(list7);
  if (!t_inside && groups) {
    for (UINT32 g = 0; g < count; ++g) {
      if (groups[g].Type != D3D12_BARRIER_TYPE_TEXTURE || !groups[g].pTextureBarriers) continue;
      for (UINT32 i = 0; i < groups[g].NumBarriers; ++i) {
        const auto& t = groups[g].pTextureBarriers[i];
        const bool from_target = t.LayoutBefore == D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        const bool to_target = t.LayoutAfter == D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        if (t_period.list == list && t.pResource == t_period.target) {
          if (from_target && !to_target) EndPeriod(list);  // still RENDER_TARGET until this barrier runs
        } else if (to_target && !from_target && IsFakeSwapchainBuffer(t.pResource)) {
          LogOnce(g_logged_enhanced_buffer,
                  "UI assist B1.");
          BeginPeriod(list, t.pResource, true);
        } else if (WrittenLayout(t.LayoutBefore) && ReadableLayout(t.LayoutAfter)) {
          BecameReadable(list, t.pResource);
        }
        if (from_target && PresentLayout(t.LayoutAfter) && IsPresentableBuffer(t.pResource))
          OnFinal(list, t.pResource, true, t.LayoutAfter);
      }
    }
  }
  g_real_enhanced_barrier(list7, count, groups);
}

inline void STDMETHODCALLTYPE HookedSetTargets(ID3D12GraphicsCommandList* list, UINT count,
                                               const D3D12_CPU_DESCRIPTOR_HANDLE* targets, BOOL range,
                                               const D3D12_CPU_DESCRIPTOR_HANDLE* depth) {
  EndComposite(list);
  g_real_set_targets(list, count, targets, range, depth);
}

inline void STDMETHODCALLTYPE HookedDraw(ID3D12GraphicsCommandList* list, UINT vertices, UINT instances,
                                         UINT first_vertex, UINT first_instance) {
  CountDraw(list, vertices, instances);
  g_real_draw(list, vertices, instances, first_vertex, first_instance);
}

inline void STDMETHODCALLTYPE HookedDrawIndexed(ID3D12GraphicsCommandList* list, UINT indices, UINT instances,
                                                UINT first_index, INT base_vertex, UINT first_instance) {
  CountDraw(list, indices, instances);
  g_real_draw_indexed(list, indices, instances, first_index, base_vertex, first_instance);
}

inline void STDMETHODCALLTYPE HookedCopyRegion(ID3D12GraphicsCommandList* list, const D3D12_TEXTURE_COPY_LOCATION* dst,
                                               UINT x, UINT y, UINT z, const D3D12_TEXTURE_COPY_LOCATION* src,
                                               const D3D12_BOX* box) {
  if (dst && src && dst->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX &&
      src->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX && !box && !x && !y && !z)
    ObserveCopy(dst->pResource, src->pResource);
  g_real_copy_region(list, dst, x, y, z, src, box);
}

inline void STDMETHODCALLTYPE HookedCopyResource(ID3D12GraphicsCommandList* list, ID3D12Resource* destination,
                                                 ID3D12Resource* source) {
  ObserveCopy(destination, source);
  g_real_copy_resource(list, destination, source);
}

inline HRESULT STDMETHODCALLTYPE HookedClose(ID3D12GraphicsCommandList* list) {
  // A composite still open here is a frame without UI: capture it. The period
  // itself ends on whichever list moves the buffer out of RENDER_TARGET.
  EndComposite(list);
  if (t_period.list == list) t_period = {};
  if (t_readable.list == list) t_readable = {};
  return g_real_close(list);
}

// The finished image exists on the GPU once the game's list executed, so our
// synthesis list goes right behind it on the same queue.
inline void STDMETHODCALLTYPE HookedExecute(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
  g_real_execute(queue, count, lists);
  if (t_inside || !lists) return;
  Final found{};
  {
    std::lock_guard guard(g_final_lock);
    for (UINT i = 0; i < count && !found.list; ++i)
      for (size_t j = 0; j < g_finals.size(); ++j)
        if (reinterpret_cast<ID3D12CommandList*>(g_finals[j].list) == lists[i]) {
          found = g_finals[j];
          g_finals.erase(g_finals.begin() + j);
          break;
        }
  }
  if (found.list && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) Synthesize(queue, found);
}

inline bool InstallDetours(const std::vector<std::pair<void**, void*>>& hooks) {
  if (DetourTransactionBegin() != NO_ERROR) return false;
  DetourUpdateThread(GetCurrentThread());
  for (const auto& [real, hook] : hooks)
    if (DetourAttach(real, hook) != NO_ERROR) {
      DetourTransactionAbort();
      return false;
    }
  return DetourTransactionCommit() == NO_ERROR;
}

// Command-list and queue methods live in the D3D12 runtime and are shared by
// every object of the device, including ones wrapped by overlays, so hooking
// the vtable targets of throwaway objects covers the game's.
inline DWORD WINAPI InstallThread(LPVOID parameter) {
  auto* device = static_cast<ID3D12Device*>(parameter);
  ID3D12CommandAllocator* allocator = nullptr;
  ID3D12GraphicsCommandList* list = nullptr;
  ID3D12CommandQueue* queue = nullptr;
  const D3D12_COMMAND_QUEUE_DESC queue_desc{D3D12_COMMAND_LIST_TYPE_DIRECT};
  bool ok = SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) &&
            SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr,
                                                IID_PPV_ARGS(&list))) &&
            SUCCEEDED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)));
  if (ok) {
    void** table = *reinterpret_cast<void***>(list);
    g_real_close = reinterpret_cast<CloseFn>(table[kCloseSlot]);
    g_real_draw = reinterpret_cast<DrawFn>(table[kDrawSlot]);
    g_real_draw_indexed = reinterpret_cast<DrawIndexedFn>(table[kDrawIndexedSlot]);
    g_real_barrier = reinterpret_cast<BarrierFn>(table[kBarrierSlot]);
    g_real_set_targets = reinterpret_cast<SetTargetsFn>(table[kSetTargetsSlot]);
    g_real_copy_region = reinterpret_cast<CopyRegionFn>(table[kCopyRegionSlot]);
    g_real_copy_resource = reinterpret_cast<CopyResourceFn>(table[kCopyResourceSlot]);
    g_real_execute = reinterpret_cast<ExecuteFn>((*reinterpret_cast<void***>(queue))[kExecuteSlot]);
    std::vector<std::pair<void**, void*>> hooks = {
        {reinterpret_cast<void**>(&g_real_close), reinterpret_cast<void*>(&HookedClose)},
        {reinterpret_cast<void**>(&g_real_draw), reinterpret_cast<void*>(&HookedDraw)},
        {reinterpret_cast<void**>(&g_real_draw_indexed), reinterpret_cast<void*>(&HookedDrawIndexed)},
        {reinterpret_cast<void**>(&g_real_barrier), reinterpret_cast<void*>(&HookedBarrier)},
        {reinterpret_cast<void**>(&g_real_set_targets), reinterpret_cast<void*>(&HookedSetTargets)},
        {reinterpret_cast<void**>(&g_real_execute), reinterpret_cast<void*>(&HookedExecute)},
        {reinterpret_cast<void**>(&g_real_copy_region), reinterpret_cast<void*>(&HookedCopyRegion)},
        {reinterpret_cast<void**>(&g_real_copy_resource), reinterpret_cast<void*>(&HookedCopyResource)}};
    // Slot 80 exists only when the runtime implements ID3D12GraphicsCommandList7.
    ID3D12GraphicsCommandList7* list7 = nullptr;
    if (SUCCEEDED(list->QueryInterface(IID_PPV_ARGS(&list7)))) {
      g_real_enhanced_barrier =
          reinterpret_cast<EnhancedBarrierFn>((*reinterpret_cast<void***>(list7))[kEnhancedBarrierSlot]);
      hooks.push_back({reinterpret_cast<void**>(&g_real_enhanced_barrier),
                       reinterpret_cast<void*>(&HookedEnhancedBarrier)});
      list7->Release();
    }
    list->Close();
    ok = InstallDetours(hooks);
  }
  if (queue) queue->Release();
  if (list) list->Release();
  if (allocator) allocator->Release();
  device->Release();
  g_installed.store(ok, std::memory_order_release);
  if (!ok)
    Message("UI assist E4.");
  return 0;
}

}  // namespace internal

// Called with every accepted game tag batch. `allowed` reflects the UI
// recomposition mode; `frame_based` marks slSetTagForFrame, whose games get
// neither capture nor synthesis because both tag through slSetTag. Records
// the game's own HUD-less/UI inputs, copies its HUD-less scene for the UI
// synthesis and, after enough DLSS-G batches, starts the one-time hook
// installation off the game's render thread.
inline void ObserveGameTags(uint32_t viewport, const sl::ResourceTag* tags, uint32_t count, bool allowed,
                            bool frame_based, sl::CommandBuffer* list = nullptr) {
  g_allowed.store(allowed && g_d3d12.load(std::memory_order_relaxed) && !frame_based, std::memory_order_relaxed);
  if (frame_based) g_frame_based_tags.store(true, std::memory_order_relaxed);
  if (!g_d3d12.load(std::memory_order_relaxed) || internal::t_inside || !tags) return;
  // A game can put HUD-less before its own UI layer in one slSetTag batch.
  // Decide whether we need a copy only after checking the whole batch.
  bool batch_has_ui = false;
  for (uint32_t i = 0; i < count; ++i) {
    const auto& t = tags[i];
    if ((t.type == sl::kBufferTypeUIColorAndAlpha || t.type == sl::kBufferTypeUIAlpha) &&
        t.resource && t.resource->native) {
      batch_has_ui = true;
      break;
    }
  }
  ID3D12Resource* resource = nullptr;
  bool depth = false;
  for (uint32_t i = 0; i < count; ++i) {
    const auto& t = tags[i];
    const bool present = t.resource && t.resource->native;
    if (t.type == sl::kBufferTypeHUDLessColor && present) {
      g_game_hudless_tick.store(GetTickCount64(), std::memory_order_relaxed);
      internal::LogOnce(internal::g_logged_game_hudless,
                        "UI assist A0.");
      if (allowed && !frame_based && !batch_has_ui && !GameProvidesUi())
        internal::CopyGameHudless(reinterpret_cast<ID3D12GraphicsCommandList*>(list),
                                  static_cast<ID3D12Resource*>(t.resource->native), t.resource->state);
    }
    if ((t.type == sl::kBufferTypeUIColorAndAlpha || t.type == sl::kBufferTypeUIAlpha) && present) {
      g_game_ui_tick.store(GetTickCount64(), std::memory_order_relaxed);
      internal::LogOnce(internal::g_logged_game_ui,
                        "UI assist A1.");
    }
    if (t.type == sl::kBufferTypeDepth) {
      depth = true;
      g_viewport.store(viewport, std::memory_order_relaxed);
    }
    if (!resource && present) resource = static_cast<ID3D12Resource*>(t.resource->native);
  }
  if (!allowed || frame_based || g_frame_based_tags.load(std::memory_order_relaxed) || !resource || !depth) return;
  if (g_dlssg_batches.fetch_add(1) + 1 < kBatchesBeforeInstall) return;
  if (g_install_started.exchange(true)) return;
  ID3D12Device* device = nullptr;
  if (FAILED(resource->GetDevice(IID_PPV_ARGS(&device)))) {
    g_install_started.store(false);
    return;
  }
  Message("UI assist A2.");
  HANDLE thread = CreateThread(nullptr, 0, internal::InstallThread, device, 0, nullptr);
  if (thread) {
    CloseHandle(thread);
  } else {
    device->Release();
    Message("UI assist E5.");
  }
}

}  // namespace hud_assist
