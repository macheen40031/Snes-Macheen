# Changelog

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
