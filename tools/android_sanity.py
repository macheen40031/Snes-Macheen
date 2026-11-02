#!/usr/bin/env python3
"""Static validation for the Android/SDL2 project scaffold."""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
errors = []
checks = 0

def require(condition: bool, message: str) -> None:
    global checks
    checks += 1
    if condition:
        print(f"[PASS] {message}")
    else:
        print(f"[FAIL] {message}")
        errors.append(message)

manifest_path = ROOT / "android/app/src/main/AndroidManifest.xml"
try:
    root = ET.parse(manifest_path).getroot()
    ns = "{http://schemas.android.com/apk/res/android}"
    activity = root.find("./application/activity")
    require(root.tag == "manifest", "Android manifest parses as XML")
    require(root.find("./application") is not None, "Android application element exists")
    require(root.find("./application").get(ns + "label") == "Snes-Macheen",
            "Android launcher label is Snes-Macheen")
    require(activity is not None and activity.get(ns + "name") == "com.snesmacheen.SnesMacheenActivity",
            "Launcher uses the custom SDLActivity subclass for Android file picking")
    require(activity is not None and activity.get(ns + "exported") == "true",
            "Android launcher activity is explicitly exported")
    actions = [node.get(ns + "name") for node in activity.findall("./intent-filter/action")] if activity is not None else []
    categories = [node.get(ns + "name") for node in activity.findall("./intent-filter/category")] if activity is not None else []
    require("android.intent.action.MAIN" in actions and "android.intent.category.LAUNCHER" in categories,
            "Android launcher intent filter is configured")
except (ET.ParseError, OSError) as exc:
    require(False, f"Android manifest parses ({exc})")

settings = (ROOT / "android/settings.gradle").read_text(encoding="utf-8")
root_gradle = (ROOT / "android/build.gradle").read_text(encoding="utf-8")
app_gradle = (ROOT / "android/app/build.gradle").read_text(encoding="utf-8")
wrapper = (ROOT / "android/gradle/wrapper/gradle-wrapper.properties").read_text(encoding="utf-8")
require("google()" in settings and "mavenCentral()" in settings,
        "Gradle plugin and dependency repositories are configured")
require("com.android.application" in root_gradle and "8.13.0" in root_gradle,
        "Android Gradle Plugin version is pinned")
require("gradle-8.13-bin.zip" in wrapper,
        "Gradle wrapper distribution matches the Android plugin requirement")
require("compileSdk 36" in app_gradle and "minSdk 23" in app_gradle,
        "Android SDK levels are declared")
require("versionName '1.22'" in app_gradle and "versionCode 32" in app_gradle,
        "Android package version matches Snes-Macheen v1.22")
require("arm64-v8a" in app_gradle and "armeabi-v7a" in app_gradle,
        "64-bit and 32-bit ARM ABIs are declared")
require("src/main/cpp/CMakeLists.txt" in app_gradle,
        "Gradle invokes the native CMake project")

native_cmake = (ROOT / "android/app/src/main/cpp/CMakeLists.txt").read_text(encoding="utf-8")
require('add_library(main SHARED' in native_cmake,
        "Android native entrypoint is built as libmain.so")
require('SDL_android_main.c' in native_cmake and 'target_link_libraries(main PRIVATE SDL2 SDL2main android log)' in native_cmake,
        "Native target includes SDL2 Android glue and platform libraries")
for source in ("cpu/cpu.cpp", "memory/bus.cpp", "cartridge/cartridge.cpp", "ppu/ppu.cpp", "frontend/emulator_session.cpp", "frontend/sdl_frontend.cpp"):
    require(source in native_cmake, f"Android native target includes {source}")

java_bridge = (ROOT / "android/app/src/main/java/com/snesmacheen/SnesMacheenActivity.java").read_text(encoding="utf-8")
require("ACTION_OPEN_DOCUMENT" in java_bridge and "copyRomToPrivateStorage" in java_bridge,
        "Android activity exposes the system ROM picker and private file import")
require("ACTION_OPEN_DOCUMENT_TREE" in java_bridge and "takePersistableUriPermission" in java_bridge,
        "Android activity supports persistent ROM-folder permission")
require("importPersistedRomFolder" in java_bridge and "importRomFilesFromTree" in java_bridge,
        "Android activity restores and imports the saved ROM folder")
require("SDLActivity.onNativeDropFile" in java_bridge,
        "Android activity delivers imported paths through SDL's registered JNI bridge")
fetch = (ROOT / "android/fetch_sdl2.sh").read_text(encoding="utf-8")
require('SDL_VERSION="2.32.10"' in fetch,
        "SDL2 dependency fetch is pinned to a known release")
require("SDLActivity.java" in fetch and "android-project/app/src/main/java/org/libsdl/app" in fetch,
        "SDL2 Android Java/JNI activity glue is fetched")
build_script = (ROOT / "android/build_android.sh").read_text(encoding="utf-8")
require("gradle --no-daemon assembleDebug" in build_script and "app-debug.apk" in build_script,
        "Android build helper verifies and reports the debug APK")

sdl_frontend = (ROOT / "frontend/sdl_frontend.cpp").read_text(encoding="utf-8")
require("callAndroidPickerMethod" in sdl_frontend and "SDL_DROPFILE" in sdl_frontend,
        "SDL frontend handles ROM-picker paths through SDL file-drop events")
require("openRomFolderPicker" in sdl_frontend and "SDL_DROPFILE" in sdl_frontend,
        "SDL frontend handles selected-folder import through SDL file-drop events")
require("SDL_CONTROLLERDEVICEADDED" in sdl_frontend and "SDL_CONTROLLERDEVICEREMOVED" in sdl_frontend,
        "SDL frontend handles controller connection lifecycle")
require("SDL_CONTROLLERAXISMOTION" in sdl_frontend and "SDL_CONTROLLERBUTTONDOWN" in sdl_frontend,
        "SDL frontend handles gamepad buttons and analog axes")
require("emulator.setControllerState" in sdl_frontend,
        "SDL controller mapper publishes input to SNES serial ports")
require("FramePacer" in sdl_frontend and "framesDue" in sdl_frontend,
        "SDL frontend uses bounded host frame pacing")
require("SDL_CreateTexture" in sdl_frontend and "SDL_UpdateTexture" in sdl_frontend and "SDL_RenderCopy" in sdl_frontend,
        "SDL frontend uploads and presents the emulator ARGB framebuffer")
require("emulator.loadRom" in sdl_frontend and "emulator.runFrame" in sdl_frontend,
        "SDL game selection launches and advances an emulator session")
require("drawPanel" in sdl_frontend and "SNES-MACHEEN" in sdl_frontend,
        "SDL frontend contains the industrial menu shell")
require("SDL_MOUSEBUTTONDOWN" in sdl_frontend and "handlePointerTap" in sdl_frontend,
        "SDL frontend accepts touch-generated and mouse pointer taps")
require("SDL_AndroidGetInternalStoragePath" in sdl_frontend and "create_directories(romDirectory" in sdl_frontend,
        "Android startup prepares a private ROM directory")

if errors:
    print(f"ANDROID SCAFFOLD SANITY: FAILED ({len(errors)} failures / {checks} checks)")
    sys.exit(1)
print(f"ANDROID SCAFFOLD SANITY: PASSED ({checks} checks)")

workflow = (ROOT / ".github/workflows/android-apk.yml").read_text(encoding="utf-8")
require("android-actions/setup-android@v4" in workflow and "packages: 'platform-tools'" in workflow,
        "Cloud Android workflow uses the verified Android SDK setup action")
