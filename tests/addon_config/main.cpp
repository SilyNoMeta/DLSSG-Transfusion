// Offline check of the ReShade add-on's in-place JSON editing against the
// layout the engine writes (WriteControlFile in source/native/patcher.cpp).
#include "../../source/addon/config_text.hpp"
#include "../../source/native/hotkey_binding.h"

#include <cstdio>
#include <string>

static int gFailures = 0;
#define CHECK(condition) do { if (!(condition)) { std::printf("FAILED line %d: %s\n", __LINE__, #condition); ++gFailures; } } while (0)

static const char* kEngineFile =
    "{\n"
    "  \"multiplier\": 4,               // Target multiplier: 2 to 6 (Fixed mode)\n"
    "  \"mode\": \"fixed\",                 // \"fixed\" (manual multiplier), \"dynamic\" (auto-adjusts to target FPS) or \"game\" (the game / Profile Inspector decides)\n"
    "  \"disableKeybinds\": false,        // Disable in-game hotkeys\n"
    "  \"dynamicTargetFrameRate\": 0,   // Target FPS for dynamic mode\n"
    "  \"overlayPosition\": \"top-left\",      // Overlay screen position: \"top-left\", \"top-right\"\n"
    "  \"qualityPolicy\": \"explained-warp\",  // valid-warp tuning\n"
    "  \"gpuArchitecture\": \"auto\"       // \"auto\", \"ada\"\n"
    "}\n";

int main()
{
    std::string text = kEngineFile;
    uint32_t number = 0;
    bool flag = true;
    std::string value;

    CHECK(config_text::GetUnsigned(text, "multiplier", number) && number == 4);
    CHECK(config_text::GetString(text, "mode", value) && value == "fixed");
    CHECK(config_text::GetBool(text, "disableKeybinds", flag) && !flag);
    CHECK(config_text::GetString(text, "gpuArchitecture", value) && value == "auto");
    CHECK(!config_text::GetBool(text, "missingKey", flag));

    // Edits replace only the value token and keep comments.
    CHECK(config_text::SetString(text, "mode", "game"));
    CHECK(config_text::SetUnsigned(text, "multiplier", 6));
    CHECK(config_text::SetBool(text, "disableKeybinds", true));
    CHECK(config_text::SetString(text, "gpuArchitecture", "ampere"));
    CHECK(config_text::SetString(text, "overlayPosition", "bottom-right"));
    CHECK(config_text::GetString(text, "mode", value) && value == "game");
    CHECK(config_text::GetUnsigned(text, "multiplier", number) && number == 6);
    CHECK(config_text::GetBool(text, "disableKeybinds", flag) && flag);
    CHECK(config_text::GetString(text, "gpuArchitecture", value) && value == "ampere");
    CHECK(config_text::GetString(text, "overlayPosition", value) && value == "bottom-right");
    CHECK(text.find("// Target multiplier: 2 to 6 (Fixed mode)") != std::string::npos);
    CHECK(text.find("\"top-left\", \"top-right\"") != std::string::npos);
    CHECK(text.find("\"gpuArchitecture\": \"ampere\"       // \"auto\", \"ada\"\n}") != std::string::npos);

    // A missing key is inserted as the first member (no trailing comma).
    CHECK(config_text::SetBool(text, "uiAssist", true));
    CHECK(text.rfind("{\n  \"uiAssist\": true,\n  \"multiplier\"", 0) == 0);
    std::string empty = "{}";
    CHECK(config_text::SetBool(empty, "showOverlay", true));
    CHECK(empty == "{\n  \"showOverlay\": true\n}");

    // Values that would break the file are refused.
    CHECK(!config_text::SetString(text, "mode", "bad\"value"));

    // Layout 3: sections, one // comment after each setting.
    std::string nested =
        "{\n  \"configVersion\": 3,                        // Settings apply live.\n"
        "  \"frameGeneration\": {\n"
        "    \"mode\": \"fixed\",                         // fixed, dynamic or game (the game decides).\n"
        "    \"multiplier\": 4                          // Fixed mode multiplier: 2 to 6.\n  }\n}\n";
    CHECK(config_text::GetString(nested, "mode", value) && value == "fixed");
    CHECK(config_text::SetString(nested, "mode", "game") && config_text::GetString(nested, "mode", value) && value == "game");
    CHECK(config_text::SetUnsigned(nested, "multiplier", 5) && config_text::GetUnsigned(nested, "multiplier", number) && number == 5);
    CHECK(nested.find("\"mode\": \"game\",                         // fixed, dynamic or game") != std::string::npos);
    CHECK(nested.find("\"multiplier\": 5                          // Fixed mode") != std::string::npos);

    // Shortcut bindings (shared by the engine and the add-on).
    using namespace hotkey_binding;
    Binding binding;
    CHECK(Parse("Ctrl+Alt+4", binding) && binding.chords[0].key == '4'
        && binding.chords[0].modifiers == (kCtrl | kAlt) && binding.chords[1].key == 0);
    CHECK(Parse("ctrl + alt + num4, Shift+F12", binding) && binding.chords[0].key == 0x64
        && binding.chords[1].key == 0x7B && binding.chords[1].modifiers == kShift);
    CHECK(Parse("", binding) && binding.Empty());
    CHECK(Parse("  ", binding) && binding.Empty());
    CHECK(Parse("Ctrl+Alt+PageUp", binding) && binding.chords[0].key == 0x21);
    CHECK(Parse("Alt+0x2D", binding) && binding.chords[0].key == 0x2D);
    CHECK(!Parse("Ctrl+Alt", binding));             // no key
    CHECK(!Parse("Ctrl+Alt+Foo", binding));         // unknown key
    CHECK(!Parse("Ctrl+4+5", binding));             // two keys
    CHECK(!Parse("Ctrl+4,", binding));              // empty alternative
    CHECK(!Parse("A, B, C, D", binding));           // too many alternatives
    for (uint32_t action = 0; action < kActionCount; ++action)
    {
        Binding defaults;
        CHECK(Parse(Info(action).defaults, defaults) && !defaults.Empty());
        for (uint32_t other = action + 1; other < kActionCount; ++other)
        {
            Binding otherDefaults;
            Parse(Info(other).defaults, otherDefaults);
            CHECK(!Conflicts(defaults, otherDefaults));
        }
    }
    KeyChord chord{0x7B, kCtrl | kShift};
    CHECK(ChordToString(chord) == "Ctrl+Shift+F12");
    CHECK(Parse(ChordToString(KeyChord{0x6B, kCtrl | kAlt}), binding) && binding.chords[0].key == 0x6B);
    CHECK(Matches(KeyChord{'4', kCtrl | kAlt}, true, kCtrl | kAlt, false));
    CHECK(!Matches(KeyChord{'4', kCtrl | kAlt}, true, kCtrl | kAlt | kShift, false));
    CHECK(Matches(KeyChord{0x26, kCtrl | kAlt}, true, kCtrl | kAlt | kShift, true));
    CHECK(!Matches(KeyChord{'4', kCtrl | kAlt}, false, kCtrl | kAlt, false));

    if (gFailures == 0)
        std::printf("addon_config: all checks passed\n");
    return gFailures == 0 ? 0 : 1;
}
