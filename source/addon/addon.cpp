// Optional ReShade add-on for DLSSG-Transfusion: an in-game settings panel.
// Every setting lives in DLSSG-Transfusion.json (nothing is stored in
// ReShade.ini). The panel edits the file the engine uses; the engine's live
// reload applies each change. The engine runs without this add-on or ReShade.
// It also draws the multiplier overlay where the engine's own (DXGI) overlay
// cannot, as in Vulkan games.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define ImTextureID ImU64
#include <windows.h>
#include <psapi.h>

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>

#include "../native/addon_api.h"
#include "../native/hotkey_binding.h"
#include "config_text.hpp"

namespace
{
constexpr const char* kPanelName = "DLSSG-Transfusion";

// ---------------------------------------------------------------------------
// Engine discovery (by export name, whatever the proxy file is called)

DLSSGTGetConfigPathFn gGetConfigPath = nullptr;
DLSSGTGetStatusFn gGetStatus = nullptr;
DLSSGTSetHotkeyCaptureFn gSetHotkeyCapture = nullptr;
DLSSGTGetOverlayFn gGetOverlay = nullptr;  // optional: absent from older engines
ULONGLONG gLastLookup = 0;

bool ConnectEngine()
{
    if (gGetConfigPath && gGetStatus)
        return true;
    const ULONGLONG now = GetTickCount64();
    if (gLastLookup != 0 && now - gLastLookup < 1000)
        return false;
    gLastLookup = now;

    HMODULE modules[1024]{};
    DWORD bytes = 0;
    if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &bytes))
        return false;
    const DWORD count = bytes / sizeof(HMODULE) < 1024 ? bytes / sizeof(HMODULE) : 1024;
    for (DWORD i = 0; i < count; ++i)
    {
        auto path = reinterpret_cast<DLSSGTGetConfigPathFn>(
            GetProcAddress(modules[i], DLSSGT_EXPORT_GET_CONFIG_PATH));
        auto status = reinterpret_cast<DLSSGTGetStatusFn>(
            GetProcAddress(modules[i], DLSSGT_EXPORT_GET_STATUS));
        if (path && status)
        {
            gGetConfigPath = path;
            gGetStatus = status;
            gSetHotkeyCapture = reinterpret_cast<DLSSGTSetHotkeyCaptureFn>(
                GetProcAddress(modules[i], DLSSGT_EXPORT_SET_HOTKEY_CAPTURE));
            gGetOverlay = reinterpret_cast<DLSSGTGetOverlayFn>(
                GetProcAddress(modules[i], DLSSGT_EXPORT_GET_OVERLAY));
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// DLSSG-Transfusion.json

std::wstring gConfigPath;
std::string gText;                 // current file contents
FILETIME gWriteTime{};
bool gLoaded = false;
char gMessage[256] = "";

struct RestartValue
{
    const char* key;
    std::string startup;           // raw value when the game started
};
// Settings the engine reads only at startup / provider load.
std::vector<RestartValue> gRestartValues = {
    {"forceOTA", {}}, {"patchFlipMetering", {}}, {"blackwellTransfusion", {}},
    {"qualityValidWarp", {}}, {"qualityPolicy", {}}, {"optimizedKernels", {}},
    {"gpuArchitecture", {}}, {"smoothMotionSm86", {}}, {"smoothMotionSm86Api", {}},
};
bool gRestartSnapshotTaken = false;

bool ReadWholeFile(const std::wstring& path, std::string& text, FILETIME& writeTime)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1 << 20)
        && GetFileTime(file, nullptr, nullptr, &writeTime);
    if (ok)
    {
        text.assign(static_cast<size_t>(size.QuadPart), '\0');
        DWORD read = 0;
        ok = ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &read, nullptr)
            && read == text.size();
    }
    CloseHandle(file);
    return ok;
}

void ReloadIfChanged()
{
    if (gConfigPath.empty())
    {
        wchar_t path[MAX_PATH * 4]{};
        if (!gGetConfigPath || gGetConfigPath(path, static_cast<uint32_t>(std::size(path))) == 0)
            return;
        gConfigPath = path;
    }
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(gConfigPath.c_str(), GetFileExInfoStandard, &attributes))
        return;
    if (gLoaded && CompareFileTime(&attributes.ftLastWriteTime, &gWriteTime) == 0)
        return;
    std::string text;
    FILETIME writeTime{};
    if (!ReadWholeFile(gConfigPath, text, writeTime))
        return;
    gText = std::move(text);
    gWriteTime = writeTime;
    gLoaded = true;
    if (!gRestartSnapshotTaken)
    {
        for (auto& value : gRestartValues)
            config_text::GetRaw(gText, value.key, value.startup);
        gRestartSnapshotTaken = true;
    }
}

// Read-modify-write of one value, then an atomic replace so the engine never
// reads a partially written file.
template <typename Edit>
bool EditConfig(const char* key, Edit edit)
{
    std::string text;
    FILETIME ignored{};
    if (!ReadWholeFile(gConfigPath, text, ignored))
    {
        snprintf(gMessage, sizeof(gMessage), "Could not read the configuration file (Windows error %lu).", GetLastError());
        return false;
    }
    if (!edit(text))
    {
        snprintf(gMessage, sizeof(gMessage), "Could not update \"%s\".", key);
        return false;
    }
    const std::wstring temporary = gConfigPath + L".addon.tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        snprintf(gMessage, sizeof(gMessage), "Could not write next to the configuration file (Windows error %lu).", GetLastError());
        return false;
    }
    DWORD written = 0;
    const bool ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr)
        && written == text.size();
    CloseHandle(file);
    // The engine may be rewriting the file itself (hotkeys): retry briefly.
    bool moved = false;
    for (int attempt = 0; ok && !moved && attempt < 10; ++attempt)
    {
        moved = MoveFileExW(temporary.c_str(), gConfigPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (!moved)
            Sleep(5);
    }
    if (!moved)
    {
        // Replacing needs delete access to the file: refused when it is read-only,
        // or open in another program (editor, antivirus) without delete sharing.
        // Rewrite it in place instead.
        const DWORD moveError = GetLastError();
        DeleteFileW(temporary.c_str());
        HANDLE target = CreateFileW(gConfigPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD directWritten = 0;
        const bool direct = target != INVALID_HANDLE_VALUE
            && WriteFile(target, text.data(), static_cast<DWORD>(text.size()), &directWritten, nullptr)
            && directWritten == text.size();
        const DWORD directError = GetLastError();
        if (target != INVALID_HANDLE_VALUE)
            CloseHandle(target);
        if (!direct)
        {
            const DWORD attributes = GetFileAttributesW(gConfigPath.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY))
                snprintf(gMessage, sizeof(gMessage), "Cannot save: DLSSG-Transfusion.json is read-only "
                    "(right-click > Properties > untick Read-only).");
            else if (directError == ERROR_SHARING_VIOLATION || moveError == ERROR_SHARING_VIOLATION)
                snprintf(gMessage, sizeof(gMessage), "Cannot save: DLSSG-Transfusion.json is locked by another "
                    "program (close it in your editor).");
            else
                snprintf(gMessage, sizeof(gMessage), "Cannot save DLSSG-Transfusion.json (Windows error %lu / %lu). "
                    "Check the game folder's permissions.", moveError, directError);
            return false;
        }
    }
    gMessage[0] = '\0';
    gText = std::move(text);
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (GetFileAttributesExW(gConfigPath.c_str(), GetFileExInfoStandard, &attributes))
        gWriteTime = attributes.ftLastWriteTime;
    return true;
}

bool SaveBool(const char* key, bool value)
{
    return EditConfig(key, [&](std::string& text) { return config_text::SetBool(text, key, value); });
}

bool SaveUnsigned(const char* key, uint32_t value)
{
    return EditConfig(key, [&](std::string& text) { return config_text::SetUnsigned(text, key, value); });
}

bool SaveString(const char* key, const char* value)
{
    return EditConfig(key, [&](std::string& text) { return config_text::SetString(text, key, value); });
}

