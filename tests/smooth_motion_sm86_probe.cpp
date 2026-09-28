#include "../source/native/smooth_motion_sm86.h"

#include <Windows.h>
#include <cstdio>
#include <cwchar>

int wmain(int argc, wchar_t** argv)
{
    auto mode = smooth_motion_sm86::ApiMode::D3D12;
    if (argc == 2 && std::wcscmp(argv[1], L"d3d11") == 0)
        mode = smooth_motion_sm86::ApiMode::D3D11;
    else if (argc == 2 && std::wcscmp(argv[1], L"vulkan") == 0)
        mode = smooth_motion_sm86::ApiMode::Vulkan;
    else if (argc != 1 && (argc != 2 || std::wcscmp(argv[1], L"d3d12") != 0))
    {
        std::fwprintf(stderr, L"usage: smooth_motion_sm86_probe [d3d12|d3d11|vulkan]\n");
        return 2;
    }
    const bool active = smooth_motion_sm86::Initialize([](const wchar_t* message)
    {
        std::fwprintf(stderr, L"%s\n", message);
    }, mode);
    std::fwprintf(stderr, L"SM86 probe active=%d\n", active ? 1 : 0);
    return active ? 0 : 1;
}
