#include <windows.h>
#include <commctrl.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kMainWindowClass[] = L"SurfaceLenovoActive3PenMapping.Window";
constexpr wchar_t kDiagnosticWindowClass[] = L"SurfaceLenovoActive3PenMapping.DiagnosticWindow";
constexpr wchar_t kMutexName[] = L"Local\\SurfaceLenovoActive3PenMapping";
constexpr wchar_t kRunValueName[] = L"SurfaceLenovoActive3PenMapping";
constexpr wchar_t kConfigKey[] = L"Software\\SurfaceLenovoActive3PenMapping";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kShowSettingsMessage = WM_APP + 2;
constexpr UINT kTrayIconId = 1;
constexpr UINT kMenuSettings = 1001;
constexpr UINT kMenuExit = 1002;

constexpr UINT kUpperClickComboId = 1101;
constexpr UINT kUpperClickKeyId = 1102;
constexpr UINT kUpperTapComboId = 1103;
constexpr UINT kUpperTapKeyId = 1104;
constexpr UINT kLowerClickComboId = 1111;
constexpr UINT kLowerClickKeyId = 1112;
constexpr UINT kLowerTapComboId = 1113;
constexpr UINT kLowerTapKeyId = 1114;
constexpr UINT kStartupCheckboxId = 1120;
constexpr UINT kApplyButtonId = 1121;
constexpr UINT kHideButtonId = 1122;
constexpr UINT kLanguageComboId = 1130;

constexpr USAGE kDigitizerPage = 0x0D;
constexpr USAGE kExternalPenUsage = 0x01;
constexpr USAGE kIntegratedPenUsage = 0x02;
constexpr USAGE kInvertUsage = 0x3C;
constexpr USAGE kInRangeUsage = 0x32;
constexpr USAGE kTipUsage = 0x42;
constexpr USAGE kBarrelUsage = 0x44;
constexpr USAGE kEraserUsage = 0x45;
constexpr USAGE kSecondaryBarrelUsage = 0x5A;

constexpr BYTE kHotkeyWin = 0x10;

enum class Language : int {
    English = 0,
    Korean = 1,
};

enum class Action : int {
    None = 0,
    Back,
    Forward,
    Left,
    Right,
    Middle,
    Shortcut,
};

enum class Gesture {
    UpperClick,
    UpperTap,
    LowerClick,
    LowerTap,
};

enum class UiText {
    WindowTitle,
    HeaderTitle,
    HeaderSubtitle,
    Language,
    ActionColumn,
    KeyColumn,
    UpperGroup,
    LowerGroup,
    ButtonClick,
    ClickHint,
    HoldTap,
    TapHint,
    UpperNote,
    LowerNote,
    Startup,
    LastInputEmpty,
    Saved,
    Unsaved,
    SavedActive,
    Apply,
    HideTray,
    Settings,
    Exit,
    PressKey,
    SecureSequence,
};

struct Mapping {
    Action action = Action::None;
    WORD shortcutVk = 'Z';
    BYTE shortcutModifiers = HOTKEYF_CONTROL;
};

struct Config {
    Mapping upperClick{Action::None, 'Z', HOTKEYF_CONTROL};
    Mapping upperTap{Action::None, 'Z', HOTKEYF_CONTROL};
    Mapping lowerClick{Action::Back, 'Z', HOTKEYF_CONTROL};
    Mapping lowerTap{Action::None, 'Z', HOTKEYF_CONTROL};
    Language language = Language::English;
};

struct KeyCaptureValue {
    WORD vk = 'Z';
    BYTE modifiers = HOTKEYF_CONTROL;
};

struct Options {
    std::optional<Action> lowerClickOverride;
    bool diagnose = false;
    bool background = false;
    bool showTray = true;
    bool startupEnable = false;
    bool startupDisable = false;
    bool help = false;
    bool valid = true;
    std::wstring error;
};

Language DefaultLanguage() {
    const LANGID lang = GetUserDefaultUILanguage();
    return PRIMARYLANGID(lang) == LANG_KOREAN ? Language::Korean : Language::English;
}

const wchar_t* Tr(Language language, UiText text) {
    if (language == Language::Korean) {
        switch (text) {
            case UiText::WindowTitle: return L"Surface 펜 매퍼";
            case UiText::HeaderTitle: return L"펜 버튼 설정";
            case UiText::HeaderSubtitle: return L"두 개의 사이드 버튼 × 두 가지 제스처 = 네 개의 독립 동작";
            case UiText::Language: return L"언어";
            case UiText::ActionColumn: return L"동작";
            case UiText::KeyColumn: return L"키 / 단축키 (클릭 후 입력)";
            case UiText::UpperGroup: return L"상단 사이드 버튼  ·  Barrel 0x44";
            case UiText::LowerGroup: return L"하단 사이드 버튼  ·  Invert 0x3C";
            case UiText::ButtonClick: return L"버튼 클릭";
            case UiText::ClickHint: return L"화면을 터치하지 않고 눌렀다 놓기";
            case UiText::HoldTap: return L"누른 채 화면 탭";
            case UiText::TapHint: return L"버튼을 누른 채 펜촉으로 화면 터치";
            case UiText::UpperNote: return L"참고: Windows의 Barrel + 펜촉 우클릭은 그대로 발생합니다. 여기 설정한 동작은 추가로 실행됩니다.";
            case UiText::LowerNote: return L"검증 기기: Surface Pro 12 + Lenovo Active Pen 3. 필기 앱의 기본 지우개 동작도 남을 수 있습니다.";
            case UiText::Startup: return L"Windows 로그인 시 자동으로 실행";
            case UiText::LastInputEmpty: return L"마지막 입력: —";
            case UiText::Saved: return L"저장됨";
            case UiText::Unsaved: return L"저장되지 않은 변경사항";
            case UiText::SavedActive: return L"저장됨 · 즉시 적용됨";
            case UiText::Apply: return L"적용";
            case UiText::HideTray: return L"트레이로 숨기기";
            case UiText::Settings: return L"설정";
            case UiText::Exit: return L"종료";
            case UiText::PressKey: return L"여기를 클릭하고 원하는 키를 누르세요";
            case UiText::SecureSequence: return L"Ctrl+Alt+Delete는 Windows 보안 시퀀스라 매핑할 수 없습니다.";
        }
    }

    switch (text) {
        case UiText::WindowTitle: return L"Surface Pen Mapper";
        case UiText::HeaderTitle: return L"Map your pen";
        case UiText::HeaderSubtitle: return L"Two side buttons × two gestures = four independent actions.";
        case UiText::Language: return L"Language";
        case UiText::ActionColumn: return L"Action";
        case UiText::KeyColumn: return L"Key / shortcut (click then press)";
        case UiText::UpperGroup: return L"UPPER SIDE BUTTON  ·  Barrel 0x44";
        case UiText::LowerGroup: return L"LOWER SIDE BUTTON  ·  Invert 0x3C";
        case UiText::ButtonClick: return L"Button click";
        case UiText::ClickHint: return L"press + release without touching the screen";
        case UiText::HoldTap: return L"Hold + tap screen";
        case UiText::TapHint: return L"hold the button, then touch the pen tip";
        case UiText::UpperNote: return L"Note: Windows still performs its native Barrel + tip right-click. Custom actions are additional.";
        case UiText::LowerNote: return L"Validated: Surface Pro 12 + Lenovo Active Pen 3. Ink apps may also keep native eraser behavior.";
        case UiText::Startup: return L"Start mapper when I sign in to Windows";
        case UiText::LastInputEmpty: return L"Last input: —";
        case UiText::Saved: return L"Saved";
        case UiText::Unsaved: return L"Unsaved changes";
        case UiText::SavedActive: return L"Saved · changes are active now";
        case UiText::Apply: return L"Apply";
        case UiText::HideTray: return L"Hide to tray";
        case UiText::Settings: return L"Settings";
        case UiText::Exit: return L"Exit";
        case UiText::PressKey: return L"Click here, then press a key or shortcut";
        case UiText::SecureSequence: return L"Ctrl+Alt+Delete is a protected Windows secure sequence and cannot be mapped.";
    }
    return L"";
}

