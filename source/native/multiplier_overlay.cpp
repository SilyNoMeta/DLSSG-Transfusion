#include "multiplier_overlay.h"
#include "detours/detours.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <array>
#include <mutex>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace multiplier_overlay
{
namespace
{
using CreateFactory = HRESULT(WINAPI*)(REFIID, void**);
using CreateFactory2 = HRESULT(WINAPI*)(UINT, REFIID, void**);
using FactoryCreateSwapChain = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
using FactoryCreateSwapChainForHwnd = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
using Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1 = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using ResizeBuffers = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

CreateFactory gCreateFactory{};
CreateFactory gCreateFactory1{};
CreateFactory2 gCreateFactory2{};
FactoryCreateSwapChain gFactoryCreateSwapChain{};
FactoryCreateSwapChainForHwnd gFactoryCreateSwapChainForHwnd{};
Present gPresent{};
Present1 gPresent1{};
ResizeBuffers gResizeBuffers{};
std::atomic<bool> gInstalled{false};
std::atomic<bool> gVisible{false};
const std::atomic<uint32_t>* gActual{};
const std::atomic<uint32_t>* gApplied{};
const std::atomic<bool>* gFgOn{};
const std::atomic<uint64_t>* gFgSession{};
const std::atomic<uint64_t>* gSampleTick{};
std::mutex gMutex;
thread_local bool gInsidePresent{};

struct Vertex { float x, y; float r, g, b, a; };

struct Context
{
    static constexpr UINT64 kVertexBufferBytes = 192u * 1024u;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12GraphicsCommandList> list;
    std::vector<ComPtr<ID3D12CommandAllocator>> allocators;
    std::vector<ComPtr<ID3D12Resource>> buffers;
    std::vector<ComPtr<ID3D12Resource>> uploads;
    std::vector<void*> mappedUploads;
    std::vector<Vertex> vertices;
    std::vector<UINT64> fenceValues;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent{};
    UINT64 nextFenceValue{1};
    LARGE_INTEGER fpsWindowStart{};
    uint64_t presentsInWindow{};
    uint32_t presentedFpsMilli{};
    bool frameGenerationWasOn{};
    uint64_t observedFgSession{};
    UINT rtvStep{};
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    UINT count{};
    bool ready{};
};
std::unordered_map<IDXGISwapChain*, Context> gContexts;

template<typename T> T Slot(void* object, size_t index)
{
    return reinterpret_cast<T>((*reinterpret_cast<void***>(object))[index]);
}

void AddRect(std::vector<Vertex>& out, float x0, float y0, float x1, float y1,
    float width, float height, float red = 1.f, float green = 1.f,
    float blue = 1.f, float alpha = .92f)
{
    const float l = x0 / width * 2.f - 1.f, r = x1 / width * 2.f - 1.f;
    const float t = 1.f - y0 / height * 2.f, b = 1.f - y1 / height * 2.f;
    const Vertex a{l,t,red,green,blue,alpha}, c{r,b,red,green,blue,alpha};
    const Vertex d{l,b,red,green,blue,alpha}, e{r,t,red,green,blue,alpha};
    out.insert(out.end(), {a,e,c,a,c,d});
}

void AddGlyph(std::vector<Vertex>& v, char glyph, float x, float y, float scale,
    float targetW, float targetH)
{
    // Five-by-seven sans bitmap glyphs. At 1.5 px per cell they remain crisp and
    // much less visually dominant than the old seven-segment display.
    static constexpr uint8_t rows[][7] = {
        {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}, // 0
        {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e}, // 1
        {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}, // 2
        {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e}, // 3
        {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}, // 4
        {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e}, // 5
        {0x0e,0x10,0x10,0x1e,0x11,0x11,0x0e}, // 6
        {0x1f,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
        {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}, // 8
        {0x0e,0x11,0x11,0x0f,0x01,0x01,0x0e}, // 9
        {0x01,0x02,0x02,0x04,0x08,0x08,0x10}, // /
        {0x00,0x00,0x11,0x0a,0x04,0x0a,0x11}, // x
        {0x06,0x08,0x08,0x1e,0x08,0x08,0x08}, // f
        {0x00,0x00,0x1e,0x11,0x1e,0x10,0x10}, // p
        {0x00,0x00,0x0f,0x10,0x0e,0x01,0x1e}  // s
    };
    const size_t index = glyph == 'x' ? 11u : glyph == 'f' ? 12u
        : glyph == 'p' ? 13u : glyph == 's' ? 14u : glyph == '/' ? 10u
        : static_cast<size_t>(glyph - '0');
    if (index > 14) return;
    for (int row=0; row<7; ++row)
        for (int col=0; col<5; ++col)
            if (rows[index][row] & (0x10 >> col))
                AddRect(v,x+col*scale,y+row*scale,x+(col+1)*scale,y+(row+1)*scale,
                    targetW,targetH,.94f,.96f,1.f,.96f);
}

bool BuildContext(IDXGISwapChain* swapChain, Context& c)
{
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc)) || !desc.BufferCount || !desc.BufferDesc.Width || !desc.BufferDesc.Height)
        return false;
    if (FAILED(c.queue->GetDevice(IID_PPV_ARGS(&c.device)))) return false;
    c.count = desc.BufferCount; c.format = desc.BufferDesc.Format;

    D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, c.count};
    if (FAILED(c.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&c.rtvHeap)))) return false;
    c.rtvStep = c.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    c.buffers.resize(c.count); c.allocators.resize(c.count); c.uploads.resize(c.count);
    c.mappedUploads.resize(c.count);
    c.fenceValues.resize(c.count);
    if (FAILED(c.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&c.fence)))) return false;
    c.fenceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr); if(!c.fenceEvent)return false;
    auto handle = c.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i=0;i<c.count;i++) {
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&c.buffers[i]))) ||
            FAILED(c.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&c.allocators[i])))) return false;
        c.device->CreateRenderTargetView(c.buffers[i].Get(), nullptr, handle); handle.ptr += c.rtvStep;
    }
    D3D12_HEAP_PROPERTIES uploadHeap{D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC uploadDesc{D3D12_RESOURCE_DIMENSION_BUFFER,0,
        Context::kVertexBufferBytes,1,1,1,DXGI_FORMAT_UNKNOWN,{1,0},
        D3D12_TEXTURE_LAYOUT_ROW_MAJOR,D3D12_RESOURCE_FLAG_NONE};
    for (UINT i=0;i<c.count;i++) {
        if (FAILED(c.device->CreateCommittedResource(&uploadHeap,D3D12_HEAP_FLAG_NONE,
            &uploadDesc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
            IID_PPV_ARGS(&c.uploads[i]))) ||
            FAILED(c.uploads[i]->Map(0,nullptr,&c.mappedUploads[i]))) return false;
    }
    c.vertices.reserve(static_cast<size_t>(Context::kVertexBufferBytes / sizeof(Vertex)));
    D3D12_ROOT_SIGNATURE_DESC rs{}; rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> sig, err;
    if (FAILED(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err)) ||
        FAILED(c.device->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&c.rootSignature)))) return false;
    static const char* shader =
        "struct V{float2 p:POSITION;float4 c:COLOR;};struct O{float4 p:SV_Position;float4 c:COLOR;};"
        "O vs(V i){O o;o.p=float4(i.p,0,1);o.c=i.c;return o;}float4 ps(O i):SV_Target{return i.c;}";
    ComPtr<ID3DBlob> vs, ps;
    if (FAILED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&err)) ||
        FAILED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&ps,&err))) return false;
    const D3D12_INPUT_ELEMENT_DESC il[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{}; pd.pRootSignature=c.rootSignature.Get(); pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()}; pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    pd.BlendState.RenderTarget[0].BlendEnable=TRUE; pd.BlendState.RenderTarget[0].SrcBlend=D3D12_BLEND_SRC_ALPHA; pd.BlendState.RenderTarget[0].DestBlend=D3D12_BLEND_INV_SRC_ALPHA; pd.BlendState.RenderTarget[0].BlendOp=D3D12_BLEND_OP_ADD; pd.BlendState.RenderTarget[0].SrcBlendAlpha=D3D12_BLEND_ONE; pd.BlendState.RenderTarget[0].DestBlendAlpha=D3D12_BLEND_ZERO; pd.BlendState.RenderTarget[0].BlendOpAlpha=D3D12_BLEND_OP_ADD; pd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask=UINT_MAX; pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID; pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE; pd.DepthStencilState.DepthEnable=FALSE; pd.DepthStencilState.StencilEnable=FALSE; pd.InputLayout={il,2}; pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; pd.NumRenderTargets=1; pd.RTVFormats[0]=c.format; pd.SampleDesc.Count=1;
    if (FAILED(c.device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&c.pipeline))) ||
        FAILED(c.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,c.allocators[0].Get(),c.pipeline.Get(),IID_PPV_ARGS(&c.list)))) return false;
    c.list->Close(); c.ready=true; return true;
}

