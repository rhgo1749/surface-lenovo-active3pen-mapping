# Surface Pen Mapper

A tiny native Windows utility that turns a two-button active pen into a more useful mouse/shortcut controller.

**Tested hardware:** Surface Pro 12 + Lenovo Active Pen 3 (MPP)

> Other Windows MPP pens may also work. If you try another device, feedback is welcome.

## Guides

- [한국어](docs/README.ko.md)
- [English](docs/README.en.md)
- [日本語](docs/README.ja.md)
- [简体中文](docs/README.zh-CN.md)

## Download

For normal users, download `surface-pen-map-arm64.zip` from **GitHub Releases**, extract it, and run `surface-pen-map.exe`.

> If there is no Release yet, the project is still in pre-release testing. Maintainers can publish one by pushing a version tag such as `v0.1.0`.

No installer, Wacom tablet driver, .NET runtime, or kernel driver is required.

## What you can map

A two-button pen provides four independent gestures:

| Gesture | Example |
| --- | --- |
| Upper button click | Enter |
| Upper button + tap screen | Right click / shortcut |
| Lower button click | Back |
| Lower button + tap screen | Esc |

Each gesture can be assigned to Back, Forward, left/right/middle click, a key or shortcut, or no extra action.

Keys such as **Enter, Esc, Tab, Space, Delete, arrow keys, F1–F24**, and combinations using **Ctrl / Alt / Shift / Win** are supported.

## Notes

- Closing the settings window hides the mapper to the system tray.
- Enable **Start with Windows** in the app if you want it to run automatically.
- Windows may still keep its native pen behavior for some hold+tap gestures (for example the upper-button right-click behavior).

For uncommon device problems, see [Troubleshooting](docs/TROUBLESHOOTING.md).

## Build

```powershell
cmake -S . -B build -A ARM64
cmake --build build --config Release
```

The project is intentionally kept as a small native Win32 executable.