bool NeedsRestart(const char* key)
{
    std::string current;
    config_text::GetRaw(gText, key, current);
    for (const auto& value : gRestartValues)
        if (std::strcmp(value.key, key) == 0)
            return value.startup != current;
    return false;
}

int PendingRestartCount()
{
    int count = 0;
    for (const auto& value : gRestartValues)
        count += NeedsRestart(value.key) ? 1 : 0;
    return count;
}

// ---------------------------------------------------------------------------
// Widgets and panel style

const ImVec4 kGood(0.4627f, 0.7255f, 0.0f, 1.0f);      // #76B900
const ImVec4 kWarning(0.9608f, 0.6510f, 0.1373f, 1.0f); // #F5A623
const ImVec4 kBad(0.9216f, 0.3412f, 0.3412f, 1.0f);     // #EB5757
const ImVec4 kIdle(0.4314f, 0.4627f, 0.5059f, 1.0f);    // #6E7681

namespace ui
{
ImVec4 Hex(unsigned rgb, float alpha = 1.0f)
{
    return ImVec4(((rgb >> 16) & 0xff) / 255.0f, ((rgb >> 8) & 0xff) / 255.0f, (rgb & 0xff) / 255.0f, alpha);
}

const ImVec4 kBgWindow = Hex(0x14161A, 0.94f);
const ImVec4 kBgCard = Hex(0x1D2026);
const ImVec4 kBgControl = Hex(0x262A31);
const ImVec4 kBgControlHover = Hex(0x2F343C);
const ImVec4 kStroke = Hex(0x2A2E36);
const ImVec4 kTextPrimary = Hex(0xE8EAED);
const ImVec4 kTextSecondary = Hex(0x9AA0A8);
const ImVec4 kAccent = Hex(0xA174DC);        // "Iris"
const ImVec4 kAccentHover = Hex(0xB28DE2);
const ImVec4 kAccentActive = Hex(0x8043D0);
const ImVec4 kClear = ImVec4(0, 0, 0, 0);

constexpr float kLabelEm = 22.0f, kLabelShare = 0.5f, kStackEm = 24.0f, kValueEm = 3.5f, kTooltipEm = 35.0f;
constexpr float kSectionGap = 16.0f;
constexpr const char* kRestartGlyph = "\xef\x80\xa1";  // ForkAwesome glyphs, merged by ReShade
constexpr const char* kResetGlyph = "\xef\x83\xa2";

// The only place the panel style is pushed. Constructed first in DrawPanel, so
// the destructor pops on every exit path and nothing leaks into ReShade's other
// tabs or add-ons. The dark background is a full-window child: ReShade has
// already opened its own window when the callback runs.
class PanelScope
{
public:
    PanelScope()
    {
        Color(ImGuiCol_Text, kTextPrimary);
        Color(ImGuiCol_TextDisabled, kTextSecondary);
        Color(ImGuiCol_ChildBg, kBgWindow);
        Color(ImGuiCol_Border, kStroke);
        Color(ImGuiCol_Separator, kStroke);
        Color(ImGuiCol_FrameBg, kBgControl);
        Color(ImGuiCol_FrameBgHovered, kBgControlHover);
        Color(ImGuiCol_FrameBgActive, kBgControlHover);
        Color(ImGuiCol_Button, kBgControl);
        Color(ImGuiCol_ButtonHovered, kBgControlHover);
        Color(ImGuiCol_ButtonActive, kAccentActive);
        Color(ImGuiCol_Header, kBgCard);
        Color(ImGuiCol_HeaderHovered, kBgControlHover);
        Color(ImGuiCol_HeaderActive, kBgControlHover);
        Color(ImGuiCol_SliderGrab, kAccent);
        Color(ImGuiCol_SliderGrabActive, kAccentHover);
        Color(ImGuiCol_CheckMark, kAccent);
        Color(ImGuiCol_PopupBg, kBgCard);
        Color(ImGuiCol_TextLink, kAccent);
        Var(ImGuiStyleVar_FrameRounding, 6.0f);
        Var(ImGuiStyleVar_GrabRounding, 4.0f);
        Var(ImGuiStyleVar_ChildRounding, 8.0f);
        Var(ImGuiStyleVar_PopupRounding, 6.0f);
        Var(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        Var(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 5.0f));
        Var(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
        ImGui::BeginChild("##DLSSG-Transfusion panel", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    }
    ~PanelScope()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar(vars_);
        ImGui::PopStyleColor(colors_);
    }
    PanelScope(const PanelScope&) = delete;
    PanelScope& operator=(const PanelScope&) = delete;

private:
    void Color(ImGuiCol index, const ImVec4& color) { ImGui::PushStyleColor(index, color); ++colors_; }
    void Var(ImGuiStyleVar index, float value) { ImGui::PushStyleVar(index, value); ++vars_; }
    void Var(ImGuiStyleVar index, const ImVec2& value) { ImGui::PushStyleVar(index, value); ++vars_; }
    int colors_ = 0, vars_ = 0;
};

void Tooltip(const char* text)
{
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * kTooltipEm);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// Longest prefix of `text` (cut on a UTF-8 boundary) that fits `width` with "...".
std::string Ellipsize(const char* text, float width, bool* truncated)
{
    *truncated = false;
    if (ImGui::CalcTextSize(text).x <= width)
        return text;
    *truncated = true;
    std::string full = text;
    size_t length = full.size();
    while (length > 0)
    {
        --length;
        while (length > 0 && (static_cast<unsigned char>(full[length]) & 0xC0) == 0x80)
            --length;
        const std::string candidate = full.substr(0, length) + "...";
        if (ImGui::CalcTextSize(candidate.c_str()).x <= width)
            return candidate;
    }
    return "...";
}

struct Row
{
    float controlWidth;
    float resetX;     // window-local x of the reset slot, always reserved at the row end
    bool labelClicked;
};

// Label column (12 em, at most 42% of the width, stacked above the control
// under 24 em), then the cursor is left where the control goes. The label is
// the click target of switches and carries the help tooltip; a pending-restart
// setting gets a badge at the end of the column.
Row RowLabel(const char* label, const char* help, bool restartPending)
{
    const float em = ImGui::GetFontSize();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float frameHeight = ImGui::GetFrameHeight();
    const float x0 = ImGui::GetCursorPosX();
    const float avail = ImGui::GetContentRegionAvail().x;
    const bool stacked = avail < kStackEm * em;
    const float labelWidth = stacked ? avail : (kLabelEm * em < kLabelShare * avail ? kLabelEm * em : kLabelShare * avail);
    const float badgeWidth = restartPending ? 1.6f * em : 0.0f;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    bool truncated = false;
    const std::string shown = Ellipsize(label, labelWidth - badgeWidth, &truncated);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
    ImGui::TextUnformatted(shown.c_str());
    ImGui::PopStyleColor();

    const ImVec2 corner(origin.x + labelWidth, origin.y + frameHeight);
    if (restartPending)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddText(ImVec2(corner.x - 1.3f * em, origin.y + ImGui::GetStyle().FramePadding.y),
            ImGui::GetColorU32(kWarning), kRestartGlyph);
    }

    Row row{};
    row.resetX = x0 + avail - frameHeight;
    const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(origin, corner);
    row.labelClicked = hovered && ImGui::IsMouseClicked(0);
    if (hovered && (help || truncated || restartPending))
    {
        std::string text;
        if (truncated)
            text = std::string(label) + "\n\n";
        if (help)
            text += help;
        if (restartPending)
            text += std::string(help ? "\n\n" : "") + "Restart the game to apply.";
        Tooltip(text.c_str());
    }

