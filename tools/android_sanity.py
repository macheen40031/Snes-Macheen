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
require("versionName '1.31'" in app_gradle and "versionCode 41" in app_gradle,
        "Android package version matches Snes-Macheen v1.31")
require("arm64-v8a" in app_gradle and "armeabi-v7a" in app_gradle,
        "64-bit and 32-bit ARM ABIs are declared")
require("src/main/cpp/CMakeLists.txt" in app_gradle,
        "Gradle invokes the native CMake project")

native_cmake = (ROOT / "android/app/src/main/cpp/CMakeLists.txt").read_text(encoding="utf-8")
require('add_library(main SHARED' in native_cmake,
        "Android native entrypoint is built as libmain.so")
require('SDL_android_main.c' in native_cmake and 'target_link_libraries(main PRIVATE SDL2 SDL2main android log dl)' in native_cmake,
        "Native target includes SDL2 Android glue and platform libraries")
for source in ("cpu/cpu.cpp", "memory/bus.cpp", "cartridge/cartridge.cpp", "ppu/ppu.cpp", "frontend/emulator_session.cpp", "frontend/libretro_session.cpp", "frontend/sdl_frontend.cpp"):
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
require("SDL_IsGameController(deviceIndex)" in sdl_frontend and "SDL_GameControllerOpen(deviceIndex)" in sdl_frontend,
        "SDL opens recognized Bluetooth/USB game controllers through the standardized gamepad API")
require("SDL_NumJoysticks()" in sdl_frontend and "SDL_CONTROLLERDEVICEADDED" in sdl_frontend,
        "SDL discovers controllers already paired and controllers connected while the emulator is running")
require("emulator.setControllerState" in sdl_frontend,
        "SDL controller mapper publishes input to SNES serial ports")
require("FramePacer" in sdl_frontend and "framesDue" in sdl_frontend,
        "SDL frontend uses bounded host frame pacing")
require("SDL_CreateTexture" in sdl_frontend and "SDL_UpdateTexture" in sdl_frontend and "SDL_RenderCopy" in sdl_frontend,
        "SDL frontend uploads and presents the emulator ARGB framebuffer")
require("emulator.loadRom" in sdl_frontend and "emulator.runFrame" in sdl_frontend,
        "SDL game selection launches and advances an emulator session")
require("#ifdef __ANDROID__" in sdl_frontend and "Never send retail cartridges into it" in sdl_frontend,
        "Android refuses the incomplete prototype CPU fallback when Snes9x fails")
require("drawPanel" in sdl_frontend and "SNES-MACHEEN" in sdl_frontend,
        "SDL frontend contains the industrial menu shell")
require("SDL_MOUSEBUTTONDOWN" in sdl_frontend and "handlePointerTap" in sdl_frontend,
        "SDL frontend accepts touch-generated and mouse pointer taps")
require("SDL_AndroidGetInternalStoragePath" in sdl_frontend and "create_directories(romDirectory" in sdl_frontend,
        "Android startup prepares a private ROM directory")
require("WindowInsetsController" in java_bridge and "WindowInsets.Type.statusBars()" in java_bridge and "WindowInsets.Type.navigationBars()" in java_bridge,
        "Android activity hides status and navigation bars using modern window insets")
require("BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE" in java_bridge and "SYSTEM_UI_FLAG_IMMERSIVE_STICKY" in java_bridge,
        "Immersive full-screen mode remains active after focus changes on modern and older Android")
require("applyImmersiveMode();" in java_bridge and "onWindowFocusChanged" in java_bridge and "onResume()" in java_bridge,
        "Android reapplies immersive mode on launch, resume, and window focus")

libretro_frontend = (ROOT / "frontend/sdl_frontend.cpp").read_text(encoding="utf-8")
require('snes9x.loadRom("snes9x_libretro_android.so"' in libretro_frontend,
        "Frontend loads the exact Snes9x core filename packaged by Android CI")
workflow = (ROOT / ".github/workflows/android-apk.yml").read_text(encoding="utf-8")
require("snes9x_libretro_android.so.zip" in workflow and
        "snes9x_libretro_android.so" in workflow,
        "Android workflow fetches and verifies the Snes9x core for both ABIs")

libretro_source = (ROOT / "frontend/libretro_session.cpp").read_text(encoding="utf-8")
require("SDL_OpenAudioDevice" in libretro_source and "SDL_QueueAudio" in libretro_source,
        "Snes9x Libretro audio callback is connected to SDL queued stereo output")
