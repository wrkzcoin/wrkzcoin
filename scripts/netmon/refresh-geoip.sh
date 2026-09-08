#!/usr/bin/env bash
#
# refresh-geoip.sh - fetch the current DB-IP Lite location files for
# wrkz-netmon and install them, atomically, only if they are valid.
#
#   bash scripts/netmon/refresh-geoip.sh [options]
#
# Options:
#   --dir <path>       Where to install the CSVs (default /opt/wrkz)
#   --month <YYYY-MM>  Fetch a specific release instead of the newest
#   --country-only     Skip the ASN file
#   --asn-only         Skip the country file
#   --restart <unit>   systemctl restart <unit>, but only if a file changed
#   --dry-run          Print what would be fetched and exit
#   -h, --help         This message
#
# Installs, under --dir:
#
#   dbip-country-lite.csv    country per IP range
#   dbip-asn-lite.csv        autonomous system per IP range
#
# Those are the stable names to point --geoip-db and --asn-db at, so the
# service configuration never has to name a month. See NETMON.md.
#
# Three things this handles that a bare curl does not:
#
#   * DB-IP publishes monthly, and the new file does not appear at the stroke
#     of midnight on the 1st. Asking for the current month early gets a 404,
#     so this falls back to the previous month rather than failing.
#
#   * A truncated download, an error page, or a rate-limit response would
#     otherwise replace a perfectly good database with rubbish. Nothing is
#     installed until the archive passes gzip -t AND the first data line looks
#     like a range.
#
#   * The install is a rename over the target, so a wrkz-netmon starting up
#     while this runs reads either the old file or the new one, never half of
#     either.
#
# DB-IP Lite is CC-BY 4.0: redistribution is allowed with attribution, which
# belongs in the dashboard footer. See https://db-ip.com/db/lite.php

set -euo pipefail

BASE_URL="https://download.db-ip.com/free"

DEST_DIR="/opt/wrkz"
WANT_MONTH=""
DO_COUNTRY=1
DO_ASN=1
RESTART_UNIT=""
DRY_RUN=0

die() {
    echo "refresh-geoip: $*" >&2
    exit 1
}

usage() {
    sed -n '3,23p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --dir)          DEST_DIR="${2:-}"; shift 2 ;;
        --month)        WANT_MONTH="${2:-}"; shift 2 ;;
        --country-only) DO_ASN=0; shift ;;
        --asn-only)     DO_COUNTRY=0; shift ;;
        --restart)      RESTART_UNIT="${2:-}"; shift 2 ;;
        --dry-run)      DRY_RUN=1; shift ;;
        -h|--help)      usage; exit 0 ;;
        *)              die "unknown option: $1 (try --help)" ;;
    esac
done

[ -n "$DEST_DIR" ] || die "--dir needs a path"

if [ "$DO_COUNTRY" -eq 0 ] && [ "$DO_ASN" -eq 0 ]; then
    die "--country-only and --asn-only are mutually exclusive"
fi

command -v curl >/dev/null 2>&1 || die "curl is not installed"
command -v gzip >/dev/null 2>&1 || die "gzip is not installed"

if [ -n "$WANT_MONTH" ] && ! echo "$WANT_MONTH" | grep -Eq '^[0-9]{4}-(0[1-9]|1[0-2])$'; then
    die "--month must look like 2026-09, got '$WANT_MONTH'"
fi

# ---- candidate releases -----------------------------------------------------

# GNU date and BSD date disagree about relative dates, so try both spellings
# rather than assuming a platform.
previous_month() {
    date -u -d "$(date -u +%Y-%m-01) -1 month" +%Y-%m 2>/dev/null \
        || date -u -v-1m +%Y-%m 2>/dev/null \
        || true
}

if [ -n "$WANT_MONTH" ]; then
    MONTHS="$WANT_MONTH"
else
    THIS_MONTH="$(date -u +%Y-%m)"
    LAST_MONTH="$(previous_month)"

    if [ -z "$LAST_MONTH" ]; then
        echo "refresh-geoip: could not compute last month; will only try $THIS_MONTH" >&2
        MONTHS="$THIS_MONTH"
    else
        MONTHS="$THIS_MONTH $LAST_MONTH"
    fi
fi

# ---- work directory ---------------------------------------------------------

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/refresh-geoip.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

CHANGED=0
INSTALLED=""

# ---- helpers ----------------------------------------------------------------