    const float resetSlot = frameHeight + spacing;
    if (stacked)
        row.controlWidth = avail - resetSlot;
    else
    {
        ImGui::SameLine(x0 + labelWidth + spacing, 0.0f);
        row.controlWidth = avail - labelWidth - spacing - resetSlot;
    }
    if (row.controlWidth < em)
        row.controlWidth = em;
    return row;
}

// Toggle switch drawn with ImDrawList. The hit-test is an InvisibleButton, so
// mouse, keyboard/gamepad focus and Space/Enter activation work as for any item.
bool Switch(const char* id, bool* value)
{
    const float height = ImGui::GetFrameHeight();
    const float width = height * 1.9f;
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));
    if (pressed)
        *value = !*value;
    const bool hovered = ImGui::IsItemHovered();
    const ImVec4& track = *value ? (hovered ? kAccentHover : kAccent) : (hovered ? kBgControlHover : kBgControl);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(position, ImVec2(position.x + width, position.y + height), ImGui::GetColorU32(track), height * 0.5f);
    const float radius = height * 0.5f - 3.0f;
    draw->AddCircleFilled(ImVec2(*value ? position.x + width - height * 0.5f : position.x + height * 0.5f,
        position.y + height * 0.5f), radius, ImGui::GetColorU32(*value ? kBgWindow : kTextPrimary));
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
        draw->AddRect(position, ImVec2(position.x + width, position.y + height), ImGui::GetColorU32(kTextPrimary),
            height * 0.5f, 0, 2.0f);
    return pressed;
}

// Flat undo button in the row's reset slot, shown only when the value differs
// from its default. True when clicked.
bool ResetButton(const Row& row, const char* defaultText, bool modified)
{
    if (!modified)
        return false;
    ImGui::SameLine(row.resetX, 0.0f);
    const float size = ImGui::GetFrameHeight();
    const std::string label = std::string(kResetGlyph) + "##reset";
    const bool clicked = ImGui::Button(label.c_str(), ImVec2(size, size));
    if (ImGui::IsItemHovered())
    {
        const std::string text = std::string("Reset to default (") + defaultText + ")";
        Tooltip(text.c_str());
    }
    return clicked;
}

// Switch on a labelled row; the label toggles it too. True when the value changed
// (click, label or reset to `fallback`).
bool SwitchRow(const char* label, const char* help, bool restartPending, bool* value,
    const bool* fallback = nullptr)
{
    ImGui::PushID(label);
    const Row row = RowLabel(label, help, restartPending);
    bool changed = row.labelClicked;
    if (changed)
        *value = !*value;
    changed |= Switch("##switch", value);
    if (fallback && ResetButton(row, *fallback ? "on" : "off", *value != *fallback))
    {
        *value = *fallback;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

// Equal-width buttons, the current one in the accent color. -1: nothing clicked;
// -2: the labels do not fit, the caller falls back to a combo.
int Segmented(const char* const* labels, int count, int current, float width)
{
    const float gap = 2.0f;
    const float segment = (width - gap * (count - 1)) / count;
    float widest = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const float w = ImGui::CalcTextSize(labels[i]).x;
        widest = w > widest ? w : widest;
    }
    if (widest + 2.0f * ImGui::GetStyle().FramePadding.x > segment)
        return -2;
    int clicked = -1;
    for (int i = 0; i < count; ++i)
    {
        if (i)
            ImGui::SameLine(0.0f, gap);
        const bool active = i == current;
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentActive);
            ImGui::PushStyleColor(ImGuiCol_Text, kBgWindow);  // dark text on the accent, never the light one
        }
        ImGui::PushID(i);
        if (ImGui::Button(labels[i], ImVec2(segment, 0)))
            clicked = i;
        ImGui::PopID();
        if (active)
            ImGui::PopStyleColor(4);
    }
    return clicked;
}

bool PrimaryButton(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentActive);
    ImGui::PushStyleColor(ImGuiCol_Text, kBgWindow);
    const bool clicked = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    return clicked;
}

enum class Tone { kGood, kWarn, kBad, kIdle, kInfo };

const ImVec4& ToneColor(Tone tone)
{
    switch (tone)
    {
    case Tone::kGood: return kGood;
    case Tone::kWarn: return kWarning;
    case Tone::kBad: return kBad;
    case Tone::kInfo: return kIdle;
    default: return kIdle;
    }
}

const char* ToneGlyph(Tone tone)
{
    switch (tone)
    {
    case Tone::kGood: return "\xef\x80\x8c";  // check
    case Tone::kWarn: return "\xef\x81\xb1";  // warning
    case Tone::kBad: return "\xef\x80\x8d";   // cross
    case Tone::kInfo: return "\xef\x81\x9a";  // info
    default: return "\xef\x84\x8c";           // idle circle
    }
}

// One line of colored status, replaces TextColored(kGood / kWarning).
void Notice(Tone tone, const char* format, ...)
{
    char text[512];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ImGui::TextColored(ToneColor(tone), "%s", ToneGlyph(tone));
    ImGui::SameLine();
    ImGui::TextWrapped("%s", text);
}

struct Card
{
    int id;  // distinct per card shown in the same frame
    Tone tone;
    const char* title;
    const char* body;
    const char* detail;
    const char* fix[3];
    int fixCount;
};

// Status card: rounded card, 4 px stripe in the tone color, glyph and title,
// optional body, detail and numbered fixes. A red or orange card carries its fix.
void StatusCard(const Card& card)
{
    ImGui::PushID(card.id);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgCard);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
    ImGui::BeginChild("##card", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::TextColored(ToneColor(card.tone), "%s", ToneGlyph(card.tone));
    ImGui::SameLine();
    ImGui::TextWrapped("%s", card.title);
    if (card.body)
        ImGui::TextWrapped("%s", card.body);
    if (card.detail)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
        ImGui::TextWrapped("%s", card.detail);
        ImGui::PopStyleColor();
    }
    for (int i = 0; i < card.fixCount; ++i)
        ImGui::TextWrapped("%d. %s", i + 1, card.fix[i]);
    const ImVec2 position = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    ImGui::GetWindowDrawList()->AddRectFilled(position, ImVec2(position.x + 4.0f, position.y + size.y),
        ImGui::GetColorU32(ToneColor(card.tone)), 8.0f, ImDrawFlags_RoundCornersLeft);
    ImGui::EndChild();
    ImGui::PopID();
}

// Per-section record of the settings drawn in it: a section is "modified" when
// one differs from its default, and "reset section" applies every default in a
// single EditConfig. The flag lags one frame (invisible).
struct RecordedSetting
{
    std::string key;
    int type;  // 0 bool, 1 unsigned, 2 string
    bool fallbackBool;
    uint32_t fallbackUnsigned;
    std::string fallbackString;
    bool modified;
};

struct SectionState
{
    bool modified = false;
    std::vector<RecordedSetting> settings;      // previous frame, used by the reset
    bool nextModified = false;
    std::vector<RecordedSetting> nextSettings;  // being recorded this frame
};

std::map<std::string, SectionState> gSections;
SectionState* gCurrentSection = nullptr;

void Record(RecordedSetting setting)
{
    if (!gCurrentSection)
        return;
    gCurrentSection->nextModified |= setting.modified;
    gCurrentSection->nextSettings.push_back(std::move(setting));
}

void RecordBool(const char* key, bool value, bool fallback)
{
    Record({key, 0, fallback, 0, {}, value != fallback});
}

void RecordUnsigned(const char* key, uint32_t value, uint32_t fallback)
{
    Record({key, 1, false, fallback, {}, value != fallback});
}

void RecordString(const char* key, const char* value, const char* fallback)
{
    Record({key, 2, false, 0, fallback, _stricmp(value, fallback) != 0});
}

void ResetSection(const SectionState& state)
{
    EditConfig("section", [&](std::string& text)
    {
        for (const auto& setting : state.settings)
        {
            if (!setting.modified)
                continue;
            const bool ok = setting.type == 0 ? config_text::SetBool(text, setting.key.c_str(), setting.fallbackBool)
                : setting.type == 1 ? config_text::SetUnsigned(text, setting.key.c_str(), setting.fallbackUnsigned)
                : config_text::SetString(text, setting.key.c_str(), setting.fallbackString.c_str());
            if (!ok)
                return false;
        }
        return true;
    });
}

