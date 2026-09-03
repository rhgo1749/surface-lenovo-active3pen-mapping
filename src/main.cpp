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

constexpr wchar_t kWindowClass[] = L"SurfaceLenovoActive3PenMapping.Window";
constexpr wchar_t kDiagnosticWindowClass[] = L"SurfaceLenovoActive3PenMapping.DiagnosticWindow";
constexpr wchar_t kWindowTitle[] = L"Surface Pen Mapper";
constexpr wchar_t kMutexName[] = L"Local\\SurfaceLenovoActive3PenMapping";
constexpr wchar_t kRunValueName[] = L"SurfaceLenovoActive3PenMapping";
constexpr wchar_t kConfigKey[] = L"Software\\SurfaceLenovoActive3PenMapping";
constexpr wchar_t kActionValue[] = L"Action";
constexpr wchar_t kShortcutVkValue[] = L"ShortcutVk";
constexpr wchar_t kShortcutModifiersValue[] = L"ShortcutModifiers";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kShowSettingsMessage = WM_APP + 2;
constexpr UINT kTrayIconId = 1;

constexpr UINT kMenuSettings = 1001;
constexpr UINT kMenuExit = 1002;
constexpr UINT kActionComboId = 1101;
constexpr UINT kShortcutHotkeyId = 1102;
constexpr UINT kStartupCheckboxId = 1103;
constexpr UINT kSaveButtonId = 1104;
constexpr UINT kHideButtonId = 1105;

constexpr USAGE kDigitizerPage = 0x0D;
constexpr USAGE kExternalPenUsage = 0x01;
constexpr USAGE kIntegratedPenUsage = 0x02;
constexpr USAGE kInvertUsage = 0x3C;
constexpr USAGE kBarrelUsage = 0x44;
constexpr USAGE kEraserUsage = 0x45;
constexpr USAGE kSecondaryBarrelUsage = 0x5A;

enum class Action {
    Back,
    Forward,
    Middle,
    Shortcut,
    None,
};

struct MappingConfig {
    Action action = Action::Back;
    WORD shortcutVk = 'Z';
    BYTE shortcutModifiers = HOTKEYF_CONTROL;
};

struct Options {
    std::optional<Action> actionOverride;
    bool diagnose = false;
    bool background = false;
    bool showTray = true;
    bool startupEnable = false;
    bool startupDisable = false;
    bool help = false;
    bool valid = true;
    std::wstring error;
};

const wchar_t* ActionName(Action action) {
    switch (action) {
        case Action::Back: return L"back";
        case Action::Forward: return L"forward";
        case Action::Middle: return L"middle";
        case Action::Shortcut: return L"shortcut";
        case Action::None: return L"none";
    }
    return L"back";
}

const wchar_t* ActionDisplayName(Action action) {
    switch (action) {
        case Action::Back: return L"Back (Mouse 4)";
        case Action::Forward: return L"Forward (Mouse 5)";
        case Action::Middle: return L"Middle click";
        case Action::Shortcut: return L"Keyboard shortcut";
        case Action::None: return L"Disabled";
    }
    return L"Back (Mouse 4)";
}

std::optional<Action> ActionFromName(const std::wstring& value) {
    if (value == L"back") return Action::Back;
    if (value == L"forward") return Action::Forward;
    if (value == L"middle") return Action::Middle;
    if (value == L"shortcut") return Action::Shortcut;
    if (value == L"none") return Action::None;
    return std::nullopt;
}

const wchar_t* UsageName(USAGE usage) {
    switch (usage) {
        case 0x32: return L"InRange";
        case 0x3C: return L"Invert";
        case 0x42: return L"TipSwitch";
        case 0x43: return L"SecondaryTipSwitch";
        case 0x44: return L"Barrel";
        case 0x45: return L"Eraser";
        case 0x5A: return L"SecondaryBarrel";
        default: return L"Other";
    }
}

bool IsLowerButtonUsage(USAGE usage) {
    return usage == kInvertUsage ||
           usage == kEraserUsage ||
           usage == kSecondaryBarrelUsage;
}

