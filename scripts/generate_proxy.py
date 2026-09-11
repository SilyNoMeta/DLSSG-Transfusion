import pefile
import os

def get_exports(dll_name):
    pe = pefile.PE(f"C:/Windows/System32/{dll_name}")
    exports = []
    for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        name = exp.name.decode("utf-8") if exp.name else None
        exports.append((exp.ordinal, name))
    return exports

version_exps = get_exports("version.dll")
dxgi_exps = get_exports("dxgi.dll")
winmm_exps = get_exports("winmm.dll")

base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../source/native"))

# 1. Generate version.def
with open(os.path.join(base_dir, "version.def"), "w") as f:
    f.write("LIBRARY version\nEXPORTS\n")
    for ord_num, name in version_exps:
        f.write(f"    {name} = Proxy_{name} @{ord_num}\n")

# 2. Generate dxgi.def
with open(os.path.join(base_dir, "dxgi.def"), "w") as f:
    f.write("LIBRARY dxgi\nEXPORTS\n")
    for ord_num, name in dxgi_exps:
        f.write(f"    {name} = Proxy_{name} @{ord_num}\n")

# 3. Generate winmm.def
with open(os.path.join(base_dir, "winmm.def"), "w") as f:
    f.write("LIBRARY winmm\nEXPORTS\n")
    for ord_num, name in winmm_exps:
        if name:
            f.write(f"    {name} = Proxy_{name} @{ord_num}\n")
        else:
            f.write(f"    Proxy_Ordinal{ord_num} @{ord_num} NONAME\n")

# 4. Generate proxy_thunks.asm
all_syms = []
for ord_num, name in version_exps:
    all_syms.append((f"Proxy_{name}", f"g_Real_{name}"))
for ord_num, name in dxgi_exps:
    all_syms.append((f"Proxy_{name}", f"g_Real_{name}"))
for ord_num, name in winmm_exps:
    sym_name = f"Proxy_{name}" if name else f"Proxy_Ordinal{ord_num}"
    real_name = f"g_Real_{name}" if name else f"g_Real_Ordinal{ord_num}"
    all_syms.append((sym_name, real_name))

with open(os.path.join(base_dir, "proxy_thunks.asm"), "w") as f:
    f.write("; Auto-generated 64-bit Proxy Thunks for version, dxgi, winmm\n.code\n\n")
    f.write("public DummyFunc\nDummyFunc proc\n    xor eax, eax\n    ret\nDummyFunc endp\n\n")
    for sym, real in all_syms:
        f.write(f"extern {real} : qword\npublic {sym}\n{sym} proc\n    jmp qword ptr [{real}]\n{sym} endp\n\n")
    f.write("end\n")

# 5. Generate proxy.h
with open(os.path.join(base_dir, "proxy.h"), "w") as f:
    f.write('''#pragma once
#include <Windows.h>

namespace proxy
{
enum class ProxyType
{
    None,
    Version,
    Winmm,
    Dxgi
};

ProxyType Initialize(HINSTANCE instance);
void Shutdown();
ProxyType GetCurrentType();
const wchar_t* GetCurrentTypeName();
const wchar_t* GetOriginalLibraryPath();
}
''')

