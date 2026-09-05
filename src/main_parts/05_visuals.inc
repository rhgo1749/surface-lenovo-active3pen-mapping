    bool CommitFromControls() {
        Config next = config_;
        next.upperClick = MappingFromControls(upperClickCombo_, upperClickKey_);
        next.upperTap = MappingFromControls(upperTapCombo_, upperTapKey_);
        next.lowerClick = MappingFromControls(lowerClickCombo_, lowerClickKey_);
        next.lowerTap = MappingFromControls(lowerTapCombo_, lowerTapKey_);

        std::wstring error;
        if (!SaveConfig(next, error)) {
            MessageBoxW(hwnd_, error.c_str(), Tr(config_.language, UiText::WindowTitle), MB_OK | MB_ICONERROR);
            return false;
        }

        const bool startup = SendMessageW(startupCheckbox_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!SetStartup(startup, error)) {
            MessageBoxW(hwnd_, error.c_str(), Tr(config_.language, UiText::WindowTitle), MB_OK | MB_ICONERROR);
            return false;
        }

        config_ = next;
        statusIsSaved_ = true;
        UpdateStatusText();
        UpdateTrayTip();
        return true;
    }

    void RestoreDefaults() {
        const Language language = config_.language;
        config_ = DefaultConfig(language);

        SetMappingControls(upperClickCombo_, upperClickKey_, config_.upperClick);
        SetMappingControls(upperTapCombo_, upperTapKey_, config_.upperTap);
        SetMappingControls(lowerClickCombo_, lowerClickKey_, config_.lowerClick);
        SetMappingControls(lowerTapCombo_, lowerTapKey_, config_.lowerTap);
        UpdateKeyCaptureEnabled();
        CommitFromControls();
    }

    void SetMappingControls(HWND combo, HWND capture, const Mapping& mapping) {
        PopulateActionCombo(combo, mapping.action);
        keyCaptures_[capture] = KeyCaptureValue{mapping.shortcutVk, mapping.shortcutModifiers};
        UpdateKeyCaptureText(capture);
    }

    void ChangeLanguage(Language language) {
        if (language == config_.language) return;
        config_.language = language;
        std::wstring error;
        if (!SaveConfig(config_, error)) {
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
        SetWindowTextW(title_, Tr(config_.language, UiText::WindowTitle));
        SetWindowTextW(subtitle_, Tr(config_.language, UiText::AppSubtitle));
        SetWindowTextW(languageLabel_, Tr(config_.language, UiText::Language));
        SetWindowTextW(heroHint_, Tr(config_.language, UiText::HeroHint));

        SetWindowTextW(upperTitle_, Tr(config_.language, UiText::UpperTitle));
        SetWindowTextW(lowerTitle_, Tr(config_.language, UiText::LowerTitle));
        SetWindowTextW(upperClickLabel_, Tr(config_.language, UiText::ButtonClick));
        SetWindowTextW(lowerClickLabel_, Tr(config_.language, UiText::ButtonClick));
        SetWindowTextW(upperTapLabel_, Tr(config_.language, UiText::HoldTap));
        SetWindowTextW(lowerTapLabel_, Tr(config_.language, UiText::HoldTap));
        SetWindowTextW(upperClickHint_, Tr(config_.language, UiText::ClickHint));
        SetWindowTextW(lowerClickHint_, Tr(config_.language, UiText::ClickHint));
        SetWindowTextW(upperTapHint_, Tr(config_.language, UiText::TapHint));
        SetWindowTextW(lowerTapHint_, Tr(config_.language, UiText::TapHint));
        SetWindowTextW(startupCheckbox_, Tr(config_.language, UiText::Startup));
        SetWindowTextW(restoreButton_, Tr(config_.language, UiText::RestoreDefaults));

        SelectLanguageInCombo(languageCombo_, config_.language);
        RefreshActionCombo(upperClickCombo_);
        RefreshActionCombo(upperTapCombo_);
        RefreshActionCombo(lowerClickCombo_);
        RefreshActionCombo(lowerTapCombo_);
        RefreshCaptureTexts();
        UpdateKeyCaptureEnabled();
        UpdateStatusText();
        UpdateLastInputText();
        UpdateTrayTip();
        InvalidateRect(hwnd_, nullptr, TRUE);
    }

    void UpdateStatusText() {
        if (!status_) return;
        SetWindowTextW(status_, Tr(config_.language,
            statusIsSaved_ ? UiText::StatusSaved : UiText::StatusReady));
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
        if (!lastGesture_) return;
        statusIsSaved_ = false;
        const Mapping& mapping = MappingForGesture(*lastGesture_);
        const std::wstring message = std::wstring(GestureName(config_.language, *lastGesture_)) +
            L" → " + ShortcutName(mapping, config_.language);
        SetWindowTextW(status_, message.c_str());
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

    void PaintUi() {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd_, &ps);

        RECT client{};
        GetClientRect(hwnd_, &client);
        HBRUSH background = CreateSolidBrush(kBackground);
        FillRect(hdc, &client, background);
        DeleteObject(background);

        DrawCard(hdc, {24, 92, 958, 337}, 18);
        DrawCard(hdc, {34, 350, 950, 518}, 16);
        DrawCard(hdc, {34, 526, 950, 694}, 16);
        DrawCard(hdc, {24, 701, 958, 752}, 16);

        HPEN separator = CreatePen(PS_SOLID, 1, kBorder);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, separator));
        MoveToEx(hdc, 54, 449, nullptr); LineTo(hdc, 930, 449);
        MoveToEx(hdc, 54, 625, nullptr); LineTo(hdc, 930, 625);
        SelectObject(hdc, oldPen);
        DeleteObject(separator);

        if (penBitmap_) {
            HDC mem = CreateCompatibleDC(hdc);
            HGDIOBJ old = SelectObject(mem, penBitmap_);
            BitBlt(hdc, 74, 110, 820, 200, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteDC(mem);
        }

        DrawCallout(hdc, {510, 112, 620, 142}, Tr(config_.language, UiText::UpperCallout),
            POINT{554, 181}, POINT{552, 142});
        DrawCallout(hdc, {588, 268, 698, 298}, Tr(config_.language, UiText::LowerCallout),
            POINT{594, 183}, POINT{612, 268});
        DrawCallout(hdc, {846, 247, 926, 277}, Tr(config_.language, UiText::TipCallout),
            POINT{865, 216}, POINT{875, 247});

        EndPaint(hwnd_, &ps);
    }

    void DrawCard(HDC hdc, RECT rect, int radius) {
        HBRUSH brush = CreateSolidBrush(kCard);
        HPEN pen = CreatePen(PS_SOLID, 1, kBorder);
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }

    void DrawCallout(HDC hdc, RECT pill, const wchar_t* text, POINT target, POINT elbow) {
        HPEN line = CreatePen(PS_SOLID, 2, kAccent);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, line));
        MoveToEx(hdc, target.x, target.y, nullptr);
        LineTo(hdc, elbow.x, elbow.y);
        LineTo(hdc, pill.left, (pill.top + pill.bottom) / 2);

        HBRUSH dot = CreateSolidBrush(kAccent);
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, dot));
        Ellipse(hdc, target.x - 4, target.y - 4, target.x + 4, target.y + 4);
        SelectObject(hdc, oldBrush);
        DeleteObject(dot);
        SelectObject(hdc, oldPen);
        DeleteObject(line);

        HBRUSH pillBrush = CreateSolidBrush(kAccentSoft);
        HPEN pillPen = CreatePen(PS_SOLID, 1, RGB(210, 224, 250));
        HGDIOBJ oldPillBrush = SelectObject(hdc, pillBrush);
        HGDIOBJ oldPillPen = SelectObject(hdc, pillPen);
        RoundRect(hdc, pill.left, pill.top, pill.right, pill.bottom, 14, 14);
        SelectObject(hdc, oldPillPen);
        SelectObject(hdc, oldPillBrush);
        DeleteObject(pillPen);
        DeleteObject(pillBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, kAccent);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font_));
        DrawTextW(hdc, text, -1, &pill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
    }