int ActionToComboIndex(Action action) {
    switch (action) {
        case Action::Back: return 0;
        case Action::Forward: return 1;
        case Action::Middle: return 2;
        case Action::Shortcut: return 3;
        case Action::None: return 4;
    }
    return 0;
}

Action ComboIndexToAction(int index) {
    switch (index) {
        case 0: return Action::Back;
        case 1: return Action::Forward;
        case 2: return Action::Middle;
        case 3: return Action::Shortcut;
        case 4: return Action::None;
        default: return Action::Back;
    }
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
            options.showTray = false;
            options.background = true;
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
                options.error = L"Unknown or unsupported CLI action: " + value;
                break;
            }
            options.actionOverride = *action;
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
        L"  surface-pen-map.exe [--action=back|forward|middle|none] [--no-tray]\n"
        L"  surface-pen-map.exe --startup-enable\n"
        L"  surface-pen-map.exe --startup-disable\n\n"
        L"Normal launch opens the native settings window. Startup uses --background.\n"
        L"The settings UI supports Back, Forward, Middle click, Disabled, and a custom\n"
        L"keyboard shortcut (Ctrl/Alt/Shift + key).\n\n"
        L"Notes:\n"
        L"  --diagnose observes pen HID usages and does not emit mapped input.\n"
        L"  The mapper does not replace or suppress the Windows pen driver.\n");
}

std::wstring ExecutablePath() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
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
    if (RegQueryValueExW(
            key,
            name,
            nullptr,
            &type,
            reinterpret_cast<BYTE*>(buffer.data()),
            &bytes) != ERROR_SUCCESS) {
        return false;
    }
    value.assign(buffer.data());
    return true;
}

bool ReadRegistryDword(HKEY key, const wchar_t* name, DWORD& value) {
    DWORD type = 0;
    DWORD bytes = sizeof(value);
    return RegQueryValueExW(
               key,
               name,
               nullptr,
               &type,
               reinterpret_cast<BYTE*>(&value),
               &bytes) == ERROR_SUCCESS &&
           type == REG_DWORD && bytes == sizeof(value);
}

MappingConfig LoadConfig() {
    MappingConfig config;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kConfigKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return config;
    }

    std::wstring actionValue;
    if (ReadRegistryString(key, kActionValue, actionValue)) {
        if (const auto action = ActionFromName(actionValue)) {
            config.action = *action;
        }
    }

    DWORD vk = 0;
    DWORD modifiers = 0;
    if (ReadRegistryDword(key, kShortcutVkValue, vk) && vk > 0 && vk <= 0xFF) {
        config.shortcutVk = static_cast<WORD>(vk);
    }
    if (ReadRegistryDword(key, kShortcutModifiersValue, modifiers)) {
        config.shortcutModifiers = static_cast<BYTE>(modifiers & 0xFF);
    }

    RegCloseKey(key);
    return config;
}

bool SaveConfig(const MappingConfig& config, std::wstring& error) {
    HKEY key = nullptr;
    const LONG openResult = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kConfigKey,
        0,
        nullptr,
        0,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);
    if (openResult != ERROR_SUCCESS) {
        error = L"Could not open the mapper settings registry key. Error " + std::to_wstring(openResult);
        return false;
    }

    const std::wstring action = ActionName(config.action);
    LONG result = RegSetValueExW(
        key,
        kActionValue,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(action.c_str()),
        static_cast<DWORD>((action.size() + 1) * sizeof(wchar_t)));

    const DWORD vk = config.shortcutVk;
    const DWORD modifiers = config.shortcutModifiers;
    if (result == ERROR_SUCCESS) {
        result = RegSetValueExW(
            key,
            kShortcutVkValue,
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&vk),
            sizeof(vk));
    }
    if (result == ERROR_SUCCESS) {
        result = RegSetValueExW(
            key,
            kShortcutModifiersValue,
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&modifiers),
            sizeof(modifiers));
    }

    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        error = L"Could not save mapper settings. Error " + std::to_wstring(result);
        return false;
    }
    return true;
}

