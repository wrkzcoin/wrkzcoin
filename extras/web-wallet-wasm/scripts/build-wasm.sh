#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-wasm}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"
OUT_DIR="$REPO_ROOT/extras/web-wallet-wasm/web/wasm/generated"

echo "Configuring wallet_wasm build in $BUILD_DIR ..."
emcmake cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DWRKZ_BUILD_EXECUTABLES=OFF \
  -DWRKZ_BUILD_WALLET_CAPI=ON \
  -DWRKZ_BUILD_WALLET_WASM=ON \
  -DWRKZ_WASM_PTHREADS=ON \
  -DENABLE_ZMQ=OFF

echo "Building wallet_wasm ..."
cmake --build "$BUILD_DIR" --target wallet_wasm --parallel "$JOBS"

mkdir -p "$OUT_DIR"
cp -f "$BUILD_DIR/wasm/wallet_wasm.js" "$OUT_DIR/wallet_wasm.js"
cp -f "$BUILD_DIR/wasm/wallet_wasm.wasm" "$OUT_DIR/wallet_wasm.wasm"
if [[ -f "$BUILD_DIR/wasm/wallet_wasm.worker.js" ]]; then
  cp -f "$BUILD_DIR/wasm/wallet_wasm.worker.js" "$OUT_DIR/wallet_wasm.worker.js"
fi

echo "WASM artifacts copied to:"
echo "  $OUT_DIR/wallet_wasm.js"
echo "  $OUT_DIR/wallet_wasm.wasm"
if [[ -f "$OUT_DIR/wallet_wasm.worker.js" ]]; then
  echo "  $OUT_DIR/wallet_wasm.worker.js"
fi
