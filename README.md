# surface-lenovo-active3pen-mapping

A tiny native Windows utility for using a two-button active pen as a better mouse replacement on Surface devices.

The initial target is **Surface Pro 12 + Lenovo Active Pen 3 over MPP**.

The default behavior is intentionally simple:

- normal/top barrel button: left alone so Windows keeps its native right-click behavior
- lower/eraser-side button: remapped to **Mouse XBUTTON1 / Back**

No Wacom tablet driver, virtual tablet driver, .NET runtime, or kernel driver is required.

## How it works

Windows pen devices expose a HID Digitizer pen collection. This utility registers for raw input from integrated/external pen collections and watches the standard Digitizer usages used for a second/eraser action:

- `0x3C` — Invert
- `0x45` — Eraser
- `0x5A` — Secondary Barrel Switch

On the rising edge of one of those signals, the mapper emits one mouse action with `SendInput`.

It does **not** replace the Surface pen driver and it does **not** suppress the original pen event. That is deliberate: the normal Surface/Windows Ink path remains intact.

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

## Diagnose the actual pen signal first

Because the exact HID representation is determined by the Surface digitizer + pen protocol path, the utility includes a diagnostic mode instead of assuming that every two-button pen reports the lower button identically.

```powershell
.\surface-pen-map.exe --diagnose
```

Then:

1. bring the pen into hover range
2. press/release the top barrel button
3. press/release the lower button

The console prints the pen HID path, advertised Digitizer button usages, and changes in the active usage set. Typical output should include `Barrel`, `Invert`, `Eraser`, or `SecondaryBarrel`.

Diagnostic mode never emits mouse clicks.

## Run

Default: lower button -> browser/file-manager Back (`Mouse XBUTTON1`).

```powershell
.\surface-pen-map.exe
```

Other mappings:

```powershell
.\surface-pen-map.exe --action=forward
.\surface-pen-map.exe --action=middle
.\surface-pen-map.exe --action=none
```

Available actions:

- `back` -> `XBUTTON1`
- `forward` -> `XBUTTON2`
- `middle` -> middle mouse click
- `none` -> observe nothing / emit nothing

A small tray icon is shown while the mapper is running. Right-click it to exit.

To run without the tray icon:

```powershell
.\surface-pen-map.exe --no-tray
```

## Start with Windows

Enable startup for the current executable path:

```powershell
.\surface-pen-map.exe --startup-enable
```

Choose another action while enabling startup:

```powershell
.\surface-pen-map.exe --startup-enable --action=middle
```

Disable startup:

```powershell
.\surface-pen-map.exe --startup-disable
```

This only creates/removes one value under the current user's standard Windows `Run` registry key. No service is installed.

## Current status

The implementation is designed around Microsoft's standard Windows Pen HID/Raw Input contracts and compiles as a native ARM64 Win32 application.

**Hardware validation on the specific Surface Pro 12 + Lenovo Active Pen 3 combination is still required.** The first useful check is `--diagnose`: if the lower button produces `Invert`, `Eraser`, or `SecondaryBarrel`, the default mapper path should trigger.

If the lower button appears under a different HID usage on this hardware, the diagnostic output gives us the exact value to add instead of guessing.

## References

- Microsoft: Required HID Top-Level Collections for Windows Pen
  - https://learn.microsoft.com/windows-hardware/design/component-guidelines/required-hid-top-level-collections
- Microsoft: Raw Input
  - https://learn.microsoft.com/windows/win32/inputdev/raw-input
- Microsoft: Using Raw Input
  - https://learn.microsoft.com/windows/win32/inputdev/using-raw-input
