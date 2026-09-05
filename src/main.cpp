// The native mapper is split into focused implementation fragments so the Win32
// UI, HID input path, and tray/runtime code stay reviewable without changing
// the single-translation-unit build model.
#include "main_parts/01_prelude.inc"
#include "main_parts/02_runtime.inc"
#include "main_parts/03_window.inc"
#include "main_parts/04_controls.inc"
#include "main_parts/05_visuals.inc"
#include "main_parts/06_input.inc"
#include "main_parts/07_tray.inc"
#include "main_parts/08_entry.inc"