bool SetStartup(bool enabled, std::wstring& error) {
    HKEY key = nullptr;
    const LONG openResult = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        nullptr,
        0,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);
    if (openResult != ERROR_SUCCESS) {
        error = L"Could not open HKCU Run registry key. Error " + std::to_wstring(openResult);
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring path = ExecutablePath();
        if (path.empty()) {
            RegCloseKey(key);
            error = L"Could not determine executable path.";
            return false;
        }

        const std::wstring command = L"\"" + path + L"\" --background";
        result = RegSetValueExW(
            key,
            kRunValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kRunValueName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
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
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0,
            KEY_QUERY_VALUE,
            &key) != ERROR_SUCCESS) {
        return false;
    }

    const LONG result = RegQueryValueExW(key, kRunValueName, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

std::wstring ShortcutName(WORD vk, BYTE modifiers) {
    std::wstring result;
    if (modifiers & HOTKEYF_CONTROL) result += L"Ctrl + ";
    if (modifiers & HOTKEYF_ALT) result += L"Alt + ";
    if (modifiers & HOTKEYF_SHIFT) result += L"Shift + ";

    wchar_t keyName[64]{};
    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG keyInfo = static_cast<LONG>(scanCode << 16);
    if (modifiers & HOTKEYF_EXT) {
        keyInfo |= 1 << 24;
    }

    if (vk != 0 && GetKeyNameTextW(keyInfo, keyName, static_cast<int>(std::size(keyName))) > 0) {
        result += keyName;
    } else if (vk != 0) {
        wchar_t fallback[16]{};
        swprintf_s(fallback, L"VK 0x%02X", vk);
        result += fallback;
    } else {
        result += L"(not set)";
    }
    return result;
}

std::wstring MappingSummary(const MappingConfig& config) {
    if (config.action == Action::Shortcut) {
        return L"Keyboard: " + ShortcutName(config.shortcutVk, config.shortcutModifiers);
    }
    return ActionDisplayName(config.action);
}

struct DeviceState {
    std::wstring name;
    std::vector<BYTE> preparsed;
    HIDP_CAPS caps{};
    std::vector<HIDP_BUTTON_CAPS> buttonCaps;
    std::unordered_set<USAGE> previousActive;
    bool lowerPressed = false;
    bool announced = false;
};

class PenMapper {
public:
    explicit PenMapper(Options options) : options_(std::move(options)), config_(LoadConfig()) {
        if (options_.actionOverride) {
            config_.action = *options_.actionOverride;
        }
    }

    ~PenMapper() {
        if (uiFont_) DeleteObject(uiFont_);
        if (titleFont_) DeleteObject(titleFont_);
    }

    bool Initialize(HINSTANCE instance) {
        instance_ = instance;
        const wchar_t* className = options_.diagnose ? kDiagnosticWindowClass : kWindowClass;

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

        DWORD style = 0;
        int width = 0;
        int height = 0;
        if (!options_.diagnose) {
            style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
            const UINT dpi = GetDpiForSystem();
            width = MulDiv(560, static_cast<int>(dpi), 96);
            height = MulDiv(410, static_cast<int>(dpi), 96);
        }

        hwnd_ = CreateWindowExW(
            0,
            className,
            kWindowTitle,
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            width,
            height,
            nullptr,
            nullptr,
            instance_,
            this);
        if (!hwnd_) {
            ReportError(L"CreateWindowExW failed", GetLastError());
            return false;
        }

        if (!options_.diagnose) {
            CreateSettingsUi();
        }

        RAWINPUTDEVICE devices[2]{};
        const DWORD flags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
        devices[0] = {kDigitizerPage, kIntegratedPenUsage, flags, hwnd_};
        devices[1] = {kDigitizerPage, kExternalPenUsage, flags, hwnd_};

        if (!RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE))) {
            ReportError(L"RegisterRawInputDevices failed", GetLastError());
            return false;
        }

        if (options_.showTray && !options_.diagnose && !AddTrayIcon()) {
            ReportError(L"Could not add tray icon", GetLastError());
            return false;
        }

        if (options_.diagnose) {
            std::wprintf(L"surface-pen-map diagnostic mode\n");
            std::wprintf(L"Listening for Digitizer/Pen raw input (Usage Page 0x0D, Usage 0x01/0x02).\n");
            std::wprintf(L"Press each side button while the pen is in hover range. Close this console to stop.\n\n");
            EnumeratePenDevices();
        } else if (!options_.background) {
            ShowSettings();
        }

        return true;
    }

    int Run() {
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
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

        if (self) {
            return self->WindowProc(message, wParam, lParam);
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_INPUT:
                ProcessRawInput(reinterpret_cast<HRAWINPUT>(lParam));
                return DefWindowProcW(hwnd_, message, wParam, lParam);

            case WM_INPUT_DEVICE_CHANGE:
                if (wParam == GIDC_REMOVAL) {
                    deviceStates_.erase(reinterpret_cast<HANDLE>(lParam));
                } else if (wParam == GIDC_ARRIVAL && options_.diagnose) {
                    std::wprintf(L"[device arrival] %p\n", reinterpret_cast<void*>(lParam));
                }
                return 0;

            case WM_COMMAND:
                HandleCommand(LOWORD(wParam), HIWORD(wParam));
                return 0;

            case WM_CLOSE:
                if (!options_.diagnose && options_.showTray) {
                    ShowWindow(hwnd_, SW_HIDE);
                    return 0;
                }
                DestroyWindow(hwnd_);
                return 0;

            case kShowSettingsMessage:
                if (!options_.diagnose) {
                    ShowSettings();
                }
                return 0;

            case kTrayMessage:
                HandleTrayMessage(static_cast<UINT>(lParam));
                return 0;

            case WM_DESTROY:
                RemoveTrayIcon();
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    void HandleCommand(UINT id, UINT notificationCode) {
        if (id == kActionComboId && notificationCode == CBN_SELCHANGE) {
            UpdateShortcutEnabled();
            return;
        }
        if (id == kSaveButtonId && notificationCode == BN_CLICKED) {
            SaveSettingsFromUi();
            return;
        }
        if (id == kHideButtonId && notificationCode == BN_CLICKED) {
            if (options_.showTray) {
                ShowWindow(hwnd_, SW_HIDE);
            } else {
                DestroyWindow(hwnd_);
            }
            return;
        }
        if (id == kMenuSettings) {
            ShowSettings();
            return;
        }
        if (id == kMenuExit) {
            DestroyWindow(hwnd_);
        }
    }

    void HandleTrayMessage(UINT mouseMessage) {
        if (mouseMessage == WM_LBUTTONUP || mouseMessage == WM_LBUTTONDBLCLK) {
            ShowSettings();
            return;
        }
        if (mouseMessage == WM_RBUTTONUP || mouseMessage == WM_CONTEXTMENU) {
            ShowTrayMenu();
        }
    }

    int Scale(int value) const {
        return MulDiv(value, static_cast<int>(dpi_), 96);
    }

    HWND CreateControl(
        DWORD exStyle,
        const wchar_t* className,
        const wchar_t* text,
        DWORD style,
        int x,
        int y,
        int width,
        int height,
        UINT id,
        HFONT font = nullptr) {
        HWND control = CreateWindowExW(
            exStyle,
            className,
            text,
            WS_CHILD | WS_VISIBLE | style,
            Scale(x),
            Scale(y),
            Scale(width),
            Scale(height),
            hwnd_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            instance_,
            nullptr);
        if (control) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : uiFont_), TRUE);
        }
        return control;
    }

    void CreateSettingsUi() {
        dpi_ = GetDpiForWindow(hwnd_);
        uiFont_ = CreateFontW(
            -MulDiv(10, static_cast<int>(dpi_), 72),
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI");
        titleFont_ = CreateFontW(
            -MulDiv(18, static_cast<int>(dpi_), 72),
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI");

        CreateControl(0, L"STATIC", L"Pen button mapping", SS_LEFT, 24, 20, 500, 34, 0, titleFont_);
        CreateControl(
            0,
            L"STATIC",
            L"Surface Pro + Lenovo Active Pen 3 / MPP — lightweight native mapper",
            SS_LEFT,
            26,
            58,
            500,
            24,
            0);

        CreateControl(0, L"BUTTON", L" Lower barrel button ", BS_GROUPBOX, 20, 92, 510, 176, 0);
        CreateControl(
            0,
            L"STATIC",
            L"Surface Pro 12 reports this button as Invert (HID 0x3C).",
            SS_LEFT,
            40,
            122,
            470,
            22,
            0);
        CreateControl(0, L"STATIC", L"Action", SS_LEFT, 40, 154, 120, 22, 0);

        actionCombo_ = CreateControl(
            0,
            WC_COMBOBOXW,
            L"",
            CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
            160,
            150,
            330,
            180,
            kActionComboId);
        SendMessageW(actionCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Back (Mouse 4)"));
        SendMessageW(actionCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Forward (Mouse 5)"));
        SendMessageW(actionCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Middle click"));
        SendMessageW(actionCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Keyboard shortcut"));
        SendMessageW(actionCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Disabled"));

        CreateControl(0, L"STATIC", L"Shortcut", SS_LEFT, 40, 194, 120, 22, 0);
        shortcutHotkey_ = CreateControl(
            WS_EX_CLIENTEDGE,
            HOTKEY_CLASSW,
            L"",
            WS_TABSTOP,
            160,
            190,
            330,
            28,
            kShortcutHotkeyId);
        SendMessageW(
            shortcutHotkey_,
            HKM_SETRULES,
            HKCOMB_NONE | HKCOMB_S,
            MAKELPARAM(HOTKEYF_CONTROL, 0));

        CreateControl(
            0,
            L"STATIC",
            L"Custom shortcuts support Ctrl / Alt / Shift + key. Windows-key and secure shortcuts are not supported.",
            SS_LEFT,
            40,
            226,
            455,
            34,
            0);

        startupCheckbox_ = CreateControl(
            0,
            L"BUTTON",
            L"Start mapper when I sign in to Windows",
            BS_AUTOCHECKBOX | WS_TABSTOP,
            24,
            286,
            350,
            26,
            kStartupCheckboxId);

        statusLabel_ = CreateControl(
            0,
            L"STATIC",
            L"Mapper is active. Changes apply immediately after Save.",
            SS_LEFT,
            26,
            324,
            350,
            24,
            0);

        CreateControl(
            0,
            L"STATIC",
            L"Top barrel button stays under Windows control so its native right-click behavior is preserved.",
            SS_LEFT,
            26,
            350,
            350,
            36,
            0);

        CreateControl(
            0,
            L"BUTTON",
            L"Save",
            BS_DEFPUSHBUTTON | WS_TABSTOP,
            402,
            306,
            104,
            34,
            kSaveButtonId);
        CreateControl(
            0,
            L"BUTTON",
            L"Hide",
            BS_PUSHBUTTON | WS_TABSTOP,
            402,
            350,
            104,
            34,
            kHideButtonId);

        RefreshSettingsUi();
    }

    void RefreshSettingsUi() {
        if (!actionCombo_) return;

        SendMessageW(actionCombo_, CB_SETCURSEL, ActionToComboIndex(config_.action), 0);
        SendMessageW(
            shortcutHotkey_,
            HKM_SETHOTKEY,
            MAKEWORD(config_.shortcutVk, config_.shortcutModifiers),
            0);
        SendMessageW(
            startupCheckbox_,
            BM_SETCHECK,
            IsStartupEnabled() ? BST_CHECKED : BST_UNCHECKED,
            0);
        UpdateShortcutEnabled();
    }

    void UpdateShortcutEnabled() {
        if (!actionCombo_ || !shortcutHotkey_) return;
        const int selection = static_cast<int>(SendMessageW(actionCombo_, CB_GETCURSEL, 0, 0));
        EnableWindow(shortcutHotkey_, ComboIndexToAction(selection) == Action::Shortcut);
    }

    void SaveSettingsFromUi() {
        MappingConfig next = config_;
        const int selection = static_cast<int>(SendMessageW(actionCombo_, CB_GETCURSEL, 0, 0));
        next.action = ComboIndexToAction(selection);

        if (next.action == Action::Shortcut) {
            const LRESULT hotkey = SendMessageW(shortcutHotkey_, HKM_GETHOTKEY, 0, 0);
            next.shortcutVk = LOBYTE(LOWORD(hotkey));
            next.shortcutModifiers = HIBYTE(LOWORD(hotkey));
            if (next.shortcutVk == 0) {
                MessageBoxW(
                    hwnd_,
                    L"Choose a keyboard shortcut before saving.",
                    kWindowTitle,
                    MB_OK | MB_ICONWARNING);
                SetFocus(shortcutHotkey_);
                return;
            }
        }

        std::wstring error;
        if (!SaveConfig(next, error)) {
            MessageBoxW(hwnd_, error.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
            return;
        }

        const bool startupEnabled = SendMessageW(startupCheckbox_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!SetStartup(startupEnabled, error)) {
            MessageBoxW(hwnd_, error.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
            return;
        }

        config_ = next;
        UpdateTrayTooltip();
        const std::wstring status = L"Saved — lower button -> " + MappingSummary(config_);
        SetWindowTextW(statusLabel_, status.c_str());
    }

    void ShowSettings() {
        if (options_.diagnose || !hwnd_) return;
        RefreshSettingsUi();
        if (IsIconic(hwnd_)) {
            ShowWindow(hwnd_, SW_RESTORE);
        } else {
            ShowWindow(hwnd_, SW_SHOWNORMAL);
        }
        SetForegroundWindow(hwnd_);
    }

    DeviceState* GetDeviceState(HANDLE device) {
        auto existing = deviceStates_.find(device);
        if (existing != deviceStates_.end()) {
            return &existing->second;
        }

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
        if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, state.preparsed.data(), &requestedPreparsedSize) == static_cast<UINT>(-1)) {
            return nullptr;
        }

        auto* preparsed = reinterpret_cast<PHIDP_PREPARSED_DATA>(state.preparsed.data());
        if (HidP_GetCaps(preparsed, &state.caps) != HIDP_STATUS_SUCCESS) {
            return nullptr;
        }

        if (state.caps.NumberInputButtonCaps > 0) {
            USHORT capCount = state.caps.NumberInputButtonCaps;
            state.buttonCaps.resize(capCount);
            if (HidP_GetButtonCaps(HidP_Input, state.buttonCaps.data(), &capCount, preparsed) != HIDP_STATUS_SUCCESS) {
                state.buttonCaps.clear();
            } else {
                state.buttonCaps.resize(capCount);
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
        if (GetRawInputData(rawInputHandle, RID_INPUT, buffer.data(), &requested, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) {
            return;
        }

        const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
        if (raw->header.dwType != RIM_TYPEHID) {
            return;
        }

        DeviceState* state = GetDeviceState(raw->header.hDevice);
        if (!state || state->buttonCaps.empty()) {
            return;
        }

        if (options_.diagnose && !state->announced) {
            PrintDevice(raw->header.hDevice, *state);
            state->announced = true;
        }

        BYTE* reports = const_cast<BYTE*>(raw->data.hid.bRawData);
        for (DWORD i = 0; i < raw->data.hid.dwCount; ++i) {
            BYTE* report = reports + (i * raw->data.hid.dwSizeHid);
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

        for (const auto& [page, linkCollection] : pageLinks) {
            const ULONG maxUsages = HidP_MaxUsageListLength(HidP_Input, page, preparsed);
            if (maxUsages == 0) {
                continue;
            }

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

        if (!matchedReport) {
            return;
        }

        if (options_.diagnose && active != state.previousActive) {
            PrintActiveUsages(active);
        }

        const bool lowerPressed = std::any_of(
            active.begin(),
            active.end(),
            [](USAGE usage) { return IsLowerButtonUsage(usage); });

        if (!options_.diagnose && lowerPressed && !state.lowerPressed) {
            SendMappedAction();
        }

        state.lowerPressed = lowerPressed;
        state.previousActive = std::move(active);
    }

    static void AppendKeyInput(std::vector<INPUT>& inputs, WORD vk, bool keyUp, bool extended = false) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        if (keyUp) input.ki.dwFlags |= KEYEVENTF_KEYUP;
        if (extended) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        inputs.push_back(input);
    }

    void SendMappedAction() {
        switch (config_.action) {
            case Action::Back:
                SendMouseButton(MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP, XBUTTON1);
                break;
            case Action::Forward:
                SendMouseButton(MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP, XBUTTON2);
                break;
            case Action::Middle:
                SendMouseButton(MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP, 0);
                break;
            case Action::Shortcut:
                SendShortcut();
                break;
            case Action::None:
                break;
        }
    }

    void SendMouseButton(DWORD downFlag, DWORD upFlag, DWORD mouseData) {
        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = downFlag;
        inputs[0].mi.mouseData = mouseData;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = upFlag;
        inputs[1].mi.mouseData = mouseData;
        SendInput(2, inputs, sizeof(INPUT));
    }

    void SendShortcut() {
        if (config_.shortcutVk == 0) return;

        std::vector<INPUT> inputs;
        inputs.reserve(8);
        if (config_.shortcutModifiers & HOTKEYF_CONTROL) AppendKeyInput(inputs, VK_CONTROL, false);
        if (config_.shortcutModifiers & HOTKEYF_ALT) AppendKeyInput(inputs, VK_MENU, false);
        if (config_.shortcutModifiers & HOTKEYF_SHIFT) AppendKeyInput(inputs, VK_SHIFT, false);

        const bool extended = (config_.shortcutModifiers & HOTKEYF_EXT) != 0;
        AppendKeyInput(inputs, config_.shortcutVk, false, extended);
        AppendKeyInput(inputs, config_.shortcutVk, true, extended);

        if (config_.shortcutModifiers & HOTKEYF_SHIFT) AppendKeyInput(inputs, VK_SHIFT, true);
        if (config_.shortcutModifiers & HOTKEYF_ALT) AppendKeyInput(inputs, VK_MENU, true);
        if (config_.shortcutModifiers & HOTKEYF_CONTROL) AppendKeyInput(inputs, VK_CONTROL, true);

        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
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
        if (result == static_cast<UINT>(-1)) {
            std::wprintf(L"GetRawInputDeviceList failed: %lu\n", GetLastError());
            return;
        }

        bool found = false;
        for (UINT i = 0; i < result; ++i) {
            if (devices[i].dwType != RIM_TYPEHID) {
                continue;
            }

            RID_DEVICE_INFO info{};
            info.cbSize = sizeof(info);
            UINT infoSize = sizeof(info);
            if (GetRawInputDeviceInfoW(devices[i].hDevice, RIDI_DEVICEINFO, &info, &infoSize) == static_cast<UINT>(-1)) {
                continue;
            }

            if (info.hid.usUsagePage != kDigitizerPage ||
                (info.hid.usUsage != kIntegratedPenUsage && info.hid.usUsage != kExternalPenUsage)) {
                continue;
            }

            found = true;
            if (DeviceState* state = GetDeviceState(devices[i].hDevice)) {
                PrintDevice(devices[i].hDevice, *state);
                state->announced = true;
            }
        }

        if (!found) {
            std::wprintf(L"No integrated/external pen HID collection is currently visible. Put the pen in hover range and try again.\n");
        }
        std::wprintf(L"\n");
    }

    void PrintDevice(HANDLE handle, const DeviceState& state) const {
        std::wprintf(L"[pen device] %p\n", handle);
        std::wprintf(L"  path: %ls\n", state.name.empty() ? L"(unknown)" : state.name.c_str());
        std::wprintf(L"  input button caps:\n");

        for (const auto& cap : state.buttonCaps) {
            if (cap.UsagePage != kDigitizerPage) {
                continue;
            }

            if (cap.IsRange) {
                std::wprintf(
                    L"    page 0x%02X link %u usages 0x%02X..0x%02X\n",
                    cap.UsagePage,
                    cap.LinkCollection,
                    cap.Range.UsageMin,
                    cap.Range.UsageMax);
            } else {
                std::wprintf(
                    L"    page 0x%02X link %u usage 0x%02X (%ls)\n",
                    cap.UsagePage,
                    cap.LinkCollection,
                    cap.NotRange.Usage,
                    UsageName(cap.NotRange.Usage));
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
        UpdateTrayTooltipText();

        if (!Shell_NotifyIconW(NIM_ADD, &tray_)) {
            return false;
        }
        trayAdded_ = true;
        return true;
    }

    void UpdateTrayTooltipText() {
        const std::wstring tooltip = L"Pen mapper: lower -> " + MappingSummary(config_);
        wcsncpy_s(tray_.szTip, _countof(tray_.szTip), tooltip.c_str(), _TRUNCATE);
    }

    void UpdateTrayTooltip() {
        if (!trayAdded_) return;
        UpdateTrayTooltipText();
        tray_.uFlags = NIF_TIP;
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
        if (!menu) {
            return;
        }

        const std::wstring status = L"Lower button -> " + MappingSummary(config_);
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, status.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuSettings, L"Settings...");
        AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

        SetForegroundWindow(hwnd_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, point.x, point.y, 0, hwnd_, nullptr);
        DestroyMenu(menu);
        PostMessageW(hwnd_, WM_NULL, 0, 0);
    }

    void ReportError(const wchar_t* message, DWORD error) const {
        const std::wstring text = std::wstring(message) + L". Error " + std::to_wstring(error);
        if (options_.diagnose) {
            std::fwprintf(stderr, L"%ls\n", text.c_str());
        } else {
            MessageBoxW(nullptr, text.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
        }
    }

    Options options_;
    MappingConfig config_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND actionCombo_ = nullptr;
    HWND shortcutHotkey_ = nullptr;
    HWND startupCheckbox_ = nullptr;
    HWND statusLabel_ = nullptr;
    HFONT uiFont_ = nullptr;
    HFONT titleFont_ = nullptr;
    UINT dpi_ = 96;
    NOTIFYICONDATAW tray_{};
    bool trayAdded_ = false;
    std::unordered_map<HANDLE, DeviceState> deviceStates_;
};

bool ShowExistingMapperWindow() {
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (HWND existing = FindWindowW(kWindowClass, kWindowTitle)) {
            PostMessageW(existing, kShowSettingsMessage, 0, 0);
            return true;
        }
        Sleep(25);
    }
    return false;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&commonControls);

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
            std::fwprintf(stderr, L"Choose only one of --startup-enable or --startup-disable.\n");
            return 2;
        }

        if (options.actionOverride) {
            MappingConfig config = LoadConfig();
            config.action = *options.actionOverride;
            std::wstring configError;
            if (!SaveConfig(config, configError)) {
                std::fwprintf(stderr, L"%ls\n", configError.c_str());
                return 1;
            }
        }

        std::wstring error;
        const bool enabled = options.startupEnable;
        if (!SetStartup(enabled, error)) {
            std::fwprintf(stderr, L"%ls\n", error.c_str());
            return 1;
        }

        std::wprintf(enabled ? L"Startup enabled.\n" : L"Startup disabled.\n");
        return 0;
    }

    HANDLE mutex = nullptr;
    if (!options.diagnose) {
        mutex = CreateMutexW(nullptr, FALSE, kMutexName);
        if (!mutex) {
            return 1;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (!options.background) {
                ShowExistingMapperWindow();
            }
            CloseHandle(mutex);
            return 0;
        }
    }

    PenMapper mapper(std::move(options));
    if (!mapper.Initialize(instance)) {
        if (mutex) CloseHandle(mutex);
        return 1;
    }

    const int result = mapper.Run();
    if (mutex) CloseHandle(mutex);
    return result;
}