const wchar_t* ActionName(Action action) {
    switch (action) {
        case Action::None: return L"none";
        case Action::Back: return L"back";
        case Action::Forward: return L"forward";
        case Action::Left: return L"left";
        case Action::Right: return L"right";
        case Action::Middle: return L"middle";
        case Action::Shortcut: return L"shortcut";
    }
    return L"none";
}

const wchar_t* ActionDisplayName(Language language, Action action) {
    if (language == Language::Korean) {
        switch (action) {
            case Action::None: return L"추가 동작 없음 (Windows 기본 유지)";
            case Action::Back: return L"뒤로 (Mouse 4)";
            case Action::Forward: return L"앞으로 (Mouse 5)";
            case Action::Left: return L"왼쪽 클릭";
            case Action::Right: return L"오른쪽 클릭";
            case Action::Middle: return L"가운데 클릭";
            case Action::Shortcut: return L"키 / 단축키";
        }
    }

    switch (action) {
        case Action::None: return L"No extra action (keep Windows behavior)";
        case Action::Back: return L"Back (Mouse 4)";
        case Action::Forward: return L"Forward (Mouse 5)";
        case Action::Left: return L"Left click";
        case Action::Right: return L"Right click";
        case Action::Middle: return L"Middle click";
        case Action::Shortcut: return L"Key / shortcut";
    }
    return L"No extra action";
}

std::optional<Action> ActionFromName(const std::wstring& value) {
    if (value == L"none" || value == L"default") return Action::None;
    if (value == L"back") return Action::Back;
    if (value == L"forward") return Action::Forward;
    if (value == L"left") return Action::Left;
    if (value == L"right") return Action::Right;
    if (value == L"middle") return Action::Middle;
    if (value == L"shortcut") return Action::Shortcut;
    return std::nullopt;
}

const wchar_t* GestureName(Language language, Gesture gesture) {
    if (language == Language::Korean) {
        switch (gesture) {
            case Gesture::UpperClick: return L"상단 버튼 클릭";
            case Gesture::UpperTap: return L"상단 버튼 + 화면 탭";
            case Gesture::LowerClick: return L"하단 버튼 클릭";
            case Gesture::LowerTap: return L"하단 버튼 + 화면 탭";
        }
    }

    switch (gesture) {
        case Gesture::UpperClick: return L"Upper button click";
        case Gesture::UpperTap: return L"Upper button + pen tap";
        case Gesture::LowerClick: return L"Lower button click";
        case Gesture::LowerTap: return L"Lower button + pen tap";
    }
    return L"Pen gesture";
}

const wchar_t* UsageName(USAGE usage) {
    switch (usage) {
        case kInRangeUsage: return L"InRange";
        case kInvertUsage: return L"Invert";
        case kTipUsage: return L"TipSwitch";
        case kBarrelUsage: return L"Barrel";
        case kEraserUsage: return L"Eraser";
        case kSecondaryBarrelUsage: return L"SecondaryBarrel";
        default: return L"Other";
    }
}

bool IsLowerButtonUsage(USAGE usage) {
    return usage == kInvertUsage ||
           usage == kEraserUsage ||
           usage == kSecondaryBarrelUsage;
}

Options ParseOptions() {
    Options options;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        options.valid = false;
        options.error = L"CommandLineToArgvW failed.";
        return options;
    }

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--diagnose") {
            options.diagnose = true;
            options.background = true;
            options.showTray = false;
        } else if (arg == L"--background") {
            options.background = true;
        } else if (arg == L"--no-tray") {
            options.showTray = false;
        } else if (arg == L"--startup-enable") {
            options.startupEnable = true;
        } else if (arg == L"--startup-disable") {
            options.startupDisable = true;
        } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            options.help = true;
        } else if (arg.rfind(L"--action=", 0) == 0) {
            const std::wstring value = arg.substr(9);
            const auto action = ActionFromName(value);
            if (!action || *action == Action::Shortcut) {
                options.valid = false;
                options.error = L"Unsupported CLI action: " + value;
                break;
            }
            options.lowerClickOverride = *action;
        } else {
            options.valid = false;
            options.error = L"Unknown argument: " + arg;
            break;
        }
    }

    LocalFree(argv);
    return options;
}

void OpenConsole() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }
    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);
}

void PrintHelp() {
    std::wprintf(
        L"surface-pen-map - lightweight Windows pen button mapper\n\n"
        L"Usage:\n"
        L"  surface-pen-map.exe\n"
        L"  surface-pen-map.exe --background\n"
        L"  surface-pen-map.exe --diagnose\n"
        L"  surface-pen-map.exe --startup-enable\n"
        L"  surface-pen-map.exe --startup-disable\n"
        L"  surface-pen-map.exe --action=back|forward|left|right|middle|none\n\n"
        L"The UI exposes four independent gestures and supports Korean/English.\n"
        L"Key capture accepts standalone Enter/Esc/Tab/Space/Delete/arrows/F-keys and\n"
        L"Ctrl/Alt/Shift/Win combinations. Ctrl+Alt+Delete remains protected by Windows.\n");
}

std::wstring ExecutablePath() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    return std::wstring(buffer.data(), length);
}

bool ReadRegistryString(HKEY key, const wchar_t* name, std::wstring& value) {
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        type != REG_SZ || bytes < sizeof(wchar_t)) {
        return false;
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type,
            reinterpret_cast<BYTE*>(buffer.data()), &bytes) != ERROR_SUCCESS) {
        return false;
    }
    value.assign(buffer.data());
    return true;
}

bool ReadRegistryDword(HKEY key, const wchar_t* name, DWORD& value) {
    DWORD type = 0;
    DWORD bytes = sizeof(value);
    return RegQueryValueExW(key, name, nullptr, &type,
               reinterpret_cast<BYTE*>(&value), &bytes) == ERROR_SUCCESS &&
           type == REG_DWORD && bytes == sizeof(value);
}

