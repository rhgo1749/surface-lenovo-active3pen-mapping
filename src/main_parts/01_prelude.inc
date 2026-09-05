#include <windows.h>
#include <commctrl.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <shellapi.h>
#include <wincodec.h>
#include <objbase.h>
#include <wincrypt.h>

#include "pen_photo_data.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cwchar>
#include <cstring>
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
constexpr UINT kRestoreDefaultsId = 1121;
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

constexpr COLORREF kBackground = RGB(246, 248, 252);
constexpr COLORREF kCard = RGB(255, 255, 255);
constexpr COLORREF kBorder = RGB(224, 229, 238);
constexpr COLORREF kText = RGB(25, 30, 40);
constexpr COLORREF kMuted = RGB(105, 113, 126);
constexpr COLORREF kAccent = RGB(20, 92, 220);
constexpr COLORREF kAccentSoft = RGB(234, 242, 255);

enum class Language : int {
    English = 0,
    Korean = 1,
    Japanese = 2,
    ChineseSimplified = 3,
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
    AppSubtitle,
    HeroHint,
    Language,
    UpperTitle,
    LowerTitle,
    ButtonClick,
    ClickHint,
    HoldTap,
    TapHint,
    Startup,
    StatusReady,
    StatusSaved,
    RestoreDefaults,
    Settings,
    Exit,
    PressKey,
    SecureSequence,
    UpperCallout,
    LowerCallout,
    TipCallout,
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

Config DefaultConfig(Language language) {
    Config config;
    config.language = language;
    return config;
}

Language DefaultLanguage() {
    const LANGID lang = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(lang)) {
        case LANG_KOREAN: return Language::Korean;
        case LANG_JAPANESE: return Language::Japanese;
        case LANG_CHINESE: return Language::ChineseSimplified;
        default: return Language::English;
    }
}

const wchar_t* LanguageCode(Language language) {
    switch (language) {
        case Language::Korean: return L"ko";
        case Language::Japanese: return L"ja";
        case Language::ChineseSimplified: return L"zh-CN";
        case Language::English: return L"en";
    }
    return L"en";
}

