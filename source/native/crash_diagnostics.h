#pragma once
#include <Windows.h>

namespace crash_diagnostics
{
bool Initialize(const wchar_t* path) noexcept;
void Shutdown() noexcept;
// Called from normal module discovery, never from the exception observer.
void RecordModule(HMODULE module, DWORD size, const wchar_t* path) noexcept;
}
