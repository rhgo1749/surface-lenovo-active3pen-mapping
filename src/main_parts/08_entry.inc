int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&controls);

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comResult);

    Options options = ParseOptions();
    if (options.help || !options.valid || options.diagnose || options.startupEnable || options.startupDisable) {
        OpenConsole();
    }

    if (!options.valid) {
        std::fwprintf(stderr, L"%ls\n\n", options.error.c_str());
        PrintHelp();
        if (comInitialized) CoUninitialize();
        return 2;
    }
    if (options.help) {
        PrintHelp();
        if (comInitialized) CoUninitialize();
        return 0;
    }

    if (options.startupEnable || options.startupDisable) {
        if (options.startupEnable && options.startupDisable) {
            std::fwprintf(stderr, L"Choose only one startup option.\n");
            if (comInitialized) CoUninitialize();
            return 2;
        }
        std::wstring error;
        if (!SetStartup(options.startupEnable, error)) {
            std::fwprintf(stderr, L"%ls\n", error.c_str());
            if (comInitialized) CoUninitialize();
            return 1;
        }
        std::wprintf(options.startupEnable ? L"Startup enabled.\n" : L"Startup disabled.\n");
        if (comInitialized) CoUninitialize();
        return 0;
    }

    Config config = LoadConfig();
    if (options.lowerClickOverride) config.lowerClick.action = *options.lowerClickOverride;

    HANDLE mutex = nullptr;
    if (!options.diagnose) {
        mutex = CreateMutexW(nullptr, FALSE, kMutexName);
        if (!mutex) {
            if (comInitialized) CoUninitialize();
            return 1;
        }
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (HWND existing = FindWindowW(kMainWindowClass, nullptr)) {
                PostMessageW(existing, kShowSettingsMessage, 0, 0);
            }
            CloseHandle(mutex);
            if (comInitialized) CoUninitialize();
            return 0;
        }
    }

    PenMapper mapper(std::move(options), std::move(config));
    if (!mapper.Initialize(instance)) {
        if (mutex) CloseHandle(mutex);
        if (comInitialized) CoUninitialize();
        return 1;
    }

    const int result = mapper.Run();
    if (mutex) CloseHandle(mutex);
    if (comInitialized) CoUninitialize();
    return result;
}