const wchar_t* Tr(Language language, UiText text) {
    if (language == Language::Korean) {
        switch (text) {
            case UiText::WindowTitle: return L"Surface Pen Mapper";
            case UiText::AppSubtitle: return L"Lenovo Active Pen 3";
            case UiText::HeroHint: return L"두 버튼에 원하는 동작을 연결합니다. Windows나 필기 앱의 기본 동작이 함께 실행될 수 있습니다.";
            case UiText::Language: return L"언어";
            case UiText::UpperTitle: return L"상단 버튼";
            case UiText::LowerTitle: return L"하단 버튼";
            case UiText::ButtonClick: return L"버튼 클릭";
            case UiText::ClickHint: return L"버튼만 눌렀다 놓았을 때 실행";
            case UiText::HoldTap: return L"누른 채 화면 탭";
            case UiText::TapHint: return L"버튼을 누른 상태에서 펜촉으로 화면을 탭했을 때 실행";
            case UiText::Startup: return L"Windows 로그인 시 자동으로 실행";
            case UiText::StatusReady: return L"설정은 변경 즉시 저장되고 적용됩니다.";
            case UiText::StatusSaved: return L"저장됨 · 지금부터 적용";
            case UiText::RestoreDefaults: return L"기본값으로 복원";
            case UiText::Settings: return L"설정";
            case UiText::Exit: return L"종료";
            case UiText::PressKey: return L"클릭 후 키 입력";
            case UiText::SecureSequence: return L"Ctrl+Alt+Delete는 Windows 보안 시퀀스라 매핑할 수 없습니다.";
            case UiText::UpperCallout: return L"상단 버튼";
            case UiText::LowerCallout: return L"하단 버튼";
            case UiText::TipCallout: return L"펜촉";
        }
    }

    if (language == Language::Japanese) {
        switch (text) {
            case UiText::WindowTitle: return L"Surface Pen Mapper";
            case UiText::AppSubtitle: return L"Lenovo Active Pen 3";
            case UiText::HeroHint: return L"2つのボタンに好きな動作を割り当てます。Windowsや筆記アプリの標準動作が同時に実行される場合があります。";
            case UiText::Language: return L"言語";
            case UiText::UpperTitle: return L"上側ボタン";
            case UiText::LowerTitle: return L"下側ボタン";
            case UiText::ButtonClick: return L"ボタンクリック";
            case UiText::ClickHint: return L"画面に触れず、ボタンを押して離したとき";
            case UiText::HoldTap: return L"押しながら画面をタップ";
            case UiText::TapHint: return L"ボタンを押したままペン先で画面をタップしたとき";
            case UiText::Startup: return L"Windows サインイン時に自動起動";
            case UiText::StatusReady: return L"変更はすぐに保存・適用されます。";
            case UiText::StatusSaved: return L"保存済み · 適用中";
            case UiText::RestoreDefaults: return L"既定値に戻す";
            case UiText::Settings: return L"設定";
            case UiText::Exit: return L"終了";
            case UiText::PressKey: return L"クリックしてキー入力";
            case UiText::SecureSequence: return L"Ctrl+Alt+Delete は Windows の保護された操作のため割り当てできません。";
            case UiText::UpperCallout: return L"上側ボタン";
            case UiText::LowerCallout: return L"下側ボタン";
            case UiText::TipCallout: return L"ペン先";
        }
    }

    if (language == Language::ChineseSimplified) {
        switch (text) {
            case UiText::WindowTitle: return L"Surface Pen Mapper";
            case UiText::AppSubtitle: return L"Lenovo Active Pen 3";
            case UiText::HeroHint: return L"为两个按钮设置所需操作。Windows 或书写应用的默认操作有时也会同时执行。";
            case UiText::Language: return L"语言";
            case UiText::UpperTitle: return L"上侧按钮";
            case UiText::LowerTitle: return L"下侧按钮";
            case UiText::ButtonClick: return L"按钮单击";
            case UiText::ClickHint: return L"不触碰屏幕，按下并松开按钮时执行";
            case UiText::HoldTap: return L"按住并点按屏幕";
            case UiText::TapHint: return L"按住按钮，再用笔尖点按屏幕时执行";
            case UiText::Startup: return L"登录 Windows 时自动启动";
            case UiText::StatusReady: return L"更改会立即保存并生效。";
            case UiText::StatusSaved: return L"已保存 · 已生效";
            case UiText::RestoreDefaults: return L"恢复默认值";
            case UiText::Settings: return L"设置";
            case UiText::Exit: return L"退出";
            case UiText::PressKey: return L"点击后按下按键";
            case UiText::SecureSequence: return L"Ctrl+Alt+Delete 是 Windows 受保护的安全序列，无法映射。";
            case UiText::UpperCallout: return L"上侧按钮";
            case UiText::LowerCallout: return L"下侧按钮";
            case UiText::TipCallout: return L"笔尖";
        }
    }

    switch (text) {
        case UiText::WindowTitle: return L"Surface Pen Mapper";
        case UiText::AppSubtitle: return L"Lenovo Active Pen 3";
        case UiText::HeroHint: return L"Assign the two buttons to the actions you want. Windows or ink apps may still run their default action too.";
        case UiText::Language: return L"Language";
        case UiText::UpperTitle: return L"Upper button";
        case UiText::LowerTitle: return L"Lower button";
        case UiText::ButtonClick: return L"Button click";
        case UiText::ClickHint: return L"press and release the button without touching the screen";
        case UiText::HoldTap: return L"Hold + tap screen";
        case UiText::TapHint: return L"hold the button, then touch the screen with the pen tip";
        case UiText::Startup: return L"Start automatically when I sign in to Windows";
        case UiText::StatusReady: return L"Changes are saved and applied immediately.";
        case UiText::StatusSaved: return L"Saved · active now";
        case UiText::RestoreDefaults: return L"Restore defaults";
        case UiText::Settings: return L"Settings";
        case UiText::Exit: return L"Exit";
        case UiText::PressKey: return L"click, then press a key";
        case UiText::SecureSequence: return L"Ctrl+Alt+Delete is protected by Windows and cannot be mapped.";
        case UiText::UpperCallout: return L"Upper button";
        case UiText::LowerCallout: return L"Lower button";
        case UiText::TipCallout: return L"Pen tip";
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
            case Action::None: return L"추가 동작 없음";
            case Action::Back: return L"뒤로가기";
            case Action::Forward: return L"앞으로가기";
            case Action::Left: return L"왼쪽 클릭";
            case Action::Right: return L"우클릭";
            case Action::Middle: return L"가운데 클릭";
            case Action::Shortcut: return L"키 / 단축키";
        }
    }
    if (language == Language::Japanese) {
        switch (action) {
            case Action::None: return L"追加動作なし";
            case Action::Back: return L"戻る";
            case Action::Forward: return L"進む";
            case Action::Left: return L"左クリック";
            case Action::Right: return L"右クリック";
            case Action::Middle: return L"中クリック";
            case Action::Shortcut: return L"キー / ショートカット";
        }
    }
    if (language == Language::ChineseSimplified) {
        switch (action) {
            case Action::None: return L"无额外操作";
            case Action::Back: return L"后退";
            case Action::Forward: return L"前进";
            case Action::Left: return L"左键单击";
            case Action::Right: return L"右键单击";
            case Action::Middle: return L"中键单击";
            case Action::Shortcut: return L"按键 / 快捷键";
        }
    }
    switch (action) {
        case Action::None: return L"No extra action";
        case Action::Back: return L"Back";
        case Action::Forward: return L"Forward";
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
    if (language == Language::Japanese) {
        switch (gesture) {
            case Gesture::UpperClick: return L"上側ボタンクリック";
            case Gesture::UpperTap: return L"上側ボタン + 画面タップ";
            case Gesture::LowerClick: return L"下側ボタンクリック";
            case Gesture::LowerTap: return L"下側ボタン + 画面タップ";
        }
    }
    if (language == Language::ChineseSimplified) {
        switch (gesture) {
            case Gesture::UpperClick: return L"上侧按钮单击";
            case Gesture::UpperTap: return L"上侧按钮 + 点按屏幕";
            case Gesture::LowerClick: return L"下侧按钮单击";
            case Gesture::LowerTap: return L"下侧按钮 + 点按屏幕";
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
    return usage == kInvertUsage || usage == kEraserUsage || usage == kSecondaryBarrelUsage;
}

