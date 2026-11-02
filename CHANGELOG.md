## v1.31 — Immersive display and controller readiness

- Hides Android status and navigation bars for an edge-to-edge emulator display.
- Reapplies immersive mode after activity resume and window-focus changes; users can temporarily reveal system bars with an edge swipe.
- Keeps the existing SDL standardized game-controller path for Bluetooth and USB gamepads, including discovery on startup and hot-plug.
- Physical controller testing remains required for the user's specific gamepad and Android device.
- Android version code 41.

## v1.30 — Android Snes9x library lookup fallback

- Tries the absolute path in Android's nativeLibraryDir first, then retries the library soname through the app's linker namespace.
- Captures every attempted path and the actual dynamic-linker error so missing files can be distinguished from missing dependencies or ABI issues.
- Android version code 40. Physical-device launch testing remains required.

## v1.29 — ROM diagnostics and video-format hardening

- Reports the exact missing Libretro symbol when the bundled Snes9x core cannot be bound, replacing an opaque generic loader error.
- Rejects failed ROM reads and files larger than 128 MiB with explicit status messages.
- Supports both Libretro XRGB8888 and RGB565 output and converts RGB565 safely into the frontend's ARGB8888 framebuffer.
- Validates video row pitch before reading pixels, preventing malformed or unexpected core frames from being read out of bounds.
- Host and Android CI validation pending; physical-device Super Mario World gameplay remains the acceptance test.

## v1.27 — Settings and interface completion

- Adds an audio-volume setting (0–100%, 5% per step) applied to Snes9x stereo samples before SDL audio queuing.
- Adds a controller-stick deadzone setting (1000–20000, 500 per step).
- Adds a Reset Defaults control and immediate status feedback.
- Supports settings navigation and adjustment from touch, keyboard, and controller D-pad.
- Rebalances menu-item spacing so all eight sections fit in the metallic console panel.
- CI and physical-device audio/controller verification are pending.

## v1.26 — Snes9x cheat engine bridge

- Binds Libretro's cheat reset and cheat set APIs.
- Supports up to 32 cheat codes per ROM and persists them in a .cht sidecar next to the ROM.
- Adds the Cheats screen, Android/SDL text entry, add action, clear-all action, and active-code count.
- Limits each entry to 128 printable characters; actual code parsing/compatibility is delegated to Snes9x.
- CI and on-device game-specific cheat tests are pending.

## v1.25 — Two-slot save-state manager

- Binds the Libretro serialize-size, serialize, and unserialize functions.
- Adds two save-state slots stored beside the selected ROM.
- Writes state data to a temporary file before replacing a slot and validates state size before restoration.
- Adds save/load controls to the industrial UI with touchscreen, keyboard, and controller navigation.
- CI and on-device save/restore verification are pending.

## v1.24 — Snes9x audio and Android core loading

- Routes Libretro stereo signed-16 audio callbacks into SDL's queued audio device.
- Caps the audio queue at approximately 250 ms to avoid runaway latency.
- Keeps audio initialization optional so an unavailable device does not prevent emulator startup.
- Resolves the core from Android ApplicationInfo.nativeLibraryDir rather than relying on a bare dlopen search name.
- Explicitly selects joypad devices on both Libretro ports.
- Added strict syntax checking for the Libretro adapter and static checks for the audio/native-library path.

## v1.23 — Snes9x backend bring-up

- Added a Libretro adapter that loads the Snes9x core, delivers rendered frames, and forwards two-player joypad input.
- Android CI fetches the Snes9x core for ARM64 and ARMv7 and checks that both native libraries are packaged in the APK.
- Fixed the runtime library name to match the downloaded artifact exactly.
- Added precise unsupported-opcode and 24-bit program address diagnostics when the prototype CPU fallback faults.
- Updated project, Android package, and release metadata to v1.23 (Android versionCode 33).
- CI and physical-device verification remain pending for this release candidate.

## v1.21 - Android interface and touch diagnostics

- Added a visible v1.21 build identifier to the SDL interface.
- Converted mouse and finger input through SDL_RenderWindowToLogical.
- Added an on-screen input diagnostic showing the last input source and logical coordinates.
- Increased Android versionCode to 31 for an in-place upgrade.

# Changelog

## v1.20 — Industrial UI and touch navigation
- Redesigned the SDL interface with brushed-steel panels, bevels, rivets, and red selection accents.
- Reworked the left navigation so menu entries remain selectable from every submenu.
- Added direct SDL finger-touch handling with correct letterbox-aware coordinate mapping.
- Separated synthesized mouse events from native finger events to prevent duplicate taps.
- Bumped Android versionCode/versionName to 30 / 1.20.


## v1.19 — Persistent ROM Folder
- Added an Android **ROM FOLDER** menu option using the Storage Access Framework.
- Persists the user's selected folder read permission across app launches.
- Imports supported ROM files from the selected folder into app-private storage and refreshes the native game library.
- Re-imports the saved folder on subsequent launches when the persisted grant remains valid.

## v1.18 — Android ROM File Picker
- Added a custom SDLActivity subclass with Android Storage Access Framework integration.
- The Load ROM menu item opens the Android document picker for `.sfc`, `.smc`, `.fig`, and `.swc` files.
- Selected ROMs are copied to app-private storage and passed back to the native frontend without broad storage permissions.

## v1.17 — Android Touch Navigation
- Added tap support for main-menu entries, library rows, and back navigation.
- Created an app-private ROM folder on Android startup instead of scanning an unavailable working-directory ROMs folder.
- Added clear on-screen touch guidance while preserving keyboard and Bluetooth-controller navigation.

## v1.16 — Release quality and Android scaffold repair

- Added the missing Android app Gradle module configuration.
- Added a guarded Android debug-build helper that checks Gradle, SDL2 source, and the produced APK instead of reporting a false success.
- Added the missing root Android Gradle plugin declaration.
- Aligned Android package version metadata with v1.16.
- Updated the quality gate to use Python isolated mode (`-S`) so host site-startup hooks cannot contaminate source-check execution.
- Preserved executable permissions on project shell scripts.

### Validation

- Source-tree sanity: passed (31 required files).
- Android scaffold sanity: passed (31 checks).
- Strict C++20 host build: passed.
- Regression suite: passed (190 assertions/checks).
- CTest: passed (1/1).
- SDL frontend syntax check: passed against the project's compile-only SDL API stub.
- ASan/UBSan: skipped because the sanitizer runtime probe was unavailable in this environment.
- Android APK: not built; Gradle, Android SDK/NDK, and fetched SDL2 source are not present in this environment.

This release repairs build/validation infrastructure. It does not claim complete SNES hardware compatibility or a verified Android APK.

## v1.16.2 — Cloud Android build route

- Added a GitHub Actions workflow for host regression checks and a real Android debug APK build.
- Pins Java 21, Gradle 8.13, Android API 36, Build Tools 36.0.0, NDK 27.0.12077973, CMake 3.22.1, and SDL2 2.32.10.
- Verifies the APK archive and confirms ARM64 and ARMv7 native libraries are packaged.
- Uploads the debug APK as a downloadable workflow artifact.
- Documents how to run the workflow when the local development container cannot reach official download hosts.

The cloud workflow is configured but has not yet been run; no APK is claimed by this release.