void Draw(IDXGISwapChain* swapChain, UINT presentFlags)
{
    if(!gActual || !gFgOn) return;
    const bool fgOn=gFgOn->load(std::memory_order_relaxed);
    std::lock_guard lock(gMutex); auto it=gContexts.find(swapChain); if(it==gContexts.end()) return; Context& c=it->second;
    if(!fgOn) {
        c.frameGenerationWasOn=false; c.fpsWindowStart={}; c.presentsInWindow=0;
        c.presentedFpsMilli=0; return;
    }
    const uint64_t fgSession=gFgSession ? gFgSession->load(std::memory_order_acquire) : 0;
    if(!c.frameGenerationWasOn || c.observedFgSession!=fgSession) {
        c.frameGenerationWasOn=true; c.fpsWindowStart={}; c.presentsInWindow=0;
        c.presentedFpsMilli=0; c.observedFgSession=fgSession;
    }
    LARGE_INTEGER now{},frequency{};
    if((presentFlags & DXGI_PRESENT_TEST)==0 && QueryPerformanceCounter(&now)
        && QueryPerformanceFrequency(&frequency)) {
        if(!c.fpsWindowStart.QuadPart) c.fpsWindowStart=now;
        ++c.presentsInWindow;
        const uint64_t elapsed=static_cast<uint64_t>(now.QuadPart-c.fpsWindowStart.QuadPart);
        if(elapsed>=static_cast<uint64_t>(frequency.QuadPart)/2u) {
            c.presentedFpsMilli=static_cast<uint32_t>(std::min<uint64_t>(UINT32_MAX,
                (c.presentsInWindow*static_cast<uint64_t>(frequency.QuadPart)*1000u+elapsed/2u)/elapsed));
            c.fpsWindowStart=now; c.presentsInWindow=0;
        }
    }
    if(!gVisible.load()) return;
    const uint64_t sampleAge=gSampleTick ? GetTickCount64()-gSampleTick->load(std::memory_order_acquire) : UINT64_MAX;
    uint32_t mult=gActual->load(std::memory_order_relaxed);
    // Turning FG off legitimately publishes zero. On re-enable, show the last
    // accepted setting immediately instead of waiting for the first state query.
    if((mult<2 || sampleAge>2500) && gApplied) mult=gApplied->load(std::memory_order_relaxed);
    if(mult<2 || mult>6) return;
    if(!c.ready && !BuildContext(swapChain,c)) return;
    ComPtr<IDXGISwapChain3> sc3; if(FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&sc3)))) return; const UINT i=sc3->GetCurrentBackBufferIndex(); if(i>=c.count) return;
    const uint32_t generatedMilli=c.presentedFpsMilli;
    const uint32_t generatedFps=(generatedMilli+500u)/1000u;
    const uint32_t baseFps=(generatedMilli+mult*500u)/(mult*1000u);
    char label[32]{};
    sprintf_s(label,"%u/%u fps %ux",generatedFps,baseFps,mult);
    const float scale=2.f, advance=12.f;
    float textWidth=0.f;
    for(const char* p=label;*p;++p) textWidth+=(*p==' ' ? 6.f : advance);
    const auto d=c.buffers[i]->GetDesc(); auto& vertices=c.vertices; vertices.clear();
    const float panelW=textWidth+8.f, panelH=19.f, x=10.f, y=10.f;
    AddRect(vertices,x,y,x+panelW,y+panelH,float(d.Width),float(d.Height),0.f,0.f,0.f,.38f);
    float glyphX=x+4.f;
    for(const char* p=label;*p;++p) {
        if(*p==' ') glyphX+=6.f;
        else { AddGlyph(vertices,*p,glyphX,y+2.5f,scale,float(d.Width),float(d.Height)); glyphX+=advance; }
    }
    if(c.fenceValues[i] && c.fence->GetCompletedValue()<c.fenceValues[i]){if(FAILED(c.fence->SetEventOnCompletion(c.fenceValues[i],c.fenceEvent)))return;WaitForSingleObject(c.fenceEvent,1000);if(c.fence->GetCompletedValue()<c.fenceValues[i])return;}
    const UINT64 bytes=vertices.size()*sizeof(Vertex);
    if(bytes>Context::kVertexBufferBytes || !c.mappedUploads[i]) return;
    memcpy(c.mappedUploads[i],vertices.data(),size_t(bytes));
    c.allocators[i]->Reset(); c.list->Reset(c.allocators[i].Get(),c.pipeline.Get()); D3D12_RESOURCE_BARRIER b{D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,D3D12_RESOURCE_BARRIER_FLAG_NONE}; b.Transition={c.buffers[i].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET}; c.list->ResourceBarrier(1,&b);
    auto rtv=c.rtvHeap->GetCPUDescriptorHandleForHeapStart(); rtv.ptr+=SIZE_T(i)*c.rtvStep; c.list->OMSetRenderTargets(1,&rtv,FALSE,nullptr); D3D12_VIEWPORT vp{0,0,float(d.Width),float(d.Height),0,1}; D3D12_RECT sr{0,0,LONG(d.Width),LONG(d.Height)}; c.list->RSSetViewports(1,&vp); c.list->RSSetScissorRects(1,&sr); c.list->SetGraphicsRootSignature(c.rootSignature.Get()); c.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); D3D12_VERTEX_BUFFER_VIEW vb{c.uploads[i]->GetGPUVirtualAddress(),UINT(bytes),sizeof(Vertex)}; c.list->IASetVertexBuffers(0,1,&vb); c.list->DrawInstanced(UINT(vertices.size()),1,0,0); std::swap(b.Transition.StateBefore,b.Transition.StateAfter); c.list->ResourceBarrier(1,&b); c.list->Close(); ID3D12CommandList* lists[]={c.list.Get()}; c.queue->ExecuteCommandLists(1,lists);const UINT64 fv=c.nextFenceValue++;if(SUCCEEDED(c.queue->Signal(c.fence.Get(),fv)))c.fenceValues[i]=fv;
}

