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
#include <cstring>
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
    {"gpuArchitecture", {}},
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
// Widgets

const ImVec4 kGood(0.45f, 0.85f, 0.62f, 1.0f);
const ImVec4 kWarning(0.95f, 0.76f, 0.40f, 1.0f);

void Section(const char* name)
{
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::SeparatorText(name);
}

void Help(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void RestartHint(const char* key, bool restart)
{
    if (!restart)
        return;
    if (NeedsRestart(key))
    {
        ImGui::SameLine();
        ImGui::TextColored(kWarning, "Restart the game to apply");
    }
}

void BoolSetting(const char* key, const char* label, bool fallback, bool restart, const char* help)
{
    bool value = fallback;
    config_text::GetBool(gText, key, value);
    if (ImGui::Checkbox(label, &value))
        SaveBool(key, value);
    Help(help);
    RestartHint(key, restart);
}

struct Choice
{
    const char* value;
    const char* label;
};

void ChoiceSetting(const char* key, const char* label, const Choice* choices, int count,
    bool restart, const char* help)
{
    std::string current;
    config_text::GetString(gText, key, current);
    int selected = 0;
    for (int i = 0; i < count; ++i)
        if (_stricmp(current.c_str(), choices[i].value) == 0)
            selected = i;
    if (ImGui::BeginCombo(label, choices[selected].label))
    {
        for (int i = 0; i < count; ++i)
            if (ImGui::Selectable(choices[i].label, i == selected) && i != selected)
                SaveString(key, choices[i].value);
        ImGui::EndCombo();
    }
    Help(help);
    RestartHint(key, restart);
}

// Slider that only writes the file once the user releases it.
void UnsignedSlider(const char* key, const char* label, int minimum, int maximum,
    int fallback, const char* format, const char* help)
{
    ImGui::PushID(key);
    static std::string editingKey;
    static int editing = 0;
    const bool active = editingKey == key;
    uint32_t stored = static_cast<uint32_t>(fallback);
    config_text::GetUnsigned(gText, key, stored);
    int value = active ? editing : static_cast<int>(stored);
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    ImGui::SliderInt(label, &value, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemActive())
    {
        editingKey = key;
        editing = value;
    }
    else if (active)
        editingKey.clear();
    if (ImGui::IsItemDeactivatedAfterEdit())
        SaveUnsigned(key, static_cast<uint32_t>(value));
    Help(help);
    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// Panel

void DrawStatus()
{
    DLSSGTStatus status{};
    status.size = sizeof(status);
    status.version = DLSSGT_ADDON_API_VERSION;
    if (!gGetStatus || !gGetStatus(&status))
    {
        ImGui::TextColored(kWarning, "Engine status unavailable (engine and add-on versions differ?)");
        return;
    }
    if (status.bridgeReady)
        ImGui::TextColored(kGood, "Engine: ready (%s)", status.route);
    else
        ImGui::TextColored(kWarning, "Engine: waiting for the DLSS Frame Generation modules");

    if (!status.setOptionsSeen)
        ImGui::TextUnformatted("Frame Generation: waiting for the game");
    else if (!status.frameGenerationOn)
        ImGui::TextUnformatted("Frame Generation: off in the game");
    else
    {
        const bool fresh = status.stateSampleAgeMs <= 2500 && status.actualFramesPresented > 0;
        if (fresh)
            ImGui::Text("Frame Generation: on | requested %ux | actual %ux",
                status.appliedMultiplier, status.actualFramesPresented);
        else
            ImGui::Text("Frame Generation: on | requested %ux", status.appliedMultiplier);
        if (status.setOptionsResult == 39)
        {
            ImGui::SameLine();
            ImGui::TextColored(kWarning, "(VRAM low)");
        }
        else if (status.setOptionsResult != 0)
        {
            ImGui::SameLine();
            ImGui::TextColored(kWarning, "(Streamline error %d)", status.setOptionsResult);
        }
    }
    if (status.pending)
        ImGui::TextColored(kWarning, "Change pending: if it is not picked up, turn Frame Generation off and on in the game.");
    if (status.fpsSampleAgeMs <= 2000 && status.realFpsMilli && status.dlssFpsMilli)
        ImGui::Text("FPS: %.0f rendered | %.0f displayed",
            status.realFpsMilli / 1000.0, status.dlssFpsMilli / 1000.0);
    static const char* const kSources[] = {"none", "game", "UI assist"};
    static const char* const kUir[] = {"off", "on", "on (forced)"};
    ImGui::Text("UIR: %s | HUD-less: %s | UI alpha: %s",
        kUir[status.uiRecomposition < 3 ? status.uiRecomposition : 0],
        kSources[status.hudlessSource < 3 ? status.hudlessSource : 0],
        status.uiAlphaSource == 2 ? "injected (UI assist)" : kSources[status.uiAlphaSource < 3 ? status.uiAlphaSource : 0]);
    if (status.pacingValid)
        ImGui::Text("Frame pacing: %.2f ms avg | %.2f ms 99th pct | %.2f ms jitter",
            status.pacingAverageUs / 1000.0, status.pacingP99Us / 1000.0, status.pacingJitterUs / 1000.0);
    if (status.gpuValid)
        ImGui::Text("GPU: %u%% | %u C | %.0f W | %u / %u MHz | VRAM %.1f / %.1f GB",
            status.gpuUtilization, status.gpuTemperatureC, status.gpuPowerMilliwatts / 1000.0,
            status.gpuClockMhz, status.gpuMemoryClockMhz, status.vramUsedMb / 1024.0, status.vramTotalMb / 1024.0);
    if (status.debugLine[0])
        ImGui::TextDisabled("Debug: %s", status.debugLine);
    ImGui::TextDisabled("DLSS %s | DLSS-G %s | Streamline %s",
        status.dlssVersion[0] ? status.dlssVersion : "not loaded",
        status.dlssgVersion[0] ? status.dlssgVersion : "not loaded",
        status.streamlineVersion[0] ? status.streamlineVersion : "not loaded");
}

void DrawGpu()
{
    ImGui::Dummy(ImVec2(0, 4));
    static const Choice gpus[] = {
        {"auto", "Automatic"}, {"ada", "RTX 40 (Ada)"},
        {"ampere", "RTX 30 (Ampere)"}, {"turing", "RTX 20 (Turing)"},
    };
    ChoiceSetting("gpuArchitecture", "GPU architecture", gpus, 4, true,
        "Which kernel patches the engine applies. Leave on Automatic unless detection fails. "
        "Read at game start: restart the game.");
}

void DrawGeneration()
{
    Section("Frame generation");
    static const Choice modes[] = {
        {"fixed", "Fixed multiplier"},
        {"dynamic", "Dynamic (target FPS)"},
        {"game", "Game decides"},
    };
    ChoiceSetting("mode", "Mode", modes, 3, false,
        "Fixed: always use the multiplier below.\n"
        "Dynamic: DLSS-G picks the multiplier to reach the target FPS.\n"
        "Game decides: follow the game's own Frame Generation setting, or NVIDIA Profile Inspector.\n"
        "Applied live.");

    std::string mode = "game";
    config_text::GetString(gText, "mode", mode);
    if (_stricmp(mode.c_str(), "fixed") == 0)
    {
        UnsignedSlider("multiplier", "Multiplier", 2, 6, 4, "%dx",
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
        if (ImGui::Checkbox("Follow display refresh rate", &followDisplay))
            SaveUnsigned("dynamicTargetFrameRate", followDisplay ? 0 : lastCustomTarget);
        Help("Aim for the refresh rate of the monitor showing the game. Applied live.");
        if (!followDisplay)
            UnsignedSlider("dynamicTargetFrameRate", "Target FPS", 30, 500, 120, "%d FPS",
                "Frame rate Dynamic mode aims for. Applied live.");
        BoolSetting("dynamicExperimental56", "Allow 5x and 6x", false, false,
            "Let Dynamic mode go up to 6x. Needs plenty of VRAM. Applied live.");
    }
    else
    {
        ImGui::TextWrapped("The game (or NVIDIA Profile Inspector) chooses the multiplier and mode.");
    }
    BoolSetting("disableKeybinds", "Disable keyboard shortcuts", false, false,
        "Turns off the Ctrl+Alt shortcuts and lets the game or Profile Inspector control the multiplier "
        "(same as \"Game decides\", plus no overlay shortcuts). Applied live.");
}

void DrawDisplay()
{
    Section("Overlay");
    BoolSetting("showOverlay", "Show multiplier / FPS overlay", false, false,
        "Small in-game counter drawn by DLSSG-Transfusion (Ctrl+Alt+O). In Vulkan games this add-on draws it, "
        "with the game's frame rate times the multiplier. Applied live.");
    static const Choice corners[] = {
        {"top-left", "Top left"}, {"top-right", "Top right"},
        {"bottom-left", "Bottom left"}, {"bottom-right", "Bottom right"},
    };
    ChoiceSetting("overlayPosition", "Overlay position", corners, 4, false,
        "Screen corner of the overlay (Ctrl+Alt+P). Applied live.");
    ImGui::TextDisabled("Extra overlay lines:");
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
    BoolSetting("overlayShowGpu", "GPU load, temperature, power, clocks", false, false,
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
    Section("Keyboard shortcuts");
    static char buffers[kActionCount][128]{};
    static FILETIME loadedFrom{};
    static bool dirty = false, loaded = false;
    static char message[160] = "";
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
        ImGui::TextColored(kWarning, "Shortcuts are off (\"Disable keyboard shortcuts\" above).");
    ImGui::TextWrapped("Record a combination or type it (e.g. Ctrl+Alt+F5). Separate alternatives with commas; "
        "leave empty to disable an action.");

    bool captureStarted = false;
    if (ImGui::BeginTable("shortcuts", 3, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Record", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.5f);
        for (uint32_t action = 0; action < kActionCount; ++action)
        {
            ImGui::PushID(static_cast<int>(action));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Info(action).label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            ImGui::BeginDisabled(gListening >= 0);
            if (ImGui::InputText("##binding", buffers[action], sizeof(buffers[action])))
                dirty = true;
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            if (ImGui::Button(gListening == static_cast<int>(action) ? "Cancel" : "Record", ImVec2(-1, 0)))
            {
                if (gListening == static_cast<int>(action))
                    gListening = -1;
                else
                {
                    gListening = static_cast<int>(action);
                    captureStarted = true;
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (gListening >= 0)
    {
        ImGui::TextColored(kWarning, "Press the combination for \"%s\" (Escape cancels).", Info(gListening).label);
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

    if (ImGui::Button("Save shortcuts"))
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
    {
        ImGui::SameLine();
        ImGui::TextColored(kWarning, "Unsaved changes");
    }
    if (message[0])
        ImGui::TextWrapped("%s", message);
}

void DrawQuality()
{
    Section("Image quality");
    BoolSetting("blackwellTransfusion", "Blackwell kernel transfusion", true, true,
        "Uses the RTX 50 (sm_120) frame-generation kernels on this GPU. Required for 3x-6x quality. "
        "Read when DLSS-G loads: restart the game.");
    BoolSetting("qualityValidWarp", "Anti-tearing / anti-ghosting protection", true, true,
        "Candidate agreement firewall and thin-geometry protection (fences, foliage). "
        "Read when DLSS-G loads: restart the game.");
    static const Choice policies[] = {
        {"explained-warp", "Explained warp (default)"},
        {"transfusion", "Transfusion"},
    };
    ChoiceSetting("qualityPolicy", "Protection tuning", policies, 2, true,
        "Tuning of the protection above. Explained warp is the recommended default. "
        "Read when DLSS-G loads: restart the game.");
    BoolSetting("optimizedKernels", "Optimized kernels", true, true,
        "Faster, bit-exact frame-generation kernels. Read when DLSS-G loads: restart the game.");
}

void DrawDlssSuperResolution()
{
    Section("DLSS Super Resolution");
    DLSSGTStatus status{};
    status.size = sizeof(status);
    status.version = DLSSGT_ADDON_API_VERSION;
    if (!gGetStatus || !gGetStatus(&status))
        return;

    if (status.srObserved && status.srOutputWidth)
        ImGui::Text("Rendering %u x %u -> %u x %u (%.1f%%)", status.srInputWidth, status.srInputHeight,
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
        "driven live when it can be found (shown below).");
    std::string preset;
    config_text::GetString(gText, "dlssRenderScale", preset);
    if (_stricmp(preset.c_str(), "custom") == 0)
        UnsignedSlider("dlssCustomScale", "Custom scale", 50, 100, 67, "%d%%",
            "Render resolution in percent of the output, per axis (width and height). Applied live.");

    if (status.unrealState == 2)
        ImGui::TextColored(kGood, "Unreal Engine: live control of r.ScreenPercentage (now %.1f%%).",
            status.unrealScreenPercentageMilli / 1000.0);
    else if (status.unrealState == 0)
        ImGui::TextDisabled("Looking for Unreal Engine's r.ScreenPercentage...");

    if (status.srScale)
    {
        if (status.srVerified)
            ImGui::TextColored(kGood, "The game renders at the requested resolution.");
        else if (status.srObserved)
            ImGui::TextColored(kWarning, "Waiting for the game to render at the requested resolution. If it never "
                "changes, this game ignores the override: use its own settings.");
    }
}

void DrawUi()
{
    Section("HUD / UI");
    BoolSetting("autoUiRecomposition", "Automatic UI recomposition", true, false,
        "Turns UI recomposition on when the game provides HUD-less and UI buffers without asking for it. "
        "Turn off to follow the game's own choice, if the generated frames look wrong with it. Applied live.");
    BoolSetting("forceUiRecomposition", "Force UI recomposition", false, false,
        "Asks DLSS-G to recompose the HUD separately even when the game does not request it. "
        "Reduces HUD ghosting when the game provides the right buffers. Applied live; some games "
        "need Frame Generation turned off and on.");
    BoolSetting("uiAssist", "UI assist (D3D12)", false, false,
        "Captures the HUD-less scene and builds the UI layer when the game does not tag them. "
        "Applied live.");
}

void DrawCompatibility()
{
    Section("Compatibility");
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
    Section("Diagnostics");
    BoolSetting("logPerformance", "Log performance", false, false,
        "Writes FPS and frame times to DLSSG-Transfusion_perf.csv. Applied live.");
    BoolSetting("logMotionTracing", "Log motion tracing", false, false,
        "Very verbose motion-vector diagnostics for debugging only. Applied live.");
}

void DrawPanel(reshade::api::effect_runtime* runtime)
{
    ImGui::PushItemWidth(ImGui::GetFontSize() * 14);
    if (!ConnectEngine())
    {
        ImGui::TextWrapped("DLSSG-Transfusion engine not found in this game. Install DLSSG-Transfusion.dll "
            "(or the .asi) next to the game executable, then restart the game.");
        ImGui::PopItemWidth();
        return;
    }
    ReloadIfChanged();
    DrawStatus();
    if (!gLoaded)
    {
        ImGui::TextWrapped("Waiting for DLSSG-Transfusion.json...");
        ImGui::PopItemWidth();
        return;
    }
    const int restart = PendingRestartCount();
    if (restart)
        ImGui::TextColored(kWarning, "%d change(s) take effect after restarting the game.", restart);
    if (gMessage[0])
        ImGui::TextColored(kWarning, "%s", gMessage);

    DrawGpu();
    DrawGeneration();
    DrawDisplay();
    DrawQuality();
    DrawDlssSuperResolution();
    DrawUi();
    DrawCompatibility();
    DrawDiagnostics();
    DrawHotkeys(runtime);

    Section("Configuration file");
    char path[MAX_PATH * 4]{};
    WideCharToMultiByte(CP_UTF8, 0, gConfigPath.c_str(), -1, path, sizeof(path), nullptr, nullptr);
    ImGui::TextDisabled("%s", path);
    ImGui::TextDisabled("Settings are saved immediately; shortcuts when you press \"Save shortcuts\".");
    ImGui::PopItemWidth();
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
