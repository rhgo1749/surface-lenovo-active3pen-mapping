class PenMapper {
public:
    PenMapper(Options options, Config config)
        : options_(std::move(options)), config_(std::move(config)) {}

    ~PenMapper() {
        if (font_) DeleteObject(font_);
        if (smallFont_) DeleteObject(smallFont_);
        if (sectionFont_) DeleteObject(sectionFont_);
        if (titleFont_) DeleteObject(titleFont_);
        if (penBitmap_) DeleteObject(penBitmap_);
    }

    bool Initialize(HINSTANCE instance) {
        instance_ = instance;
        const wchar_t* className = options_.diagnose ? kDiagnosticWindowClass : kMainWindowClass;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &PenMapper::StaticWindowProc;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;

        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
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
            options_.diagnose ? 0 : 1000,
            options_.diagnose ? 0 : 800,
            nullptr, nullptr, instance_, this);

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
            EnumeratePenDevices();
            return true;
        }

        CreateUi();
        penBitmap_ = DecodeEmbeddedPenPhoto(820, 200);

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
                if (!IsKeyCaptureControl(focus) && IsDialogMessageW(hwnd_, &message)) continue;
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
        HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
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
                if (self && IsModifierKey(static_cast<WORD>(wParam))) self->UpdateKeyCaptureText(hwnd);
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
            case WM_PAINT:
                PaintUi();
                return 0;

            case WM_ERASEBKGND:
                return 1;

            case WM_CTLCOLORSTATIC: {
                HDC hdc = reinterpret_cast<HDC>(wParam);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, kText);
                return reinterpret_cast<LRESULT>(GetStockObject(HOLLOW_BRUSH));
            }

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
                if (lParam == WM_LBUTTONUP) ShowSettings();
                else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) ShowTrayMenu();
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

        if (id == kMenuSettings) {
            ShowSettings();
            return 0;
        }
        if (id == kMenuExit) {
            DestroyWindow(hwnd_);
            return 0;
        }
        if (id == kRestoreDefaultsId && notify == BN_CLICKED) {
            RestoreDefaults();
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
                if (SelectedAction(ComboForId(id)) == Action::Shortcut) SetFocus(capture);
            }
            CommitFromControls();
            return 0;
        }

        if (id == kStartupCheckboxId && notify == BN_CLICKED) {
            CommitFromControls();
            return 0;
        }
        return 0;
    }

