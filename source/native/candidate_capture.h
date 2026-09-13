#pragma once
// Opt-in observation kernel and owned-resource readback. No game resource is
// transitioned, written, or mapped. Included only in the capture build/harness.
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
#include "detours/detours.h"
#include "capture_binary.generated.h"

namespace candidate_capture {
using Microsoft::WRL::ComPtr;
inline constexpr UINT side = 256, planes = 9, maxJobs = 6;
using Query = void*(__cdecl*)(unsigned);
using CreateShader = int(__cdecl*)(ID3D12Device*, const void*, UINT, UINT, UINT, UINT, const char*, void**);
using LaunchShader = int(__cdecl*)(ID3D12GraphicsCommandList*, void*, UINT, UINT, UINT, const void*, UINT);
using GetSurface = int(__cdecl*)(ID3D12Device*, D3D12_CPU_DESCRIPTOR_HANDLE, UINT*);
using Execute = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using Reset = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*);
inline CreateShader createShader = nullptr;
inline LaunchShader launchShader = nullptr;
inline GetSurface getSurface = nullptr;
inline Execute executeOriginal = nullptr;
inline Reset resetOriginal = nullptr;
inline void (*logger)(const wchar_t*) = nullptr;
inline void Report(const wchar_t* text) { if (logger) logger(text); }
inline std::mutex mutex;
inline ComPtr<ID3D12Device> captureDevice;
inline void* probeShader = nullptr;
inline bool failed = false, keyDown = false;
inline UINT remaining = 0;
inline uint64_t dispatchId = 0;

struct Job {
    ComPtr<ID3D12Resource> atlas, readback;
    ComPtr<ID3D12DescriptorHeap> heap;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12GraphicsCommandList> list;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes = 0;
    UINT handle = 0, originX = 0, originY = 0, width = 0, height = 0;
    uint64_t sequence = 0;
    LARGE_INTEGER qpc{};
    std::array<uint8_t, 240> originalParams{};
    bool submitted = false, done = false;
};
inline std::vector<std::shared_ptr<Job>> jobs;

inline bool InitializeApi() {
    HMODULE nvapi = GetModuleHandleW(L"nvapi64.dll");
    if (!nvapi) nvapi = LoadLibraryW(L"nvapi64.dll");
    auto query = nvapi ? reinterpret_cast<Query>(GetProcAddress(nvapi, "nvapi_QueryInterface")) : nullptr;
    if (!query) return false;
    createShader = reinterpret_cast<CreateShader>(query(0x1dc7261f));
    launchShader = reinterpret_cast<LaunchShader>(query(0x5c52bb86));
    getSurface = reinterpret_cast<GetSurface>(query(0x48f5b2ee));
    return createShader && launchShader && getSurface;
}

inline bool CreateJob(ID3D12Device* device, Job& job) {
    D3D12_HEAP_PROPERTIES gpu{}; gpu.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = side; desc.Height = side * planes;
    desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.SampleDesc.Count = 1; desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if (FAILED(device->CreateCommittedResource(&gpu,D3D12_HEAP_FLAG_NONE,&desc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&job.atlas)))) return false;
    device->GetCopyableFootprints(&desc,0,1,0,&job.footprint,nullptr,nullptr,&job.bytes);
    if (!job.bytes || job.bytes > 16ull*1024*1024) return false;
    D3D12_HEAP_PROPERTIES cpu{}; cpu.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{}; buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = job.bytes; buffer.Height = 1; buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1; buffer.SampleDesc.Count = 1; buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(device->CreateCommittedResource(&cpu,D3D12_HEAP_FLAG_NONE,&buffer,
        D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&job.readback)))) return false;
    D3D12_DESCRIPTOR_HEAP_DESC heap{}; heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; heap.NumDescriptors = 1;
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&job.heap)))) return false;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{}; uav.Format = desc.Format; uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    auto descriptor = job.heap->GetCPUDescriptorHandleForHeapStart();
    device->CreateUnorderedAccessView(job.atlas.Get(),nullptr,&uav,descriptor);
    int surf_res = getSurface(device,descriptor,&job.handle);
    if (surf_res != 0) return false;
    return SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&job.fence)));
}

