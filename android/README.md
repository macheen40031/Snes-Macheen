# Snes-Macheen Android / SDL2 shell

This is the first real Android Gradle + NDK project scaffold. It is designed to
package the C++ emulator sources into an Android `libmain.so` and launch the
SDL2 industrial-menu shell through SDL2's own `SDLActivity` and JNI glue.

## What is included now

- Gradle Android application module and launcher manifest.
- CMake native project that compiles the SDL2 frontend plus the existing CPU,
  cartridge, memory bus, PPU, menu model, and controller mapper.
- ARM64 and ARMv7 ABI declarations.
- A dependency setup script for the official SDL2 2.32.10 source archive and
  its `org.libsdl.app` Java activity classes.
- A desktop SDL2 target that can be enabled from the root CMake project with
  `-DSNES_BUILD_SDL2_FRONTEND=ON` when SDL2 development files are installed.

## Prepare SDL2 source

From the repository root:

```bash
bash android/fetch_sdl2.sh
```

The script downloads SDL2 from the official release directory, places the
source at `android/app/src/main/cpp/SDL`, and copies the matching Android Java
activity support files into the app source tree.

## Build the debug APK

Install Android Studio or the Android command-line SDK, an Android NDK,
CMake 3.22.1, and Gradle 8.13. Open the `android/` directory in Android Studio
or run from that directory:

```bash
gradle assembleDebug
```

The expected debug package path is:

`android/app/build/outputs/apk/debug/app-debug.apk`

This repository snapshot was not compiled into an APK in the development
container: Android SDK/NDK and Gradle are not installed here, and the network
is unavailable for fetching SDL2. The quality gate validates the project
structure and runs all host C++ tests, but does not claim an Android APK build.

## Current app-shell limitations

The SDL2 application currently renders the industrial menu, scans a supplied
filesystem ROM directory, and accepts standardized SDL game-controller input.
It does not yet boot the selected ROM, draw the live PPU framebuffer, use the
Android Storage Access Framework picker, persist frontend settings, or provide
an in-app Bluetooth pairing dialog. On Android, pair a controller in Android
Settings first; the app can then receive it if SDL recognizes the device.

SDL's Android port uses a Java Activity/JNI shim to load the SDL library and
native game code. See the official SDL2 Android guide:
https://wiki.libsdl.org/SDL2/README-android
