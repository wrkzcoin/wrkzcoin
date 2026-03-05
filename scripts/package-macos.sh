#!/usr/bin/env bash
set -euo pipefail

# Package macOS cross-build artifacts.
# Usage: scripts/package-macos.sh [build-dir] [out-dir] [stamp] [target-arch]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${1:-$REPO_ROOT/build-macos-x86_64}"
OUT_DIR="${2:-$REPO_ROOT/builds}"
STAMP="${3:-$(date +%Y%m%d-%H%M)}"
TARGET_ARCH="${4:-x86_64}"

BIN_DIR="$BUILD_DIR/src"
PKG_NAME="wrkzcoin-macos-${TARGET_ARCH}-${STAMP}"
PKG_DIR="$BIN_DIR/$PKG_NAME"
TAR_FILE="$OUT_DIR/${PKG_NAME}.tar.gz"

TARGETS=(
  "Wrkzd"
  "miner"
  "wrkz-wallet"
  "wrkz-service"
  "cryptotest"
  "wrkz-wallet-api"
  "wallet-upgrader"
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

if [ -n "${OSXCROSS_ROOT:-}" ] && [ -n "${OSXCROSS_TARGET:-}" ]; then
  STRIP_BIN="$OSXCROSS_ROOT/target/bin/${OSXCROSS_TARGET}-strip"
  if [ -x "$STRIP_BIN" ]; then
    "$STRIP_BIN" "${TARGETS[@]/#/$BIN_DIR/}" || true
  fi
fi

cp "$REPO_ROOT/LICENSE" "$PKG_DIR/"
for target in "${TARGETS[@]}"; do
  cp "$BIN_DIR/$target" "$PKG_DIR/"
done

rm -f "$TAR_FILE"
(
  cd "$BIN_DIR"
  tar czf "$TAR_FILE" "$PKG_NAME"
)

echo "Created package: $TAR_FILE"

