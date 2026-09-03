#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kWindowClass[] = L"SurfaceLenovoActive3PenMapping.Window";
constexpr wchar_t kMutexName[] = L"Local\\SurfaceLenovoActive3PenMapping";
constexpr wchar_t kRunValueName[] = L"SurfaceLenovoActive3PenMapping";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
constexpr UINT kMenuExit = 1001;

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
    None,
};

struct Options {
    Action action = Action::Back;
    bool diagnose = false;
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
        case Action::None: return L"none";
    }
    return L"back";
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
            if (value == L"back") {
                options.action = Action::Back;
            } else if (value == L"forward") {
                options.action = Action::Forward;
            } else if (value == L"middle") {
                options.action = Action::Middle;
            } else if (value == L"none") {
                options.action = Action::None;
            } else {
                options.valid = false;
                options.error = L"Unknown action: " + value;
                break;
            }
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
        L"  surface-pen-map.exe [--action=back|forward|middle|none] [--no-tray]\n"
        L"  surface-pen-map.exe --diagnose\n"
        L"  surface-pen-map.exe --startup-enable [--action=...]\n"
        L"  surface-pen-map.exe --startup-disable\n\n"
        L"Default mapping:\n"
        L"  Invert / Eraser / SecondaryBarrel -> Mouse XBUTTON1 (Back)\n\n"
        L"Notes:\n"
        L"  --diagnose observes pen HID usages and does not emit mapped mouse input.\n"
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

bool SetStartup(bool enabled, Action action, std::wstring& error) {
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

        const std::wstring command = L"\"" + path + L"\" --action=" + ActionName(action);
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
    explicit PenMapper(Options options) : options_(std::move(options)) {}

    bool Initialize(HINSTANCE instance) {
        instance_ = instance;

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &PenMapper::StaticWindowProc;
        windowClass.hInstance = instance_;
        windowClass.lpszClassName = kWindowClass;

        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            ReportError(L"RegisterClassExW failed", GetLastError());
            return false;
        }

        hwnd_ = CreateWindowExW(
            0,
            kWindowClass,
            L"Surface Lenovo Active Pen Mapper",
            0,
            0,
            0,
            0,
            0,
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

        if (options_.showTray && !AddTrayIcon()) {
            ReportError(L"Could not add tray icon", GetLastError());
            return false;
        }

        if (options_.diagnose) {
            std::wprintf(L"surface-pen-map diagnostic mode\n");
            std::wprintf(L"Listening for Digitizer/Pen raw input (Usage Page 0x0D, Usage 0x01/0x02).\n");
            std::wprintf(L"Press each side button while the pen is in hover range. Close this console to stop.\n\n");
            EnumeratePenDevices();
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
                if (LOWORD(wParam) == kMenuExit) {
                    DestroyWindow(hwnd_);
                    return 0;
                }
                break;

            case kTrayMessage:
                if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
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

    void SendMappedAction() {
        if (options_.action == Action::None) {
            return;
        }

        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[1].type = INPUT_MOUSE;

        switch (options_.action) {
            case Action::Back:
                inputs[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
                inputs[0].mi.mouseData = XBUTTON1;
                inputs[1].mi.dwFlags = MOUSEEVENTF_XUP;
                inputs[1].mi.mouseData = XBUTTON1;
                break;

            case Action::Forward:
                inputs[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
                inputs[0].mi.mouseData = XBUTTON2;
                inputs[1].mi.dwFlags = MOUSEEVENTF_XUP;
                inputs[1].mi.mouseData = XBUTTON2;
                break;

            case Action::Middle:
                inputs[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                inputs[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
                break;

            case Action::None:
                return;
        }

        SendInput(2, inputs, sizeof(INPUT));
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

        const std::wstring tooltip = L"Pen mapper: lower button -> " + std::wstring(ActionName(options_.action));
        wcsncpy_s(tray_.szTip, _countof(tray_.szTip), tooltip.c_str(), _TRUNCATE);

        if (!Shell_NotifyIconW(NIM_ADD, &tray_)) {
            return false;
        }
        trayAdded_ = true;
        return true;
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

        const std::wstring status = L"Lower button -> " + std::wstring(ActionName(options_.action));
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, status.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
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
            MessageBoxW(nullptr, text.c_str(), L"surface-pen-map", MB_OK | MB_ICONERROR);
        }
    }

    Options options_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW tray_{};
    bool trayAdded_ = false;
    std::unordered_map<HANDLE, DeviceState> deviceStates_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
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

        std::wstring error;
        const bool enabled = options.startupEnable;
        if (!SetStartup(enabled, options.action, error)) {
            std::fwprintf(stderr, L"%ls\n", error.c_str());
            return 1;
        }

        std::wprintf(
            enabled ? L"Startup enabled (action=%ls).\n" : L"Startup disabled.\n",
            ActionName(options.action));
        return 0;
    }

    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (!mutex) {
        if (options.diagnose) {
            std::fwprintf(stderr, L"CreateMutexW failed: %lu\n", GetLastError());
        }
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (options.diagnose) {
            std::fwprintf(stderr, L"Another mapper instance is already running. Exit it from the tray first.\n");
        }
        CloseHandle(mutex);
        return 3;
    }

    PenMapper mapper(std::move(options));
    if (!mapper.Initialize(instance)) {
        CloseHandle(mutex);
        return 1;
    }

    const int result = mapper.Run();
    CloseHandle(mutex);
    return result;
}
