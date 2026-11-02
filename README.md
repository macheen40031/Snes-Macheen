# Snes-Macheen v1.15

Original C++ SNES emulator development project. This milestone extends the existing CPU/cartridge/PPU prototype and SDL2/Android application scaffold.

## v1.15 changes

- Added a platform-neutral host FramePacer with NTSC (~60.0988 Hz) and PAL (~50.0070 Hz) presentation intervals.
- Added fractional-time accumulation across updates.
- Added bounded catch-up work after long stalls and dropped-frame diagnostics.
- Integrated paced SDL UI/emulator frame updates and deadline-based SDL_Delay.
- Explicitly kept host presentation pacing separate from cycle-accurate SNES CPU/PPU timing.

## Build and test

```bash
./build.sh
./test.sh
./quality.sh
```

The quality gate performs source-tree checks, strict clean compilation, regression tests, SDL frontend syntax validation against a test-only SDL API stub, Android project structure checks, and AddressSanitizer/UndefinedBehaviorSanitizer runs when supported.

## Platform verification

The portable C++ host build and test suite were run successfully for this release. A real SDL2 link and Android APK/Gradle build were **not run** in this environment because the Android SDK/NDK, Gradle toolchain, and SDL2 source dependency were not installed. The package contains source code and build setup, not an installable APK.

## Compatibility limits

This remains an early emulator prototype, not a broadly compatible SNES game player. CPU instruction coverage, PPU modes/timing, audio/SPC700, DMA, interrupts, accurate memory mapping, and full controller timing are incomplete. The host frame pacer, where present, paces presentation only and does not make CPU/PPU emulation cycle-accurate.


## Android v1.17 notes
The Android app now creates its private `files/roms` directory on launch and accepts touchscreen taps on the main menu and visible library entries. ROM import through Android's system file picker is being added in the next release.


## Android v1.18 ROM import
Choose **Load ROM** to open Android's system document picker. Supported `.sfc`, `.smc`, `.fig`, and `.swc` files are copied into the app-private `files/roms` directory, then the selected title is opened by the current emulator session. No broad shared-storage permission is requested.


## Android v1.19 persistent ROM library
Choose **ROM FOLDER** from the main menu and select a folder that directly contains your `.sfc`, `.smc`, `.fig`, or `.swc` files. Android grants the app persistent read access to that folder. Snes-Macheen copies supported ROMs into app-private storage and refreshes the library. On later launches it re-imports from the saved folder permission, so the library can be rebuilt without asking you to select the folder again. This release intentionally reads files directly inside the selected folder; it does not recurse through nested folders.