// Starts a section: a small caption with a separator, or a collapsible header
// (with an optional grey hint after the title). "reset section" shows on the
// right when a setting in it is modified. Returns false when collapsed, and the
// caller then draws nothing.
bool BeginSection(const char* name, const char* hint = nullptr, bool collapsible = false, bool openByDefault = true)
{
    SectionState& state = gSections[name];
    state.modified = state.nextModified;
    state.settings = std::move(state.nextSettings);
    state.nextModified = false;
    state.nextSettings.clear();
    gCurrentSection = &state;

    ImGui::PushID(name);
    ImGui::Dummy(ImVec2(0, kSectionGap - ImGui::GetStyle().ItemSpacing.y));
    bool open = true;
    float lineOffset = 0.0f;
    if (!collapsible)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
        ImGui::TextUnformatted(name);
        ImGui::PopStyleColor();
    }
    else
    {
        const float paddingX = 9.0f, paddingY = 7.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(paddingX, paddingY));
        ImGui::SetNextItemOpen(openByDefault, ImGuiCond_Once);
        open = ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::PopStyleVar();
        const ImVec2 corner = ImGui::GetItemRectMin();
        if (hint)
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(corner.x + ImGui::GetFontSize() + paddingX * 3.0f + ImGui::CalcTextSize(name).x
                    + ImGui::GetFontSize() * 1.5f, corner.y + paddingY),
                ImGui::GetColorU32(kTextSecondary), hint);
        lineOffset = paddingY;
    }
    if (state.modified)
    {
        const float x0 = ImGui::GetCursorPosX();
        const float avail = ImGui::GetContentRegionAvail().x;
        const char* link = "Reset section";
        ImGui::SameLine(x0 + avail - ImGui::CalcTextSize(link).x - ImGui::GetStyle().FramePadding.x * 2.0f, 0.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (lineOffset > 0.0f ? lineOffset - 2.0f : 0.0f));
        if (ImGui::SmallButton(link))
            ResetSection(state);
    }
    if (!collapsible)
        ImGui::Separator();
    ImGui::PopID();
    return open;
}
} // namespace ui

void SubSection(const char* name)
{
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushStyleColor(ImGuiCol_Text, ui::kTextSecondary);
    ImGui::TextUnformatted(name);
    ImGui::PopStyleColor();
}

bool RestartPending(const char* key, bool restart)
{
    return restart && NeedsRestart(key);
}

void BoolSetting(const char* key, const char* label, bool fallback, bool restart, const char* help)
{
    bool value = fallback;
    config_text::GetBool(gText, key, value);
    ui::RecordBool(key, value, fallback);
    if (ui::SwitchRow(label, help, RestartPending(key, restart), &value, &fallback))
        SaveBool(key, value);
}

struct Choice
{
    const char* value;
    const char* label;
};

// Segmented buttons when up to four choices fit the row, a combo otherwise.
// `fallback` (a choice value) enables the reset button and "reset section".
void ChoiceSetting(const char* key, const char* label, const Choice* choices, int count,
    bool restart, const char* help, const char* fallback = nullptr)
{
    std::string current = fallback ? fallback : "";
    config_text::GetString(gText, key, current);
    int selected = 0;
    for (int i = 0; i < count; ++i)
        if (_stricmp(current.c_str(), choices[i].value) == 0)
            selected = i;
    if (fallback)
        ui::RecordString(key, choices[selected].value, fallback);
    ImGui::PushID(key);
    const ui::Row row = ui::RowLabel(label, help, RestartPending(key, restart));
    int picked = -1;
    bool fits = false;
    if (count <= 4)
    {
        const char* labels[4];
        for (int i = 0; i < count; ++i)
            labels[i] = choices[i].label;
        const int result = ui::Segmented(labels, count, selected, row.controlWidth);
        fits = result != -2;
        picked = result >= 0 ? result : -1;
    }
    if (!fits)
    {
        ImGui::SetNextItemWidth(row.controlWidth);
        if (ImGui::BeginCombo("##choice", choices[selected].label))
        {
            for (int i = 0; i < count; ++i)
            {
                // The selected row shares the popup's card color: mark it with the accent.
                ImGui::PushStyleColor(ImGuiCol_Text, i == selected ? ui::kAccent : ui::kTextPrimary);
                if (ImGui::Selectable(choices[i].label, i == selected))
                    picked = i;
                ImGui::PopStyleColor();
            }
            ImGui::EndCombo();
        }
    }
    if (picked >= 0 && picked != selected)
        SaveString(key, choices[picked].value);
    if (fallback)
    {
        const char* defaultLabel = fallback;
        for (int i = 0; i < count; ++i)
            if (_stricmp(fallback, choices[i].value) == 0)
                defaultLabel = choices[i].label;
        if (ui::ResetButton(row, defaultLabel, _stricmp(current.c_str(), fallback) != 0))
            SaveString(key, fallback);
    }
    ImGui::PopID();
}

// Numeric setting picked among a few values, as segmented buttons.
void UnsignedSegmented(const char* key, const char* label, const uint32_t* values, const char* const* labels,
    int count, uint32_t fallback, const char* help)
{
    uint32_t stored = fallback;
    config_text::GetUnsigned(gText, key, stored);
    int selected = -1;
    const char* defaultLabel = "default";
    for (int i = 0; i < count; ++i)
    {
        if (values[i] == stored)
            selected = i;
        if (values[i] == fallback)
            defaultLabel = labels[i];
    }
    ui::RecordUnsigned(key, stored, fallback);
    ImGui::PushID(key);
    const ui::Row row = ui::RowLabel(label, help, false);
    const int picked = ui::Segmented(labels, count, selected, row.controlWidth);
    if (picked >= 0 && picked != selected)
        SaveUnsigned(key, values[picked]);
    if (ui::ResetButton(row, defaultLabel, stored != fallback))
        SaveUnsigned(key, fallback);
    ImGui::PopID();
}

