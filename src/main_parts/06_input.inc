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
            case Action::None: return;
            case Action::Back: SendMouse(MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP, XBUTTON1); return;
            case Action::Forward: SendMouse(MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP, XBUTTON2); return;
            case Action::Left: SendMouse(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP); return;
            case Action::Right: SendMouse(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP); return;
            case Action::Middle: SendMouse(MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP); return;
            case Action::Shortcut: SendShortcut(mapping); return;
        }
    }

    DeviceState* GetDeviceState(HANDLE device) {
        if (auto found = deviceStates_.find(device); found != deviceStates_.end()) return &found->second;

        DeviceState state;
        UINT nameLength = 0;
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &nameLength) != static_cast<UINT>(-1) &&
            nameLength > 0) {
            std::vector<wchar_t> buffer(nameLength + 1, L'\0');
            UINT requested = nameLength;
            if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, buffer.data(), &requested) != static_cast<UINT>(-1)) {
                state.name.assign(buffer.data());
            }
        }

        UINT preparsedSize = 0;
        if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, nullptr, &preparsedSize) == static_cast<UINT>(-1) ||
            preparsedSize == 0) {
            return nullptr;
        }
        state.preparsed.resize(preparsedSize);
        UINT requested = preparsedSize;
        if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA,
                state.preparsed.data(), &requested) == static_cast<UINT>(-1)) {
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

    void ProcessRawInput(HRAWINPUT handle) {
        UINT size = 0;
        if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) return;

        std::vector<BYTE> buffer(size);
        UINT requested = size;
        if (GetRawInputData(handle, RID_INPUT, buffer.data(), &requested,
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
            ProcessHidReport(*state, reports + index * raw->data.hid.dwSizeHid, raw->data.hid.dwSizeHid);
        }
    }

    void ProcessHidReport(DeviceState& state, BYTE* report, ULONG reportLength) {
        auto* preparsed = reinterpret_cast<PHIDP_PREPARSED_DATA>(state.preparsed.data());
        std::set<std::pair<USAGE, USHORT>> pageLinks;
        for (const auto& cap : state.buttonCaps) {
            if (cap.UsagePage == kDigitizerPage) pageLinks.emplace(cap.UsagePage, cap.LinkCollection);
        }

        std::unordered_set<USAGE> active;
        bool matchedReport = false;
        for (const auto& pageLink : pageLinks) {
            const ULONG maxUsages = HidP_MaxUsageListLength(HidP_Input, pageLink.first, preparsed);
            if (maxUsages == 0) continue;

            std::vector<USAGE> usages(maxUsages);
            ULONG usageCount = maxUsages;
            const NTSTATUS status = HidP_GetUsages(
                HidP_Input, pageLink.first, pageLink.second,
                usages.data(), &usageCount, preparsed,
                reinterpret_cast<PCHAR>(report), reportLength);

            if (status == HIDP_STATUS_SUCCESS) {
                matchedReport = true;
                for (ULONG index = 0; index < usageCount; ++index) active.insert(usages[index]);
            }
        }
        if (!matchedReport) return;

        if (options_.diagnose && active != state.previousActive) PrintActiveUsages(active);

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

        if (!upperNow && state.upperPressed && !state.upperUsedWithTip) TriggerGesture(Gesture::UpperClick);
        if (!lowerNow && state.lowerPressed && !state.lowerUsedWithTip) TriggerGesture(Gesture::LowerClick);

        state.upperPressed = upperNow;
        state.lowerPressed = lowerNow;
        state.tipPressed = tipNow;
        state.previousActive = std::move(active);
    }

