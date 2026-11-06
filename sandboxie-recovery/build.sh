#!/bin/sh
# Cross-compile the standalone Sandboxie Plus helper with Zig 0.14+.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ZIG=${ZIG:-zig}
OUT=${OUT:-$ROOT/build/SandboxieRecoverySwitch.exe}
mkdir -p "$(dirname "$OUT")" "$ROOT/build"
"$ZIG" rc /c 65001 /i "$ROOT/sandboxie-recovery/resources" \
  "$ROOT/sandboxie-recovery/resources/app.rc" "$ROOT/build/sandboxie-recovery.res"
"$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O2 -municode -Wl,--subsystem,windows -s \
  -Wall -Wextra -Wno-unused-parameter -DNOMINMAX -DUNICODE -D_UNICODE \
  "$ROOT/sandboxie-recovery/src/main.cpp" "$ROOT/sandboxie-recovery/src/platform.cpp" \
  "$ROOT/sandboxie-recovery/src/ini.cpp" "$ROOT/build/sandboxie-recovery.res" -o "$OUT" \
  -lshell32 -lcomdlg32 -lcomctl32 -lgdi32 -luser32 -ladvapi32 -lole32 -luuid
echo "built $OUT"
