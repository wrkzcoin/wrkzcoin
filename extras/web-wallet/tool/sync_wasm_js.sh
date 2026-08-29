#!/usr/bin/env bash
#
# The JS bridge lives in extras/web-wallet-wasm/wasm/js/ and has to be copied
# into extras/web-wallet/web/ for Flutter to serve it. Keeping two copies in
# sync by hand is a matter of time before they drift, so:
#
#   tool/sync_wasm_js.sh          copy canonical -> web/
#   tool/sync_wasm_js.sh --check  fail if they differ (for CI)
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
src="$here/../../web-wallet-wasm/wasm/js"
dst="$here/../web"

files=(wallet_bridge.js wallet_storage.js wallet_worker.js)
check=0
[[ "${1:-}" == "--check" ]] && check=1

status=0
for f in "${files[@]}"; do
  if [[ ! -f "$src/$f" ]]; then
    echo "missing source: $src/$f" >&2
    status=1
    continue
  fi
  if [[ $check -eq 1 ]]; then
    if ! diff -q "$src/$f" "$dst/$f" >/dev/null 2>&1; then
      echo "OUT OF SYNC: $f" >&2
      status=1
    else
      echo "ok: $f"
    fi
  else
    cp "$src/$f" "$dst/$f"
    echo "copied: $f"
  fi
done

if [[ $check -eq 1 && $status -ne 0 ]]; then
  echo >&2
  echo "Run tool/sync_wasm_js.sh to update extras/web-wallet/web/." >&2
fi
exit $status