// Slider that only writes the file once the user releases it. A SliderInt made
// invisible stays the ImGui item (drag, keyboard, Ctrl+click typing, active and
// deactivated state); the rail, knob and value are drawn over it.
// `resettable` false: no reset button and not part of "reset section".
void UnsignedSlider(const char* key, const char* label, int minimum, int maximum,
    int fallback, const char* format, const char* help, bool resettable = true)
{
    ImGui::PushID(key);
    static std::string editingKey;
    static int editing = 0;
    static std::string typingKey;
    const bool active = editingKey == key;
    const bool typing = typingKey == key;
    uint32_t stored = static_cast<uint32_t>(fallback);
    config_text::GetUnsigned(gText, key, stored);
    if (resettable)
        ui::RecordUnsigned(key, stored, static_cast<uint32_t>(fallback));
    int value = active ? editing : static_cast<int>(stored);
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;

    const float em = ImGui::GetFontSize();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const ui::Row row = ui::RowLabel(label, help, false);
    char widest[32];
    snprintf(widest, sizeof(widest), format, maximum);
    const float valueWidth = ImGui::CalcTextSize(widest).x > ui::kValueEm * em ? ImGui::CalcTextSize(widest).x : ui::kValueEm * em;
    const float sliderWidth = row.controlWidth - valueWidth - spacing > 2 * em ? row.controlWidth - valueWidth - spacing : 2 * em;

    const float frameHeight = ImGui::GetFrameHeight();
    const float knobRadius = 0.32f * frameHeight;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, typing ? ui::kBgControl : ui::kClear);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, typing ? ui::kBgControl : ui::kClear);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, typing ? ui::kBgControl : ui::kClear);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ui::kClear);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ui::kClear);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 2.0f * knobRadius);
    ImGui::SetNextItemWidth(sliderWidth);
    ImGui::SliderInt("##value", &value, minimum, maximum, "", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);

    const bool itemActive = ImGui::IsItemActive();
    const bool itemHovered = ImGui::IsItemHovered();
    const bool typingNow = itemActive && ImGui::GetIO().WantTextInput;
    if (itemActive)
    {
        editingKey = key;
        editing = value;
    }
    else if (active)
        editingKey.clear();
    if (typingNow)
        typingKey = key;
    else if (typing)
        typingKey.clear();
    const bool committed = ImGui::IsItemDeactivatedAfterEdit();

    const ImVec2 low = ImGui::GetItemRectMin();
    const ImVec2 high = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (!typingNow)
    {
        // Same knob travel as ImGui's own grab (imgui_widgets.cpp, SliderBehaviorT).
        const float padding = 2.0f;
        const float usable = (high.x - low.x) - padding * 2.0f;
        float grab = usable / static_cast<float>(maximum - minimum + 1);
        if (grab < 2.0f * knobRadius) grab = 2.0f * knobRadius;
        if (grab > usable) grab = usable;
        const float first = low.x + padding + grab * 0.5f;
        const float last = high.x - padding - grab * 0.5f;
        const float t = maximum > minimum ? static_cast<float>(value - minimum) / static_cast<float>(maximum - minimum) : 0.0f;
        const float knobX = first + (last - first) * t;
        const float centerY = (low.y + high.y) * 0.5f;
        const float railHalf = (0.14f * frameHeight > 2.0f ? 0.14f * frameHeight : 2.0f) * 0.5f;
        draw->AddRectFilled(ImVec2(low.x, centerY - railHalf), ImVec2(high.x, centerY + railHalf),
            ImGui::GetColorU32(ui::kBgControlHover), railHalf);
        draw->AddRectFilled(ImVec2(low.x, centerY - railHalf), ImVec2(knobX, centerY + railHalf),
            ImGui::GetColorU32(ui::kAccent), railHalf);
        if (itemHovered || itemActive)
        {
            ImVec4 halo = ui::kAccent;
            halo.w = 0.35f;
            draw->AddCircleFilled(ImVec2(knobX, centerY), knobRadius + 3.0f, ImGui::GetColorU32(halo));
        }
        draw->AddCircleFilled(ImVec2(knobX, centerY), knobRadius, ImGui::GetColorU32(ui::kTextPrimary));
    }
    char text[32];
    snprintf(text, sizeof(text), format, value);
    draw->AddText(ImVec2(high.x + spacing + valueWidth - ImGui::CalcTextSize(text).x,
        low.y + ImGui::GetStyle().FramePadding.y), ImGui::GetColorU32(ui::kTextPrimary), text);

    if (committed)
        SaveUnsigned(key, static_cast<uint32_t>(value));
    if (resettable)
    {
        char defaultText[32];
        snprintf(defaultText, sizeof(defaultText), format, fallback);
        if (ui::ResetButton(row, defaultText, stored != static_cast<uint32_t>(fallback)))
            SaveUnsigned(key, static_cast<uint32_t>(fallback));
    }
    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// Panel

void DrawStatusCard(const DLSSGTStatus& status, bool haveStatus)
{
    using ui::Tone;
    ui::Card card{};
    char detail[192] = "";
    static char title[64];
    if (!haveStatus)
        card = {1, Tone::kWarn, "Engine status unavailable", "Engine and add-on versions may differ.", nullptr, {}, 0};
    else if (!status.bridgeReady)
        card = {1, Tone::kIdle, "Waiting for the DLSS Frame Generation modules", nullptr, nullptr, {}, 0};
    else if (!gLoaded)
        card = {1, Tone::kIdle, "Waiting for DLSSG-Transfusion.json...", nullptr, nullptr, {}, 0};
    else if (!status.setOptionsSeen)
        card = {1, Tone::kIdle, "Waiting for the game", nullptr, nullptr, {}, 0};
    else if (!status.frameGenerationOn)
        card = {1, Tone::kIdle, "Frame Generation is off in the game", nullptr, nullptr,
            {"Turn Frame Generation on in the game's settings."}, 1};
    else if (status.setOptionsResult == 39)
        card = {1, Tone::kWarn, "Low VRAM", nullptr, nullptr, {"Lower the resolution or the multiplier."}, 1};
    else if (status.setOptionsResult != 0)
    {
        snprintf(title, sizeof(title), "Streamline error %d", status.setOptionsResult);
        card = {1, Tone::kWarn, title, nullptr, nullptr, {}, 0};
    }
    else if (status.pending)
        card = {1, Tone::kWarn, "Change pending", nullptr, nullptr,
            {"If nothing happens, turn Frame Generation off and on in the game."}, 1};
    else
    {
        const bool fresh = status.stateSampleAgeMs <= 2500 && status.actualFramesPresented > 0;
        const int length = fresh ? snprintf(detail, sizeof(detail), "requested %ux | actual %ux",
                status.appliedMultiplier, status.actualFramesPresented)
            : snprintf(detail, sizeof(detail), "requested %ux", status.appliedMultiplier);
        if (status.fpsSampleAgeMs <= 2000 && status.realFpsMilli && status.dlssFpsMilli && length > 0
            && length < static_cast<int>(sizeof(detail)))
            snprintf(detail + length, sizeof(detail) - length, "\n%.0f rendered | %.0f displayed FPS",
                status.realFpsMilli / 1000.0, status.dlssFpsMilli / 1000.0);
        card = {1, Tone::kGood, "Frame Generation is active", nullptr, detail, {}, 0};
    }
    ui::StatusCard(card);
}

void DrawEngineDetails(const DLSSGTStatus& status)
{
    static const char* const kSources[] = {"none", "game", "UI assist"};
    static const char* const kUir[] = {"off", "on", "on (forced)"};
    static char text[1024];
    int length = 0;
    auto add = [&](const char* format, auto... values)
    {
        if (length > 0 && length < static_cast<int>(sizeof(text)) - 1)
            text[length++] = '\n';
        if (length < static_cast<int>(sizeof(text)))
            length += snprintf(text + length, sizeof(text) - length, format, values...);
        if (length >= static_cast<int>(sizeof(text)))
            length = static_cast<int>(sizeof(text)) - 1;
    };
    add("Route: %s", status.route);
    add("UIR: %s | HUD-less: %s | UI alpha: %s",
        kUir[status.uiRecomposition < 3 ? status.uiRecomposition : 0],
        kSources[status.hudlessSource < 3 ? status.hudlessSource : 0],
        status.uiAlphaSource == 2 ? "injected (UI assist)" : kSources[status.uiAlphaSource < 3 ? status.uiAlphaSource : 0]);
    if (status.pacingValid)
        add("Frame pacing: %.2f ms avg | %.2f ms 99th pct | %.2f ms jitter",
            status.pacingAverageUs / 1000.0, status.pacingP99Us / 1000.0, status.pacingJitterUs / 1000.0);
    if (status.gpuValid)
        add("GPU: %u%% | %u C | %.0f W | %u / %u MHz | VRAM %.1f / %.1f GB",
            status.gpuUtilization, status.gpuTemperatureC, status.gpuPowerMilliwatts / 1000.0,
            status.gpuClockMhz, status.gpuMemoryClockMhz, status.vramUsedMb / 1024.0, status.vramTotalMb / 1024.0);
    if (status.debugLine[0])
        add("Debug: %s", status.debugLine);
    add("DLSS %s | DLSS-G %s | Streamline %s",
        status.dlssVersion[0] ? status.dlssVersion : "not loaded",
        status.dlssgVersion[0] ? status.dlssgVersion : "not loaded",
        status.streamlineVersion[0] ? status.streamlineVersion : "not loaded");
    ui::StatusCard({4, ui::Tone::kInfo, "Engine details", nullptr, text, {}, 0});
}

