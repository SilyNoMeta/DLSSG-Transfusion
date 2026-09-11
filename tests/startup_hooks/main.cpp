// Include the implementation to exercise the actual installers in both orders.
// This is an EXE: the mod's DllMain is never invoked and no worker is started.
#include "../../source/native/patcher.cpp"
#include <thread>
#include <malloc.h>

namespace
{
void InstallImports()
{
    InstallFeatureFunctionHook();
    InstallD3DDeviceHook();
    InstallUiTagHooks();
}

DWORD WINAPI SmallStackNotifications(void*)
{
    __try
    {
        // Driver helper threads can reserve only 64 KiB. Even notifications
        // doing no work must fit: the function prologue runs before its branch.
        DllMain(GetModuleHandleW(nullptr), DLL_THREAD_ATTACH, nullptr);
        DllMain(GetModuleHandleW(nullptr), DLL_THREAD_DETACH, nullptr);
        return 0;
    }
    __except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW
        ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
    }
    _resetstkoflw();
    return 8;
}
}

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "small-stack") == 0)
    {
        HANDLE thread = CreateThread(nullptr, 64 * 1024, &SmallStackNotifications,
            nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
        if (!thread) return 9;
        WaitForSingleObject(thread, INFINITE);
        DWORD result = 10;
        GetExitCodeThread(thread, &result);
        CloseHandle(thread);
        if (result == 0) puts("SMALL_STACK_THREAD_NOTIFICATIONS_OK");
        else fprintf(stderr, "Thread notification failed: %lu (8=stack overflow)\n", result);
        return static_cast<int>(result);
    }
    // Force all four imports into the executable, including the frame-tag API.
    auto* volatile frameImport = &slSetTagForFrame;
    (void)frameImport;
    if (argc == 2 && strcmp(argv[1], "concurrent") == 0)
    {
        std::vector<std::thread> installers;
        for (unsigned i = 0; i < 8; ++i)
            installers.emplace_back([] {
                for (unsigned retry = 0; retry < 50; ++retry)
                {
                    InstallInterposerDetours();
                    InstallImports();
                }
            });
        for (auto& installer : installers)
            installer.join();
    }
    if (argc == 2 && strcmp(argv[1], "busy-transaction") == 0)
    {
        if (DetourTransactionBegin() != NO_ERROR)
            return 5;
        InstallInterposerDetours();
        if (gInterposerDetoursInstalled.load())
            return 6;
        if (DetourTransactionAbort() != NO_ERROR)
            return 7;
    }
    if (argc == 2 && strcmp(argv[1], "imports-first") == 0)
        InstallImports();
    InstallInterposerDetours();
    if (!gInterposerDetoursInstalled.load())
        return 1;
    InstallImports();
    InstallImports(); // Repeat discovery, as the load hook and worker do.

    bool valid = true;
#define CHECK_ORIGINAL(original, trampoline) \
    if (!(trampoline) || (original).load() != (trampoline)) { \
        fprintf(stderr, "%s no longer points to its trampoline\n", #original); \
        valid = false; \
    }
    CHECK_ORIGINAL(gOriginalGetFeatureFunction, gDetourSlGetFeatureFunction);
    CHECK_ORIGINAL(gOriginalSetD3DDevice, gDetourSlSetD3DDevice);
    CHECK_ORIGINAL(gOriginalSetTag, gDetourSlSetTag);
    CHECK_ORIGINAL(gOriginalSetTagForFrame, gDetourSlSetTagForFrame);
#undef CHECK_ORIGINAL
    // Do not recurse into a known-bad original: fail with a useful diagnostic.
    if (!valid)
        return 2;

    void* function = nullptr;
    const sl::ViewportHandle viewport{0u};
    if (slGetFeatureFunction(sl::kFeatureDLSS_G, "missing", function)
            != sl::Result::eErrorFeatureMissing
        || slSetD3DDevice(nullptr) != sl::Result::eOk
        || slSetTag(viewport, nullptr, 0, nullptr) != sl::Result::eOk)
        return 3;
    // Exercise the export route used by games resolving functions dynamically.
    auto* dynamicLookup = reinterpret_cast<PFun_slGetFeatureFunction*>(
        GetProcAddress(GetModuleHandleW(L"sl.interposer.dll"), "slGetFeatureFunction"));
    if (!dynamicLookup || dynamicLookup(sl::kFeatureDLSS_G, "missing", function)
            != sl::Result::eErrorFeatureMissing)
        return 4;
    puts("STARTUP_HOOK_ORDER_OK");
    return 0;
}
