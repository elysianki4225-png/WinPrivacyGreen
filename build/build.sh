#!/bin/sh
# Cross-compiles WinPrivacyGreen.exe for 64 bit Windows 10 / 11 (x86_64).
#
# Requirements: a Zig toolchain (zig 0.14 or newer) and a Linux/macOS host.
#   ZIG=/path/to/zig ./build/build.sh
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${ZIG:-zig}
OUT=${OUT:-$ROOT/build/WinPrivacyGreen.exe}
export HOME=${HOME:-/tmp}
if [ -z "$XDG_CACHE_HOME" ] && [ ! -w "$HOME" ]; then
  export XDG_CACHE_HOME=/tmp/wpg-zig-cache
fi

SRC="$ROOT/src/main.cpp $ROOT/src/ui.cpp $ROOT/src/ui_views.cpp $ROOT/src/ui_draw.cpp \
$ROOT/src/color.cpp $ROOT/src/json.cpp $ROOT/src/sys.cpp $ROOT/src/items.cpp \
$ROOT/src/backup.cpp $ROOT/src/config.cpp"

# Icon + version info + manifest
"$ZIG" rc /c 65001 /i "$ROOT/resources" "$ROOT/resources/app.rc" "$ROOT/build/app.res"

"$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O2 -municode -mwindows -s \
  -Wall -Wextra -Wno-unused-parameter \
  $SRC "$ROOT/build/app.res" -o "$OUT" \
  -lgdiplus -ldwmapi -lshell32 -lole32 -luuid -loleaut32 -lgdi32 -luser32 -ladvapi32 -lshlwapi

echo "built $OUT"