void DrawGeneration()
{
    ui::BeginSection("Frame generation");
    static const Choice modes[] = {
        {"fixed", "Fixed"},
        {"dynamic", "Dynamic"},
        {"game", "Game decides"},
    };
    ChoiceSetting("mode", "Mode", modes, 3, false,
        "Fixed: always use the multiplier below.\n"
        "Dynamic: DLSS-G picks the multiplier to reach the target FPS.\n"
        "Game decides: follow the game's own Frame Generation setting, or NVIDIA Profile Inspector.\n"
        "Applied live.", "game");

    std::string mode = "game";
    config_text::GetString(gText, "mode", mode);
    if (_stricmp(mode.c_str(), "fixed") == 0)
    {
        static const uint32_t values[] = {2, 3, 4, 5, 6};
        static const char* const labels[] = {"2x", "3x", "4x", "5x", "6x"};
        UnsignedSegmented("multiplier", "Multiplier", values, labels, 5, 4,
            "Total frames shown per rendered frame. 5x and 6x are experimental. Applied live.");
    }
    else if (_stricmp(mode.c_str(), "dynamic") == 0)
    {
        static uint32_t lastCustomTarget = 120;
        uint32_t target = 0;
        config_text::GetUnsigned(gText, "dynamicTargetFrameRate", target);
        if (target != 0)
            lastCustomTarget = target;
        bool followDisplay = target == 0;
        static const bool followDefault = true;
        if (ui::SwitchRow("Follow display refresh rate",
                "Aim for the refresh rate of the monitor showing the game. Applied live.", false, &followDisplay,
                &followDefault))
            SaveUnsigned("dynamicTargetFrameRate", followDisplay ? 0 : lastCustomTarget);
        ui::RecordUnsigned("dynamicTargetFrameRate", target, 0);
        if (!followDisplay)
            UnsignedSlider("dynamicTargetFrameRate", "Target FPS", 30, 500, 120, "%d FPS",
                "Frame rate Dynamic mode aims for. Applied live.", false);
        BoolSetting("dynamicExperimental56", "Allow 5x and 6x", false, false,
            "Let Dynamic mode go up to 6x. Needs plenty of VRAM. Applied live.");
    }
    else
    {
        ImGui::TextWrapped("The game (or NVIDIA Profile Inspector) chooses the multiplier and mode.");
    }
}

void DrawDisplay()
{
    if (!ui::BeginSection("Overlay", "corner, extra lines", true, false))
        return;
    BoolSetting("showOverlay", "Show multiplier / FPS overlay", false, false,
        "Small in-game counter drawn by DLSSG-Transfusion (Ctrl+Alt+O). In Vulkan games this add-on draws it, "
        "with the game's frame rate times the multiplier. Applied live.");
    static const Choice corners[] = {
        {"top-left", "Top left"}, {"top-right", "Top right"},
        {"bottom-left", "Bottom left"}, {"bottom-right", "Bottom right"},
    };
    ChoiceSetting("overlayPosition", "Overlay position", corners, 4, false,
        "Screen corner of the overlay (Ctrl+Alt+P). Applied live.", "top-left");
    SubSection("Extra overlay lines");
    BoolSetting("overlayShowUiRecomposition", "UI recomposition (UIR)", false, false,
        "Adds \"UIR ON / ON FORCED / OFF\": whether DLSS-G recomposes the HUD separately. Applied live.");
    BoolSetting("overlayShowHudless", "HUD-less source", false, false,
        "Adds \"HUDLESS GAME / ASSIST / NONE\": where the scene without HUD comes from "
        "(tagged by the game, captured by UI assist, or missing). Applied live.");
    BoolSetting("overlayShowUiAlpha", "UI alpha source", false, false,
        "Adds \"UI ALPHA GAME / INJECTED / NONE\": where the UI layer comes from "
        "(tagged by the game, injected by UI assist, or missing). Applied live.");
    BoolSetting("overlayShowVersions", "DLSS / DLSS-G / Streamline versions", false, false,
        "Adds \"SR x FG y SL z\": versions of nvngx_dlss.dll, nvngx_dlssg.dll and sl.interposer.dll "
        "loaded by the game (OTA and swapped DLLs included). Applied live.");
    BoolSetting("overlayShowFramePacing", "Frame pacing", false, false,
        "Adds \"FT / P99 / JIT\": average time between displayed frames (generated ones included), its 99th "
        "percentile and its jitter (standard deviation). Even pacing means P99 close to FT and a low JIT. Applied live.");
    BoolSetting("overlayShowGpu", "GPU load, temp, power, clocks", false, false,
        "Adds \"GPU % C W MHZ\" read from the NVIDIA driver (NVML) once per second. Applied live.");
    BoolSetting("overlayShowVram", "VRAM usage", false, false,
        "Adds \"VRAM used/total GB\" for the whole GPU (all processes), read from NVML. Applied live.");
    BoolSetting("overlayShowDebug", "Debug line", false, false,
        "Adds mode, generated-frame ceiling (MAX), Dynamic pacer hook (PACER ON/OFF: OFF means Streamline's own "
        "calculator picks the Dynamic multiplier), last Streamline result (SL 0 = OK) and patch route. Applied live.");
}

// ---------------------------------------------------------------------------
// Keyboard shortcuts

int gListening = -1;  // action being recorded

void SetCapture(bool active)
{
    static bool current = false;
    if (active != current && gSetHotkeyCapture)
        gSetHotkeyCapture(active ? 1 : 0);
    current = active;
}

void DrawHotkeys(reshade::api::effect_runtime* runtime)
{
    using namespace hotkey_binding;
    if (!ui::BeginSection("Keyboard shortcuts", nullptr, true, false))
    {
        // Collapsed: nothing is recorded or typed, so the engine keeps its shortcuts.
        gListening = -1;
        SetCapture(false);
        return;
    }
    static char buffers[kActionCount][128]{};
    static FILETIME loadedFrom{};
    static bool dirty = false, loaded = false;
    static char message[160] = "";
    static bool messageGood = false;
    if (!dirty && (!loaded || CompareFileTime(&loadedFrom, &gWriteTime) != 0))
    {
        for (uint32_t action = 0; action < kActionCount; ++action)
        {
            std::string value = Info(action).defaults;
            config_text::GetString(gText, Info(action).jsonKey, value);
            strncpy_s(buffers[action], value.c_str(), _TRUNCATE);
        }
        loadedFrom = gWriteTime;
        loaded = true;
    }

    bool disabled = false;
    config_text::GetBool(gText, "disableKeybinds", disabled);
    if (disabled)
        ui::Notice(ui::Tone::kWarn, "Keyboard shortcuts are disabled (disableKeybinds in DLSSG-Transfusion.json).");
    ImGui::PushStyleColor(ImGuiCol_Text, ui::kTextSecondary);
    ImGui::TextWrapped("Record a combination or type it (e.g. Ctrl+Alt+F5). Separate alternatives with commas; "
        "leave empty to disable an action.");
    ImGui::PopStyleColor();

    const float em = ImGui::GetFontSize();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    bool captureStarted = false;
    for (uint32_t action = 0; action < kActionCount; ++action)
    {
        ImGui::PushID(static_cast<int>(action));
        const ui::Row row = ui::RowLabel(Info(action).label, nullptr, false);
        const float recordWidth = em * 5.5f;
        const float bindingWidth = row.controlWidth - recordWidth - spacing;
        ImGui::SetNextItemWidth(bindingWidth > 2 * em ? bindingWidth : 2 * em);
        ImGui::BeginDisabled(gListening >= 0);
        if (ImGui::InputText("##binding", buffers[action], sizeof(buffers[action])))
            dirty = true;
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(gListening == static_cast<int>(action) ? "Cancel" : "Record", ImVec2(recordWidth, 0)))
        {
            if (gListening == static_cast<int>(action))
                gListening = -1;
            else
            {
                gListening = static_cast<int>(action);
                captureStarted = true;
            }
        }
        if (ui::ResetButton(row, Info(action).defaults[0] ? Info(action).defaults : "none",
                std::strcmp(buffers[action], Info(action).defaults) != 0))
        {
            strncpy_s(buffers[action], Info(action).defaults, _TRUNCATE);
            dirty = true;
        }
        ImGui::PopID();
    }

    if (gListening >= 0)
    {
        ui::Notice(ui::Tone::kWarn, "Press the combination for \"%s\" (Escape cancels).", Info(gListening).label);
        for (uint32_t key = 8; !captureStarted && key < 255; ++key)
        {
            if (!runtime->is_key_pressed(key))
                continue;
            if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU || key == VK_LWIN || key == VK_RWIN
                || (key >= VK_LSHIFT && key <= VK_RMENU) || key == VK_LBUTTON || key == VK_RBUTTON || key == VK_MBUTTON)
                continue;
            if (key == VK_ESCAPE)
            {
                gListening = -1;
                break;
            }
            KeyChord chord;
            chord.key = static_cast<uint8_t>(key);
            chord.modifiers = static_cast<uint8_t>((runtime->is_key_down(VK_CONTROL) ? kCtrl : 0)
                | (runtime->is_key_down(VK_MENU) ? kAlt : 0) | (runtime->is_key_down(VK_SHIFT) ? kShift : 0)
                | ((runtime->is_key_down(VK_LWIN) || runtime->is_key_down(VK_RWIN)) ? kWin : 0));
            strncpy_s(buffers[gListening], ChordToString(chord).c_str(), _TRUNCATE);
            dirty = true;
            gListening = -1;
            break;
        }
    }
    // The engine ignores its shortcuts while one is recorded or typed.
    SetCapture(gListening >= 0 || ImGui::IsAnyItemActive());

    if (ui::PrimaryButton("Save shortcuts"))
    {
        Binding parsed[kActionCount];
        int invalid = -1, conflictA = -1, conflictB = -1;
        for (uint32_t action = 0; action < kActionCount && invalid < 0; ++action)
            if (!Parse(buffers[action], parsed[action]))
                invalid = static_cast<int>(action);
        for (uint32_t a = 0; invalid < 0 && conflictA < 0 && a < kActionCount; ++a)
            for (uint32_t b = a + 1; b < kActionCount; ++b)
                if (Conflicts(parsed[a], parsed[b]))
                {
                    conflictA = static_cast<int>(a);
                    conflictB = static_cast<int>(b);
                    break;
                }
        messageGood = false;
        if (invalid >= 0)
            snprintf(message, sizeof(message), "\"%s\": unknown key or invalid combination.", Info(invalid).label);
        else if (conflictA >= 0)
            snprintf(message, sizeof(message), "\"%s\" and \"%s\" use the same combination.",
                Info(conflictA).label, Info(conflictB).label);
        else if (EditConfig("hotkeys", [&](std::string& text)
            {
                for (uint32_t action = 0; action < kActionCount; ++action)
                    if (!config_text::SetString(text, Info(action).jsonKey, buffers[action]))
                        return false;
                return true;
            }))
        {
            dirty = false;
            messageGood = true;
            snprintf(message, sizeof(message), "Shortcuts saved and active. No restart needed.");
        }
        else
            snprintf(message, sizeof(message), "%s", gMessage);
    }
    ImGui::SameLine();
    if (ImGui::Button("Default shortcuts"))
    {
        for (uint32_t action = 0; action < kActionCount; ++action)
            strncpy_s(buffers[action], Info(action).defaults, _TRUNCATE);
        dirty = true;
    }
    if (dirty)
        ui::Notice(ui::Tone::kWarn, "Unsaved changes");
    if (message[0])
        ui::Notice(messageGood ? ui::Tone::kGood : ui::Tone::kWarn, "%s", message);
}

