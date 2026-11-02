#!/usr/bin/env bash
# Install a reproducible Android command-line build toolchain in the current user's HOME.
# Requires outbound access to services.gradle.org and dl.google.com.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="${ANDROID_HOME:-$HOME/android-sdk}"
TOOLS="$SDK/cmdline-tools/latest"
GRADLE_VERSION="8.13"
NDK_VERSION="27.0.12077973"
CMAKE_VERSION="3.22.1"
PLATFORM="android-36"
BUILD_TOOLS="36.0.0"
CMDLINE_ZIP="commandlinetools-linux-11076708_latest.zip"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

die() { echo "ERROR: $*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || die "Missing prerequisite: $1"; }

need java
need curl
need unzip
need tar
need sha256sum

JAVA_MAJOR="$(java -version 2>&1 | sed -n '1s/.*version "\([0-9]*\).*/\1/p')"
[[ -n "$JAVA_MAJOR" && "$JAVA_MAJOR" -ge 17 ]] || die "JDK 17+ required; found Java major ${JAVA_MAJOR:-unknown}."

mkdir -p "$SDK/cmdline-tools"
if [[ ! -x "$TOOLS/bin/sdkmanager" ]]; then
    echo "Downloading Android command-line tools..."
    curl --fail --location --retry 2 --connect-timeout 20 \
      "https://dl.google.com/android/repository/$CMDLINE_ZIP" \
      -o "$TMP/$CMDLINE_ZIP"
    mkdir -p "$TMP/cmdline-tools"
    unzip -q "$TMP/$CMDLINE_ZIP" -d "$TMP"
    [[ -x "$TMP/cmdline-tools/bin/sdkmanager" ]] || die "Unexpected command-line tools archive layout."
    rm -rf "$TOOLS"
    mv "$TMP/cmdline-tools" "$TOOLS"
fi

export ANDROID_HOME="$SDK"
export PATH="$TOOLS/bin:$SDK/platform-tools:$PATH"

echo "Installing Android SDK platform, build tools, NDK and CMake..."
yes | sdkmanager --sdk_root="$SDK" --licenses >/dev/null
sdkmanager --sdk_root="$SDK" --install \
  "platform-tools" \
  "platforms;$PLATFORM" \
  "build-tools;$BUILD_TOOLS" \
  "ndk;$NDK_VERSION" \
  "cmake;$CMAKE_VERSION"

GRADLE_HOME="$HOME/.local/gradle/gradle-$GRADLE_VERSION"
if [[ ! -x "$GRADLE_HOME/bin/gradle" ]]; then
    mkdir -p "$HOME/.local/gradle"
    echo "Downloading Gradle $GRADLE_VERSION..."
    curl --fail --location --retry 2 --connect-timeout 20 \
      "https://services.gradle.org/distributions/gradle-$GRADLE_VERSION-bin.zip" \
      -o "$TMP/gradle.zip"
    unzip -q "$TMP/gradle.zip" -d "$HOME/.local/gradle"
fi
export PATH="$GRADLE_HOME/bin:$PATH"

cat > "$ROOT/android/local.properties" <<EOF
sdk.dir=$SDK
EOF

echo
echo "=== Installed toolchain ==="
java -version
gradle --version | sed -n '1,12p'
sdkmanager --sdk_root="$SDK" --list_installed | grep -E 'platforms;android-36|build-tools;36.0.0|ndk;$NDK_VERSION|cmake;$CMAKE_VERSION|platform-tools' || true
echo
echo "=== Fetching SDL2 and building debug APK ==="
bash "$ROOT/android/fetch_sdl2.sh"
cd "$ROOT/android"
gradle --no-daemon assembleDebug
APK="$ROOT/android/app/build/outputs/apk/debug/app-debug.apk"
[[ -s "$APK" ]] || die "Gradle finished without producing the expected APK."
echo "ANDROID APK BUILD: PASSED"
echo "APK: $APK"
