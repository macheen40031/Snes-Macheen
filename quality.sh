#!/usr/bin/env bash
# Full release quality gate: source checks, clean strict build, CTest,
# and AddressSanitizer/UndefinedBehaviorSanitizer when supported.
set -euo pipefail
cd "$(dirname "$0")"

REPORT="${QUALITY_REPORT:-quality-report.txt}"
exec > >(tee "$REPORT") 2>&1

printf '\n========================================\n'
printf ' Snes-Macheen automated quality gate\n'
printf '========================================\n'
printf 'Date: %s\n' "$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
printf 'Compiler: %s\n' "${CXX:-c++}"

python3 -S tools/source_sanity.py
python3 -S tools/android_sanity.py

# Compile the SDL frontend translation unit against a narrow test-only API
# stub. This catches C++ syntax/type regressions without pretending that SDL2
# itself is installed or linked on this host.
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -fsyntax-only -I tools/sdl2_compile_stub -I include frontend/sdl_frontend.cpp
printf '[PASS] SDL2 frontend C++ syntax check (test API stub; no SDL link)\n'

rm -rf build-quality
cmake -S . -B build-quality -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSNES_STRICT_WARNINGS=ON \
  -DBUILD_TESTING=ON
cmake --build build-quality --clean-first
printf '\n--- Regression suite details ---\n'
./build-quality/supernes
ctest --test-dir build-quality --output-on-failure

# Probe sanitizer compiler + runtime support before attempting a sanitized build.
CXX_BIN="${CXX:-c++}"
PROBE_DIR="$(mktemp -d)"
trap 'rm -rf "$PROBE_DIR"' EXIT
cat > "$PROBE_DIR/probe.cpp" <<'CPP'
int main() { int value = 1; return value == 1 ? 0 : 1; }
CPP
if "$CXX_BIN" -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$PROBE_DIR/probe.cpp" -o "$PROBE_DIR/probe" >/dev/null 2>&1 \
    && (ASAN_OPTIONS=detect_leaks=0 "$PROBE_DIR/probe" >/dev/null 2>&1); then
    printf '\n[INFO] AddressSanitizer + UBSan supported; running sanitized build.\n'
    rm -rf build-sanitized
    cmake -S . -B build-sanitized -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DSNES_STRICT_WARNINGS=ON \
      -DBUILD_TESTING=ON \
      -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
      -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
    cmake --build build-sanitized --clean-first
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
      UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
      ctest --test-dir build-sanitized --output-on-failure
    printf '[PASS] AddressSanitizer and UndefinedBehaviorSanitizer tests passed.\n'
else
    printf '\n[WARN] Sanitizer runtime unavailable on this platform; sanitized run skipped.\n'
fi

printf '\n========================================\n'
printf ' QUALITY GATE: PASSED\n'
printf ' Clean strict build: PASS\n'
printf ' Regression tests: PASS\n'
printf ' Source sanity: PASS\n'
printf '========================================\n'
printf 'Report saved to: %s\n' "$REPORT"
