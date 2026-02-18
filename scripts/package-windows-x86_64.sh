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

rm -f "$ZIP_FILE"
(
  cd "$BIN_DIR"
  zip -r "$ZIP_FILE" "$(basename "$PKG_DIR")"
)

echo "Created package: $ZIP_FILE"

