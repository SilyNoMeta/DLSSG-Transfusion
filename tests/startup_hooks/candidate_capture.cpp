#include "../../source/native/candidate_capture.h"
#include <iostream>
#include <cmath>

int main() {
    using namespace candidate_capture;
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return 1;
    ComPtr<ID3D12Device> device;
    for (UINT i=0;;++i) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(i,&adapter) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
        if (desc.VendorId == 0x10de && SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))) break;
    }
    if (!device || !InitializeApi()) { std::cerr << "NVAPI/device unavailable\n"; return 2; }
    void* shader = nullptr;
    int status = createShader(device.Get(),capture_binary::smoke,sizeof(capture_binary::smoke),256,1,1,"TransfusionCapture",&shader);
    if (status) { std::cerr << "CreateShader " << status << '\n'; return 3; }
    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC desc{};
    if (FAILED(device->CreateCommandQueue(&desc,IID_PPV_ARGS(&queue)))) return 4;
    ComPtr<ID3D12CommandAllocator> allocator;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)))) return 5;
    ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)))) return 6;
    Job job;
    if (!CreateJob(device.Get(),job)) { std::cerr << "CreateJob failed\n"; return 7; }
    if (!Record(list.Get(),shader,job,1,256,1)) { std::cerr << "Record failed\n"; return 8; }
    if (FAILED(list->Close())) return 9;
    ID3D12CommandList* commands[] = {list.Get()};
    queue->ExecuteCommandLists(1,commands);
    if (FAILED(queue->Signal(job.fence.Get(),1))) return 10;
    job.submitted = true;
    HANDLE event = CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if (!event) return 11;
    if (FAILED(job.fence->SetEventOnCompletion(1,event))) return 12;
    DWORD wait = WaitForSingleObject(event,15000); CloseHandle(event);
    if (wait != WAIT_OBJECT_0) { std::cerr << "GPU timeout\n"; return 13; }
    std::vector<float> pixels;
    if (!Read(job,pixels)) return 14;
    for (size_t i=0;i<pixels.size();++i) {
        float expected = i < 8ull*side*side*4 ? .5f : 1.f;
        if (pixels[i] != expected) { std::cerr << "Mismatch " << i << ": " << pixels[i] << " expected " << expected << '\n'; return 15; }
    }
    void* probe = nullptr;
    status = createShader(device.Get(),capture_binary::probe,sizeof(capture_binary::probe),256,1,1,"TransfusionCapture",&probe);
    if (status) { std::cerr << "Probe CreateShader " << status << '\n'; return 16; }
    std::cout << "D3D12_NVAPI_CAPTURE_READBACK_OK: " << pixels.size() << " floats; actual probe shader created\n";
    return 0;
}
