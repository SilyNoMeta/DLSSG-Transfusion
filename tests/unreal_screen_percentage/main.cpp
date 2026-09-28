// Runs on Windows (or Wine): a mock of Unreal's
//   static TAutoConsoleVariable<float> CVarScreenPercentage(TEXT("r.ScreenPercentage"), ...);
// in both code shapes (inlined constructor / constructor called with the
// static object), checked against the scanner in unreal_screen_percentage.h.
#include "../../source/native/unreal_screen_percentage.h"

#include <cstdio>

struct IConsoleObject { virtual ~IConsoleObject() = default; virtual float Value() const = 0; };
struct FConsoleVariableFloat final : IConsoleObject
{
    char help[48]{};
    unsigned flags = 0;
    float shadowed[2]{};
    float Value() const override { return shadowed[0]; }
};

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#define FORCEINLINE __forceinline
#else
#define NOINLINE __attribute__((noinline))
#undef FORCEINLINE
#define FORCEINLINE inline __attribute__((always_inline))
#endif

NOINLINE IConsoleObject* RegisterConsoleVariable(const wchar_t* name, float value)
{
    auto* variable = new FConsoleVariableFloat;
    variable->shadowed[0] = variable->shadowed[1] = value;
    std::printf("registered %ls\n", name);
    return variable;
}

struct FAutoConsoleObject
{
    virtual ~FAutoConsoleObject() = default;
    IConsoleObject* Target = nullptr;
};

template <bool Inline>
struct TAutoConsoleVariable : FAutoConsoleObject
{
    float* Ref = nullptr;
    FORCEINLINE TAutoConsoleVariable(const wchar_t* name, float value, int) { Init(name, value); }
    NOINLINE TAutoConsoleVariable(const wchar_t* name, float value) { Init(name, value); }
    FORCEINLINE void Init(const wchar_t* name, float value)
    {
        Target = RegisterConsoleVariable(name, value);
        Ref = static_cast<FConsoleVariableFloat*>(Target)->shadowed;
    }
};

// Decoys declared right before and after it (Unreal initializes many variables
// in a row): a longer name and an unrelated one must not match.
#ifdef OUT_OF_LINE
static TAutoConsoleVariable<false> CVarBefore(L"r.SecondaryScreenPercentage.GameViewport", 50.f);
static TAutoConsoleVariable<false> CVarScreenPercentage(L"r.ScreenPercentage", 100.f);
static TAutoConsoleVariable<false> CVarMode(L"r.ScreenPercentage.Mode", 3.f);
#else
static TAutoConsoleVariable<true> CVarBefore(L"r.SecondaryScreenPercentage.GameViewport", 50.f, 0);
static TAutoConsoleVariable<true> CVarScreenPercentage(L"r.ScreenPercentage", 100.f, 0);
static TAutoConsoleVariable<true> CVarMode(L"r.ScreenPercentage.Mode", 3.f, 0);
#endif

int main()
{
    using namespace unreal_screen_percentage;
    int failures = 0;
    auto check = [&](bool ok, const char* what) { if (!ok) { std::printf("FAILED: %s\n", what); ++failures; } };
    g_log = [](const char* text) { std::printf("%s\n", text); };
    for (int i = 0; i < 40 && g_state.load() == State::Searching; ++i)
        Tick(0);
    check(g_state.load() == State::Found, "r.ScreenPercentage found");
    if (g_state.load() == State::Found)
    {
        check(internal::g_data == CVarScreenPercentage.Ref, "resolved to the right variable");
        Tick(66667);
        check(CVarScreenPercentage.Ref[0] > 66.66f && CVarScreenPercentage.Ref[0] < 66.67f
            && CVarScreenPercentage.Ref[1] == CVarScreenPercentage.Ref[0], "override written to both copies");
        check(CVarMode.Ref[0] == 3.f && CVarBefore.Ref[0] == 50.f, "decoys untouched");
        CVarScreenPercentage.Ref[0] = CVarScreenPercentage.Ref[1] = 80.f;  // the game changes it
        Tick(50000);
        check(CVarScreenPercentage.Ref[0] == 50.f, "override re-applied after a game change");
        Tick(0);
        check(CVarScreenPercentage.Ref[0] == 80.f && CVarScreenPercentage.Ref[1] == 80.f, "game value restored");
    }
    std::printf(failures ? "unreal_screen_percentage: %d failure(s)\n" : "unreal_screen_percentage: all checks passed\n", failures);
    return failures ? 1 : 0;
}
