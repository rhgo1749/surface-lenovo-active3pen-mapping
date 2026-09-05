# Surface Pen Mapper — User Guide

[한국어](README.ko.md) · [日本語](README.ja.md) · [简体中文](README.zh-CN.md)

Surface Pen Mapper is a tiny Windows utility that turns **two pen side buttons into four configurable gestures**.

It has been hardware-tested with **Surface Pro 12 + Lenovo Active Pen 3 (MPP)**.

## 1. Install

1. Open GitHub **Releases** and download the latest `surface-pen-map-arm64.zip`.
2. Extract the ZIP anywhere you like.
3. Run `surface-pen-map.exe`.

There is no installer and no separate Wacom tablet driver is required.

Windows SmartScreen may ask you to confirm the first launch. Make sure the file came from this repository's GitHub Releases page before running it.

## 2. Configure the pen

Each side button supports two different gestures:

| Gesture | How to perform it |
| --- | --- |
| Upper button click | Press and release without touching the screen |
| Upper button + tap | Hold the upper button, then tap the screen with the pen tip |
| Lower button click | Press and release without touching the screen |
| Lower button + tap | Hold the lower button, then tap the screen with the pen tip |

Each gesture can independently be assigned to:

- Back / Forward
- Left / Right / Middle click
- Any supported key or shortcut
- No extra action

## 3. Assign a key or shortcut

Choose **Key / shortcut**, click the field on the right, then press the key you want.

Examples:

- `Enter`
- `Esc`
- `Tab`
- `Space`
- `Delete`
- Arrow keys
- `F1`–`F24`
- `Ctrl+Z`
- `Ctrl+Shift+T`
- `Alt+Left`
- `Win+D`

`Ctrl+Alt+Delete` cannot be mapped because Windows reserves it as a secure-attention sequence.

## 4. Example setup

For mouse-free Windows use, this is a useful starting point:

| Gesture | Suggested action |
| --- | --- |
| Upper button click | Enter |
| Upper button + tap | No extra action |
| Lower button click | Back |
| Lower button + tap | Esc |

Change the four slots however you like for your workflow.

## 5. Apply and start with Windows

Press **Apply** after changing a setting. The new mapping becomes active immediately.

Enable **Start mapper when I sign in to Windows** if you want it to launch automatically.

Closing the settings window hides the app to the system tray instead of quitting it. Click the tray icon to open settings again.

## Good to know

The mapper adds actions; it does not replace the Windows pen driver.

In particular, **holding the upper button while tapping the screen may still trigger Windows' native right-click behavior as well**. Ink apps may also keep their native eraser behavior for the lower button.

Only if something is not working, see [Troubleshooting](TROUBLESHOOTING.md).