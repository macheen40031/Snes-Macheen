#!/usr/bin/env python3
"""Fast source-tree checks run before compilation."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "CMakeLists.txt",
    "cpu/cpu.cpp",
    "include/cpu.hpp",
    "include/bus.hpp",
    "memory/bus.cpp",
    "include/cartridge.hpp",
    "cartridge/cartridge.cpp",
    "include/ppu.hpp",
    "ppu/ppu.cpp",
    "src/main.cpp",
    "include/frontend_model.hpp",
    "frontend/frontend_model.cpp",
    "include/controller_mapper.hpp",
    "frontend/controller_mapper.cpp",
    "include/joypad.hpp",
    "input/joypad.cpp",
    "include/frame_pacer.hpp",
    "timing/frame_pacer.cpp",
    "include/emulator_session.hpp",
    "frontend/emulator_session.cpp",
    "frontend/sdl_frontend.cpp",
    "include/emulator_session.hpp",
    "frontend/emulator_session.cpp",
    "android/settings.gradle",
    "android/app/build.gradle",
    "android/app/src/main/AndroidManifest.xml",
    "android/app/src/main/java/com/snesmacheen/SnesMacheenActivity.java",
    "android/app/src/main/cpp/CMakeLists.txt",
    "android/fetch_sdl2.sh",
    "android/build_android.sh",
    "android/BUILD_ENVIRONMENT.md",
    "tools/bootstrap_android_build_env.sh",
    ".github/workflows/android-apk.yml",
    "android/CLOUD_BUILD.md",
    "tools/android_sanity.py",
    "tools/sdl2_compile_stub/SDL.h",
    "tools/sdl2_compile_stub/SDL_system.h",
]
SOURCE_SUFFIXES = {".cpp", ".hpp", ".h", ".cc", ".cxx"}
errors = []

for item in REQUIRED:
    if not (ROOT / item).is_file():
        errors.append(f"Missing required project file: {item}")

for path in ROOT.rglob("*"):
    if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
        continue
    if any(part in {"build", "build-quality", "build-sanitized", ".git"}
           for part in path.parts):
        continue
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        errors.append(f"Source is not valid UTF-8: {path.relative_to(ROOT)}")
        continue
    rel = path.relative_to(ROOT)
    for marker in ("<<<<<<< ", "=======\n", ">>>>>>> "):
        if marker in text:
            errors.append(f"Possible merge-conflict marker in {rel}: {marker.strip()}")
            break

if errors:
    print("SOURCE SANITY: FAILED")
    for error in errors:
        print(f"[FAIL] {error}")
    sys.exit(1)

print("SOURCE SANITY: PASSED")
print(f"[PASS] {len(REQUIRED)} required project files are present")
print("[PASS] No merge-conflict markers found in C/C++ sources")
print("[PASS] C/C++ source files are valid UTF-8")
