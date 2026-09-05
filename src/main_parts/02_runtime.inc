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
            const auto action = ActionFromName(arg.substr(9));
            if (!action || *action == Action::Shortcut) {
                options.valid = false;
                options.error = L"Unsupported CLI action: " + arg.substr(9);
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
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
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
        L"The settings UI exposes four independent gestures and supports Korean, English,\n"
        L"Japanese and Simplified Chinese.\n");
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
    const std::wstring base(prefix);
    std::wstring actionValue;
    if (ReadRegistryString(key, (base + L"Action").c_str(), actionValue)) {
        if (const auto action = ActionFromName(actionValue)) mapping.action = *action;
    }

    DWORD value = 0;
    if (ReadRegistryDword(key, (base + L"ShortcutVk").c_str(), value) && value > 0 && value <= 0xFF) {
        mapping.shortcutVk = static_cast<WORD>(value);
    }
    if (ReadRegistryDword(key, (base + L"ShortcutModifiers").c_str(), value)) {
        mapping.shortcutModifiers = static_cast<BYTE>(value & 0xFF);
    }
}

Config LoadConfig() {
    Config config = DefaultConfig(DefaultLanguage());

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
        else if (languageValue == L"ja") config.language = Language::Japanese;
        else if (languageValue == L"zh-CN") config.language = Language::ChineseSimplified;
        else config.language = Language::English;
    }

    // Migrate the original single-action configuration into lower-click.
    std::wstring lowerAction;
    if (!ReadRegistryString(key, L"LowerClickAction", lowerAction)) {
        std::wstring legacyAction;
        if (ReadRegistryString(key, L"Action", legacyAction)) {
            if (const auto action = ActionFromName(legacyAction)) config.lowerClick.action = *action;
        }
    }

    RegCloseKey(key);
    return config;
}

LONG WriteMapping(HKEY key, const wchar_t* prefix, const Mapping& mapping) {
    const std::wstring base(prefix);
    LONG result = WriteRegistryString(key, base + L"Action", ActionName(mapping.action));
    if (result == ERROR_SUCCESS) result = WriteRegistryDword(key, base + L"ShortcutVk", mapping.shortcutVk);
    if (result == ERROR_SUCCESS) result = WriteRegistryDword(key, base + L"ShortcutModifiers", mapping.shortcutModifiers);
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
    if (result == ERROR_SUCCESS) result = WriteRegistryString(key, L"Language", LanguageCode(config.language));

    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        error = L"Could not save mapper settings. Error " + std::to_wstring(result);
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

    if (vk == VK_RETURN) return ext ? L"Numpad Enter" : L"Enter";
    if (vk == VK_ESCAPE) return L"Esc";
    if (vk == VK_TAB) return L"Tab";
    if (vk == VK_SPACE) return L"Space";
    if (vk == VK_BACK) return L"Backspace";
    if (vk == VK_DELETE) return L"Delete";
    if (vk == VK_INSERT) return L"Insert";
    if (vk == VK_HOME) return L"Home";
    if (vk == VK_END) return L"End";
    if (vk == VK_PRIOR) return L"Page Up";
    if (vk == VK_NEXT) return L"Page Down";
    if (vk == VK_LEFT) return L"←";
    if (vk == VK_RIGHT) return L"→";
    if (vk == VK_UP) return L"↑";
    if (vk == VK_DOWN) return L"↓";
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);

    wchar_t keyName[64]{};
    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG keyInfo = static_cast<LONG>(scanCode << 16);
    if (ext) keyInfo |= 1 << 24;
    if (GetKeyNameTextW(keyInfo, keyName, 64) > 0) return keyName;

    wchar_t fallback[16]{};
    swprintf_s(fallback, L"VK 0x%02X", vk);
    return fallback;
}

std::wstring ShortcutName(const Mapping& mapping, Language language) {
    if (mapping.action != Action::Shortcut) return ActionDisplayName(language, mapping.action);

    std::wstring text;
    if (mapping.shortcutModifiers & HOTKEYF_CONTROL) text += L"Ctrl+";
    if (mapping.shortcutModifiers & HOTKEYF_ALT) text += L"Alt+";
    if (mapping.shortcutModifiers & HOTKEYF_SHIFT) text += L"Shift+";
    if (mapping.shortcutModifiers & kHotkeyWin) text += L"Win+";
    text += KeyDisplayName(mapping.shortcutVk, mapping.shortcutModifiers, language);
    return text;
}

HBITMAP DecodeEmbeddedPenPhoto(UINT targetWidth, UINT targetHeight) {
    DWORD jpegSize = 0;
    if (!CryptStringToBinaryA(
            kLenovoActivePen3PhotoJpegBase64.data(),
            static_cast<DWORD>(kLenovoActivePen3PhotoJpegBase64.size()),
            CRYPT_STRING_BASE64, nullptr, &jpegSize, nullptr, nullptr) || jpegSize == 0) {
        return nullptr;
    }

    std::vector<BYTE> jpeg(jpegSize);
    if (!CryptStringToBinaryA(
            kLenovoActivePen3PhotoJpegBase64.data(),
            static_cast<DWORD>(kLenovoActivePen3PhotoJpegBase64.size()),
            CRYPT_STRING_BASE64, jpeg.data(), &jpegSize, nullptr, nullptr)) {
        return nullptr;
    }
    jpeg.resize(jpegSize);

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, jpeg.size());
    if (!memory) return nullptr;

    void* bytes = GlobalLock(memory);
    if (!bytes) {
        GlobalFree(memory);
        return nullptr;
    }
    memcpy(bytes, jpeg.data(), jpeg.size());
    GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return nullptr;
    }

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        stream->Release();
        return nullptr;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        factory->Release();
        stream->Release();
        return nullptr;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);

    IWICBitmapScaler* scaler = nullptr;
    if (SUCCEEDED(hr)) hr = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(hr)) hr = scaler->Initialize(frame, targetWidth, targetHeight, WICBitmapInterpolationModeFant);

    IWICFormatConverter* converter = nullptr;
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    }

    HBITMAP bitmap = nullptr;
    if (SUCCEEDED(hr)) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = static_cast<LONG>(targetWidth);
        info.bmiHeader.biHeight = -static_cast<LONG>(targetHeight);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* pixels = nullptr;
        HDC screen = GetDC(nullptr);
        bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        ReleaseDC(nullptr, screen);

        if (bitmap && pixels) {
            const UINT stride = targetWidth * 4;
            const UINT size = stride * targetHeight;
            if (FAILED(converter->CopyPixels(nullptr, stride, size, static_cast<BYTE*>(pixels)))) {
                DeleteObject(bitmap);
                bitmap = nullptr;
            }
        }
    }

    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    factory->Release();
    stream->Release();
    return bitmap;
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

