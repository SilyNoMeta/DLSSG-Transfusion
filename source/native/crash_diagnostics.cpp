#include "scatter_experiment.h"
#include "crash_diagnostics.h"
#include <atomic>
#include <cstdint>

namespace crash_diagnostics
{
namespace
{
HANDLE gFile = INVALID_HANDLE_VALUE;
void* gObserver = nullptr;
std::atomic_flag gWritingException = ATOMIC_FLAG_INIT;
std::atomic<unsigned> gExceptionCount{0};
// Preallocated: a stack overflow or damaged heap must not require allocation.
char gExceptionBuffer[16384]{};
CONTEXT gUnwindContext{};
struct ExceptionSite { void* address; DWORD code; unsigned count; };
ExceptionSite gSites[64]{}; // Protected by gWritingException, never a blocking lock.

struct Text
{
    char* data;
    size_t capacity;
    size_t used = 0;
    void Add(const char* value) noexcept
    {
        while (*value && used < capacity) data[used++] = *value++;
    }
    void Hex(uint64_t value) noexcept
    {
        Add("0x");
        for (int shift = 60; shift >= 0 && used < capacity; shift -= 4)
            data[used++] = "0123456789ABCDEF"[(value >> shift) & 15];
    }
    void Field(const char* name, uint64_t value) noexcept
    {
        Add(name); Hex(value); Add(" ");
    }
    void Flush() noexcept
    {
        DWORD written = 0;
        if (gFile != INVALID_HANDLE_VALUE && used)
            WriteFile(gFile, data, static_cast<DWORD>(used), &written, nullptr);
        used = 0;
    }
};

void Address(Text& text, uint64_t address) noexcept
{
    text.Field("pc=", address);
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory)))
    {
        const auto base = reinterpret_cast<uintptr_t>(memory.AllocationBase);
        text.Field("base=", base);
        if (base) text.Field("rva=", address - base);
        text.Field("protect=", memory.Protect);
        text.Field("type=", memory.Type);
    }
    text.Add("\r\n");
}

bool IsRelevant(DWORD code) noexcept
{
    return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR
        || code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_STACK_OVERFLOW
        || code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_PRIV_INSTRUCTION
        || code == 0xC0000374u /* heap corruption */
        || code == 0xC0000409u /* fail fast, if dispatched to VEH */;
}

