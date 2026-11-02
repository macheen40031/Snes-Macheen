# Cloud Android APK build (network-restricted development hosts)

This project includes `.github/workflows/android-apk.yml`. It uses a GitHub-hosted
Ubuntu runner, which has outbound access to the official Gradle and Android
package servers. It runs the host regression/quality gate and independently
builds the Android debug APK for ARM64 and ARMv7.

## Run it

1. Create a GitHub repository for Snes-Macheen.
2. Extract this project ZIP and upload/push the **contents** of the `Snes-Macheen`
   folder to the repository root (the `.github/workflows/` folder must be present).
3. In the repository, open **Actions** and enable workflows if GitHub asks.
4. Select **Snes-Macheen Android APK**, choose **Run workflow**, and start it.
5. Open the completed workflow run. Both `Host regression and strict build` and
   `Build Android APK (ARM64 + ARMv7)` must be green.
6. Download the `Snes-Macheen-debug-APK` artifact from the run's Artifacts area.

The workflow fetches Gradle 8.13, Android SDK Platform 36, Build Tools 36.0.0,
NDK 27.0.12077973, CMake 3.22.1, and SDL2 2.32.10. It verifies the APK archive
and confirms both native ABI libraries are packaged.

## Why use this route?

The current ChatGPT-hosted development container has DNS/network restrictions
that prevent reaching `services.gradle.org` and `dl.google.com`. GitHub-hosted
runners perform those downloads outside this restricted container. This is a
real build workflow, not a claim that an APK has already been built here.

The debug APK is for development/testing. A signed release APK/AAB and Play
Store publishing configuration are separate release steps.
