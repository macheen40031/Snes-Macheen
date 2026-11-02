# Snes-Macheen v1.29

Snes-Macheen is an Android/desktop SNES emulator frontend with an industrial console UI. Version 1.23 introduced the Libretro Snes9x backend for Android. On Android, Snes9x is now the only gameplay backend; the incomplete in-development C++ core is never used as a fallback for retail cartridges.

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

## v1.30 Android core lookup fallback

- Attempts the app-private `nativeLibraryDir` absolute path and then the Android linker soname lookup for the bundled Snes9x core.
- Reports each attempted path and the actual dynamic-linker error rather than a vague missing-library message.
- Physical-device verification is still required; the APK must be installed and Super Mario World launched on Android.

## v1.29 ROM and video compatibility hardening

- Reports the exact missing Libretro entry point when Snes9x cannot be loaded.
- Detects ROM read errors and applies a 128 MiB safety limit.
- Accepts both XRGB8888 and RGB565 Libretro video output and converts frames to the SDL ARGB8888 format.
- Validates each frame's row pitch before reading pixel data.
- These changes improve compatibility and diagnostics but do not replace the required physical-device Super Mario World test.

## v1.28 compatibility milestone

- Makes Android ROM launch fail closed when the bundled Snes9x core cannot be loaded or rejects the selected cartridge.
- Displays the actual Snes9x loader error instead of launching the incomplete prototype CPU and showing an unrelated unsupported-opcode fault.
- Keeps the prototype fallback available only in desktop development builds.
- This is a diagnostic/selection fix, not yet proof that a physical-device Super Mario World session works.

## v1.30–v1.32 compatibility work

- v1.31: verify ROM import, copier-header handling, cartridge metadata, and LoROM/HiROM detection against legally dumped test images.
- v1.30: validate Libretro video pixel format, pitch, framebuffer resizing, aspect ratio, and first-frame presentation.
- v1.31: validate SNES controller mapping, frame pacing, audio synchronization, SRAM persistence, and state restore.
- v1.32: add repeatable compatibility checks and a physical-device acceptance run using a legally obtained Super Mario World dump. No copyrighted ROM is stored in the repository or CI artifacts.

## Build and test

Run `bash quality.sh` for host source checks, strict compilation, regression tests, and SDL frontend syntax checks. The Android workflow builds the APK with SDL2 and bundled Snes9x cores.

## Important limitations

The Snes9x runtime is the intended gameplay backend. Audio output, save-state UI, and several settings controls remain follow-up work. A successful source change or CI build does not by itself confirm gameplay on a physical Android device; test with a legally obtained ROM and verify video, audio, controls, and battery saves on-device.