struct PresentScope
{
    bool outer{!gInsidePresent};
    PresentScope(){if(outer)gInsidePresent=true;}
    ~PresentScope(){if(outer)gInsidePresent=false;}
};
HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* s,UINT a,UINT b)
{
    PresentScope scope;
    if(scope.outer) Draw(s,b);
    return gPresent(s,a,b);
}
HRESULT STDMETHODCALLTYPE HookPresent1(IDXGISwapChain1* s,UINT a,UINT b,const DXGI_PRESENT_PARAMETERS* p)
{
    PresentScope scope;
    if(scope.outer) Draw(s,b);
    return gPresent1(s,a,b,p);
}
HRESULT STDMETHODCALLTYPE HookResize(IDXGISwapChain* s,UINT a,UINT b,UINT c,DXGI_FORMAT d,UINT e)
{
    std::lock_guard l(gMutex);
    auto i=gContexts.find(s);
    if(i==gContexts.end()) return gResizeBuffers(s,a,b,c,d,e);
    Context& old=i->second;
    for(UINT64 value:old.fenceValues) {
        if(value && old.fence->GetCompletedValue()<value) {
            old.fence->SetEventOnCompletion(value,old.fenceEvent);
            WaitForSingleObject(old.fenceEvent,1000);
        }
    }
    auto queue=old.queue;
    if(old.fenceEvent) CloseHandle(old.fenceEvent);
    gContexts.erase(i);
    const HRESULT hr=gResizeBuffers(s,a,b,c,d,e);
    if(SUCCEEDED(hr)){Context fresh;fresh.queue=queue;gContexts.emplace(s,std::move(fresh));}
    return hr;
}

