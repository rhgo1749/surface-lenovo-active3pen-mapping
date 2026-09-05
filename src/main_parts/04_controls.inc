    void CreateUi() {
        font_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        smallFont_ = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        sectionFont_ = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        titleFont_ = CreateFontW(-30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        title_ = CreateStatic(L"", 34, 20, 460, 38, titleFont_);
        subtitle_ = CreateStatic(L"", 36, 57, 360, 24, font_);
        languageLabel_ = CreateStatic(L"", 744, 27, 70, 22, smallFont_);
        languageCombo_ = CreateLanguageCombo(812, 21);

        heroHint_ = CreateStatic(L"", 58, 305, 830, 24, smallFont_);

        upperTitle_ = CreateStatic(L"", 52, 366, 220, 28, sectionFont_);
        upperClickLabel_ = CreateStatic(L"", 72, 407, 190, 24, font_);
        upperClickHint_ = CreateStatic(L"", 72, 430, 450, 22, smallFont_);
        upperClickCombo_ = CreateActionCombo(kUpperClickComboId, 588, 401, config_.upperClick.action);
        upperClickKey_ = CreateKeyCapture(kUpperClickKeyId, 790, 401, config_.upperClick);

        upperTapLabel_ = CreateStatic(L"", 72, 457, 220, 24, font_);
        upperTapHint_ = CreateStatic(L"", 72, 480, 450, 22, smallFont_);
        upperTapCombo_ = CreateActionCombo(kUpperTapComboId, 588, 451, config_.upperTap.action);
        upperTapKey_ = CreateKeyCapture(kUpperTapKeyId, 790, 451, config_.upperTap);

        lowerTitle_ = CreateStatic(L"", 52, 542, 220, 28, sectionFont_);
        lowerClickLabel_ = CreateStatic(L"", 72, 583, 190, 24, font_);
        lowerClickHint_ = CreateStatic(L"", 72, 606, 450, 22, smallFont_);
        lowerClickCombo_ = CreateActionCombo(kLowerClickComboId, 588, 577, config_.lowerClick.action);
        lowerClickKey_ = CreateKeyCapture(kLowerClickKeyId, 790, 577, config_.lowerClick);

        lowerTapLabel_ = CreateStatic(L"", 72, 633, 220, 24, font_);
        lowerTapHint_ = CreateStatic(L"", 72, 656, 450, 22, smallFont_);
        lowerTapCombo_ = CreateActionCombo(kLowerTapComboId, 588, 627, config_.lowerTap.action);
        lowerTapKey_ = CreateKeyCapture(kLowerTapKeyId, 790, 627, config_.lowerTap);

        startupCheckbox_ = CreateWindowExW(0, WC_BUTTONW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            42, 714, 430, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStartupCheckboxId)),
            instance_, nullptr);
        SetControlFont(startupCheckbox_, font_);
        SendMessageW(startupCheckbox_, BM_SETCHECK,
            IsStartupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);

        status_ = CreateStatic(L"", 492, 718, 260, 22, smallFont_);

        restoreButton_ = CreateWindowExW(0, WC_BUTTONW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            785, 706, 164, 36, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRestoreDefaultsId)),
            instance_, nullptr);
        SetControlFont(restoreButton_, font_);

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

    static void SetControlFont(HWND control, HFONT font) {
        if (control && font) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }

    HWND CreateLanguageCombo(int x, int y) {
        HWND combo = CreateWindowExW(0, WC_COMBOBOXW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            x, y, 140, 140, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLanguageComboId)),
            instance_, nullptr);
        SetControlFont(combo, font_);

        const std::array<std::pair<const wchar_t*, Language>, 4> languages{{
            {L"한국어", Language::Korean},
            {L"English", Language::English},
            {L"日本語", Language::Japanese},
            {L"简体中文", Language::ChineseSimplified},
        }};
        for (const auto& entry : languages) {
            const LRESULT index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.first));
            SendMessageW(combo, CB_SETITEMDATA, index, static_cast<LPARAM>(entry.second));
        }
        SelectLanguageInCombo(combo, config_.language);
        return combo;
    }

    void SelectLanguageInCombo(HWND combo, Language language) {
        const LRESULT count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
        for (LRESULT index = 0; index < count; ++index) {
            if (static_cast<Language>(SendMessageW(combo, CB_GETITEMDATA, index, 0)) == language) {
                SendMessageW(combo, CB_SETCURSEL, index, 0);
                return;
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
        if (index >= 0) SendMessageW(combo, CB_SETITEMDATA, index, static_cast<LPARAM>(action));
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
            if (static_cast<Action>(SendMessageW(combo, CB_GETITEMDATA, index, 0)) == selected) {
                SendMessageW(combo, CB_SETCURSEL, index, 0);
                return;
            }
        }
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
    }

    HWND CreateActionCombo(UINT id, int x, int y, Action selected) {
        HWND combo = CreateWindowExW(0, WC_COMBOBOXW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            x, y, 192, 220, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        SetControlFont(combo, font_);
        PopulateActionCombo(combo, selected);
        return combo;
    }

    HWND CreateKeyCapture(UINT id, int x, int y, const Mapping& mapping) {
        HWND control = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_CENTER,
            x, y, 158, 30, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
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
        if (const auto found = keyCaptures_.find(keyCapture); found != keyCaptures_.end()) {
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

        const bool ctrl = (modifiers & HOTKEYF_CONTROL) != 0;
        const bool alt = (modifiers & HOTKEYF_ALT) != 0;
        if (vk == VK_DELETE && ctrl && alt) {
            MessageBoxW(hwnd_, Tr(config_.language, UiText::SecureSequence),
                Tr(config_.language, UiText::WindowTitle), MB_OK | MB_ICONWARNING);
            return;
        }

        keyCaptures_[control] = KeyCaptureValue{vk, modifiers};
        UpdateKeyCaptureText(control);
        CommitFromControls();
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

