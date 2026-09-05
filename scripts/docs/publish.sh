#!/usr/bin/env bash
# publish.sh - build the documentation and put it on the nginx web root.
#
# Layout: the live docs sit at the web root, exactly where they are today, and
# each archived release sits beside them in its own directory:
#
#   /var/www/docs.wrkz.work/
#       index.html  guides/  daemon-rpc/  ...   <- current, the canonical URLs
#       versions.json                           <- drives the version selector
#       v0.4.8/                                 <- archived releases
#       v0.4.7/
#
# Nothing about the live URLs changes, so existing links and bookmarks keep
# working. See README.md in this directory.
#
# Usage:
#   ./publish.sh                      build and sync the live docs
#   ./publish.sh --archive            also snapshot this build as v<version>
#   ./publish.sh --dry-run            show what would change, touch nothing
#   ./publish.sh --root /path/to/web  override the web root
#
# Environment:
#   DOCS_WEB_ROOT   web root (default /var/www/docs.wrkz.work)
#   DOCS_BASE_URL   public base URL (default https://docs.wrkz.work)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WEB_ROOT="${DOCS_WEB_ROOT:-/var/www/docs.wrkz.work}"
BASE_URL="${DOCS_BASE_URL:-https://docs.wrkz.work}"
ARCHIVE=0
DRY_RUN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --archive)  ARCHIVE=1 ;;
        --dry-run)  DRY_RUN=1 ;;
        --root)     WEB_ROOT="$2"; shift ;;
        -h|--help)  sed -n '2,25p' "$0"; exit 0 ;;
        *)          echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

# Version comes from the same header the binaries report, so the docs and
# `Wrkzd --version` can never disagree about what release this is.
version_header="$REPO_ROOT/src/config/version.h.in"
read_ver() { grep -E "^#define $1 " "$version_header" | awk '{print $3}'; }
VERSION="$(read_ver APP_VER_MAJOR).$(read_ver APP_VER_MINOR).$(read_ver APP_VER_REV)"

if [ -z "$VERSION" ] || [ "$VERSION" = ".." ]; then
    echo "could not read a version from $version_header" >&2
    exit 1
fi

echo "==> version $VERSION, web root $WEB_ROOT"

for tool in mkdocs rsync python3; do
    command -v "$tool" >/dev/null || { echo "$tool is not installed" >&2; exit 1; }
done

# Build into a staging directory, never straight into the web root: `mkdocs
# build` wipes its output directory first, which would take the archives with
# it.
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

echo "==> building live docs"
DOCS_SITE_URL="$BASE_URL/" mkdocs build \
    --strict -f "$REPO_ROOT/docs/mkdocs.yml" -d "$STAGE/live"

if [ "$ARCHIVE" = 1 ]; then
    echo "==> building archive copy for v$VERSION"
    DOCS_SITE_URL="$BASE_URL/v$VERSION/" mkdocs build \
        --strict -f "$REPO_ROOT/docs/mkdocs.yml" -d "$STAGE/v$VERSION"
fi

RSYNC_OPTS=(-a --delete)
[ "$DRY_RUN" = 1 ] && RSYNC_OPTS+=(--dry-run --itemize-changes)

# --delete would remove the archive directories and versions.json, which are
# not part of any single build. Protect them explicitly.
echo "==> syncing live docs"
rsync "${RSYNC_OPTS[@]}" \
    --exclude '/v[0-9]*/' \
    --exclude '/versions.json' \
    "$STAGE/live/" "$WEB_ROOT/"

if [ "$ARCHIVE" = 1 ]; then
    echo "==> syncing archive v$VERSION"
    rsync "${RSYNC_OPTS[@]}" "$STAGE/v$VERSION/" "$WEB_ROOT/v$VERSION/"
fi

# versions.json is rebuilt from what is actually on disk rather than kept by
# hand, so it can never advertise a version that is not served.
echo "==> regenerating versions.json"
DRY_RUN="$DRY_RUN" WEB_ROOT="$WEB_ROOT" VERSION="$VERSION" python3 <<'PY'
import json, os, re

web_root = os.environ["WEB_ROOT"]
current = os.environ["VERSION"]
dry_run = os.environ["DRY_RUN"] == "1"


def sort_key(name):
    return [int(part) for part in re.findall(r"\d+", name)]


archives = sorted(
    (d[1:] for d in os.listdir(web_root)
     if re.fullmatch(r"v\d+(\.\d+)*", d)
     and os.path.isdir(os.path.join(web_root, d))),
    key=sort_key,
    reverse=True,
) if os.path.isdir(web_root) else []

# The live site sits at the root, which has no path segment for the version
# selector to match, so Material falls back to entry zero. The current release
# must therefore come first.
entries = [{
    "version": "v%s" % current,
    "title": "%s (latest)" % current,
    "aliases": ["latest"],
}]
entries += [
    {"version": "v%s" % v, "title": v, "aliases": []}
    for v in archives if v != current
]

payload = json.dumps(entries, indent=2) + "\n"
target = os.path.join(web_root, "versions.json")

if dry_run:
    print("would write %s:\n%s" % (target, payload))
else:
    with open(target, "w", encoding="utf-8") as handle:
        handle.write(payload)
    print("wrote %s (%d entries)" % (target, len(entries)))
PY

echo "==> done"
[ "$ARCHIVE" = 1 ] || echo "    (run with --archive at release time to snapshot v$VERSION)"
