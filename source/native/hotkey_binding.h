#pragma once
// Keyboard shortcut bindings stored in DLSSG-Transfusion.json, shared by the
// engine (which polls them) and the ReShade add-on (which edits them).
// A binding is "Ctrl+Alt+4", optionally several separated by commas
// ("Ctrl+Alt+4, Ctrl+Alt+Num4"); an empty string disables the action.
// Windows-independent so it can be unit tested: key codes are Win32 VK values.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace hotkey_binding
{
enum Modifier : uint8_t
{
    kCtrl = 1,
    kAlt = 2,
    kShift = 4,
    kWin = 8,
};

struct KeyChord
{
    uint8_t key = 0;        // Win32 virtual-key code, 0 = unused
    uint8_t modifiers = 0;  // Modifier bits
};

constexpr size_t kMaxChords = 3;

struct Binding
{
    KeyChord chords[kMaxChords]{};
    bool Empty() const { return chords[0].key == 0; }
};

enum Action : uint32_t
{
    kFixed2, kFixed3, kFixed4, kFixed5, kFixed6,
    kMultiplierUp, kMultiplierDown,
    kToggleDynamic, kTargetFpsUp, kTargetFpsDown,
    kGameMode,
    kOverlay, kOverlayPosition,
    kActionCount
};

struct ActionInfo
{
    const char* jsonKey;
    const char* label;
    const char* defaults;
    bool repeat;  // fires again every 200 ms while held
};

inline const ActionInfo& Info(uint32_t action)
{
    static const ActionInfo kInfo[kActionCount] = {
        {"hotkeyFixed2", "Fixed 2x", "Ctrl+Alt+2, Ctrl+Alt+Num2", false},
        {"hotkeyFixed3", "Fixed 3x", "Ctrl+Alt+3, Ctrl+Alt+Num3", false},
        {"hotkeyFixed4", "Fixed 4x", "Ctrl+Alt+4, Ctrl+Alt+Num4", false},
        {"hotkeyFixed5", "Fixed 5x", "Ctrl+Alt+5, Ctrl+Alt+Num5", false},
        {"hotkeyFixed6", "Fixed 6x", "Ctrl+Alt+6, Ctrl+Alt+Num6", false},
        {"hotkeyMultiplierUp", "Multiplier +1", "Ctrl+Alt+PageUp", true},
        {"hotkeyMultiplierDown", "Multiplier -1", "Ctrl+Alt+PageDown", true},
        {"hotkeyToggleDynamic", "Toggle Fixed / Dynamic", "Ctrl+Alt+D", false},
        {"hotkeyTargetFpsUp", "Target FPS +5 (+Shift: +1)", "Ctrl+Alt+Up, Ctrl+Alt+Plus, Ctrl+Alt+NumAdd", true},
        {"hotkeyTargetFpsDown", "Target FPS -5 (+Shift: -1)", "Ctrl+Alt+Down, Ctrl+Alt+Minus, Ctrl+Alt+NumSubtract", true},
        {"hotkeyGameMode", "Game decides", "Ctrl+Alt+G", false},
        {"hotkeyOverlay", "Show / hide overlay", "Ctrl+Alt+O", false},
        {"hotkeyOverlayPosition", "Move overlay", "Ctrl+Alt+P", false},
    };
    return kInfo[action < kActionCount ? action : 0];
}

struct KeyName
{
    const char* name;
    uint8_t key;
};

// Named keys (letters, digits and F1-F24 are handled separately).
inline const KeyName* NamedKeys(size_t& count)
{
    static const KeyName kNames[] = {
        {"PageUp", 0x21}, {"PageDown", 0x22}, {"End", 0x23}, {"Home", 0x24},
        {"Left", 0x25}, {"Up", 0x26}, {"Right", 0x27}, {"Down", 0x28},
        {"Insert", 0x2D}, {"Delete", 0x2E}, {"Space", 0x20}, {"Tab", 0x09},
        {"Backspace", 0x08}, {"Enter", 0x0D}, {"Pause", 0x13},
        {"Num0", 0x60}, {"Num1", 0x61}, {"Num2", 0x62}, {"Num3", 0x63}, {"Num4", 0x64},
        {"Num5", 0x65}, {"Num6", 0x66}, {"Num7", 0x67}, {"Num8", 0x68}, {"Num9", 0x69},
        {"NumMultiply", 0x6A}, {"NumAdd", 0x6B}, {"NumSubtract", 0x6D},
        {"NumDecimal", 0x6E}, {"NumDivide", 0x6F},
        {"Plus", 0xBB}, {"Comma", 0xBC}, {"Minus", 0xBD}, {"Period", 0xBE},
    };
    count = sizeof(kNames) / sizeof(kNames[0]);
    return kNames;
}

inline bool EqualsNoCase(const char* a, size_t length, const char* b)
{
    if (std::strlen(b) != length)
        return false;
    for (size_t i = 0; i < length; ++i)
    {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
        if (x != y)
            return false;
    }
    return true;
}

inline bool ParseKey(const char* text, size_t length, uint8_t& key)
{
    if (length == 1)
    {
        char c = text[0];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        {
            key = static_cast<uint8_t>(c);
            return true;
        }
        return false;
    }
    if ((text[0] == 'F' || text[0] == 'f') && length <= 3)
    {
        int number = 0;
        for (size_t i = 1; i < length; ++i)
        {
            if (text[i] < '0' || text[i] > '9')
                return false;
            number = number * 10 + (text[i] - '0');
        }
        if (number >= 1 && number <= 24)
        {
            key = static_cast<uint8_t>(0x70 + number - 1);
            return true;
        }
        return false;
    }
    if (length == 4 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        unsigned value = 0;
        for (size_t i = 2; i < 4; ++i)
        {
            const char c = text[i];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned>(c - 'A' + 10);
            else return false;
        }
        if (value == 0)
            return false;
        key = static_cast<uint8_t>(value);
        return true;
    }
    size_t count = 0;
    const KeyName* names = NamedKeys(count);
    for (size_t i = 0; i < count; ++i)
    {
        if (EqualsNoCase(text, length, names[i].name))
        {
            key = names[i].key;
            return true;
        }
    }
    return false;
}

// Parses one chord such as "Ctrl+Alt+F5". Every chord needs a non-modifier key.
inline bool ParseChord(const char* text, size_t length, KeyChord& chord)
{
    chord = {};
    size_t start = 0;
    bool haveKey = false;
    while (start <= length)
    {
        size_t end = start;
        while (end < length && text[end] != '+')
            ++end;
        // "Ctrl+Alt++" is not supported: use "Plus".
        size_t first = start, last = end;
        while (first < last && text[first] == ' ') ++first;
        while (last > first && text[last - 1] == ' ') --last;
        const char* token = text + first;
        const size_t size = last - first;
        if (size == 0 || haveKey)
            return false;
        if (EqualsNoCase(token, size, "ctrl") || EqualsNoCase(token, size, "control"))
            chord.modifiers |= kCtrl;
        else if (EqualsNoCase(token, size, "alt"))
            chord.modifiers |= kAlt;
        else if (EqualsNoCase(token, size, "shift"))
            chord.modifiers |= kShift;
        else if (EqualsNoCase(token, size, "win"))
            chord.modifiers |= kWin;
        else if (ParseKey(token, size, chord.key))
            haveKey = true;
        else
            return false;
        start = end + 1;
    }
    return haveKey;
}

inline bool Parse(const std::string& text, Binding& binding)
{
    binding = {};
    size_t count = 0, start = 0;
    const size_t firstNonSpace = text.find_first_not_of(' ');
    if (firstNonSpace == std::string::npos)
        return true;  // empty: disabled
    while (start <= text.size())
    {
        size_t end = text.find(',', start);
        if (end == std::string::npos)
            end = text.size();
        if (count == kMaxChords || !ParseChord(text.data() + start, end - start, binding.chords[count]))
            return false;
        ++count;
        start = end + 1;
    }
    return true;
}

inline std::string KeyToString(uint8_t key)
{
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        return std::string(1, static_cast<char>(key));
    if (key >= 0x70 && key <= 0x87)
        return "F" + std::to_string(key - 0x70 + 1);
    size_t count = 0;
    const KeyName* names = NamedKeys(count);
    for (size_t i = 0; i < count; ++i)
        if (names[i].key == key)
            return names[i].name;
    char hex[8];
    std::snprintf(hex, sizeof(hex), "0x%02X", key);
    return hex;
}

inline std::string ChordToString(const KeyChord& chord)
{
    std::string text;
    if (chord.modifiers & kCtrl) text += "Ctrl+";
    if (chord.modifiers & kAlt) text += "Alt+";
    if (chord.modifiers & kShift) text += "Shift+";
    if (chord.modifiers & kWin) text += "Win+";
    return text + KeyToString(chord.key);
}

inline bool Conflicts(const Binding& a, const Binding& b)
{
    for (const KeyChord& x : a.chords)
        for (const KeyChord& y : b.chords)
            if (x.key && x.key == y.key && x.modifiers == y.modifiers)
                return true;
    return false;
}

// True when `chord` is held given the current modifiers. For repeatable FPS
// target actions an extra Shift is allowed (fine step).
inline bool Matches(const KeyChord& chord, bool keyDown, uint8_t modifiers, bool allowExtraShift)
{
    if (!chord.key || !keyDown)
        return false;
    if (modifiers == chord.modifiers)
        return true;
    return allowExtraShift && !(chord.modifiers & kShift) && modifiers == (chord.modifiers | kShift);
}
} // namespace hotkey_binding
