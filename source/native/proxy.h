#pragma once
#include <Windows.h>

namespace proxy
{
enum class ProxyType
{
    None,
    Version,
    Winmm,
    Dxgi,
    Dinput8,
    Asi,
    Unsupported
};

ProxyType Initialize(HINSTANCE instance);
void Shutdown();
ProxyType GetCurrentType();
const wchar_t* GetCurrentTypeName();
const wchar_t* GetOriginalLibraryPath();
}