# 6. Generate proxy.cpp
with open(os.path.join(base_dir, "proxy.cpp"), "w") as f:
    f.write('''#include "proxy.h"
#include <cwctype>
#include <string>

extern "C" void DummyFunc();

extern "C" {
''')
    for ord_num, name in version_exps:
        f.write(f"void* g_Real_{name} = reinterpret_cast<void*>(&DummyFunc);\n")
    for ord_num, name in dxgi_exps:
        f.write(f"void* g_Real_{name} = reinterpret_cast<void*>(&DummyFunc);\n")
    for ord_num, name in winmm_exps:
        real_name = f"g_Real_{name}" if name else f"g_Real_Ordinal{ord_num}"
        f.write(f"void* {real_name} = reinterpret_cast<void*>(&DummyFunc);\n")

    f.write('''
} // extern "C"

namespace proxy
{
namespace
{
HMODULE g_SystemLibrary = nullptr;
ProxyType g_CurrentType = ProxyType::None;
std::wstring g_OriginalLibraryPath;

std::wstring ToLower(std::wstring str)
{
    for (auto& ch : str)
        ch = static_cast<wchar_t>(std::towlower(ch));
    return str;
}
} // namespace

ProxyType GetCurrentType()
{
    return g_CurrentType;
}

const wchar_t* GetCurrentTypeName()
{
    switch (g_CurrentType)
    {
    case ProxyType::Version: return L"version.dll";
    case ProxyType::Winmm: return L"winmm.dll";
    case ProxyType::Dxgi: return L"dxgi.dll";
    default: return L"standalone / ASI";
    }
}

const wchar_t* GetOriginalLibraryPath()
{
    return g_OriginalLibraryPath.c_str();
}

ProxyType Initialize(HINSTANCE instance)
{
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(instance, path, MAX_PATH))
        return ProxyType::None;

    std::wstring modulePath = path;
    const size_t lastSlash = modulePath.find_last_of(L"\\\\/");
    const std::wstring filename = (lastSlash != std::wstring::npos) ? modulePath.substr(lastSlash + 1) : modulePath;
    const std::wstring lowerName = ToLower(filename);

    wchar_t sysDir[MAX_PATH]{};
    GetSystemDirectoryW(sysDir, MAX_PATH);

    if (lowerName == L"version.dll")
    {
        g_CurrentType = ProxyType::Version;
        g_OriginalLibraryPath = std::wstring(sysDir) + L"\\\\version.dll";
        g_SystemLibrary = LoadLibraryExW(g_OriginalLibraryPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (g_SystemLibrary)
        {
''')
    for ord_num, name in version_exps:
        f.write(f'            if (auto* p = GetProcAddress(g_SystemLibrary, "{name}")) g_Real_{name} = reinterpret_cast<void*>(p);\n')
    f.write('''        }
        return g_CurrentType;
    }

    if (lowerName == L"dxgi.dll")
    {
        g_CurrentType = ProxyType::Dxgi;
        g_OriginalLibraryPath = std::wstring(sysDir) + L"\\\\dxgi.dll";
        g_SystemLibrary = LoadLibraryExW(g_OriginalLibraryPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (g_SystemLibrary)
        {
''')
    for ord_num, name in dxgi_exps:
        f.write(f'            if (auto* p = GetProcAddress(g_SystemLibrary, "{name}")) g_Real_{name} = reinterpret_cast<void*>(p);\n')
    f.write('''        }
        return g_CurrentType;
    }

    if (lowerName == L"winmm.dll")
    {
        g_CurrentType = ProxyType::Winmm;
        g_OriginalLibraryPath = std::wstring(sysDir) + L"\\\\winmm.dll";
        g_SystemLibrary = LoadLibraryExW(g_OriginalLibraryPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (g_SystemLibrary)
        {
''')
    for ord_num, name in winmm_exps:
        if name:
            f.write(f'            if (auto* p = GetProcAddress(g_SystemLibrary, "{name}")) g_Real_{name} = reinterpret_cast<void*>(p);\n')
        else:
            f.write(f'            if (auto* p = GetProcAddress(g_SystemLibrary, MAKEINTRESOURCEA({ord_num}))) g_Real_Ordinal{ord_num} = reinterpret_cast<void*>(p);\n')
    f.write('''        }
        return g_CurrentType;
    }

    g_CurrentType = ProxyType::None;
    return g_CurrentType;
}

void Shutdown()
{
    if (g_SystemLibrary)
    {
        FreeLibrary(g_SystemLibrary);
        g_SystemLibrary = nullptr;
    }
}

} // namespace proxy
''')

print("All proxy files generated successfully!")