void Track(IDXGISwapChain* s,IUnknown* device)
{
    if(!s||!device)return; ComPtr<ID3D12CommandQueue> q; if(FAILED(device->QueryInterface(IID_PPV_ARGS(&q))))return;
    {std::lock_guard l(gMutex);auto& c=gContexts[s];c.queue=q;}
    if(gPresent)return; gPresent=Slot<Present>(s,8); gResizeBuffers=Slot<ResizeBuffers>(s,13); ComPtr<IDXGISwapChain1> s1;if(SUCCEEDED(s->QueryInterface(IID_PPV_ARGS(&s1))))gPresent1=Slot<Present1>(s1.Get(),22);
    DetourTransactionBegin();DetourUpdateThread(GetCurrentThread());DetourAttach(reinterpret_cast<void**>(&gPresent),HookPresent);DetourAttach(reinterpret_cast<void**>(&gResizeBuffers),HookResize);if(gPresent1)DetourAttach(reinterpret_cast<void**>(&gPresent1),HookPresent1);DetourTransactionCommit();
}
HRESULT STDMETHODCALLTYPE HookCreateSwapChain(IDXGIFactory* f,IUnknown* d,DXGI_SWAP_CHAIN_DESC* x,IDXGISwapChain** s){auto hr=gFactoryCreateSwapChain(f,d,x,s);if(SUCCEEDED(hr)&&s)Track(*s,d);return hr;}
HRESULT STDMETHODCALLTYPE HookCreateSwapChainForHwnd(IDXGIFactory2* f,IUnknown* d,HWND w,const DXGI_SWAP_CHAIN_DESC1* x,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fs,IDXGIOutput* o,IDXGISwapChain1** s){auto hr=gFactoryCreateSwapChainForHwnd(f,d,w,x,fs,o,s);if(SUCCEEDED(hr)&&s)Track(*s,d);return hr;}
void TrackFactory(void* p){if(!p)return;if(!gFactoryCreateSwapChain){gFactoryCreateSwapChain=Slot<FactoryCreateSwapChain>(p,10);DetourTransactionBegin();DetourUpdateThread(GetCurrentThread());DetourAttach(reinterpret_cast<void**>(&gFactoryCreateSwapChain),HookCreateSwapChain);DetourTransactionCommit();}ComPtr<IDXGIFactory2> f2;if(SUCCEEDED(static_cast<IDXGIFactory*>(p)->QueryInterface(IID_PPV_ARGS(&f2)))&&!gFactoryCreateSwapChainForHwnd){gFactoryCreateSwapChainForHwnd=Slot<FactoryCreateSwapChainForHwnd>(f2.Get(),15);DetourTransactionBegin();DetourUpdateThread(GetCurrentThread());DetourAttach(reinterpret_cast<void**>(&gFactoryCreateSwapChainForHwnd),HookCreateSwapChainForHwnd);DetourTransactionCommit();}}
HRESULT WINAPI HookFactory(REFIID r,void** p){auto hr=gCreateFactory(r,p);if(SUCCEEDED(hr))TrackFactory(*p);return hr;} HRESULT WINAPI HookFactory1(REFIID r,void** p){auto hr=gCreateFactory1(r,p);if(SUCCEEDED(hr))TrackFactory(*p);return hr;} HRESULT WINAPI HookFactory2(UINT f,REFIID r,void** p){auto hr=gCreateFactory2(f,r,p);if(SUCCEEDED(hr))TrackFactory(*p);return hr;}
}

