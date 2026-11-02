# Reproducible Android build environment

The project uses Android Gradle Plugin 8.13.0, which requires Gradle 8.13.
The bootstrap script installs a user-local toolchain without root privileges:

```bash
bash tools/bootstrap_android_build_env.sh
```

Pinned components:

- Java: 17 or newer (Java 21 is suitable)
- Gradle: 8.13
- Android SDK Platform: 36
- Android SDK Build Tools: 36.0.0
- Android NDK: 27.0.12077973
- Android CMake: 3.22.1
- SDL2: 2.32.10 (downloaded by `android/fetch_sdl2.sh`)

The script installs under `$HOME/android-sdk` and `$HOME/.local/gradle`, accepts SDK licenses, fetches SDL2, and attempts a real debug APK build. It requires outbound network access to `dl.google.com` and `services.gradle.org`. It writes `android/local.properties` locally; that machine-specific file must not be committed or shipped in source archives.

If the build host has no outbound network, run the script later in an environment that can reach those official download hosts. A failed DNS/network check is not an installation and must not be treated as an APK build.