require("SDL_GetQueuedAudioSize" in libretro_source and "maxQueuedBytes" in libretro_source,
        "Snes9x audio buffering is bounded to prevent runaway latency")
require("androidNativeLibraryPath" in libretro_source and "nativeLibraryDir" in libretro_source,
        "Android Snes9x core is resolved from the app native-library directory")
require("tryLoadCore(resolvedCorePath)" in libretro_source and "tryLoadCore(corePath)" in libretro_source,
        "Android Snes9x loader retries the library soname after the absolute path")
require("loaderErrors" in libretro_source and "Snes9x core could not be opened" in libretro_source,
        "Snes9x loader reports each attempted path and its dynamic-linker error")
require("missing Libretro function:" in libretro_source and "missingSymbol" in libretro_source,
        "Snes9x loader reports the exact missing Libretro entry point")
require("kMaximumRomBytes" in libretro_source and "file.bad()" in libretro_source,
        "ROM loader checks I/O failures and rejects oversized files safely")
require("kPixelRgb565" in libretro_source and "minimumPitch" in libretro_source,
        "Libretro video accepts RGB565 and validates row pitch")
require("std::memcpy" in libretro_source and "red * 255u" in libretro_source,
        "Libretro RGB565 pixels are safely expanded into ARGB8888")
require("setControllerPortDevice(0, kDeviceJoypad)" in libretro_source and
        "setControllerPortDevice(1, kDeviceJoypad)" in libretro_source,
        "Libretro core ports are explicitly configured as joypads")

require("retro_serialize_size" in libretro_source and "retro_serialize" in libretro_source and
        "retro_unserialize" in libretro_source,
        "Snes9x core serialization functions are bound for save states")
sdl_frontend = (ROOT / "frontend/sdl_frontend.cpp").read_text(encoding="utf-8")
require("SAVE SLOT 1" in sdl_frontend and "LOAD SLOT 1" in sdl_frontend and
        "SAVE SLOT 2" in sdl_frontend and "LOAD SLOT 2" in sdl_frontend,
        "SDL saves screen exposes two save-state slots")
require("performSaveAction" in sdl_frontend and "GamepadControl::DpadUp" in sdl_frontend,
        "Save-state actions are reachable from keyboard, touch and controller navigation")

require("retro_cheat_set" in libretro_source and "retro_cheat_reset" in libretro_source,
        "Libretro cheat set/reset API is bound to the Snes9x core")
require('romPath + ".cht"' in libretro_source and "currentCheats.size() < 32u" in libretro_source,
        "Cheat codes are restored from a per-ROM sidecar with a bounded slot count")
require("SDL_TEXTINPUT" in sdl_frontend and "SDL_StartTextInput" in sdl_frontend,
        "Cheat entry accepts text input from the SDL keyboard/Android soft keyboard")
require('"CHEAT ENGINE"' in sdl_frontend and '"ADD CHEAT"' in sdl_frontend and
        '"CLEAR ALL"' in sdl_frontend,
        "Industrial UI exposes cheat entry and clear-all controls")

require("setAudioVolumePercent" in libretro_source and "volumePercent" in libretro_source,
        "Snes9x audio volume adjustment is available at runtime")
require("audioVolumePercent + direction * 5" in sdl_frontend and
        "SDL_QueueAudio" in libretro_source,
        "Settings expose an audio volume adjustment connected to the core audio queue")
require("mapper.setDeadzone" in sdl_frontend and "controllerDeadzone + direction * 500" in sdl_frontend,
        "Settings expose a bounded controller-stick deadzone adjustment")
require('"RESET DEFAULTS"' in sdl_frontend and "settingsSelection" in sdl_frontend,
        "Settings include a selectable reset-to-defaults action")

if errors:
    print(f"ANDROID SCAFFOLD SANITY: FAILED ({len(errors)} failures / {checks} checks)")
    sys.exit(1)
print(f"ANDROID SCAFFOLD SANITY: PASSED ({checks} checks)")

workflow = (ROOT / ".github/workflows/android-apk.yml").read_text(encoding="utf-8")
require("android-actions/setup-android@v4" in workflow and "packages: 'platform-tools'" in workflow,
        "Cloud Android workflow uses the verified Android SDK setup action")
