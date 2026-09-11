#include "../../source/native/crash_diagnostics.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

// Raise a real protection fault, with the game's own SEH handler still in
// charge of recovery. The diagnostics observer must not consume the exception.
bool HandledAccessViolation(void* page)
{
    __try
    {
        *static_cast<volatile unsigned*>(page) = 42;
    }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
        ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return true;
    }
    return false;
}

int main()
{
    constexpr wchar_t path[] = L"diagnostics-test.crash.log";
    if (!crash_diagnostics::Initialize(path)) return 1;
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    crash_diagnostics::RecordModule(GetModuleHandleW(nullptr), 0, executable);
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    if (!page) return 2;
    bool caught = true;
    for (unsigned probe = 0; probe < 5; ++probe)
        caught = HandledAccessViolation(page) && caught;
    VirtualFree(page, 0, MEM_RELEASE);
    crash_diagnostics::Shutdown();
    if (!caught) return 3;
    std::ifstream file(path, std::ios::binary);
    const std::string log{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    size_t count = 0;
    for (size_t pos = 0; (pos = log.find("FIRST_CHANCE_EXCEPTION", pos)) != std::string::npos; ++pos)
        ++count;
    if (count != 3) return 5;
    if (log.find("FIRST_CHANCE_EXCEPTION") == std::string::npos
        || log.find("code=0x00000000C0000005") == std::string::npos
        || log.find("operation=0x0000000000000001") == std::string::npos
        || log.find("Rip=0x") == std::string::npos
        || log.find("STACK (") == std::string::npos
        || log.find("frame=0x0000000000000000") == std::string::npos
        || log.find("rva=0x") == std::string::npos
        || log.find("END_EXCEPTION") == std::string::npos
        || log.find("MODULE ") == std::string::npos
        || log.find("DiagnosticsHarness.exe") == std::string::npos)
    {
        fputs(log.c_str(), stderr);
        return 4;
    }
    puts("REAL_ACCESS_VIOLATION_LOGGED_AND_GAME_HANDLER_PRESERVED");
    return 0;
}
