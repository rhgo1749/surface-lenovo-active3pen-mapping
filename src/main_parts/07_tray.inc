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

        if (!found) std::wprintf(L"No pen HID collection is visible. Put the pen in hover range and try again.\n");
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
                    cap.UsagePage, cap.LinkCollection, cap.Range.UsageMin, cap.Range.UsageMax);
            } else {
                std::wprintf(L"    page 0x%02X link %u usage 0x%02X (%ls)\n",
                    cap.UsagePage, cap.LinkCollection, cap.NotRange.Usage, UsageName(cap.NotRange.Usage));
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
            for (const USAGE usage : sorted) std::wprintf(L" 0x%02X(%ls)", usage, UsageName(usage));
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
        const std::wstring tooltip = L"Surface Pen Mapper · " + std::wstring(Tr(config_.language, UiText::AppSubtitle));
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
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, point.x, point.y, 0, hwnd_, nullptr);
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
    HFONT smallFont_ = nullptr;
    HFONT sectionFont_ = nullptr;
    HFONT titleFont_ = nullptr;
    HBITMAP penBitmap_ = nullptr;

    HWND title_ = nullptr;
    HWND subtitle_ = nullptr;
    HWND languageLabel_ = nullptr;
    HWND languageCombo_ = nullptr;
    HWND heroHint_ = nullptr;

    HWND upperTitle_ = nullptr;
    HWND upperClickLabel_ = nullptr;
    HWND upperClickHint_ = nullptr;
    HWND upperTapLabel_ = nullptr;
    HWND upperTapHint_ = nullptr;

    HWND lowerTitle_ = nullptr;
    HWND lowerClickLabel_ = nullptr;
    HWND lowerClickHint_ = nullptr;
    HWND lowerTapLabel_ = nullptr;
    HWND lowerTapHint_ = nullptr;

    HWND upperClickCombo_ = nullptr;
    HWND upperClickKey_ = nullptr;
    HWND upperTapCombo_ = nullptr;
    HWND upperTapKey_ = nullptr;
    HWND lowerClickCombo_ = nullptr;
    HWND lowerClickKey_ = nullptr;
    HWND lowerTapCombo_ = nullptr;
    HWND lowerTapKey_ = nullptr;

    HWND startupCheckbox_ = nullptr;
    HWND status_ = nullptr;
    HWND restoreButton_ = nullptr;

    std::unordered_map<HWND, KeyCaptureValue> keyCaptures_;
    std::optional<Gesture> lastGesture_;
    bool statusIsSaved_ = false;

    NOTIFYICONDATAW tray_{};
    bool trayAdded_ = false;
    std::unordered_map<HANDLE, DeviceState> deviceStates_;
};

} // namespace