inline bool Record(ID3D12GraphicsCommandList* list, void* shader, Job& job, UINT x, UINT y, UINT z) {
    // Original parameter bytes are copied, never modified in place.
    alignas(8) std::array<uint8_t,256> params{};
    std::memcpy(params.data(),job.originalParams.data(),240);
    const uint64_t surface = job.handle;
    std::memcpy(params.data()+240,&surface,8);
    std::memcpy(params.data()+248,&job.originX,4);
    std::memcpy(params.data()+252,&job.originY,4);
    ID3D12DescriptorHeap* heaps[] = {job.heap.Get()};
    list->SetDescriptorHeaps(1, heaps);
    if (launchShader(list,shader,x,y,z,params.data(),static_cast<UINT>(params.size())) != 0) return false;
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = job.atlas.Get(); barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = job.atlas.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = job.readback.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = job.footprint;
    list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
    job.list = list;
    return true;
}

inline bool Read(Job& job, std::vector<float>& pixels) {
    if (!job.submitted || job.fence->GetCompletedValue() != 1) return false;
    void* mapped = nullptr; D3D12_RANGE range{0,static_cast<SIZE_T>(job.bytes)};
    if (FAILED(job.readback->Map(0,&range,&mapped))) return false;
    pixels.resize(static_cast<size_t>(side)*side*planes*4);
    auto source = static_cast<const uint8_t*>(mapped) + job.footprint.Offset;
    for (UINT row=0; row<side*planes; ++row)
        std::memcpy(pixels.data()+static_cast<size_t>(row)*side*4,
            source+static_cast<size_t>(row)*job.footprint.Footprint.RowPitch,side*16);
    D3D12_RANGE empty{0,0}; job.readback->Unmap(0,&empty);
    return true;
}

inline void SaveCompleted() {
    for (auto& job : jobs) {
        if (job->done || !job->submitted) continue;
        const auto completed = job->fence->GetCompletedValue();
        if (completed == UINT64_MAX) { job->done = true; Report(L"[CAPTURE] device removed; capture unavailable"); continue; }
        if (completed != 1) continue;
        std::vector<float> pixels;
        if (!Read(*job,pixels)) { job->done = true; Report(L"[CAPTURE] readback failed"); continue; }
        try {
            const std::filesystem::path directory(QUALITY_CAPTURE_DIRECTORY);
            std::filesystem::create_directories(directory);
            auto stem = directory / (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(job->sequence));
            std::ofstream raw(stem.wstring()+L".rgba32f",std::ios::binary);
            raw.write(reinterpret_cast<const char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()*4));
            raw.close();
            std::ofstream metadata(stem.wstring()+L".json");
            metadata << "{\"schema\":1,\"probe\":\"mode9-recomputed-pre-network\",\"dispatch\":" << job->sequence
                << ",\"qpc\":" << job->qpc.QuadPart << ",\"width\":" << job->width << ",\"height\":" << job->height
                << ",\"origin\":[" << job->originX << ',' << job->originY << "],\"side\":256,\"planes\":9,"
                << "\"plane_names\":[\"raw0\",\"raw1\",\"reference0\",\"reference1\",\"weights\",\"uv01\",\"corrected0\",\"corrected1\",\"flags-valid0-valid1-conflict-copy\"]}";
            metadata.close();
            std::ofstream params(stem.wstring()+L".params",std::ios::binary);
            params.write(reinterpret_cast<const char*>(job->originalParams.data()),240); params.close();
            Report(raw && metadata && params ? L"[CAPTURE] saved GPU atlas, metadata and launch parameters" : L"[CAPTURE] file write failed");
        } catch (...) { Report(L"[CAPTURE] output directory/write exception"); }
        job->done = true;
        job->atlas.Reset(); job->readback.Reset(); job->heap.Reset(); job->list.Reset(); job->fence.Reset();
    }
}

inline void STDMETHODCALLTYPE HookExecute(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    // Serialize submission marking with probe recording/reset. Never wait for GPU.
    std::unique_lock lock(mutex);
    executeOriginal(queue,count,lists);
    for (auto& job : jobs) {
        if (job->submitted || job->done) continue;
        for (UINT i=0;i<count;++i) if (lists[i] == job->list.Get()) {
            if (SUCCEEDED(queue->Signal(job->fence.Get(),1))) job->submitted = true;
            else { job->done = true; Report(L"[CAPTURE] queue signal failed; resources retained"); }
            break;
        }
    }
}
inline HRESULT STDMETHODCALLTYPE HookReset(ID3D12GraphicsCommandList* list, ID3D12CommandAllocator* allocator, ID3D12PipelineState* state) {
    std::unique_lock lock(mutex);
    const HRESULT hr = resetOriginal(list,allocator,state);
    if (SUCCEEDED(hr)) for (auto& job : jobs) if (!job->submitted && !job->done && job->list.Get() == list) {
        job->done = true; Report(L"[CAPTURE] unsubmitted list reset; capture discarded");
        job->atlas.Reset(); job->readback.Reset(); job->heap.Reset(); job->list.Reset(); job->fence.Reset();
    }
    return hr;
}