void DrawQuality()
{
    if (!ui::BeginSection("Image quality", "restart", true, true))
        return;
    BoolSetting("blackwellTransfusion", "Blackwell kernel transfusion", true, true,
        "Uses the RTX 50 (sm_120) frame-generation kernels on this GPU. Required for 3x-6x quality. "
        "Read when DLSS-G loads: restart the game.");
    BoolSetting("qualityValidWarp", "Anti-tearing / anti-ghosting", true, true,
        "Candidate agreement firewall and thin-geometry protection (fences, foliage). "
        "Read when DLSS-G loads: restart the game.");
    static const Choice policies[] = {
        {"explained-warp", "Explained warp (default)"},
        {"transfusion", "Transfusion"},
    };
    ChoiceSetting("qualityPolicy", "Protection tuning", policies, 2, true,
        "Tuning of the protection above. Explained warp is the recommended default. "
        "Read when DLSS-G loads: restart the game.", "explained-warp");
    BoolSetting("optimizedKernels", "Optimized kernels", true, true,
        "Faster, bit-exact frame-generation kernels. Read when DLSS-G loads: restart the game.");
}

void DrawDlssSuperResolution()
{
    ui::BeginSection("DLSS Super Resolution");
    DLSSGTStatus status{};
    status.size = sizeof(status);
    status.version = DLSSGT_ADDON_API_VERSION;
    if (!gGetStatus || !gGetStatus(&status))
        return;

    if (status.srObserved && status.srOutputWidth)
        ImGui::TextDisabled("Rendering %u x %u -> %u x %u (%.1f%%)", status.srInputWidth, status.srInputHeight,
            status.srOutputWidth, status.srOutputHeight, 100.0 * status.srInputWidth / status.srOutputWidth);
    else if (status.srHooked)
        ImGui::TextDisabled("DLSS Super Resolution is not running right now.");
    else
        ImGui::TextDisabled("DLSS Super Resolution not detected yet (D3D12 games only).");

    static const Choice presets[] = {
        {"game", "Game setting"}, {"dlaa", "DLAA (100%)"}, {"quality", "Quality (66.7%)"},
        {"balanced", "Balanced (58.8%)"}, {"performance", "Performance (50%)"},
        {"ultra-performance", "Ultra Performance (33.3%)"}, {"custom", "Custom"},
    };
    ChoiceSetting("dlssRenderScale", "Render resolution", presets, 7, false,
        "Forces the resolution the game renders at before DLSS upscales it, for games that use DLSS but "
        "hide its quality setting. Game setting leaves the game in control. It changes the render "
        "resolution, not the K/M neural model. Applied live, but some games only pick it up after a "
        "resolution or graphics change, or ignore it. In Unreal Engine games, r.ScreenPercentage is "
        "driven live when it can be found (shown below).", "game");
    std::string preset;
    config_text::GetString(gText, "dlssRenderScale", preset);
    if (_stricmp(preset.c_str(), "custom") == 0)
        UnsignedSlider("dlssCustomScale", "Custom scale", 50, 100, 67, "%d%%",
            "Render resolution in percent of the output, per axis (width and height). Applied live.");

    if (status.unrealState == 2)
        ui::Notice(ui::Tone::kGood, "Unreal Engine: live control of r.ScreenPercentage (now %.1f%%).",
            status.unrealScreenPercentageMilli / 1000.0);
    else if (status.unrealState == 0)
        ImGui::TextDisabled("Looking for Unreal Engine's r.ScreenPercentage...");

    if (status.srScale)
    {
        if (status.srVerified)
            ui::Notice(ui::Tone::kGood, "The game renders at the requested resolution.");
        else if (status.srObserved)
            ui::Notice(ui::Tone::kWarn, "Waiting for the game to render at the requested resolution. If it never "
                "changes, this game ignores the override: use its own settings.");
    }
}

void DrawUi()
{
    if (!ui::BeginSection("HUD / UI", nullptr, true, true))
        return;
    BoolSetting("autoUiRecomposition", "Automatic UI recomposition", true, false,
        "Turns UI recomposition on when the game provides HUD-less and UI buffers without asking for it. "
        "Turn off to follow the game's own choice, if the generated frames look wrong with it. Applied live.");
    BoolSetting("forceUiRecomposition", "Force UI recomposition", false, false,
        "Asks DLSS-G to recompose the HUD separately even when the game does not request it. "
        "Reduces HUD ghosting when the game provides the right buffers. Applied live; some games "
        "need Frame Generation turned off and on.");
    BoolSetting("uiAssist", "UI assist (D3D12)", true, false,
        "Captures the HUD-less scene and builds the UI layer when the game does not tag them. "
        "Applied live.");
}