LONG WriteRegistryString(HKEY key, const std::wstring& name, const std::wstring& value) {
    return RegSetValueExW(key, name.c_str(), 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

LONG WriteRegistryDword(HKEY key, const std::wstring& name, DWORD value) {
    return RegSetValueExW(key, name.c_str(), 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

void ReadMapping(HKEY key, const wchar_t* prefix, Mapping& mapping) {
    const std::wstring actionName = std::wstring(prefix) + L"Action";
    const std::wstring vkName = std::wstring(prefix) + L"ShortcutVk";
    const std::wstring modifiersName = std::wstring(prefix) + L"ShortcutModifiers";

    std::wstring actionValue;
    if (ReadRegistryString(key, actionName.c_str(), actionValue)) {
        if (const auto action = ActionFromName(actionValue)) mapping.action = *action;
    }

    DWORD value = 0;
    if (ReadRegistryDword(key, vkName.c_str(), value) && value > 0 && value <= 0xFF) {
        mapping.shortcutVk = static_cast<WORD>(value);
    }
    if (ReadRegistryDword(key, modifiersName.c_str(), value)) {
        mapping.shortcutModifiers = static_cast<BYTE>(value & 0xFF);
    }
}

Config LoadConfig() {
    Config config;
    config.language = DefaultLanguage();

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kConfigKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return config;
    }

    ReadMapping(key, L"UpperClick", config.upperClick);
    ReadMapping(key, L"UpperTap", config.upperTap);
    ReadMapping(key, L"LowerClick", config.lowerClick);
    ReadMapping(key, L"LowerTap", config.lowerTap);

    std::wstring languageValue;
    if (ReadRegistryString(key, L"Language", languageValue)) {
        if (languageValue == L"ko") config.language = Language::Korean;
        else if (languageValue == L"en") config.language = Language::English;
    }

    std::wstring lowerAction;
    if (!ReadRegistryString(key, L"LowerClickAction", lowerAction)) {
        std::wstring legacyAction;
        if (ReadRegistryString(key, L"Action", legacyAction)) {
            if (const auto action = ActionFromName(legacyAction)) config.lowerClick.action = *action;
        }
        DWORD value = 0;
        if (ReadRegistryDword(key, L"ShortcutVk", value) && value > 0 && value <= 0xFF) {
            config.lowerClick.shortcutVk = static_cast<WORD>(value);
        }
        if (ReadRegistryDword(key, L"ShortcutModifiers", value)) {
            config.lowerClick.shortcutModifiers = static_cast<BYTE>(value & 0xFF);
        }
    }

    RegCloseKey(key);
    return config;
}

LONG WriteMapping(HKEY key, const wchar_t* prefix, const Mapping& mapping) {
    const std::wstring base(prefix);
    LONG result = WriteRegistryString(key, base + L"Action", ActionName(mapping.action));
    if (result == ERROR_SUCCESS) {
        result = WriteRegistryDword(key, base + L"ShortcutVk", mapping.shortcutVk);
    }
    if (result == ERROR_SUCCESS) {
        result = WriteRegistryDword(key, base + L"ShortcutModifiers", mapping.shortcutModifiers);
    }
    return result;
}

bool SaveConfig(const Config& config, std::wstring& error) {
    HKEY key = nullptr;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER, kConfigKey, 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS) {
        error = L"Could not open mapper settings. Error " + std::to_wstring(result);
        return false;
    }

    result = WriteMapping(key, L"UpperClick", config.upperClick);
    if (result == ERROR_SUCCESS) result = WriteMapping(key, L"UpperTap", config.upperTap);
    if (result == ERROR_SUCCESS) result = WriteMapping(key, L"LowerClick", config.lowerClick);
    if (result == ERROR_SUCCESS) result = WriteMapping(key, L"LowerTap", config.lowerTap);
    if (result == ERROR_SUCCESS) {
        result = WriteRegistryString(key, L"Language",
            config.language == Language::Korean ? L"ko" : L"en");
    }

    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        error = L"Could not save mapper settings. Error " + std::to_wstring(result);
        return false;
    }
    return true;
}

bool SaveLanguage(Language language, std::wstring& error) {
    HKEY key = nullptr;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER, kConfigKey, 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS) {
        error = L"Could not open mapper settings. Error " + std::to_wstring(result);
        return false;
    }
    result = WriteRegistryString(key, L"Language",
        language == Language::Korean ? L"ko" : L"en");
    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        error = L"Could not save language setting. Error " + std::to_wstring(result);
        return false;
    }
    return true;
}

bool SetStartup(bool enabled, std::wstring& error) {
    HKEY key = nullptr;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS) {
        error = L"Could not open Windows startup settings. Error " + std::to_wstring(result);
        return false;
    }

    if (enabled) {
        const std::wstring path = ExecutablePath();
        if (path.empty()) {
            RegCloseKey(key);
            error = L"Could not determine executable path.";
            return false;
        }
        const std::wstring command = L"\"" + path + L"\" --background";
        result = RegSetValueExW(key, kRunValueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kRunValueName);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }

    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        error = L"Could not update startup setting. Error " + std::to_wstring(result);
        return false;
    }
    return true;
}

bool IsStartupEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
            KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    const LONG result = RegQueryValueExW(key, kRunValueName, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool IsModifierKey(WORD vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

BYTE CurrentModifiers() {
    BYTE result = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) result |= HOTKEYF_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) result |= HOTKEYF_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) result |= HOTKEYF_ALT;
    if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) result |= kHotkeyWin;
    return result;
}

std::wstring KeyDisplayName(WORD vk, BYTE modifiers, Language language) {
    if (!vk) return Tr(language, UiText::PressKey);

    const bool ext = (modifiers & HOTKEYF_EXT) != 0;
    if (vk == VK_RETURN) {
        if (ext) return language == Language::Korean ? L"숫자패드 Enter" : L"Numpad Enter";
        return L"Enter";
    }
    if (vk == VK_ESCAPE) return L"Esc";
    if (vk == VK_TAB) return L"Tab";
    if (vk == VK_SPACE) return language == Language::Korean ? L"스페이스" : L"Space";
    if (vk == VK_BACK) return language == Language::Korean ? L"백스페이스" : L"Backspace";
    if (vk == VK_DELETE) return L"Delete";
    if (vk == VK_INSERT) return L"Insert";
    if (vk == VK_HOME) return L"Home";
    if (vk == VK_END) return L"End";
    if (vk == VK_PRIOR) return L"Page Up";
    if (vk == VK_NEXT) return L"Page Down";
    if (vk == VK_LEFT) return language == Language::Korean ? L"← 왼쪽" : L"← Left";
    if (vk == VK_RIGHT) return language == Language::Korean ? L"오른쪽 →" : L"Right →";
    if (vk == VK_UP) return language == Language::Korean ? L"↑ 위" : L"↑ Up";
    if (vk == VK_DOWN) return language == Language::Korean ? L"↓ 아래" : L"↓ Down";
    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    }

    wchar_t keyName[64]{};
    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG keyInfo = static_cast<LONG>(scanCode << 16);
    if (ext) keyInfo |= 1 << 24;
    if (GetKeyNameTextW(keyInfo, keyName, 64) > 0) {
        return keyName;
    }

    wchar_t fallback[16]{};
    swprintf_s(fallback, L"VK 0x%02X", vk);
    return fallback;
}

std::wstring ShortcutName(const Mapping& mapping, Language language) {
    if (mapping.action != Action::Shortcut) {
        return ActionDisplayName(language, mapping.action);
    }

    std::wstring text;
    if (mapping.shortcutModifiers & HOTKEYF_CONTROL) text += L"Ctrl+";
    if (mapping.shortcutModifiers & HOTKEYF_ALT) text += L"Alt+";
    if (mapping.shortcutModifiers & HOTKEYF_SHIFT) text += L"Shift+";
    if (mapping.shortcutModifiers & kHotkeyWin) text += L"Win+";
    text += KeyDisplayName(mapping.shortcutVk, mapping.shortcutModifiers, language);
    return text;
}

struct DeviceState {
    std::wstring name;
    std::vector<BYTE> preparsed;
    HIDP_CAPS caps{};
    std::vector<HIDP_BUTTON_CAPS> buttonCaps;
    std::unordered_set<USAGE> previousActive;
    bool upperPressed = false;
    bool lowerPressed = false;
    bool tipPressed = false;
    bool upperUsedWithTip = false;
    bool lowerUsedWithTip = false;
    bool announced = false;
};

class PenMapper {
public:
    PenMapper(Options options, Config config)
        : options_(std::move(options)), config_(std::move(config)) {}

    ~PenMapper() {
        if (font_) DeleteObject(font_);
        if (titleFont_) DeleteObject(titleFont_);
    }

    bool Initialize(HINSTANCE instance) {
        instance_ = instance;
        const wchar_t* className = options_.diagnose ? kDiagnosticWindowClass : kMainWindowClass;

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &PenMapper::StaticWindowProc;
        windowClass.hInstance = instance_;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = className;

        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            ReportError(L"RegisterClassExW failed", GetLastError());
            return false;
        }

        const DWORD style = options_.diagnose
            ? 0
            : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

        hwnd_ = CreateWindowExW(
            options_.diagnose ? 0 : WS_EX_CONTROLPARENT,
            className,
            Tr(config_.language, UiText::WindowTitle),
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            options_.diagnose ? 0 : 870,
            options_.diagnose ? 0 : 680,
            nullptr,
            nullptr,
            instance_,
            this);
        if (!hwnd_) {
            ReportError(L"CreateWindowExW failed", GetLastError());
            return false;
        }

