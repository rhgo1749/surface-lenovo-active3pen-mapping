# surface-lenovo-active3pen-mapping

A tiny native Windows utility for using a two-button active pen as a better mouse replacement on Surface devices.

The initial target is **Surface Pro 12 + Lenovo Active Pen 3 over MPP**. That combination has been hardware-validated:

```text
upper side button -> 0x44 (Barrel)
lower side button -> 0x3C (Invert)
pen tip contact   -> 0x42 (TipSwitch)
hover             -> 0x32 (InRange)
```

No Wacom tablet driver, virtual tablet driver, .NET runtime, or kernel driver is required.

## Four independent gestures

The mapper combines each physical side-button state with the pen-tip state, so a two-button pen exposes four user-configurable gestures:

1. **Upper button click** — press and release the upper button without touching the screen.
2. **Upper button + pen tap** — hold the upper button, then touch the pen tip to the screen.
3. **Lower button click** — press and release the lower button without touching the screen.
4. **Lower button + pen tap** — hold the lower button, then touch the pen tip to the screen.

The recognizer makes the gestures mutually exclusive. A button-click action fires on button release only if no `TipSwitch (0x42)` occurred during that hold. If the tip touches the screen while the button is held, the corresponding `button + pen tap` action fires instead and the later button release does not also fire the button-click action.

Repeated tip taps while continuing to hold a side button can fire the hold+tap action repeatedly.

## Settings UI

Double-click `surface-pen-map.exe` to open the native settings window. It presents the **Upper side button** and **Lower side button** as separate sections, with an independent action selector for both `Button click` and `Hold + tap screen`.

Each of the four gesture slots can be mapped to:

- No extra action / keep Windows behavior
- Back (`Mouse XBUTTON1`)
- Forward (`Mouse XBUTTON2`)
- Left click
- Right click
- Middle click
- a custom keyboard shortcut using `Ctrl`, `Alt`, and/or `Shift` plus a key

Default mappings:

```text
Upper button click      -> no extra action
Upper button + pen tap  -> no extra action (Windows native right-click remains)
Lower button click      -> Back / Mouse 4
Lower button + pen tap  -> no extra action
```

The UI includes a **Last input** line so the physical gesture recognized by the mapper can be checked without opening diagnostic mode.

Settings are stored per-user under:

```text
HKCU\Software\SurfaceLenovoActive3PenMapping
```

The UI also has a **Start mapper when I sign in to Windows** checkbox. Startup uses the current executable path with `--background`, so Windows sign-in does not pop the settings window open.

### Native Windows behavior is not suppressed

This utility observes HID Raw Input and emits an additional configured action. It does **not** replace or suppress the Windows pen stack.

In particular, Windows uses `Barrel (0x44)` to modify pen-tip behavior into a native secondary/right action. Therefore **Upper button + pen tap** still produces Windows' normal right-click behavior; a custom mapping in that slot is additional.

Likewise, software that interprets `Invert/Eraser` as an eraser may retain that native behavior while a custom **Lower button + pen tap** action is also emitted.

## How it works

Windows pen devices expose a HID Digitizer pen collection. This utility registers for Raw Input from integrated/external pen collections and tracks these usages:

- `0x44` — Barrel / upper side button
- `0x3C` — Invert / validated lower button signal on the target hardware
- `0x45` — Eraser fallback
- `0x5A` — Secondary Barrel fallback
- `0x42` — TipSwitch / pen-tip contact
- `0x32` — InRange / hover

The current-state transitions are used to recognize the four gestures, then the configured extra action is emitted with `SendInput`.

## Build

Requirements:

- Windows 11
- Visual Studio with Desktop C++ / Windows SDK
- CMake 3.24+

ARM64 build:

```powershell
cmake -S . -B build -A ARM64
cmake --build build --config Release
```

The executable will be under:

```text
build\Release\surface-pen-map.exe
```

The repository also has a GitHub Actions build that compiles natively on a Windows 11 ARM64 runner and publishes the executable as an artifact.

## Diagnose pen signals and gestures

Diagnostic mode can run alongside the normal mapper and never emits mapped input:

```powershell
.\surface-pen-map.exe --diagnose
```

Try all four patterns:

1. upper button press/release
2. upper button held while tapping the pen tip
3. lower button press/release
4. lower button held while tapping the pen tip

The console prints the raw active HID usages and the recognized gesture.

## Run modes

Normal launch opens the settings UI:

```powershell
.\surface-pen-map.exe
```

Start silently in the background:

```powershell
.\surface-pen-map.exe --background
```

The legacy `--action=` option remains as a temporary runtime override for the **Lower button click** mapping:

```powershell
.\surface-pen-map.exe --action=back
.\surface-pen-map.exe --action=forward
.\surface-pen-map.exe --action=left
.\surface-pen-map.exe --action=right
.\surface-pen-map.exe --action=middle
.\surface-pen-map.exe --action=none
```

A tray icon is shown while the mapper is running. Left-click it to open Settings; right-click it for Settings/Exit. Closing the settings window hides it to the tray.

## Start with Windows from the CLI

The UI checkbox is the preferred method, but the CLI remains available:

```powershell
.\surface-pen-map.exe --startup-enable
.\surface-pen-map.exe --startup-disable
```

This only creates/removes one value under the current user's standard Windows `Run` registry key. No service is installed.

## Shortcut limitations

The settings UI uses the native Windows HOTKEY control. It supports ordinary keys with `Ctrl`, `Alt`, and `Shift` modifiers.

Windows-key combinations and protected secure sequences such as `Ctrl+Alt+Delete` are intentionally not supported.

## References

- Microsoft: Supporting Usages in Digitizer Report Descriptors
  - https://learn.microsoft.com/windows-hardware/design/component-guidelines/supporting-usages-in-digitizer-report-descriptors
- Microsoft: Required HID Top-Level Collections for Windows Pen
  - https://learn.microsoft.com/windows-hardware/design/component-guidelines/required-hid-top-level-collections
- Microsoft: Raw Input
  - https://learn.microsoft.com/windows/win32/inputdev/raw-input
- Microsoft: Using Raw Input
  - https://learn.microsoft.com/windows/win32/inputdev/using-raw-input
