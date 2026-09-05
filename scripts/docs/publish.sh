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
#   ./publish.sh --archive-only       write v<version> only, leave the live
#                                     docs alone. For snapshotting a release
#                                     from its tag after the fact
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
LIVE=1
DRY_RUN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --archive)      ARCHIVE=1 ;;
        --archive-only) ARCHIVE=1; LIVE=0 ;;
        --dry-run)      DRY_RUN=1 ;;
        --root)         WEB_ROOT="$2"; shift ;;
        -h|--help)      sed -n '2,29p' "$0"; exit 0 ;;
        *)              echo "unknown option: $1" >&2; exit 2 ;;
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

# `mkdocs` being on PATH is not enough: a distribution package gives you base
# mkdocs, whose only themes are mkdocs and readthedocs, and the config is then
# rejected with "Unrecognised theme name: 'material'".
#
# Rather than guess which interpreter that mkdocs belongs to - which is wrong
# on Windows, and wherever python3 is not its sibling - let mkdocs answer, and
# explain the failure. Nothing has been synced at this point, so a failed build
# costs nothing but the message.
build_docs() {
    local site_url="$1" out_dir="$2"

    if DOCS_SITE_URL="$site_url" mkdocs build \
        --strict -f "$REPO_ROOT/docs/mkdocs.yml" -d "$out_dir"; then
        return 0
    fi

    cat >&2 <<EOF

The build failed. If the error above names an unrecognised theme ('material')
or an unknown plugin ('minify'), the mkdocs on PATH cannot build this config:

    $(command -v mkdocs)

Install the documentation requirements into the same environment:

    python3 -m venv .venv-docs
    .venv-docs/bin/pip install -r docs/requirements.txt
    source .venv-docs/bin/activate

"apt install mkdocs" provides base mkdocs only, and a bare "pip install" is
refused on Debian 12+/Ubuntu 23.04+ as an externally-managed environment -
hence the virtualenv.
EOF
    exit 1
}

# Build into a staging directory, never straight into the web root: `mkdocs
# build` wipes its output directory first, which would take the archives with
# it.
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

if [ "$LIVE" = 1 ]; then
    echo "==> building live docs"
    build_docs "$BASE_URL/" "$STAGE/live"
fi

if [ "$ARCHIVE" = 1 ]; then
    echo "==> building archive copy for v$VERSION"
    build_docs "$BASE_URL/v$VERSION/" "$STAGE/v$VERSION"
fi

RSYNC_OPTS=(-a --delete)
[ "$DRY_RUN" = 1 ] && RSYNC_OPTS+=(--dry-run --itemize-changes)

if [ "$LIVE" = 1 ]; then
    # --delete would remove the archive directories and versions.json, which
    # are not part of any single build. Protect them explicitly.
    echo "==> syncing live docs"
    rsync "${RSYNC_OPTS[@]}" \
        --exclude '/v[0-9]*/' \
        --exclude '/versions.json' \
        "$STAGE/live/" "$WEB_ROOT/"
else
    echo "==> leaving the live docs untouched (--archive-only)"
fi

if [ "$ARCHIVE" = 1 ]; then
    echo "==> syncing archive v$VERSION"
    rsync "${RSYNC_OPTS[@]}" "$STAGE/v$VERSION/" "$WEB_ROOT/v$VERSION/"
fi

# versions.json is rebuilt from what is actually on disk rather than kept by
# hand, so it can never advertise a version that is not served.
echo "==> regenerating versions.json"
DRY_RUN="$DRY_RUN" WEB_ROOT="$WEB_ROOT" python3 <<'PY'
import json, os, re

web_root = os.environ["WEB_ROOT"]
dry_run = os.environ["DRY_RUN"] == "1"
target = os.path.join(web_root, "versions.json")


def sort_key(name):
    return [int(part) for part in re.findall(r"\d+", name)]


# Derived entirely from what is on disk. Every entry the selector offers is
# therefore a directory that is actually served, whichever mode produced it.
archives = sorted(
    (d[1:] for d in os.listdir(web_root)
     if re.fullmatch(r"v\d+(\.\d+)*", d)
     and os.path.isdir(os.path.join(web_root, d))),
    key=sort_key,
    reverse=True,
) if os.path.isdir(web_root) else []

if not archives:
    # Nothing to switch between yet. Material fetches versions.json and simply
    # renders no selector when it 404s, which is the honest state before the
    # first release is archived.
    print("no archived versions on disk yet - leaving versions.json absent")
    if os.path.exists(target) and not dry_run:
        os.remove(target)
    raise SystemExit(0)

# The live site sits at the root, which has no path segment for the selector to
# match against, so Material falls back to entry zero. The newest archive must
# therefore come first: that is what the root is showing, give or take doc
# fixes made since that release.
newest, rest = archives[0], archives[1:]

entries = [{
    "version": "v%s" % newest,
    "title": "%s (latest)" % newest,
    "aliases": ["latest"],
}]
entries += [{"version": "v%s" % v, "title": v, "aliases": []} for v in rest]

payload = json.dumps(entries, indent=2) + "\n"

if dry_run:
    print("would write %s:\n%s" % (target, payload))
else:
    with open(target, "w", encoding="utf-8") as handle:
        handle.write(payload)
    print("wrote %s (%d entries, latest %s)" % (target, len(entries), newest))
PY

echo "==> done"
[ "$ARCHIVE" = 1 ] || echo "    (run with --archive at release time to snapshot v$VERSION)"
