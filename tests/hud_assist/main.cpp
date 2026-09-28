// GPU tests of the automatic HUD-less capture and UI layer synthesis on a real
// (ported from dlssg_for_sm86 tests/companion/hudless_capture_tests.cpp)
// D3D12 device. Frames replay the structure recorded in Crimson Desert: the
// finished scene becomes readable, the bound fake swapchain buffer becomes a
// render target, one full-screen composite draw paints the green scene, another
// target is bound (the boundary), then UI is drawn (an opaque blue rectangle
// and a 50% blue band) before the buffer returns to present.
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "hud_assist.h"

namespace hl = hud_assist;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

struct TagRecord {
  unsigned calls = 0, lifecycle = 0, state = 0, width = 0, height = 0, format = 0, viewport = 0;
  void* native = nullptr;
};
static TagRecord hudless_tags, ui_tags;
static bool Submit(uint32_t viewport, const sl::ResourceTag& tag, sl::CommandBuffer*) {
  auto& r = tag.type == sl::kBufferTypeUIColorAndAlpha ? ui_tags : hudless_tags;
  CHECK(tag.type == sl::kBufferTypeUIColorAndAlpha || tag.type == sl::kBufferTypeHUDLessColor);
  r = {r.calls + 1, static_cast<unsigned>(tag.lifecycle), tag.resource->state, tag.resource->width,
       tag.resource->height, tag.resource->nativeFormat, viewport, tag.resource->native};
  return true;
}

static const char* kShader = R"(
float4 vs(uint id : SV_VertexID) : SV_Position {
  float2 uv = float2((id << 1) & 2, id & 2);
  return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 scene() : SV_Target { return float4(0, 1, 0, 1); }
float4 ui() : SV_Target { return float4(0, 0, 1, 0.5); }
)";

constexpr UINT kSize = 64;
constexpr uint32_t kGreen = 0xFF00FF00u, kBlue = 0xFFFF0000u;  // RGBA8, little endian
constexpr D3D12_RECT kOpaque{8, 8, 24, 24}, kBand{36, 8, 52, 56};

struct Gpu {
  ID3D12Device* device = nullptr;
  ID3D12CommandQueue* queue = nullptr;
  ID3D12CommandAllocator* allocator = nullptr;
  ID3D12GraphicsCommandList* list = nullptr;
  ID3D12Fence* fence = nullptr;
  uint64_t value = 0;
  ID3D12RootSignature* root = nullptr;
  ID3D12PipelineState *scene = nullptr, *ui = nullptr;
  ID3D12DescriptorHeap* rtv = nullptr;
  UINT rtv_size = 0;
  void Run() {
    list->Close();
    ID3D12CommandList* lists[] = {list};
    queue->ExecuteCommandLists(1, lists);
    queue->Signal(fence, ++value);
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(value, event);
    WaitForSingleObject(event, INFINITE);
    CloseHandle(event);
    allocator->Reset();
    list->Reset(allocator, nullptr);
  }
  D3D12_CPU_DESCRIPTOR_HANDLE Rtv(UINT index) {
    auto h = rtv->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(index) * rtv_size;
    return h;
  }
};

static ID3D12Resource* Texture(Gpu& gpu, const wchar_t* name, D3D12_RESOURCE_STATES state) {
  D3D12_RESOURCE_DESC d{};
  d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  d.Width = d.Height = kSize;
  d.DepthOrArraySize = d.MipLevels = 1;
  d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  d.SampleDesc.Count = 1;
  d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
  ID3D12Resource* r = nullptr;
  gpu.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&r));
  if (r && *name) r->SetName(name);
  return r;
}

static void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* r, D3D12_RESOURCE_STATES a,
                       D3D12_RESOURCE_STATES b) {
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, a, b};
  list->ResourceBarrier(1, &barrier);
}