bool Install(const std::atomic<uint32_t>* actual,const std::atomic<uint32_t>* applied,const std::atomic<bool>* fg,const std::atomic<uint64_t>* session,const std::atomic<uint64_t>* tick)
{
    if(gInstalled.exchange(true))return true;gActual=actual;gApplied=applied;gFgOn=fg;gFgSession=session;gSampleTick=tick;HMODULE dxgi=GetModuleHandleW(L"dxgi.dll");if(!dxgi)dxgi=LoadLibraryW(L"dxgi.dll");if(!dxgi)return false;
    gCreateFactory=reinterpret_cast<CreateFactory>(GetProcAddress(dxgi,"CreateDXGIFactory"));gCreateFactory1=reinterpret_cast<CreateFactory>(GetProcAddress(dxgi,"CreateDXGIFactory1"));gCreateFactory2=reinterpret_cast<CreateFactory2>(GetProcAddress(dxgi,"CreateDXGIFactory2"));
    DetourTransactionBegin();DetourUpdateThread(GetCurrentThread());if(gCreateFactory)DetourAttach(reinterpret_cast<void**>(&gCreateFactory),HookFactory);if(gCreateFactory1)DetourAttach(reinterpret_cast<void**>(&gCreateFactory1),HookFactory1);if(gCreateFactory2)DetourAttach(reinterpret_cast<void**>(&gCreateFactory2),HookFactory2);return DetourTransactionCommit()==NO_ERROR;
}
void SetVisible(bool visible){gVisible.store(visible,std::memory_order_release);}
bool IsVisible(){return gVisible.load(std::memory_order_acquire);}
void Uninstall(){gVisible=false;/* Hooks intentionally remain until process teardown; detaching while Present is active is less safe. */}
}