# A country row is start,end,CC. An ASN row is start,end,number,name. Both
# begin with two addresses, which is the cheap, format-independent check that
# what arrived is a database and not an error page.
looks_like_ranges() {
    local file="$1"
    local expected_min_fields="$2"
    local line

    line="$(grep -v -e '^[[:space:]]*$' -e '^#' "$file" 2>/dev/null | head -n 1 || true)"

    [ -n "$line" ] || return 1

    local first
    first="$(echo "$line" | cut -d, -f1 | tr -d '"' | tr -d '[:space:]')"

    # IPv4 dotted quad or anything with a colon in it (IPv6).
    echo "$first" | grep -Eq '^([0-9]{1,3}\.){3}[0-9]{1,3}$|:' || return 1

    local fields
    fields="$(echo "$line" | awk -F, '{print NF}')"

    [ "$fields" -ge "$expected_min_fields" ] || return 1

    return 0
}

# fetch_one <slug> <stable-name> <min-fields>
fetch_one() {
    local slug="$1"
    local stable="$2"
    local min_fields="$3"

    local archive="$WORK_DIR/$slug.csv.gz"
    local plain="$WORK_DIR/$slug.csv"
    local month=""
    local url=""

    for candidate in $MONTHS; do
        url="$BASE_URL/dbip-$slug-lite-$candidate.csv.gz"

        if [ "$DRY_RUN" -eq 1 ]; then
            echo "would fetch $url"
            month="$candidate"
            break
        fi

        echo "refresh-geoip: trying $url"

        # -f so a 404 is a failure rather than a saved error page.
        if curl -fsSL --retry 3 --retry-delay 5 --connect-timeout 20 \
                --max-time 900 -o "$archive" "$url"; then
            month="$candidate"
            break
        fi

        echo "refresh-geoip:   not available yet"
    done

    [ -n "$month" ] || die "no $slug release found for: $MONTHS"

    if [ "$DRY_RUN" -eq 1 ]; then
        return 0
    fi

    gzip -t "$archive" 2>/dev/null || die "$slug: the download is not a valid gzip archive"

    gzip -dc "$archive" > "$plain" || die "$slug: could not decompress"

    looks_like_ranges "$plain" "$min_fields" \
        || die "$slug: the file does not look like DB-IP ranges - first line is not start,end,..."

    local rows
    rows="$(grep -c -v -e '^[[:space:]]*$' -e '^#' "$plain" || true)"

    [ "${rows:-0}" -gt 1000 ] || die "$slug: only ${rows:-0} rows, refusing to install a stub"

    local target="$DEST_DIR/$stable"

    if [ -f "$target" ] && cmp -s "$plain" "$target"; then
        echo "refresh-geoip: $stable is already the $month release ($rows rows), leaving it"
        INSTALLED="$INSTALLED $stable=unchanged"
        return 0
    fi

    # Same filesystem as the target, so the rename is atomic.
    local staged="$target.new.$$"

    mv "$plain" "$staged"
    chmod 0644 "$staged"
    mv "$staged" "$target"

    echo "refresh-geoip: installed $stable from the $month release ($rows rows)"
    INSTALLED="$INSTALLED $stable=$month"
    CHANGED=1
}

# ---- run --------------------------------------------------------------------

if [ "$DRY_RUN" -eq 0 ]; then
    mkdir -p "$DEST_DIR" || die "cannot create $DEST_DIR"
    [ -w "$DEST_DIR" ] || die "$DEST_DIR is not writable by $(id -un)"
fi

if [ "$DO_COUNTRY" -eq 1 ]; then
    fetch_one country dbip-country-lite.csv 3
fi

if [ "$DO_ASN" -eq 1 ]; then
    fetch_one asn dbip-asn-lite.csv 3
fi

if [ "$DRY_RUN" -eq 1 ]; then
    exit 0
fi

echo "refresh-geoip: done -$INSTALLED"

if [ -n "$RESTART_UNIT" ]; then
    if [ "$CHANGED" -eq 0 ]; then
        echo "refresh-geoip: nothing changed, not restarting $RESTART_UNIT"
    elif ! command -v systemctl >/dev/null 2>&1; then
        echo "refresh-geoip: systemctl not found, restart $RESTART_UNIT yourself" >&2
    else
        echo "refresh-geoip: restarting $RESTART_UNIT"
        systemctl restart "$RESTART_UNIT"
    fi
fi

# wrkz-netmon reads both files once, at startup, so a refresh has no effect
# until it is restarted. Nothing breaks in the meantime: nodes on newly
# allocated ranges just show as unlocated.
exit 0