        RAWINPUTDEVICE devices[2]{};
        const DWORD flags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
        devices[0] = {kDigitizerPage, kIntegratedPenUsage, flags, hwnd_};
        devices[1] = {kDigitizerPage, kExternalPenUsage, flags, hwnd_};
        if (!RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE))) {
            ReportError(L"RegisterRawInputDevices failed", GetLastError());
            return false;
        }

        if (options_.diagnose) {
            std::wprintf(L"surface-pen-map diagnostic mode\n");
            std::wprintf(L"Validated on Surface Pro 12 + Lenovo Active Pen 3:\n");
            std::wprintf(L"  upper side button = 0x44 Barrel\n");
            std::wprintf(L"  lower side button = 0x3C Invert\n");
            std::wprintf(L"  pen tip contact   = 0x42 TipSwitch\n\n");
            EnumeratePenDevices();
            return true;
        }

        CreateUi();
        if (options_.showTray && !AddTrayIcon()) {
            ReportError(L"Could not add tray icon", GetLastError());
            return false;
        }
        if (!options_.background) ShowSettings();
        return true;
    }

    int Run() {
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (!options_.diagnose) {
                const HWND focus = GetFocus();
                if (!IsKeyCaptureControl(focus) && IsDialogMessageW(hwnd_, &message)) {
                    continue;
                }
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        PenMapper* self = reinterpret_cast<PenMapper*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<PenMapper*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        }
        if (self) return self->WindowProc(message, wParam, lParam);
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK KeyCaptureProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR,
        DWORD_PTR refData) {
        auto* self = reinterpret_cast<PenMapper*>(refData);
        switch (message) {
            case WM_GETDLGCODE:
                return DefSubclassProc(hwnd, message, wParam, lParam) |
                       DLGC_WANTALLKEYS | DLGC_WANTTAB | DLGC_WANTARROWS;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (self) self->CaptureKey(hwnd, static_cast<WORD>(wParam), lParam);
                return 0;

            case WM_KEYUP:
            case WM_SYSKEYUP:
                if (self && IsModifierKey(static_cast<WORD>(wParam))) {
                    self->UpdateKeyCaptureText(hwnd);
                }
                return 0;

            case WM_CHAR:
            case WM_SYSCHAR:
            case WM_PASTE:
            case WM_CUT:
            case WM_CLEAR:
            case WM_CONTEXTMENU:
                return 0;

            case WM_KILLFOCUS:
                if (self) self->UpdateKeyCaptureText(hwnd);
                break;

            case WM_NCDESTROY:
                RemoveWindowSubclass(hwnd, KeyCaptureProc, 1);
                break;
        }
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_INPUT:
                ProcessRawInput(reinterpret_cast<HRAWINPUT>(lParam));
                return DefWindowProcW(hwnd_, message, wParam, lParam);

            case WM_INPUT_DEVICE_CHANGE:
                if (wParam == GIDC_REMOVAL) {
                    deviceStates_.erase(reinterpret_cast<HANDLE>(lParam));
                } else if (options_.diagnose && wParam == GIDC_ARRIVAL) {
                    std::wprintf(L"[device arrival] %p\n", reinterpret_cast<void*>(lParam));
                }
                return 0;

            case WM_COMMAND:
                return HandleCommand(wParam);

            case WM_CLOSE:
                if (!options_.diagnose && options_.showTray) {
                    ShowWindow(hwnd_, SW_HIDE);
                    return 0;
                }
                DestroyWindow(hwnd_);
                return 0;

            case kShowSettingsMessage:
                ShowSettings();
                return 0;

            case kTrayMessage:
                if (lParam == WM_LBUTTONUP) {
                    ShowSettings();
                } else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                    ShowTrayMenu();
                }
                return 0;

            case WM_DESTROY:
                RemoveTrayIcon();
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    LRESULT HandleCommand(WPARAM wParam) {
        const UINT id = LOWORD(wParam);
        const UINT notify = HIWORD(wParam);
        const HWND source = reinterpret_cast<HWND>(GetMessageExtraInfo());
        (void)source;

        if (id == kMenuSettings) {
            ShowSettings();
            return 0;
        }
        if (id == kMenuExit) {
            DestroyWindow(hwnd_);
            return 0;
        }
        if (id == kApplyButtonId && notify == BN_CLICKED) {
            ApplySettings();
            return 0;
        }
        if (id == kHideButtonId && notify == BN_CLICKED) {
            ShowWindow(hwnd_, SW_HIDE);
            return 0;
        }
        if (id == kLanguageComboId && notify == CBN_SELCHANGE) {
            ChangeLanguage(SelectedLanguage());
            return 0;
        }

        const bool actionCombo =
            id == kUpperClickComboId || id == kUpperTapComboId ||
            id == kLowerClickComboId || id == kLowerTapComboId;

        if (actionCombo && notify == CBN_SELCHANGE) {
            UpdateKeyCaptureEnabled();
            if (HWND capture = CaptureForComboId(id)) {
                HWND combo = ComboForId(id);
                if (SelectedAction(combo) == Action::Shortcut) SetFocus(capture);
            }
            MarkDirty();
        } else if (id == kStartupCheckboxId && notify == BN_CLICKED) {
            MarkDirty();
        }
        return 0;
    }

    void CreateUi() {
        font_ = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        titleFont_ = CreateFontW(-26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        title_ = CreateStatic(L"", 28, 18, 420, 38, titleFont_);
        subtitle_ = CreateStatic(L"", 30, 56, 620, 24, font_);
        languageLabel_ = CreateStatic(L"", 638, 25, 70, 24, font_);
        languageCombo_ = CreateLanguageCombo(704, 20);

        actionHeader_ = CreateStatic(L"", 350, 78, 240, 22, font_);
        keyHeader_ = CreateStatic(L"", 628, 78, 205, 22, font_);

        upperGroup_ = CreateGroup(L"", 28, 100, 800, 190);
        upperClickLabel_ = CreateStatic(L"", 50, 132, 170, 24, font_);
        upperClickHint_ = CreateStatic(L"", 50, 155, 275, 22, font_);
        upperClickCombo_ = CreateActionCombo(kUpperClickComboId, 340, 130, config_.upperClick.action);
        upperClickKey_ = CreateKeyCapture(kUpperClickKeyId, 625, 130, config_.upperClick);

        upperTapLabel_ = CreateStatic(L"", 50, 194, 170, 24, font_);
        upperTapHint_ = CreateStatic(L"", 50, 217, 275, 22, font_);
        upperTapCombo_ = CreateActionCombo(kUpperTapComboId, 340, 192, config_.upperTap.action);
        upperTapKey_ = CreateKeyCapture(kUpperTapKeyId, 625, 192, config_.upperTap);
        upperNote_ = CreateStatic(L"", 50, 255, 750, 24, font_);

        lowerGroup_ = CreateGroup(L"", 28, 305, 800, 190);
        lowerClickLabel_ = CreateStatic(L"", 50, 337, 170, 24, font_);
        lowerClickHint_ = CreateStatic(L"", 50, 360, 275, 22, font_);
        lowerClickCombo_ = CreateActionCombo(kLowerClickComboId, 340, 335, config_.lowerClick.action);
        lowerClickKey_ = CreateKeyCapture(kLowerClickKeyId, 625, 335, config_.lowerClick);

        lowerTapLabel_ = CreateStatic(L"", 50, 399, 170, 24, font_);
        lowerTapHint_ = CreateStatic(L"", 50, 422, 275, 22, font_);
        lowerTapCombo_ = CreateActionCombo(kLowerTapComboId, 340, 397, config_.lowerTap.action);
        lowerTapKey_ = CreateKeyCapture(kLowerTapKeyId, 625, 397, config_.lowerTap);
        lowerNote_ = CreateStatic(L"", 50, 460, 750, 24, font_);

        startupCheckbox_ = CreateWindowExW(0, WC_BUTTONW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            32, 515, 430, 28, hwnd_, reinterpret_cast<HMENU>(kStartupCheckboxId), instance_, nullptr);
        SetControlFont(startupCheckbox_, font_);
        SendMessageW(startupCheckbox_, BM_SETCHECK,
            IsStartupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);

        lastInput_ = CreateStatic(L"", 32, 550, 520, 24, font_);
        saveStatus_ = CreateStatic(L"", 32, 582, 430, 24, font_);

        applyButton_ = CreateWindowExW(0, WC_BUTTONW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            610, 558, 96, 36, hwnd_, reinterpret_cast<HMENU>(kApplyButtonId), instance_, nullptr);
        hideButton_ = CreateWindowExW(0, WC_BUTTONW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            716, 558, 112, 36, hwnd_, reinterpret_cast<HMENU>(kHideButtonId), instance_, nullptr);
        SetControlFont(applyButton_, font_);
        SetControlFont(hideButton_, font_);

        UpdateKeyCaptureEnabled();
        ApplyLocalization();
    }

    HWND CreateStatic(const wchar_t* text, int x, int y, int width, int height, HFONT font) {
        HWND control = CreateWindowExW(0, WC_STATICW, text,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height, hwnd_, nullptr, instance_, nullptr);
        SetControlFont(control, font);
        return control;
    }

    HWND CreateGroup(const wchar_t* text, int x, int y, int width, int height) {
        HWND control = CreateWindowExW(0, WC_BUTTONW, text,
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            x, y, width, height, hwnd_, nullptr, instance_, nullptr);
        SetControlFont(control, font_);
        return control;
    }

    static void SetControlFont(HWND control, HFONT font) {
        if (control && font) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
    }

    HWND CreateLanguageCombo(int x, int y) {
        HWND combo = CreateWindowExW(0, WC_COMBOBOXW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            x, y, 124, 100, hwnd_, reinterpret_cast<HMENU>(kLanguageComboId), instance_, nullptr);
        SetControlFont(combo, font_);

        const LRESULT ko = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"한국어"));
        SendMessageW(combo, CB_SETITEMDATA, ko, static_cast<LPARAM>(Language::Korean));
        const LRESULT en = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
        SendMessageW(combo, CB_SETITEMDATA, en, static_cast<LPARAM>(Language::English));
        SelectLanguageInCombo(combo, config_.language);
        return combo;
    }

    void SelectLanguageInCombo(HWND combo, Language language) {
        const LRESULT count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
        for (LRESULT index = 0; index < count; ++index) {
            const auto value = static_cast<Language>(SendMessageW(combo, CB_GETITEMDATA, index, 0));
            if (value == language) {
                SendMessageW(combo, CB_SETCURSEL, index, 0);
                break;
            }
        }
    }

    Language SelectedLanguage() const {
        const LRESULT index = SendMessageW(languageCombo_, CB_GETCURSEL, 0, 0);
        if (index == CB_ERR) return config_.language;
        return static_cast<Language>(SendMessageW(languageCombo_, CB_GETITEMDATA, index, 0));
    }

    void AddActionToCombo(HWND combo, Action action) {
        const LRESULT index = SendMessageW(combo, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(ActionDisplayName(config_.language, action)));
        if (index >= 0) {
            SendMessageW(combo, CB_SETITEMDATA, index, static_cast<LPARAM>(action));
        }
    }

    void PopulateActionCombo(HWND combo, Action selected) {
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        AddActionToCombo(combo, Action::None);
        AddActionToCombo(combo, Action::Back);
        AddActionToCombo(combo, Action::Forward);
        AddActionToCombo(combo, Action::Left);
        AddActionToCombo(combo, Action::Right);
        AddActionToCombo(combo, Action::Middle);
        AddActionToCombo(combo, Action::Shortcut);

        const LRESULT count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
        for (LRESULT index = 0; index < count; ++index) {
            const auto action = static_cast<Action>(SendMessageW(combo, CB_GETITEMDATA, index, 0));
            if (action == selected) {
                SendMessageW(combo, CB_SETCURSEL, index, 0);
                return;
            }
        }
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
    }

    HWND CreateActionCombo(UINT id, int x, int y, Action selected) {
        HWND combo = CreateWindowExW(0, WC_COMBOBOXW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            x, y, 270, 220, hwnd_, reinterpret_cast<HMENU>(id), instance_, nullptr);
        SetControlFont(combo, font_);
        PopulateActionCombo(combo, selected);
        return combo;
    }

    HWND CreateKeyCapture(UINT id, int x, int y, const Mapping& mapping) {
        HWND control = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_CENTER,
            x, y, 203, 28, hwnd_, reinterpret_cast<HMENU>(id), instance_, nullptr);
        SetControlFont(control, font_);
        SetWindowSubclass(control, KeyCaptureProc, 1, reinterpret_cast<DWORD_PTR>(this));
        keyCaptures_[control] = KeyCaptureValue{mapping.shortcutVk, mapping.shortcutModifiers};
        UpdateKeyCaptureText(control);
        return control;
    }

    Action SelectedAction(HWND combo) const {
        if (!combo) return Action::None;
        const LRESULT index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
        if (index == CB_ERR) return Action::None;
        return static_cast<Action>(SendMessageW(combo, CB_GETITEMDATA, index, 0));
    }

    HWND ComboForId(UINT id) const {
        switch (id) {
            case kUpperClickComboId: return upperClickCombo_;
            case kUpperTapComboId: return upperTapCombo_;
            case kLowerClickComboId: return lowerClickCombo_;
            case kLowerTapComboId: return lowerTapCombo_;
        }
        return nullptr;
    }

    HWND CaptureForComboId(UINT id) const {
        switch (id) {
            case kUpperClickComboId: return upperClickKey_;
            case kUpperTapComboId: return upperTapKey_;
            case kLowerClickComboId: return lowerClickKey_;
            case kLowerTapComboId: return lowerTapKey_;
        }
        return nullptr;
    }

    bool IsKeyCaptureControl(HWND control) const {
        return control &&
            (control == upperClickKey_ || control == upperTapKey_ ||
             control == lowerClickKey_ || control == lowerTapKey_);
    }

    Mapping MappingFromControls(HWND combo, HWND keyCapture) const {
        Mapping mapping;
        mapping.action = SelectedAction(combo);
        const auto found = keyCaptures_.find(keyCapture);
        if (found != keyCaptures_.end()) {
            mapping.shortcutVk = found->second.vk;
            mapping.shortcutModifiers = found->second.modifiers;
        }
        return mapping;
    }

    void UpdateKeyCaptureEnabled() {
        if (!upperClickCombo_) return;
        EnableWindow(upperClickKey_, SelectedAction(upperClickCombo_) == Action::Shortcut);
        EnableWindow(upperTapKey_, SelectedAction(upperTapCombo_) == Action::Shortcut);
        EnableWindow(lowerClickKey_, SelectedAction(lowerClickCombo_) == Action::Shortcut);
        EnableWindow(lowerTapKey_, SelectedAction(lowerTapCombo_) == Action::Shortcut);
    }

    void CaptureKey(HWND control, WORD vk, LPARAM lParam) {
        if (!IsKeyCaptureControl(control)) return;

        if (IsModifierKey(vk)) {
            std::wstring preview;
            const BYTE modifiers = CurrentModifiers();
            if (modifiers & HOTKEYF_CONTROL) preview += L"Ctrl+";
            if (modifiers & HOTKEYF_ALT) preview += L"Alt+";
            if (modifiers & HOTKEYF_SHIFT) preview += L"Shift+";
            if (modifiers & kHotkeyWin) preview += L"Win+";
            preview += L"…";
            SetWindowTextW(control, preview.c_str());
            return;
        }

        BYTE modifiers = CurrentModifiers();
        if ((static_cast<ULONG_PTR>(lParam) & (static_cast<ULONG_PTR>(1) << 24)) != 0) {
            modifiers |= HOTKEYF_EXT;
        }

        keyCaptures_[control] = KeyCaptureValue{vk, modifiers};
        UpdateKeyCaptureText(control);
        MarkDirty();
    }

    void UpdateKeyCaptureText(HWND control) {
        const auto found = keyCaptures_.find(control);
        if (found == keyCaptures_.end()) return;

        Mapping mapping;
        mapping.action = Action::Shortcut;
        mapping.shortcutVk = found->second.vk;
        mapping.shortcutModifiers = found->second.modifiers;
        const std::wstring text = ShortcutName(mapping, config_.language);
        SetWindowTextW(control, text.c_str());
    }

    void RefreshCaptureTexts() {
        UpdateKeyCaptureText(upperClickKey_);
        UpdateKeyCaptureText(upperTapKey_);
        UpdateKeyCaptureText(lowerClickKey_);
        UpdateKeyCaptureText(lowerTapKey_);
    }

    void MarkDirty() {
        dirty_ = true;
        savedActive_ = false;
        UpdateSaveStatusText();
    }

    void UpdateSaveStatusText() {
        if (!saveStatus_) return;
        if (dirty_) SetWindowTextW(saveStatus_, Tr(config_.language, UiText::Unsaved));
        else if (savedActive_) SetWindowTextW(saveStatus_, Tr(config_.language, UiText::SavedActive));
        else SetWindowTextW(saveStatus_, Tr(config_.language, UiText::Saved));
    }

    bool ValidateShortcut(const Mapping& mapping, Gesture gesture) {
        if (mapping.action != Action::Shortcut) return true;
        if (!mapping.shortcutVk) {
            const std::wstring message =
                std::wstring(config_.language == Language::Korean ? L"키 또는 단축키를 지정하세요: " : L"Choose a key or shortcut for: ") +
                GestureName(config_.language, gesture);
            MessageBoxW(hwnd_, message.c_str(), Tr(config_.language, UiText::WindowTitle), MB_OK | MB_ICONWARNING);
            return false;
        }

        const bool ctrl = (mapping.shortcutModifiers & HOTKEYF_CONTROL) != 0;
        const bool alt = (mapping.shortcutModifiers & HOTKEYF_ALT) != 0;
        if (mapping.shortcutVk == VK_DELETE && ctrl && alt) {
            MessageBoxW(hwnd_, Tr(config_.language, UiText::SecureSequence),
                Tr(config_.language, UiText::WindowTitle), MB_OK | MB_ICONWARNING);
            return false;
        }
        return true;
    }

    void ApplySettings() {
        Config next;
        next.language = config_.language;
        next.upperClick = MappingFromControls(upperClickCombo_, upperClickKey_);
        next.upperTap = MappingFromControls(upperTapCombo_, upperTapKey_);
        next.lowerClick = MappingFromControls(lowerClickCombo_, lowerClickKey_);
        next.lowerTap = MappingFromControls(lowerTapCombo_, lowerTapKey_);

        if (!ValidateShortcut(next.upperClick, Gesture::UpperClick) ||
            !ValidateShortcut(next.upperTap, Gesture::UpperTap) ||
            !ValidateShortcut(next.lowerClick, Gesture::LowerClick) ||
            !ValidateShortcut(next.lowerTap, Gesture::LowerTap)) {
            return;
        }

        std::wstring error;
        if (!SaveConfig(next, error)) {
            MessageBoxW(hwnd_, error.c_str(), Tr(config_.language, UiText::WindowTitle), MB_OK | MB_ICONERROR);
            return;
        }

        const bool startup = SendMessageW(startupCheckbox_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!SetStartup(startup, error)) {
            MessageBoxW(hwnd_, error.c_str(), Tr(config_.language, UiText::WindowTitle), MB_OK | MB_ICONERROR);
            return;
        }

        config_ = next;
        dirty_ = false;
        savedActive_ = true;
        UpdateSaveStatusText();
        UpdateLastInputText();
        UpdateTrayTip();
    }

    void ChangeLanguage(Language language) {
        if (language == config_.language) return;
        config_.language = language;

        std::wstring error;
        if (!SaveLanguage(language, error)) {
            MessageBoxW(hwnd_, error.c_str(), Tr(config_.language, UiText::WindowTitle), MB_OK | MB_ICONERROR);
        }
        ApplyLocalization();
    }

    void RefreshActionCombo(HWND combo) {
        if (!combo) return;
        const Action selected = SelectedAction(combo);
        PopulateActionCombo(combo, selected);
    }

    void ApplyLocalization() {
        SetWindowTextW(hwnd_, Tr(config_.language, UiText::WindowTitle));
        SetWindowTextW(title_, Tr(config_.language, UiText::HeaderTitle));
        SetWindowTextW(subtitle_, Tr(config_.language, UiText::HeaderSubtitle));
        SetWindowTextW(languageLabel_, Tr(config_.language, UiText::Language));
        SetWindowTextW(actionHeader_, Tr(config_.language, UiText::ActionColumn));
        SetWindowTextW(keyHeader_, Tr(config_.language, UiText::KeyColumn));

        SetWindowTextW(upperGroup_, Tr(config_.language, UiText::UpperGroup));
        SetWindowTextW(lowerGroup_, Tr(config_.language, UiText::LowerGroup));

        SetWindowTextW(upperClickLabel_, Tr(config_.language, UiText::ButtonClick));
        SetWindowTextW(lowerClickLabel_, Tr(config_.language, UiText::ButtonClick));
        SetWindowTextW(upperClickHint_, Tr(config_.language, UiText::ClickHint));
        SetWindowTextW(lowerClickHint_, Tr(config_.language, UiText::ClickHint));
        SetWindowTextW(upperTapLabel_, Tr(config_.language, UiText::HoldTap));
        SetWindowTextW(lowerTapLabel_, Tr(config_.language, UiText::HoldTap));
        SetWindowTextW(upperTapHint_, Tr(config_.language, UiText::TapHint));
        SetWindowTextW(lowerTapHint_, Tr(config_.language, UiText::TapHint));

        SetWindowTextW(upperNote_, Tr(config_.language, UiText::UpperNote));
        SetWindowTextW(lowerNote_, Tr(config_.language, UiText::LowerNote));
        SetWindowTextW(startupCheckbox_, Tr(config_.language, UiText::Startup));
        SetWindowTextW(applyButton_, Tr(config_.language, UiText::Apply));
        SetWindowTextW(hideButton_, Tr(config_.language, UiText::HideTray));

        SelectLanguageInCombo(languageCombo_, config_.language);
        RefreshActionCombo(upperClickCombo_);
        RefreshActionCombo(upperTapCombo_);
        RefreshActionCombo(lowerClickCombo_);
        RefreshActionCombo(lowerTapCombo_);
        RefreshCaptureTexts();
        UpdateKeyCaptureEnabled();
        UpdateSaveStatusText();
        UpdateLastInputText();
        UpdateTrayTip();
    }

    void ShowSettings() {
        if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
        else ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
    }

    const Mapping& MappingForGesture(Gesture gesture) const {
        switch (gesture) {
            case Gesture::UpperClick: return config_.upperClick;
            case Gesture::UpperTap: return config_.upperTap;
            case Gesture::LowerClick: return config_.lowerClick;
            case Gesture::LowerTap: return config_.lowerTap;
        }
        return config_.lowerClick;
    }

    void UpdateLastInputText() {
        if (!lastInput_) return;
        if (!lastGesture_) {
            SetWindowTextW(lastInput_, Tr(config_.language, UiText::LastInputEmpty));
            return;
        }

        const Mapping& mapping = MappingForGesture(*lastGesture_);
        const std::wstring prefix = config_.language == Language::Korean ? L"마지막 입력: " : L"Last input: ";
        const std::wstring message = prefix + GestureName(config_.language, *lastGesture_) +
            L" → " + ShortcutName(mapping, config_.language);
        SetWindowTextW(lastInput_, message.c_str());
    }

    void TriggerGesture(Gesture gesture) {
        const Mapping& mapping = MappingForGesture(gesture);
        if (options_.diagnose) {
            std::wprintf(L"  gesture: %ls\n", GestureName(Language::English, gesture));
            std::fflush(stdout);
            return;
        }

        SendAction(mapping);
        lastGesture_ = gesture;
        UpdateLastInputText();
    }

    static void SendMouse(DWORD downFlag, DWORD upFlag, DWORD data = 0) {
        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = downFlag;
        inputs[0].mi.mouseData = data;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = upFlag;
        inputs[1].mi.mouseData = data;
        SendInput(2, inputs, sizeof(INPUT));
    }

    static void AddKeyInput(std::vector<INPUT>& inputs, WORD vk, DWORD flags = 0) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.dwFlags = flags;
        inputs.push_back(input);
    }

    static void SendShortcut(const Mapping& mapping) {
        if (!mapping.shortcutVk) return;
        std::vector<INPUT> inputs;
        inputs.reserve(10);

        if (mapping.shortcutModifiers & kHotkeyWin) AddKeyInput(inputs, VK_LWIN);
        if (mapping.shortcutModifiers & HOTKEYF_CONTROL) AddKeyInput(inputs, VK_CONTROL);
        if (mapping.shortcutModifiers & HOTKEYF_ALT) AddKeyInput(inputs, VK_MENU);
        if (mapping.shortcutModifiers & HOTKEYF_SHIFT) AddKeyInput(inputs, VK_SHIFT);

        const DWORD keyFlags = (mapping.shortcutModifiers & HOTKEYF_EXT) ? KEYEVENTF_EXTENDEDKEY : 0;
        AddKeyInput(inputs, mapping.shortcutVk, keyFlags);
        AddKeyInput(inputs, mapping.shortcutVk, keyFlags | KEYEVENTF_KEYUP);

        if (mapping.shortcutModifiers & HOTKEYF_SHIFT) AddKeyInput(inputs, VK_SHIFT, KEYEVENTF_KEYUP);
        if (mapping.shortcutModifiers & HOTKEYF_ALT) AddKeyInput(inputs, VK_MENU, KEYEVENTF_KEYUP);
        if (mapping.shortcutModifiers & HOTKEYF_CONTROL) AddKeyInput(inputs, VK_CONTROL, KEYEVENTF_KEYUP);
        if (mapping.shortcutModifiers & kHotkeyWin) AddKeyInput(inputs, VK_LWIN, KEYEVENTF_KEYUP);

        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }

    static void SendAction(const Mapping& mapping) {
        switch (mapping.action) {
            case Action::None:
                return;
            case Action::Back:
                SendMouse(MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP, XBUTTON1);
                return;
            case Action::Forward:
                SendMouse(MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP, XBUTTON2);
                return;
            case Action::Left:
                SendMouse(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP);
                return;
            case Action::Right:
                SendMouse(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP);
                return;
            case Action::Middle:
                SendMouse(MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP);
                return;
            case Action::Shortcut:
                SendShortcut(mapping);
                return;
        }
    }

    DeviceState* GetDeviceState(HANDLE device) {
        auto existing = deviceStates_.find(device);
        if (existing != deviceStates_.end()) return &existing->second;

        DeviceState state;

        UINT nameLength = 0;
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &nameLength) != static_cast<UINT>(-1) && nameLength > 0) {
            std::vector<wchar_t> nameBuffer(nameLength + 1, L'\0');
            UINT requested = nameLength;
            if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nameBuffer.data(), &requested) != static_cast<UINT>(-1)) {
                state.name.assign(nameBuffer.data());
            }
        }

        UINT preparsedSize = 0;
        if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, nullptr, &preparsedSize) == static_cast<UINT>(-1) || preparsedSize == 0) {
            return nullptr;
        }
        state.preparsed.resize(preparsedSize);
        UINT requestedPreparsedSize = preparsedSize;
        if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA,
                state.preparsed.data(), &requestedPreparsedSize) == static_cast<UINT>(-1)) {
            return nullptr;
        }

        auto* preparsed = reinterpret_cast<PHIDP_PREPARSED_DATA>(state.preparsed.data());
        if (HidP_GetCaps(preparsed, &state.caps) != HIDP_STATUS_SUCCESS) return nullptr;

        if (state.caps.NumberInputButtonCaps > 0) {
            USHORT count = state.caps.NumberInputButtonCaps;
            state.buttonCaps.resize(count);
            if (HidP_GetButtonCaps(HidP_Input, state.buttonCaps.data(), &count, preparsed) != HIDP_STATUS_SUCCESS) {
                state.buttonCaps.clear();
            } else {
                state.buttonCaps.resize(count);
            }
        }

        auto [inserted, ok] = deviceStates_.emplace(device, std::move(state));
        (void)ok;
        return &inserted->second;
    }

    void ProcessRawInput(HRAWINPUT rawInputHandle) {
        UINT size = 0;
        if (GetRawInputData(rawInputHandle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) {
            return;
        }

        std::vector<BYTE> buffer(size);
        UINT requested = size;
        if (GetRawInputData(rawInputHandle, RID_INPUT, buffer.data(), &requested,
                sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) {
            return;
        }

        const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
        if (raw->header.dwType != RIM_TYPEHID) return;

        DeviceState* state = GetDeviceState(raw->header.hDevice);
        if (!state || state->buttonCaps.empty()) return;

        if (options_.diagnose && !state->announced) {
            PrintDevice(raw->header.hDevice, *state);
            state->announced = true;
        }

        BYTE* reports = const_cast<BYTE*>(raw->data.hid.bRawData);
        for (DWORD index = 0; index < raw->data.hid.dwCount; ++index) {
            BYTE* report = reports + (index * raw->data.hid.dwSizeHid);
            ProcessHidReport(*state, report, raw->data.hid.dwSizeHid);
        }
    }

    void ProcessHidReport(DeviceState& state, BYTE* report, ULONG reportLength) {
        auto* preparsed = reinterpret_cast<PHIDP_PREPARSED_DATA>(state.preparsed.data());
        std::set<std::pair<USAGE, USHORT>> pageLinks;
        for (const auto& cap : state.buttonCaps) {
            if (cap.UsagePage == kDigitizerPage) {
                pageLinks.emplace(cap.UsagePage, cap.LinkCollection);
            }
        }

        std::unordered_set<USAGE> active;
        bool matchedReport = false;
        for (const auto& pageLink : pageLinks) {
            const USAGE page = pageLink.first;
            const USHORT linkCollection = pageLink.second;
            const ULONG maxUsages = HidP_MaxUsageListLength(HidP_Input, page, preparsed);
            if (maxUsages == 0) continue;

            std::vector<USAGE> usages(maxUsages);
            ULONG usageCount = maxUsages;
            const NTSTATUS status = HidP_GetUsages(
                HidP_Input,
                page,
                linkCollection,
                usages.data(),
                &usageCount,
                preparsed,
                reinterpret_cast<PCHAR>(report),
                reportLength);

            if (status == HIDP_STATUS_SUCCESS) {
                matchedReport = true;
                for (ULONG index = 0; index < usageCount; ++index) {
                    active.insert(usages[index]);
                }
            }
        }
        if (!matchedReport) return;

        if (options_.diagnose && active != state.previousActive) {
            PrintActiveUsages(active);
        }

        const bool upperNow = active.find(kBarrelUsage) != active.end();
        const bool lowerNow = std::any_of(active.begin(), active.end(),
            [](USAGE usage) { return IsLowerButtonUsage(usage); });
        const bool tipNow = active.find(kTipUsage) != active.end();

        if (upperNow && !state.upperPressed) state.upperUsedWithTip = false;
        if (lowerNow && !state.lowerPressed) state.lowerUsedWithTip = false;

        const bool tipDown = tipNow && !state.tipPressed;
        if (tipDown) {
            if (upperNow) {
                state.upperUsedWithTip = true;
                TriggerGesture(Gesture::UpperTap);
            }
            if (lowerNow) {
                state.lowerUsedWithTip = true;
                TriggerGesture(Gesture::LowerTap);
            }
        }

        if (!upperNow && state.upperPressed && !state.upperUsedWithTip) {
            TriggerGesture(Gesture::UpperClick);
        }
        if (!lowerNow && state.lowerPressed && !state.lowerUsedWithTip) {
            TriggerGesture(Gesture::LowerClick);
        }

        state.upperPressed = upperNow;
        state.lowerPressed = lowerNow;
        state.tipPressed = tipNow;
        state.previousActive = std::move(active);
    }

    void EnumeratePenDevices() {
        UINT count = 0;
        if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0) {
            std::wprintf(L"No raw-input devices enumerated.\n");
            return;
        }

        std::vector<RAWINPUTDEVICELIST> devices(count);
        UINT requested = count;
        const UINT result = GetRawInputDeviceList(devices.data(), &requested, sizeof(RAWINPUTDEVICELIST));
        if (result == static_cast<UINT>(-1)) return;

        bool found = false;
        for (UINT index = 0; index < result; ++index) {
            if (devices[index].dwType != RIM_TYPEHID) continue;

            RID_DEVICE_INFO info{};
            info.cbSize = sizeof(info);
            UINT infoSize = sizeof(info);
            if (GetRawInputDeviceInfoW(devices[index].hDevice, RIDI_DEVICEINFO,
                    &info, &infoSize) == static_cast<UINT>(-1)) {
                continue;
            }
            if (info.hid.usUsagePage != kDigitizerPage ||
                (info.hid.usUsage != kIntegratedPenUsage && info.hid.usUsage != kExternalPenUsage)) {
                continue;
            }

            found = true;
            if (DeviceState* state = GetDeviceState(devices[index].hDevice)) {
                PrintDevice(devices[index].hDevice, *state);
                state->announced = true;
            }
        }

        if (!found) {
            std::wprintf(L"No pen HID collection is currently visible. Put the pen in hover range and try again.\n");
        }
        std::wprintf(L"\n");
    }

    void PrintDevice(HANDLE handle, const DeviceState& state) const {
        std::wprintf(L"[pen device] %p\n", handle);
        std::wprintf(L"  path: %ls\n", state.name.empty() ? L"(unknown)" : state.name.c_str());
        std::wprintf(L"  input button caps:\n");
        for (const auto& cap : state.buttonCaps) {
            if (cap.UsagePage != kDigitizerPage) continue;
            if (cap.IsRange) {
                std::wprintf(L"    page 0x%02X link %u usages 0x%02X..0x%02X\n",
                    cap.UsagePage, cap.LinkCollection,
                    cap.Range.UsageMin, cap.Range.UsageMax);
            } else {
                std::wprintf(L"    page 0x%02X link %u usage 0x%02X (%ls)\n",
                    cap.UsagePage, cap.LinkCollection,
                    cap.NotRange.Usage, UsageName(cap.NotRange.Usage));
            }
        }
    }

    void PrintActiveUsages(const std::unordered_set<USAGE>& active) const {
        std::vector<USAGE> sorted(active.begin(), active.end());
        std::sort(sorted.begin(), sorted.end());
        std::wprintf(L"  active:");
        if (sorted.empty()) {
            std::wprintf(L" (none)");
        } else {
            for (const USAGE usage : sorted) {
                std::wprintf(L" 0x%02X(%ls)", usage, UsageName(usage));
            }
        }
        std::wprintf(L"\n");
        std::fflush(stdout);
    }

    bool AddTrayIcon() {
        tray_ = {};
        tray_.cbSize = sizeof(tray_);
        tray_.hWnd = hwnd_;
        tray_.uID = kTrayIconId;
        tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        tray_.uCallbackMessage = kTrayMessage;
        tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        if (!Shell_NotifyIconW(NIM_ADD, &tray_)) return false;
        trayAdded_ = true;
        UpdateTrayTip();
        return true;
    }

    void UpdateTrayTip() {
        if (!trayAdded_) return;
        tray_.uFlags = NIF_TIP;
        const std::wstring tooltip = config_.language == Language::Korean
            ? L"Surface 펜 매퍼 · 4개 제스처"
            : L"Surface Pen Mapper · four gesture mappings";
        wcsncpy_s(tray_.szTip, _countof(tray_.szTip), tooltip.c_str(), _TRUNCATE);
        Shell_NotifyIconW(NIM_MODIFY, &tray_);
        tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    }

    void RemoveTrayIcon() {
        if (trayAdded_) {
            Shell_NotifyIconW(NIM_DELETE, &tray_);
            trayAdded_ = false;
        }
    }

    void ShowTrayMenu() {
        POINT point{};
        GetCursorPos(&point);
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        AppendMenuW(menu, MF_STRING, kMenuSettings, Tr(config_.language, UiText::Settings));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuExit, Tr(config_.language, UiText::Exit));

        SetForegroundWindow(hwnd_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
            point.x, point.y, 0, hwnd_, nullptr);
        DestroyMenu(menu);
        PostMessageW(hwnd_, WM_NULL, 0, 0);
    }

    void ReportError(const wchar_t* message, DWORD error) const {
        const std::wstring text = std::wstring(message) + L". Error " + std::to_wstring(error);
        if (options_.diagnose) std::fwprintf(stderr, L"%ls\n", text.c_str());
        else MessageBoxW(nullptr, text.c_str(), Tr(config_.language, UiText::WindowTitle), MB_OK | MB_ICONERROR);
    }

    Options options_;
    Config config_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    HFONT titleFont_ = nullptr;

    HWND title_ = nullptr;
    HWND subtitle_ = nullptr;
    HWND languageLabel_ = nullptr;
    HWND languageCombo_ = nullptr;
    HWND actionHeader_ = nullptr;
    HWND keyHeader_ = nullptr;

    HWND upperGroup_ = nullptr;
    HWND upperClickLabel_ = nullptr;
    HWND upperClickHint_ = nullptr;
    HWND upperTapLabel_ = nullptr;
    HWND upperTapHint_ = nullptr;
    HWND upperNote_ = nullptr;

    HWND lowerGroup_ = nullptr;
    HWND lowerClickLabel_ = nullptr;
    HWND lowerClickHint_ = nullptr;
    HWND lowerTapLabel_ = nullptr;
    HWND lowerTapHint_ = nullptr;
    HWND lowerNote_ = nullptr;

    HWND upperClickCombo_ = nullptr;
    HWND upperClickKey_ = nullptr;
    HWND upperTapCombo_ = nullptr;
    HWND upperTapKey_ = nullptr;
    HWND lowerClickCombo_ = nullptr;
    HWND lowerClickKey_ = nullptr;
    HWND lowerTapCombo_ = nullptr;
    HWND lowerTapKey_ = nullptr;
    HWND startupCheckbox_ = nullptr;
    HWND lastInput_ = nullptr;
    HWND saveStatus_ = nullptr;
    HWND applyButton_ = nullptr;
    HWND hideButton_ = nullptr;

    std::unordered_map<HWND, KeyCaptureValue> keyCaptures_;
    std::optional<Gesture> lastGesture_;
    bool dirty_ = false;
    bool savedActive_ = false;

    NOTIFYICONDATAW tray_{};
    bool trayAdded_ = false;
    std::unordered_map<HANDLE, DeviceState> deviceStates_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&controls);

    Options options = ParseOptions();
    if (options.help || !options.valid || options.diagnose || options.startupEnable || options.startupDisable) {
        OpenConsole();
    }

    if (!options.valid) {
        std::fwprintf(stderr, L"%ls\n\n", options.error.c_str());
        PrintHelp();
        return 2;
    }
    if (options.help) {
        PrintHelp();
        return 0;
    }

    if (options.startupEnable || options.startupDisable) {
        if (options.startupEnable && options.startupDisable) {
            std::fwprintf(stderr, L"Choose only one startup option.\n");
            return 2;
        }
        std::wstring error;
        if (!SetStartup(options.startupEnable, error)) {
            std::fwprintf(stderr, L"%ls\n", error.c_str());
            return 1;
        }
        std::wprintf(options.startupEnable ? L"Startup enabled.\n" : L"Startup disabled.\n");
        return 0;
    }

    Config config = LoadConfig();
    if (options.lowerClickOverride) config.lowerClick.action = *options.lowerClickOverride;

    HANDLE mutex = nullptr;
    if (!options.diagnose) {
        mutex = CreateMutexW(nullptr, FALSE, kMutexName);
        if (!mutex) return 1;
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (HWND existing = FindWindowW(kMainWindowClass, nullptr)) {
                PostMessageW(existing, kShowSettingsMessage, 0, 0);
            }
            CloseHandle(mutex);
            return 0;
        }
    }

    PenMapper mapper(std::move(options), std::move(config));
    if (!mapper.Initialize(instance)) {
        if (mutex) CloseHandle(mutex);
        return 1;
    }

    const int result = mapper.Run();
    if (mutex) CloseHandle(mutex);
    return result;
}
