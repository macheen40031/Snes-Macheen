# Snes-Macheen v1.27

Snes-Macheen is an Android/desktop SNES emulator frontend with an industrial console UI. Version 1.23 introduces the Libretro Snes9x backend for Android, while retaining the in-development C++ core as a diagnostic fallback.

## v1.24 changes

- Connects Snes9x stereo audio callbacks to SDL queued audio.
- Bounds queued audio to approximately 250 ms and drops excess samples rather than allowing latency to grow without limit.
- Treats audio-device initialization as optional so devices without an available audio driver can still launch the emulator UI.
- Resolves the bundled Snes9x core through Android's native-library directory.
- Configures both Libretro controller ports as joypads.

## v1.27 changes

- Adds a runtime audio-volume control that scales Snes9x stereo samples before they enter SDL's audio queue.
- Adds a controller analog-stick deadzone adjustment, bounded to a sensible operating range.
- Adds a Reset Defaults action.
- Supports adjusting settings with touch, keyboard arrows, and a gamepad D-pad.
- Reduces main-menu row spacing so all eight sections fit in the industrial chassis layout.

## v1.26 changes

- Adds the Libretro cheat reset/set bridge to Snes9x.
- Supports up to 32 per-ROM cheat codes, persisted in a .cht sidecar.
- Adds an on-screen cheat manager with text entry, add, and clear-all actions.
- Accepts printable codes up to 128 characters; the Snes9x core remains responsible for interpreting code syntax.

## v1.25 changes

- Adds two save-state slots using the Snes9x Libretro serialize/unserialize interface.
- Saves state files beside the selected ROM, validates their size against the active core, and writes through a temporary file before replacement.
- Exposes Save Slot 1, Load Slot 1, Save Slot 2, and Load Slot 2 in the metal UI with keyboard and controller selection.

## v1.23 changes

- Adds a Libretro adapter for Snes9x with video-frame delivery and two-player joypad input.
- Android CI fetches the Snes9x Libretro core for ARM64 and ARMv7 and verifies both libraries are packaged in the APK.
- Fixes the runtime core filename so it matches the library downloaded and packaged by CI.
- Reports the exact unsupported 65C816 opcode and program-bank address when the prototype fallback faults.
- Preserves the existing ROM picker, persistent ROM folder, controller navigation, and industrial interface.

## Build and test

Run `bash quality.sh` for host source checks, strict compilation, regression tests, and SDL frontend syntax checks. The Android workflow builds the APK with SDL2 and bundled Snes9x cores.

## Important limitations

The Snes9x runtime is the intended gameplay backend. Audio output, save-state UI, and several settings controls remain follow-up work. A successful source change or CI build does not by itself confirm gameplay on a physical Android device; test with a legally obtained ROM and verify video, audio, controls, and battery saves on-device.
