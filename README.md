# Snes-Macheen v1.24

Snes-Macheen is an Android/desktop SNES emulator frontend with an industrial console UI. Version 1.23 introduces the Libretro Snes9x backend for Android, while retaining the in-development C++ core as a diagnostic fallback.

## v1.24 changes

- Connects Snes9x stereo audio callbacks to SDL queued audio.
- Bounds queued audio to approximately 250 ms and drops excess samples rather than allowing latency to grow without limit.
- Treats audio-device initialization as optional so devices without an available audio driver can still launch the emulator UI.
- Resolves the bundled Snes9x core through Android's native-library directory.
- Configures both Libretro controller ports as joypads.

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
