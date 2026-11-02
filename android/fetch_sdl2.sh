#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANDROID_DIR="$SCRIPT_DIR"
CPP_DIR="$ANDROID_DIR/app/src/main/cpp"
SDL_VERSION="2.32.10"
SDL_DEST="$CPP_DIR/SDL"
JAVA_DEST="$ANDROID_DIR/app/src/main/java/org/libsdl/app"

if [[ -f "$SDL_DEST/CMakeLists.txt" && -f "$JAVA_DEST/SDLActivity.java" ]]; then
    echo "SDL2 $SDL_VERSION already appears to be installed."
    exit 0
fi

command -v tar >/dev/null 2>&1 || { echo "Missing required tool: tar" >&2; exit 2; }
if command -v curl >/dev/null 2>&1; then
    DOWNLOADER=(curl -fL --retry 2 --connect-timeout 20)
elif command -v wget >/dev/null 2>&1; then
    DOWNLOADER=(wget -O)
else
    echo "Install curl or wget, then run this script again." >&2
    exit 2
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT
ARCHIVE="$TMP_DIR/SDL2-$SDL_VERSION.tar.gz"
URL="https://www.libsdl.org/release/SDL2-$SDL_VERSION.tar.gz"
echo "Downloading official SDL2 source: $URL"
if [[ "${DOWNLOADER[0]}" == "curl" ]]; then
    "${DOWNLOADER[@]}" "$URL" -o "$ARCHIVE"
else
    wget -O "$ARCHIVE" "$URL"
fi

tar -xzf "$ARCHIVE" -C "$TMP_DIR"
SDL_SOURCE="$TMP_DIR/SDL2-$SDL_VERSION"
[[ -f "$SDL_SOURCE/CMakeLists.txt" ]] || { echo "SDL2 source archive layout was unexpected." >&2; exit 3; }
[[ -f "$SDL_SOURCE/android-project/app/src/main/java/org/libsdl/app/SDLActivity.java" ]] || {
    echo "SDL2 Android Activity source is missing from the archive." >&2; exit 3;
}

rm -rf "$SDL_DEST"
mkdir -p "$SDL_DEST" "$JAVA_DEST"
cp -a "$SDL_SOURCE/." "$SDL_DEST/"
cp -a "$SDL_SOURCE/android-project/app/src/main/java/org/libsdl/app/." "$JAVA_DEST/"
echo "SDL2 $SDL_VERSION source and Android Java glue installed."
echo "Next: install Android SDK/NDK + Gradle, then run gradle assembleDebug from: $ANDROID_DIR"