// Reads every RGBA8 pixel of a 2D texture.
static std::vector<uint32_t> Read(Gpu& gpu, ID3D12Resource* source, D3D12_RESOURCE_STATES state) {
  const D3D12_RESOURCE_DESC desc = source->GetDesc();
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
  UINT rows = 0;
  UINT64 row = 0, total = 0;
  gpu.device->GetCopyableFootprints(&desc, 0, 1, 0, &fp, &rows, &row, &total);
  D3D12_RESOURCE_DESC buffer{};
  buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  buffer.Width = total;
  buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
  buffer.SampleDesc.Count = 1;
  buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_READBACK};
  ID3D12Resource* readback = nullptr;
  gpu.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                      IID_PPV_ARGS(&readback));
  Transition(gpu.list, source, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_TEXTURE_COPY_LOCATION dst{readback, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
  dst.PlacedFootprint = fp;
  D3D12_TEXTURE_COPY_LOCATION src{source, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
  src.SubresourceIndex = 0;
  gpu.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
  Transition(gpu.list, source, D3D12_RESOURCE_STATE_COPY_SOURCE, state);
  gpu.Run();
  std::vector<uint32_t> pixels(static_cast<size_t>(desc.Width) * desc.Height);
  uint8_t* data = nullptr;
  readback->Map(0, nullptr, reinterpret_cast<void**>(&data));
  for (UINT y = 0; y < rows; ++y) std::memcpy(&pixels[y * desc.Width], data + y * fp.Footprint.RowPitch, desc.Width * 4);
  readback->Unmap(0, nullptr);
  readback->Release();
  return pixels;
}

static bool Uniform(const std::vector<uint32_t>& pixels, uint32_t value) {
  for (uint32_t p : pixels)
    if (p != value) return false;
  return true;
}
static bool CopyIs(Gpu& gpu, uint32_t expected) {
  return Uniform(Read(gpu, hl::internal::g_copy, D3D12_RESOURCE_STATE_COPY_DEST), expected);
}

static bool Setup(Gpu& gpu) {
  if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&gpu.device)))) return false;
  D3D12_COMMAND_QUEUE_DESC q{D3D12_COMMAND_LIST_TYPE_DIRECT};
  gpu.device->CreateCommandQueue(&q, IID_PPV_ARGS(&gpu.queue));
  gpu.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&gpu.allocator));
  gpu.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, gpu.allocator, nullptr, IID_PPV_ARGS(&gpu.list));
  gpu.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gpu.fence));
  D3D12_DESCRIPTOR_HEAP_DESC h{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4};
  gpu.device->CreateDescriptorHeap(&h, IID_PPV_ARGS(&gpu.rtv));
  gpu.rtv_size = gpu.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
  auto compile = compiler ? reinterpret_cast<pD3DCompile>(GetProcAddress(compiler, "D3DCompile")) : nullptr;
  if (!compile) return false;
  ID3DBlob *vs = nullptr, *scene = nullptr, *ui = nullptr, *root = nullptr;
  compile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "vs", "vs_5_0", 0, 0, &vs, nullptr);
  compile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "scene", "ps_5_0", 0, 0, &scene, nullptr);
  compile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "ui", "ps_5_0", 0, 0, &ui, nullptr);
  auto serialize = reinterpret_cast<decltype(&D3D12SerializeRootSignature)>(
      GetProcAddress(GetModuleHandleW(L"d3d12.dll"), "D3D12SerializeRootSignature"));
  D3D12_ROOT_SIGNATURE_DESC rs{};
  if (!vs || !scene || !ui || !serialize || FAILED(serialize(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &root, nullptr)))
    return false;
  gpu.device->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(), IID_PPV_ARGS(&gpu.root));
  D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
  p.pRootSignature = gpu.root;
  p.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
  p.PS = {scene->GetBufferPointer(), scene->GetBufferSize()};
  p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  p.SampleMask = UINT_MAX;
  p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  p.NumRenderTargets = 1;
  p.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  p.SampleDesc.Count = 1;
  if (FAILED(gpu.device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&gpu.scene)))) return false;
  auto& blend = p.BlendState.RenderTarget[0];
  blend.BlendEnable = TRUE;
  blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
  blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  blend.BlendOp = D3D12_BLEND_OP_ADD;
  blend.SrcBlendAlpha = D3D12_BLEND_ONE;
  blend.DestBlendAlpha = D3D12_BLEND_ZERO;
  blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  p.PS = {ui->GetBufferPointer(), ui->GetBufferSize()};
  return SUCCEEDED(gpu.device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&gpu.ui)));
}

struct Scene {
  ID3D12Resource *fake = nullptr, *finished = nullptr, *other = nullptr;
};

enum class Ui { none, partial, full };

