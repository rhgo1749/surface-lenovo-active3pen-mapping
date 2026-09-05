# Troubleshooting

Most users should not need this page.

## The app is running but nothing happens

1. Make sure the pen is in hover range.
2. Open the settings window and check whether **Last input** changes when you use a side button.
3. Confirm the gesture is not set to **No extra action**.
4. If an older copy is still running, close it from the tray or run:

```cmd
taskkill /IM surface-pen-map.exe /F
```

Then launch the new copy again.

## A hold + tap action also opens the right-click menu

This can be normal. Surface/Windows may keep its native pen behavior while this utility adds the configured action.

The upper side button + pen-tip gesture is commonly interpreted by Windows as a right-click. Some ink apps may also keep native eraser behavior for the lower button.

## A key or shortcut does not work

- Click the key field first, then press the desired key or shortcut.
- `Enter`, `Esc`, `Tab`, arrows, F-keys and modifier combinations are supported.
- `Ctrl+Alt+Delete` cannot be generated because Windows reserves it as a secure-attention sequence.

## Advanced device check

Only use this when the mapper does not recognize your pen at all:

```cmd
surface-pen-map.exe --diagnose
```

Put the pen in hover range and try both side buttons and both hold+tap gestures. The console shows the HID signals received from the pen.

Validated hardware:

```text
Surface Pro 12 + Lenovo Active Pen 3 (MPP)
upper side button -> Barrel 0x44
lower side button -> Invert 0x3C
pen tip           -> TipSwitch 0x42
```

If another MPP pen reports different button signals, open a GitHub issue and include the diagnostic output.