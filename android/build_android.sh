#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
if ! command -v gradle >/dev/null 2>&1; then
    echo "ERROR: Gradle 8.13 is required; install it or open android/ in Android Studio." >&2
    exit 2
fi
if [[ ! -f app/src/main/cpp/SDL/CMakeLists.txt ]]; then
    echo "ERROR: SDL2 source is missing. Run: bash fetch_sdl2.sh" >&2
    exit 2
fi
gradle --no-daemon assembleDebug
APK="app/build/outputs/apk/debug/app-debug.apk"
if [[ ! -s "$APK" ]]; then
    echo "ERROR: Expected APK was not produced: $APK" >&2
    exit 3
fi
echo "ANDROID APK BUILD: PASSED"
echo "APK: $SCRIPT_DIR/$APK"