inline bool Initialize(ID3D12GraphicsCommandList* list) {
    if (probeShader) return true;
    if (failed) return false;
    failed = true;
    if (!InitializeApi() || FAILED(list->GetDevice(IID_PPV_ARGS(&captureDevice)))) return false;
    D3D12_COMMAND_QUEUE_DESC desc{}; desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if (FAILED(captureDevice->CreateCommandQueue(&desc,IID_PPV_ARGS(&queue)))) return false;
    if (createShader(captureDevice.Get(),capture_binary::probe,sizeof(capture_binary::probe),256,1,1,"TransfusionCapture",&probeShader) != 0) {
        probeShader = nullptr; return false;
    }
    executeOriginal = reinterpret_cast<Execute>((*reinterpret_cast<void***>(queue.Get()))[10]);
    resetOriginal = reinterpret_cast<Reset>((*reinterpret_cast<void***>(list))[10]);
    if (DetourTransactionBegin() != NO_ERROR) { probeShader = nullptr; return false; }
    LONG result = DetourUpdateThread(GetCurrentThread());
    if (result == NO_ERROR) result = DetourAttach(reinterpret_cast<void**>(&executeOriginal),reinterpret_cast<void*>(&HookExecute));
    if (result == NO_ERROR) result = DetourAttach(reinterpret_cast<void**>(&resetOriginal),reinterpret_cast<void*>(&HookReset));
    if (result != NO_ERROR) { DetourTransactionAbort(); probeShader = nullptr; return false; }
    if (DetourTransactionCommit() != NO_ERROR) { probeShader = nullptr; return false; }
    // Hooks and retained GPU jobs must not outlive this DLL.
    HMODULE pin{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Initialize),&pin);
    failed = false; Report(L"[CAPTURE] ready: center sirens, press F8 once; six 256x256 probes");
    return true;
}

inline void Observe(void* commandList, const std::array<uint8_t,240>& params, UINT x, UINT y, UINT z) noexcept {
    try {
        std::unique_lock lock(mutex);
        ++dispatchId;
        auto* list = static_cast<ID3D12GraphicsCommandList*>(commandList);
        if (!list || !Initialize(list)) {
            static bool reported = false;
            if (!reported) { reported=true; Report(L"[CAPTURE] initialization failed; game dispatch unchanged"); }
            return;
        }
        SaveCompleted();
        DWORD foregroundProcess=0; GetWindowThreadProcessId(GetForegroundWindow(),&foregroundProcess);
        const bool pressed = foregroundProcess == GetCurrentProcessId() && (GetAsyncKeyState(VK_F8)&0x8000);
        if (pressed && !keyDown && jobs.empty()) { remaining=maxJobs; Report(L"[CAPTURE] burst armed"); }
        keyDown = pressed;
        if (!remaining) return;
        UINT width=0,height=0;
        std::memcpy(&width,params.data()+212,4); std::memcpy(&height,params.data()+216,4);
        if (width<side || height<side || width>16384 || height>16384 || !x || !y || z!=1) { remaining=0; Report(L"[CAPTURE] unsupported dimensions/grid"); return; }
        ComPtr<ID3D12Device> device;
        if (FAILED(list->GetDevice(IID_PPV_ARGS(&device))) || device.Get()!=captureDevice.Get()) { remaining=0; Report(L"[CAPTURE] device changed; burst stopped"); return; }
        auto job=std::make_shared<Job>(); job->sequence=dispatchId; QueryPerformanceCounter(&job->qpc);
        job->width=width; job->height=height; job->originX=(width-side)/2; job->originY=(height-side)/2;
        job->originalParams=params;
        if (!CreateJob(device.Get(),*job)) { remaining=0; Report(L"[CAPTURE] resource allocation failed"); return; }
        if (!Record(list,probeShader,*job,x,y,z)) { remaining=0; Report(L"[CAPTURE] probe launch failed"); return; }
        jobs.push_back(std::move(job)); --remaining;
        Report(L"[CAPTURE] probe and owned-resource copy recorded");
    } catch (...) { Report(L"[CAPTURE] exception; capture unavailable"); remaining=0; }
}
}