LONG CALLBACK ObserveException(EXCEPTION_POINTERS* pointers) noexcept
{
    if (!pointers || !pointers->ExceptionRecord || !pointers->ContextRecord
        || !IsRelevant(pointers->ExceptionRecord->ExceptionCode)
        || gWritingException.test_and_set(std::memory_order_acquire))
        return EXCEPTION_CONTINUE_SEARCH;

    // Do not let a driver's repeated handled probe exhaust the whole budget
    // before the later fault we actually need to diagnose.
    for (auto& site : gSites)
    {
        if (!site.count || (site.address == pointers->ExceptionRecord->ExceptionAddress
            && site.code == pointers->ExceptionRecord->ExceptionCode))
        {
            site.address = pointers->ExceptionRecord->ExceptionAddress;
            site.code = pointers->ExceptionRecord->ExceptionCode;
            if (site.count >= 3)
            {
                gWritingException.clear(std::memory_order_release);
                return EXCEPTION_CONTINUE_SEARCH;
            }
            ++site.count;
            break;
        }
    }
    const unsigned count = gExceptionCount.fetch_add(1) + 1;
    Text text{gExceptionBuffer, sizeof(gExceptionBuffer)};
    // Driver probes can intentionally raise handled exceptions. Bound output,
    // and label first-chance records rather than misidentifying them as crashes.
    if (count > 128)
    {
        if (count == 129)
        {
            text.Add("Exception record limit reached (128); subsequent records omitted.\r\n");
            text.Flush();
        }
        gWritingException.clear(std::memory_order_release);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto& exception = *pointers->ExceptionRecord;
    const auto& context = *pointers->ContextRecord;
    text.Add("\r\nFIRST_CHANCE_EXCEPTION (may be handled by the game/driver)\r\n");
    text.Field("sequence=", count);
    text.Field("tickMs=", GetTickCount64());
    text.Field("thread=", GetCurrentThreadId());
    text.Field("code=", exception.ExceptionCode);
    text.Field("flags=", exception.ExceptionFlags);
    text.Add("\r\nexception-address: ");
    Address(text, reinterpret_cast<uintptr_t>(exception.ExceptionAddress));
    if ((exception.ExceptionCode == EXCEPTION_ACCESS_VIOLATION
            || exception.ExceptionCode == EXCEPTION_IN_PAGE_ERROR)
        && exception.NumberParameters >= 2)
    {
        text.Add("access: 0=read, 1=write, 8=execute; ");
        text.Field("operation=", exception.ExceptionInformation[0]);
        text.Field("address=", exception.ExceptionInformation[1]);
        text.Add("\r\n");
    }
#define REG(name) text.Field(#name "=", context.name)
    REG(Rip); REG(Rsp); REG(Rbp); text.Add("\r\n");
    REG(Rax); REG(Rbx); REG(Rcx); REG(Rdx); text.Add("\r\n");
    REG(Rsi); REG(Rdi); REG(R8); REG(R9); text.Add("\r\n");
    REG(R10); REG(R11); REG(R12); REG(R13); REG(R14); REG(R15); text.Add("\r\n");
#undef REG
    // Save the essentials before attempting the optional unwind. Do not use
    // the regular logger, CRT formatting, heap allocation or its file locks.
    text.Flush();
    FlushFileBuffers(gFile);
    if (exception.ExceptionCode != EXCEPTION_STACK_OVERFLOW)
    {
        __try
        {
            gUnwindContext = context;
            text.Add("STACK (best effort, fault context; module bases mapped below/above):\r\n");
            for (unsigned frame = 0; frame < 32 && gUnwindContext.Rip; ++frame)
            {
                text.Field("frame=", frame);
                Address(text, gUnwindContext.Rip);
                const DWORD64 previousSp = gUnwindContext.Rsp;
                DWORD64 imageBase = 0;
                const auto entry = RtlLookupFunctionEntry(gUnwindContext.Rip, &imageBase, nullptr);
                if (entry)
                {
                    void* handlerData = nullptr;
                    DWORD64 establisherFrame = 0;
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, gUnwindContext.Rip,
                        entry, &gUnwindContext, &handlerData, &establisherFrame, nullptr);
                }
                else
                {
                    gUnwindContext.Rip = *reinterpret_cast<const DWORD64*>(gUnwindContext.Rsp);
                    gUnwindContext.Rsp += sizeof(DWORD64);
                }
                if (gUnwindContext.Rsp <= previousSp) break;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            text.Add("Stack unwind stopped: unreadable context/stack.\r\n");
        }
    }
    text.Add("END_EXCEPTION (continuing normal exception dispatch)\r\n");
    text.Flush();
    FlushFileBuffers(gFile);
    gWritingException.clear(std::memory_order_release);
    return EXCEPTION_CONTINUE_SEARCH;
}
}

bool Initialize(const wchar_t* path) noexcept
{
    if (gObserver) return true;
    gFile = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (gFile == INVALID_HANDLE_VALUE) return false;
    char buffer[512]{};
    Text text{buffer, sizeof(buffer)};
    text.Add("DLSSG-Transfusion ");
    text.Add(scatter_experiment::kName);
    text.Add("\r\n");
    text.Field("pid=", GetCurrentProcessId());
    text.Field("startTickMs=", GetTickCount64());
    text.Add("\r\nFirst-chance records are observations, not proof of a fatal crash.\r\n");
    text.Add("Registers/frames use hexadecimal. Match base/RVA against MODULE records.\r\n");
    text.Add("Limits: first 3 occurrences per exception site, 128 records total.\r\n");
    text.Flush();
    gObserver = AddVectoredExceptionHandler(1, &ObserveException);
    if (!gObserver)
    {
        CloseHandle(gFile);
        gFile = INVALID_HANDLE_VALUE;
    }
    return gObserver != nullptr;
}

void Shutdown() noexcept
{
    if (gObserver) RemoveVectoredExceptionHandler(gObserver);
    gObserver = nullptr;
    // Keep the file handle until process teardown: an observer already running
    // on another thread must not write to a closed/reused handle.
}

void RecordModule(HMODULE module, DWORD size, const wchar_t* path) noexcept
{
    if (gFile == INVALID_HANDLE_VALUE) return;
    char buffer[2048]{};
    Text text{buffer, sizeof(buffer)};
    text.Add("MODULE ");
    text.Field("tickMs=", GetTickCount64());
    text.Field("base=", reinterpret_cast<uintptr_t>(module));
    text.Field("size=", size);
    text.Add("path=");
    if (path)
    {
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, path, -1,
            buffer + text.used, static_cast<int>(sizeof(buffer) - text.used - 2), nullptr, nullptr);
        if (bytes > 0) text.used += bytes - 1;
    }
    text.Add("\r\n");
    text.Flush();
}
}