// One frame in the recorded order. `source` makes the finished scene readable
// before the composite; `ui` draws the UI after the boundary.
static void Frame(Gpu& gpu, const Scene& s, bool source, UINT composite_draws, Ui ui) {
  const float red[4] = {1, 0, 0, 1}, blue[4] = {0, 0, 1, 1};
  const D3D12_VIEWPORT viewport{0, 0, kSize, kSize, 0, 1};
  const D3D12_RECT all{0, 0, kSize, kSize};
  auto* list = gpu.list;
  gpu.device->CreateRenderTargetView(s.fake, nullptr, gpu.Rtv(0));
  const auto fake_rtv = gpu.Rtv(0), other_rtv = gpu.Rtv(1);
  list->OMSetRenderTargets(1, &fake_rtv, FALSE, nullptr);  // bound before its barrier, as in the game
  Transition(list, s.fake, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  if (source)
    Transition(list, s.finished, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  list->ClearRenderTargetView(fake_rtv, red, 0, nullptr);
  list->SetGraphicsRootSignature(gpu.root);
  list->SetPipelineState(gpu.scene);
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  list->RSSetViewports(1, &viewport);
  list->RSSetScissorRects(1, &all);
  for (UINT i = 0; i < composite_draws; ++i) list->DrawInstanced(3, 1, 0, 0);
  if (ui != Ui::none) {
    list->OMSetRenderTargets(1, &other_rtv, FALSE, nullptr);  // boundary
    list->OMSetRenderTargets(1, &fake_rtv, FALSE, nullptr);
    if (ui == Ui::full) {
      list->ClearRenderTargetView(fake_rtv, blue, 0, nullptr);
    } else {
      list->ClearRenderTargetView(fake_rtv, blue, 1, &kOpaque);
      list->SetPipelineState(gpu.ui);
      list->RSSetScissorRects(1, &kBand);
      list->DrawInstanced(3, 1, 0, 0);  // translucent band
    }
    list->OMSetRenderTargets(1, &other_rtv, FALSE, nullptr);
  }
  Transition(list, s.fake, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  if (source)
    Transition(list, s.finished, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
  gpu.Run();
}

static int Channel(uint32_t pixel, int shift) { return static_cast<int>((pixel >> shift) & 0xFF); }

// Checks the synthesized layer against the HUD-less copy and the final image:
// the layer reproduces the final image exactly, is empty away from the UI and
// fully covers the opaque rectangle and the translucent band.
static void CheckUiLayer(Gpu& gpu, ID3D12Resource* final_image) {
  const auto layer = Read(gpu, hl::internal::g_synthesis.ui, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  const auto hudless = Read(gpu, hl::internal::g_copy, D3D12_RESOURCE_STATE_COPY_DEST);
  const auto final_pixels = Read(gpu, final_image, D3D12_RESOURCE_STATE_PRESENT);
  int worst = 0, empty_away = 0, covered = 0;
  for (UINT y = 0; y < kSize; ++y)
    for (UINT x = 0; x < kSize; ++x) {
      const size_t i = y * kSize + x;
      const double a = Channel(layer[i], 24) / 255.0;
      for (int shift : {0, 8, 16}) {
        const double rebuilt = Channel(layer[i], shift) + (1 - a) * Channel(hudless[i], shift);
        worst = (std::max)(worst, static_cast<int>(std::lround(std::fabs(rebuilt - Channel(final_pixels[i], shift)))));
      }
      auto inside = [&](const D3D12_RECT& r, int margin) {
        return static_cast<LONG>(x) >= r.left - margin && static_cast<LONG>(x) < r.right + margin &&
               static_cast<LONG>(y) >= r.top - margin && static_cast<LONG>(y) < r.bottom + margin;
      };
      if (!inside(kOpaque, 2) && !inside(kBand, 2) && layer[i] == 0) ++empty_away;
      if ((inside(kOpaque, 0) || inside(kBand, 0)) && Channel(layer[i], 24) == 255) ++covered;
    }
  CHECK(worst <= 1);  // real frames are reproduced
  const int away = kSize * kSize - (16 + 4) * (16 + 4) - (16 + 4) * (48 + 4);
  CHECK(empty_away == away);
  CHECK(covered == 16 * 16 + 16 * 48);
  CHECK(Channel(layer[12 * kSize + 12], 16) == 255);  // opaque rectangle: pure UI colour
  CHECK(std::abs(Channel(layer[30 * kSize + 44], 8) - Channel(final_pixels[30 * kSize + 44], 8)) <= 1);  // band keeps final
}

static void GameTags(sl::BufferType type, ID3D12Resource* native, uint32_t state, ID3D12GraphicsCommandList* list) {
  sl::Resource resource(sl::ResourceType::eTex2d, native, state);
  const sl::ResourceTag tag(&resource, type, sl::ResourceLifecycle::eOnlyValidNow);
  hl::ObserveGameTags(0, &tag, 1, true, false, reinterpret_cast<sl::CommandBuffer*>(list));
}

int main() {
  Gpu gpu;
  if (!Setup(gpu)) {
    std::printf("SKIP no D3D12 hardware device or shader compiler\n");
    return 0;
  }
  Scene s{Texture(gpu, L"nv.sl.dlss_g.tex2d.fake-swapchain-buffer", D3D12_RESOURCE_STATE_PRESENT),
          Texture(gpu, L"game.finished-scene", D3D12_RESOURCE_STATE_RENDER_TARGET),
          Texture(gpu, L"ui.helper", D3D12_RESOURCE_STATE_RENDER_TARGET)};
  gpu.device->CreateRenderTargetView(s.other, nullptr, gpu.Rtv(1));

  hl::g_d3d12 = true;
  hl::g_allowed = true;
  hl::g_viewport = 7;
  hl::g_submit = &Submit;
  hl::internal::g_learn_min_width = hl::internal::g_learn_min_height = kSize;
  gpu.device->AddRef();  // released by the install thread
  hl::internal::InstallThread(gpu.device);
  CHECK(hl::g_installed.load());

  // 1. Composite, boundary, UI: one capture of the scene, then a UI layer.
  Frame(gpu, s, true, 1, Ui::partial);
  CHECK(hudless_tags.calls == 1);
  CHECK(hudless_tags.lifecycle == sl::ResourceLifecycle::eOnlyValidNow);
  CHECK(hudless_tags.state == D3D12_RESOURCE_STATE_COPY_DEST);
  CHECK(hudless_tags.width == kSize && hudless_tags.height == kSize && hudless_tags.format == DXGI_FORMAT_R8G8B8A8_UNORM);
  CHECK(hudless_tags.viewport == 7);
  CHECK(hudless_tags.native == hl::internal::g_copy && hudless_tags.native != s.fake);
  CHECK(CopyIs(gpu, kGreen));
  CHECK(ui_tags.calls == 1);
  CHECK(ui_tags.native == hl::internal::g_synthesis.ui);
  CHECK(ui_tags.state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && ui_tags.format == DXGI_FORMAT_R8G8B8A8_UNORM);
  CHECK(ui_tags.lifecycle == sl::ResourceLifecycle::eOnlyValidNow && ui_tags.viewport == 7);
  CheckUiLayer(gpu, s.fake);

  // 2. Two composite draws are still a composite.
  Frame(gpu, s, true, 2, Ui::partial);
  CHECK(hudless_tags.calls == 2 && CopyIs(gpu, kGreen) && ui_tags.calls == 2);

  // 3. Without a finished scene made readable first, the draws are not a
  //    composite; after a recent match the finished image is tagged instead.
  Frame(gpu, s, false, 1, Ui::partial);
  const auto final3 = Read(gpu, s.fake, D3D12_RESOURCE_STATE_PRESENT);
  CHECK(hudless_tags.calls == 3 && Read(gpu, hl::internal::g_copy, D3D12_RESOURCE_STATE_COPY_DEST) == final3);

  // 4. Three draws are not a composite either: finished-image fallback.
  Frame(gpu, s, true, 3, Ui::partial);
  CHECK(hudless_tags.calls == 4);

  // 5. A frame without UI ends at the transition for present: scene captured,
  //    and its synthesized layer is empty.
  Frame(gpu, s, true, 1, Ui::none);
  CHECK(hudless_tags.calls == 5 && CopyIs(gpu, kGreen));
  CHECK(Uniform(Read(gpu, hl::internal::g_synthesis.ui, D3D12_RESOURCE_STATE_UNORDERED_ACCESS), 0));

  // 6. Without a recent scene capture there is no fallback.
  const ULONGLONG last = hl::g_scene_capture_tick.exchange(0);
  Frame(gpu, s, false, 1, Ui::partial);
  CHECK(hudless_tags.calls == 5);
  hl::g_scene_capture_tick = last;

  // 7. A buffer that is not a presentable swapchain buffer is ignored.
  const unsigned ui_before_plain = ui_tags.calls;
  Scene plain = s;
  plain.fake = Texture(gpu, L"game.final", D3D12_RESOURCE_STATE_PRESENT);
  Frame(gpu, plain, true, 1, Ui::partial);
  CHECK(hudless_tags.calls == 5 && ui_tags.calls == ui_before_plain);

  // 7b. Once Streamline copied that buffer into its clone at present, it is
  //     the final image whatever its name, and gets a UI layer.
  {
    ID3D12Resource* clone = Texture(gpu, L"nv.sl.dlss_g.clone.dlfg-output_0", D3D12_RESOURCE_STATE_COPY_DEST);
    Transition(gpu.list, plain.fake, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
    gpu.list->CopyResource(clone, plain.fake);
    Transition(gpu.list, plain.fake, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
    gpu.Run();
    CHECK(hl::internal::IsPresentableBuffer(plain.fake));
    CHECK(!hl::internal::IsPresentableBuffer(clone));
    hl::g_hudless_copy_tick = GetTickCount64();  // the green scene copied earlier is this frame's HUD-less
    Frame(gpu, plain, true, 1, Ui::partial);
    CHECK(hudless_tags.calls == 5 && ui_tags.calls == ui_before_plain + 1);
    CheckUiLayer(gpu, plain.fake);
    clone->Release();
  }

  // 8. The game's own HUD-less scene has priority over the capture, is copied
  //    for the synthesis, and a game-named swapchain buffer is presentable.
  {
    const float green[4] = {0, 1, 0, 1};
    gpu.device->CreateRenderTargetView(s.finished, nullptr, gpu.Rtv(2));
    gpu.list->ClearRenderTargetView(gpu.Rtv(2), green, 0, nullptr);
    GameTags(sl::kBufferTypeHUDLessColor, s.finished, D3D12_RESOURCE_STATE_RENDER_TARGET, gpu.list);
    CHECK(hl::GameProvidesHudless());
    CHECK(hl::Recent(hl::g_hudless_copy_tick, hl::kHudlessFreshMs));  // the game's scene was copied
    Scene game = s;
    game.fake = Texture(gpu, L"Swapchain[32].Buffer[0]", D3D12_RESOURCE_STATE_PRESENT);
    const unsigned ui_before = ui_tags.calls;
    hl::g_hudless_copy_tick = GetTickCount64();  // a first run can exceed the in-frame freshness window
    Frame(gpu, game, true, 1, Ui::partial);
    CHECK(hudless_tags.calls == 5);  // no capture
    CHECK(ui_tags.calls == ui_before + 1);
    CheckUiLayer(gpu, game.fake);
    hl::g_game_hudless_tick = 0;
  }

  // 9. The game's own UI layer has priority, and disallowed modes tag nothing.
  {
    const unsigned ui_before = ui_tags.calls;
    GameTags(sl::kBufferTypeUIColorAndAlpha, s.other, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr);
    CHECK(hl::GameProvidesUi());
    Frame(gpu, s, true, 1, Ui::partial);
    CHECK(hudless_tags.calls == 6 && ui_tags.calls == ui_before);
    hl::g_game_ui_tick = 0;
    // A game UIAlpha (not only UIColorAndAlpha) is a game UI layer too.
    GameTags(sl::kBufferTypeUIAlpha, s.other, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr);
    CHECK(hl::GameProvidesUi());
    Frame(gpu, s, true, 1, Ui::partial);
    CHECK(hudless_tags.calls == 7 && ui_tags.calls == ui_before);  // the scene is still captured
    hl::g_game_ui_tick = 0;
    hl::g_allowed = false;
    Frame(gpu, s, true, 1, Ui::partial);
    CHECK(hudless_tags.calls == 7 && ui_tags.calls == ui_before);
    hl::g_allowed = true;
  }

  // 10. UI covering the whole screen (post effects after the UI) pauses the
  //     synthesis once measured; ordinary frames resume it.
  {
    for (int i = 0; i < 4; ++i) Frame(gpu, s, true, 1, Ui::full);
    CHECK(hl::g_ui_paused.load());
    const unsigned paused_at = ui_tags.calls;
    Frame(gpu, s, true, 1, Ui::full);
    CHECK(ui_tags.calls == paused_at);
    for (int i = 0; i < 4; ++i) Frame(gpu, s, true, 1, Ui::partial);
    CHECK(!hl::g_ui_paused.load());
    CHECK(ui_tags.calls > paused_at);
  }

  // 11. An ANSI debug name (WKPDID_D3DDebugObjectName) is recognized too.
  Scene ansi = s;
  ansi.fake = Texture(gpu, L"", D3D12_RESOURCE_STATE_PRESENT);
  const char name[] = "nv.sl.dlss_g.tex2d.fake-swapchain-buffer";
  ansi.fake->SetPrivateData(hl::internal::kDebugObjectName, sizeof(name) - 1, name);
  const unsigned before_ansi = hudless_tags.calls;
  Frame(gpu, ansi, true, 1, Ui::partial);
  CHECK(hudless_tags.calls == before_ansi + 1 && CopyIs(gpu, kGreen));

  // 12. A game moving the buffer with enhanced barriers gets both inputs.
  D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12{};
  ID3D12GraphicsCommandList7* list7 = nullptr;
  if (SUCCEEDED(gpu.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12))) &&
      options12.EnhancedBarriersSupported && SUCCEEDED(gpu.list->QueryInterface(IID_PPV_ARGS(&list7)))) {
    auto layout = [&](ID3D12Resource* r, D3D12_BARRIER_LAYOUT before, D3D12_BARRIER_LAYOUT after) {
      D3D12_TEXTURE_BARRIER b{};
      b.SyncBefore = D3D12_BARRIER_SYNC_ALL;
      b.SyncAfter = D3D12_BARRIER_SYNC_ALL;
      b.AccessBefore = before == D3D12_BARRIER_LAYOUT_RENDER_TARGET ? D3D12_BARRIER_ACCESS_RENDER_TARGET
                                                                     : D3D12_BARRIER_ACCESS_COMMON;
      b.AccessAfter = after == D3D12_BARRIER_LAYOUT_RENDER_TARGET ? D3D12_BARRIER_ACCESS_RENDER_TARGET
                      : after == D3D12_BARRIER_LAYOUT_SHADER_RESOURCE ? D3D12_BARRIER_ACCESS_SHADER_RESOURCE
                                                                      : D3D12_BARRIER_ACCESS_COMMON;
      b.LayoutBefore = before;
      b.LayoutAfter = after;
      b.pResource = r;
      b.Subresources.IndexOrFirstMipLevel = 0xffffffff;
      D3D12_BARRIER_GROUP group{D3D12_BARRIER_TYPE_TEXTURE, 1};
      group.pTextureBarriers = &b;
      list7->Barrier(1, &group);
    };
    const float blue[4] = {0, 0, 1, 1};
    const D3D12_VIEWPORT viewport{0, 0, kSize, kSize, 0, 1};
    const D3D12_RECT all{0, 0, kSize, kSize};
    gpu.device->CreateRenderTargetView(s.fake, nullptr, gpu.Rtv(0));
    const auto fake_rtv = gpu.Rtv(0), other_rtv = gpu.Rtv(1);
    auto* list = gpu.list;
    const unsigned hudless_before = hudless_tags.calls, ui_before = ui_tags.calls;
    list->OMSetRenderTargets(1, &fake_rtv, FALSE, nullptr);
    layout(s.fake, D3D12_BARRIER_LAYOUT_PRESENT, D3D12_BARRIER_LAYOUT_RENDER_TARGET);
    layout(s.finished, D3D12_BARRIER_LAYOUT_RENDER_TARGET, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE);
    list->SetGraphicsRootSignature(gpu.root);
    list->SetPipelineState(gpu.scene);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &all);
    list->DrawInstanced(3, 1, 0, 0);
    list->OMSetRenderTargets(1, &other_rtv, FALSE, nullptr);  // boundary
    list->OMSetRenderTargets(1, &fake_rtv, FALSE, nullptr);
    list->ClearRenderTargetView(fake_rtv, blue, 1, &kOpaque);
    list->SetPipelineState(gpu.ui);
    list->RSSetScissorRects(1, &kBand);
    list->DrawInstanced(3, 1, 0, 0);
    layout(s.fake, D3D12_BARRIER_LAYOUT_RENDER_TARGET, D3D12_BARRIER_LAYOUT_PRESENT);
    layout(s.finished, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_RENDER_TARGET);
    gpu.Run();
    CHECK(hudless_tags.calls == hudless_before + 1 && CopyIs(gpu, kGreen));
    CHECK(ui_tags.calls == ui_before + 1);
    CheckUiLayer(gpu, s.fake);
    list7->Release();
  } else {
    std::printf("SKIP enhanced barriers unsupported\n");
  }

  // 13. The device is healthy after every recorded capture and synthesis.
  CHECK(gpu.device->GetDeviceRemovedReason() == S_OK);
  std::printf(failures ? "hud_assist: %d failure(s)\n" : "hud_assist: all tests passed\n", failures);
  return failures ? 1 : 0;
}