void DrawCompatibility()
{
    if (!ui::BeginSection("Compatibility", "only change if you know what you are doing", true, true))
        return;
    SubSection("Smooth Motion (RTX 30, experimental)");
    BoolSetting("smoothMotionSm86", "Smooth Motion (RTX 30)", false, true,
        "Experimental driver Smooth Motion for RTX 30 (Ampere). Only the NVIDIA 617.14 driver build "
        "has been verified. Read at game start: restart the game.");
    bool smoothMotion = false;
    config_text::GetBool(gText, "smoothMotionSm86", smoothMotion);
    static const Choice smoothMotionApis[] = {
        {"d3d12", "Direct3D 12"}, {"d3d11", "Direct3D 11"}, {"vulkan", "Vulkan"},
    };
    ImGui::BeginDisabled(!smoothMotion);
    ChoiceSetting("smoothMotionSm86Api", "Graphics API", smoothMotionApis, 3, true,
        "Pick the graphics API the game really uses, not the engine (see the game's page on "
        "https://www.pcgamingwiki.com/). Read at game start: restart the game.", "d3d12");
    ImGui::EndDisabled();
    SubSection("Other");
    static const Choice gpus[] = {
        {"auto", "Automatic"}, {"ada", "RTX 40 (Ada)"},
        {"ampere", "RTX 30 (Ampere)"}, {"turing", "RTX 20 (Turing)"},
    };
    ChoiceSetting("gpuArchitecture", "GPU architecture", gpus, 4, true,
        "Which kernel patches the engine applies. Leave on Automatic unless detection fails. "
        "Read at game start: restart the game.", "auto");
    BoolSetting("disableMenuDetection", "Disable menu detection", false, false,
        "Keeps frame generation running in menus and loading screens. Leave off: idling at 1x there "
        "avoids device-hang crashes in several games. Applied live.");
    BoolSetting("forceOTA", "Force NVIDIA OTA models", false, true,
        "Loads the Frame Generation models downloaded by the NVIDIA App. Read at game start: restart the game.");
    BoolSetting("patchFlipMetering", "OptiScaler flip metering bypass", false, true,
        "Only for OptiScaler setups that need it. Read at game start: restart the game.");
    BoolSetting("disableMvDilation", "Declare motion vectors as dilated", false, false,
        "Legacy experiment: tells DLSS-G the game's motion vectors are already dilated, so it skips its own "
        "dilation. Not needed with the image-quality protection; leave off. Applied live.");
}

void DrawDiagnostics()
{
    if (!ui::BeginSection("Diagnostics", nullptr, true, false))
        return;
    BoolSetting("logPerformance", "Log performance", false, false,
        "Writes FPS and frame times to DLSSG-Transfusion_perf.csv. Applied live.");
    BoolSetting("logMotionTracing", "Log motion tracing", false, false,
        "Very verbose motion-vector diagnostics for debugging only. Applied live.");
    BoolSetting("logHudUi", "Log HUD/UI", false, false,
        "Trace the first three game HUD-less copies and D3D12 barrier steps. Applied live.");
}

void DrawPanel(reshade::api::effect_runtime* runtime)
{
    // Owns every style push (and the background child) until this function
    // returns, whichever way it returns. Nothing is pushed anywhere else.
    ui::PanelScope style;
    static const ULONGLONG firstFrame = GetTickCount64();
    if (!ConnectEngine())
    {
        // Red only once the engine is really missing, so it does not flash while the game loads.
        if (GetTickCount64() - firstFrame > 5000)
            ui::StatusCard({1, ui::Tone::kBad, "DLSSG-Transfusion engine not found", nullptr, nullptr,
                {"Copy DLSSG-Transfusion.dll (or the .asi) next to the game executable.", "Restart the game."}, 2});
        else
            ui::StatusCard({1, ui::Tone::kIdle, "Looking for the engine...", nullptr, nullptr, {}, 0});
        return;
    }
    ReloadIfChanged();
    DLSSGTStatus status{};
    status.size = sizeof(status);
    status.version = DLSSGT_ADDON_API_VERSION;
    const bool haveStatus = gGetStatus && gGetStatus(&status);
    DrawStatusCard(status, haveStatus);
    if (!gLoaded)
        return;
    const int restart = PendingRestartCount();
    if (restart)
    {
        static char title[96];
        snprintf(title, sizeof(title), "%d change(s) take effect after restarting the game", restart);
        ui::StatusCard({2, ui::Tone::kWarn, title, nullptr, nullptr, {}, 0});
    }
    if (gMessage[0])
        ui::StatusCard({3, ui::Tone::kWarn, "Could not save the settings", gMessage, nullptr, {}, 0});
    if (haveStatus)
        DrawEngineDetails(status);
    DrawGeneration();
    DrawDlssSuperResolution();
    DrawDisplay();
    DrawQuality();
    DrawUi();
    DrawCompatibility();
    DrawDiagnostics();
    DrawHotkeys(runtime);

    ui::BeginSection("Configuration file");
    char path[MAX_PATH * 4]{};
    WideCharToMultiByte(CP_UTF8, 0, gConfigPath.c_str(), -1, path, sizeof(path), nullptr, nullptr);
    ImGui::TextDisabled("%s", path);
    ImGui::TextDisabled("Settings are saved immediately; shortcuts when you press \"Save shortcuts\".");
}

bool OnOverlay(reshade::api::effect_runtime*, bool open, reshade::api::input_source)
{
    if (!open)
    {
        gListening = -1;
        SetCapture(false);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Overlay (every frame, menu open or closed)

// Draws the engine's overlay text when its DXGI overlay has not drawn for a
// second: Vulkan games, or a DXGI overlay that could not start. The text is
// the engine's, refreshed four times a second like a counter.
void DrawGameOverlay(reshade::api::effect_runtime*)
{
    static DLSSGTOverlay overlay{};
    static ULONGLONG refreshed = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - refreshed >= 250)
    {
        refreshed = now;
        overlay = {};
        overlay.size = sizeof(overlay);
        if (!ConnectEngine() || !gGetOverlay || !gGetOverlay(&overlay))
            overlay = {};
    }
    if (!overlay.visible || overlay.nativeDrawing || overlay.lineCount == 0)
        return;

    // Only functions of ReShade's ImGui table are available (no viewports).
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float margin = 10.0f;
    const bool right = overlay.position == 1 || overlay.position == 2;
    const bool bottom = overlay.position == 2 || overlay.position == 3;
    ImGui::SetNextWindowPos(ImVec2(right ? display.x - margin : margin, bottom ? display.y - margin : margin),
        ImGuiCond_Always, ImVec2(right ? 1.0f : 0.0f, bottom ? 1.0f : 0.0f));
    ImGui::SetNextWindowBgAlpha(0.38f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("##DLSSG-Transfusion overlay", nullptr, flags))
    {
        const uint32_t count = overlay.lineCount < DLSSGT_OVERLAY_MAX_LINES ? overlay.lineCount : DLSSGT_OVERLAY_MAX_LINES;
        for (uint32_t line = 0; line < count; ++line)
        {
            overlay.lines[line][DLSSGT_OVERLAY_LINE_LENGTH - 1] = '\0';
            ImGui::TextUnformatted(overlay.lines[line]);
        }
    }
    ImGui::End();
}

bool gRegistered = false;
} // namespace

extern "C" __declspec(dllexport) const char* NAME = "DLSSG-Transfusion";
extern "C" __declspec(dllexport) const char* DESCRIPTION =
    "Optional settings panel for DLSSG-Transfusion. Edits DLSSG-Transfusion.json; the engine runs without it.";

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addonModule, HMODULE reshadeModule)
{
    if (gRegistered)
        return true;
    if (!reshade::register_addon(addonModule, reshadeModule))
        return false;
    reshade::register_overlay(kPanelName, DrawPanel);
    reshade::register_event<reshade::addon_event::reshade_open_overlay>(OnOverlay);
    // Called every frame, even with the ReShade menu closed.
    reshade::register_event<reshade::addon_event::reshade_overlay>(DrawGameOverlay);
    gRegistered = true;
    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addonModule, HMODULE reshadeModule)
{
    if (!gRegistered)
        return;
    SetCapture(false);
    reshade::unregister_event<reshade::addon_event::reshade_overlay>(DrawGameOverlay);
    reshade::unregister_event<reshade::addon_event::reshade_open_overlay>(OnOverlay);
    reshade::unregister_overlay(kPanelName, DrawPanel);
    reshade::unregister_addon(addonModule, reshadeModule);
    gRegistered = false;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
