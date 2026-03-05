#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${1:-$REPO_ROOT/build-windows-x86_64}"
OUT_DIR="${2:-$REPO_ROOT/builds}"
STAMP="${3:-$(date +%Y%m%d-%H%M)}"

BIN_DIR="$BUILD_DIR/src"
PKG_DIR="$BIN_DIR/wrkzcoin-windows-x86_64-$STAMP"
ZIP_FILE="$OUT_DIR/wrkzcoin-windows-x86_64-$STAMP.zip"

TARGETS=(
  "Wrkzd.exe"
  "miner.exe"
  "wrkz-wallet.exe"
  "wrkz-service.exe"
  "cryptotest.exe"
  "wrkz-wallet-api.exe"
  "wallet-upgrader.exe"
)

RUNTIME_DLLS=(
  "libwinpthread-1.dll"
  "libgcc_s_seh-1.dll"
  "libstdc++-6.dll"
)

for target in "${TARGETS[@]}"; do
  if [ ! -f "$BIN_DIR/$target" ]; then
    echo "Missing binary: $BIN_DIR/$target"
    exit 1
  fi
done

mkdir -p "$OUT_DIR"
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"

if command -v x86_64-w64-mingw32-strip >/dev/null 2>&1; then
  x86_64-w64-mingw32-strip "${TARGETS[@]/#/$BIN_DIR/}" || true
fi

cp "$REPO_ROOT/LICENSE" "$PKG_DIR/"
for target in "${TARGETS[@]}"; do
  cp "$BIN_DIR/$target" "$PKG_DIR/"
done

# Copy common MinGW runtime DLLs when available.
for dll in "${RUNTIME_DLLS[@]}"; do
  copied=0

  # Preferred: ask the compiler where this runtime DLL is.
  for cc in x86_64-w64-mingw32-g++-posix x86_64-w64-mingw32-g++ x86_64-w64-mingw32-gcc-posix x86_64-w64-mingw32-gcc; do
    if command -v "$cc" >/dev/null 2>&1; then
      dll_path="$("$cc" -print-file-name="$dll" 2>/dev/null || true)"
      if [ -n "$dll_path" ] && [ "$dll_path" != "$dll" ] && [ -f "$dll_path" ]; then
        cp "$dll_path" "$PKG_DIR/"
        copied=1
        break
      fi
    fi
  done

  # Fallback: common distro locations.
  if [ "$copied" -eq 0 ]; then
    for dir in \
      "/usr/x86_64-w64-mingw32/bin" \
      "/usr/lib/gcc/x86_64-w64-mingw32" \
      "/usr/lib/gcc/x86_64-w64-mingw32/10-posix" \
      "/usr/lib/gcc/x86_64-w64-mingw32/11-posix" \
      "/usr/lib/gcc/x86_64-w64-mingw32/12-posix" \
      "/usr/lib/gcc/x86_64-w64-mingw32/13-posix"
    do
      if [ -f "$dir/$dll" ]; then
        cp "$dir/$dll" "$PKG_DIR/"
        copied=1
        break
      fi
    done
  fi

  if [ "$copied" -eq 0 ]; then
    echo "Warning: runtime DLL not found on host: $dll"
  fi
done

rm -f "$ZIP_FILE"
(
  cd "$BIN_DIR"
  zip -r "$ZIP_FILE" "$(basename "$PKG_DIR")"
)

echo "Created package: $ZIP_FILE"
