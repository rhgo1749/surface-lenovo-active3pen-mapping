# surface-lenovo-active3pen-mapping

A tiny native Windows utility for using a two-button active pen as a better mouse replacement on Surface devices.

The initial target is **Surface Pro 12 + Lenovo Active Pen 3 over MPP**. That combination has now been hardware-validated: the lower barrel button reports `Invert (0x3C)` and the top barrel button reports `Barrel (0x44)`.

The mapper deliberately leaves the top barrel button alone so Windows keeps its native right-click behavior. The lower button can be mapped from a small native settings window.

No Wacom tablet driver, virtual tablet driver, .NET runtime, or kernel driver is required.

## Settings UI

Double-click `surface-pen-map.exe` to open the settings window. The mapper continues running in the tray when the window is hidden.

The lower button can currently be mapped to:

- Back (`Mouse XBUTTON1`)
- Forward (`Mouse XBUTTON2`)
- Middle click
- a custom keyboard shortcut using `Ctrl`, `Alt`, and/or `Shift` plus a key
- Disabled

Settings are stored per-user under:

```text
HKCU\Software\SurfaceLenovoActive3PenMapping
```

The UI also has a **Start mapper when I sign in to Windows** checkbox. Startup uses the current executable path with `--background`, so Windows sign-in does not pop the settings window open.

The top barrel button is intentionally not remapped in this version. Raw Input lets us observe its `Barrel (0x44)` signal, but it does not suppress Windows' native right-click path. Remapping it without double actions requires a different interception strategy.

## How it works

Windows pen devices expose a HID Digitizer pen collection. This utility registers for raw input from integrated/external pen collections and watches the standard Digitizer usages used for a second/eraser action:

- `0x3C` — Invert
- `0x45` — Eraser
- `0x5A` — Secondary Barrel Switch

On the rising edge of one of those signals, the mapper emits the configured action with `SendInput`.

It does **not** replace the Surface pen driver and it does **not** suppress the original pen event. That is deliberate: the normal Surface/Windows Ink path remains intact.

## Validated hardware

Surface Pro 12 + Lenovo Active Pen 3 over MPP produced the following diagnostic behavior:

```text
lower barrel button -> 0x3C (Invert)
top barrel button   -> 0x44 (Barrel)
pen tip              -> 0x42 (TipSwitch)
hover                -> 0x32 (InRange)
```

The default Back mapping has also been verified on that hardware.

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

## Diagnose pen signals

Diagnostic mode can run alongside the normal mapper and never emits mapped input:

```powershell
.\surface-pen-map.exe --diagnose
```

Then:

1. bring the pen into hover range
2. press/release the top barrel button
3. press/release the lower button

The console prints the pen HID path, advertised Digitizer button usages, and changes in the active usage set.

## Run modes

Normal launch opens the settings UI:

```powershell
.\surface-pen-map.exe
```

Start silently in the background:

```powershell
.\surface-pen-map.exe --background
```

The older CLI presets remain available for automation/testing:

```powershell
.\surface-pen-map.exe --action=back
.\surface-pen-map.exe --action=forward
.\surface-pen-map.exe --action=middle
.\surface-pen-map.exe --action=none
```

A tray icon is shown while the mapper is running. Left-click it to open Settings; right-click it for Settings/Exit. Closing the settings window hides it to the tray.

To run without the tray icon:

```powershell
.\surface-pen-map.exe --no-tray
```

## Start with Windows from the CLI

The UI checkbox is the preferred method, but the CLI remains available:

```powershell
.\surface-pen-map.exe --startup-enable
.\surface-pen-map.exe --startup-disable
```

This only creates/removes one value under the current user's standard Windows `Run` registry key. No service is installed.

## Shortcut limitations

The first settings UI uses the native Windows HOTKEY control. It supports ordinary keys with `Ctrl`, `Alt`, and `Shift` modifiers.

Windows-key combinations and protected secure sequences such as `Ctrl+Alt+Delete` are intentionally not supported yet.

## References

- Microsoft: Required HID Top-Level Collections for Windows Pen
  - https://learn.microsoft.com/windows-hardware/design/component-guidelines/required-hid-top-level-collections
- Microsoft: Raw Input
  - https://learn.microsoft.com/windows/win32/inputdev/raw-input
- Microsoft: Using Raw Input
  - https://learn.microsoft.com/windows/win32/inputdev/using-raw-input
