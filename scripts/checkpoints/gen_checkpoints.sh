#!/usr/bin/env bash
# gen_checkpoints.sh — Generate CryptoNote checkpoint data from a running daemon
#
# Connects to a daemon over RPC and writes every block from height 0 to the top
# of the chain as "height,hash" lines — the format --load-checkpoints consumes:
#
#   0,877e55b4e902b9bf4c9e0a7c16440f449339d56679c49d62261ae5c92596a6ce
#   1,93bb1fd850d9e904ca810cdb57935b6df45cd75fc3a86358a421e126c1ae7b51
#   2,4fc480b6507b6df08a92496f3af83dd16b5b44ea1ba76792bd4e6381696c29c3
#   ...
#
# Usage:
#   ./gen_checkpoints.sh [OPTIONS]
#
# Options:
#   -H, --host HOST         Daemon RPC host (default: 127.0.0.1)
#   -p, --port PORT         Daemon RPC port (default: 17856)
#       --token TOKEN       X-API-Key value, if the daemon runs with --rpc-password
#   -s, --start HEIGHT      First checkpoint height (default: 0, or the resume point)
#   -e, --end HEIGHT        Last checkpoint height  (default: top height - confirmations)
#       --step N            Interval between checkpoints (default: 1 for csv, 1000 otherwise)
#       --confirmations N   Stay this many blocks behind the tip (default: 180)
#                             0 = go all the way to the top height
#   -c, --checkpoint-file F Existing file to resume from (csv or CryptoNoteCheckpoints.h)
#   -o, --output FILE       Output file, or '-' for stdout (default: checkpoints.csv)
#   -m, --mode MODE         Output mode: csv | append | full | raw (default: csv)
#                             csv     — "height,hash" lines for --load-checkpoints
#                             append  — C++ entries only, ready to paste into CHECKPOINTS
#                             full    — complete CryptoNoteCheckpoints.h file
#                             raw     — plain "height hash" lines
#                           csv and full write a COMPLETE file (existing + new);
#                           append and raw write ONLY the newly fetched entries.
#   -j, --workers N         Parallel RPC requests in flight (default: 8)
#       --batch N           Blocks per bulk request, max 100 (default: 100)
#       --timeout SECS      Per-request timeout (default: 30)
#       --no-bulk           Never use /getwalletsyncdata; one RPC call per height
#       --verify            Check an existing file against the daemon, then exit
#       --dry-run           Print what would be queried without making RPC calls
#   -v, --verbose           Print progress to stderr
#   -h, --help              Show this help
#
# Examples:
#   # Full per-block checkpoints.csv, genesis to (tip - 180)
#   ./gen_checkpoints.sh -o checkpoints.csv -v
#
#   # Extend an existing csv with whatever the daemon has learned since
#   ./gen_checkpoints.sh -o checkpoints.csv -v
#
#   # Re-verify a published file against a trusted node
#   ./gen_checkpoints.sh --verify -c checkpoints.csv
#
#   # Sparse C++ header entries, every 1000 blocks
#   ./gen_checkpoints.sh -m append --step 1000 -s 4189000
#
# Requires: curl, jq

set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────────────────────
HOST="127.0.0.1"
PORT=17856
TOKEN="${WRKZ_RPC_TOKEN:-}"
START_HEIGHT=""
END_HEIGHT=""
STEP=""
CHECKPOINT_FILE=""
OUTPUT_FILE="checkpoints.csv"
MODE="csv"
WORKERS=8
BATCH=100
TIMEOUT=30
NO_BULK=false
VERIFY=false
DRY_RUN=false
VERBOSE=false

# Core::getWalletSyncData clamps blockCount to BLOCKS_SYNCHRONIZING_DEFAULT_COUNT,
# so asking for more than this per request just wastes a round trip.
readonly MAX_BULK_BATCH=100

# CRYPTONOTE_MAX_ALT_BLOCK_DEPTH — how deep the daemon keeps alt chains, and so
# the deepest reorg it will follow. A checkpoint any shallower can be baked in on
# a block that later gets orphaned, which bricks every node that loads the file.
# A missing checkpoint costs sync speed; a wrong one costs the whole chain.
CONFIRMATIONS=180

WORKDIR=""

# ── Helpers ───────────────────────────────────────────────────────────────────
die()  { echo "ERROR: $*" >&2; exit 1; }
log()  { $VERBOSE && echo "$*" >&2 || true; }
info() { echo "$*" >&2; }

cleanup() {
    [[ -n "$WORKDIR" && -d "$WORKDIR" ]] && rm -rf "$WORKDIR"
    return 0
}
trap cleanup EXIT
trap 'info "Interrupted — ${OUTPUT_FILE} left untouched."; exit 130' INT TERM

usage() {
    sed -n '2,/^set -/{ /^set -/d; s/^# \{0,1\}//; p }' "$0"
    exit 0
}

# ── Argument parsing ──────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        -H|--host)              HOST="$2";             shift 2 ;;
        -p|--port)              PORT="$2";             shift 2 ;;
        --token)                TOKEN="$2";            shift 2 ;;
        -s|--start)             START_HEIGHT="$2";     shift 2 ;;
        -e|--end)               END_HEIGHT="$2";       shift 2 ;;
        --step)                 STEP="$2";             shift 2 ;;
        --confirmations)        CONFIRMATIONS="$2";    shift 2 ;;
        -c|--checkpoint-file)   CHECKPOINT_FILE="$2";  shift 2 ;;
        -o|--output)            OUTPUT_FILE="$2";      shift 2 ;;
        -m|--mode)              MODE="$2";             shift 2 ;;
        -j|--workers)           WORKERS="$2";          shift 2 ;;
        --batch)                BATCH="$2";            shift 2 ;;
        --timeout)              TIMEOUT="$2";          shift 2 ;;
        --no-bulk)              NO_BULK=true;          shift   ;;
        --verify)               VERIFY=true;           shift   ;;
        --dry-run)              DRY_RUN=true;          shift   ;;
        -v|--verbose)           VERBOSE=true;          shift   ;;
        -h|--help)              usage ;;
        # Internal: one parallel worker fetching [lo,hi] into a part file.
        --_segment)             SEGMENT_MODE=true;     shift   ;;
        *) die "Unknown option: $1" ;;
    esac
done

[[ "$MODE" =~ ^(csv|append|full|raw)$ ]] || die "Invalid mode '$MODE'. Use: csv, append, full, raw"

is_uint() { [[ "$1" =~ ^[0-9]+$ ]]; }

is_uint "$PORT"          || die "--port must be a number"
is_uint "$WORKERS"       || die "--workers must be a number"
is_uint "$BATCH"         || die "--batch must be a number"
is_uint "$CONFIRMATIONS" || die "--confirmations must be a number"
[[ "$WORKERS" -ge 1 ]]   || die "--workers must be >= 1"
[[ "$BATCH"   -ge 1 ]]   || die "--batch must be >= 1"
[[ -z "$START_HEIGHT" ]] || is_uint "$START_HEIGHT" || die "--start must be a number"
[[ -z "$END_HEIGHT"   ]] || is_uint "$END_HEIGHT"   || die "--end must be a number"

if [[ "$BATCH" -gt "$MAX_BULK_BATCH" ]]; then
    info "Note: clamping --batch to $MAX_BULK_BATCH (Core::getWalletSyncData caps blockCount at BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)"
    BATCH=$MAX_BULK_BATCH
fi

# csv is per-block by default; the C++ header modes are traditionally sparse.
if [[ -z "$STEP" ]]; then
    if [[ "$MODE" == "csv" ]]; then STEP=1; else STEP=1000; fi
fi
is_uint "$STEP" || die "--step must be a number"
[[ "$STEP" -ge 1 ]] || die "--step must be >= 1"

$VERIFY && $DRY_RUN && die "--verify and --dry-run are mutually exclusive"

# ── Dependency check ──────────────────────────────────────────────────────────
command -v curl >/dev/null 2>&1 || die "curl is required but not found"
command -v jq   >/dev/null 2>&1 || die "jq is required but not found"

RPC_URL="http://${HOST}:${PORT}"

# ── RPC ───────────────────────────────────────────────────────────────────────
# Retries transient failures. A daemon mid-reorg or flushing to RocksDB can
# stall for a second or two, and remote nodes rate limit non-localhost callers
# to --rpc-max-requests-per-minute (240 by default).
rpc_post() {
    local path="$1" body="$2"
    local attempt=0 max=5 resp="" code=""
    local -a auth=()
    [[ -n "$TOKEN" ]] && auth=(-H "X-API-Key: ${TOKEN}")

    while [[ $attempt -lt $max ]]; do
        [[ $attempt -gt 0 ]] && sleep "$(awk "BEGIN{print ($attempt*$attempt)*0.25}")"
        attempt=$((attempt + 1))

        resp=$(curl -sS --max-time "$TIMEOUT" \
                    -H "Content-Type: application/json" \
                    "${auth[@]+"${auth[@]}"}" \
                    -w $'\n%{http_code}' \
                    -d "$body" "${RPC_URL}${path}" 2>/dev/null) || continue

        code="${resp##*$'\n'}"
        resp="${resp%$'\n'*}"

        case "$code" in
            200) printf '%s' "$resp"; return 0 ;;
            401) die "unauthorized (HTTP 401) — the daemon wants --token" ;;
            429) sleep "$((attempt * 2))" ;;   # rate limited; back off harder
            *)   : ;;
        esac
    done
    return 1
}

rpc_json() {
    local method="$1" params="$2" resp
    resp=$(rpc_post "/json_rpc" \
        "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"${method}\",\"params\":${params}}") \
        || return 1
    if [[ "$(printf '%s' "$resp" | jq -r 'if .error then "y" else "n" end')" == "y" ]]; then
        info "RPC ${method}: $(printf '%s' "$resp" | jq -r '.error.message // "unknown error"')"
        return 1
    fi
    printf '%s' "$resp"
}

get_current_height() {
    local resp
    resp=$(rpc_json "getlastblockheader" "{}") || return 1
    printf '%s' "$resp" | jq -r '.result.block_header.height'
}

get_block_hash() {
    local height="$1" resp
    resp=$(rpc_json "getblockheaderbyheight" "{\"height\":${height}}") || return 1
    printf '%s' "$resp" | jq -r '.result.block_header.hash' | tr -d '\r' | tr 'A-F' 'a-f'
}

# Bulk: up to $BATCH contiguous blocks in one call.
# DatabaseBlockchainCache::getWalletSyncBlocks walks [start,end) block by block,
# so on a normal RocksDB node this is contiguous — the caller checks anyway,
# because the in-memory fallback (getNonEmptyBlocks) genuinely does skip blocks.
get_sync_blocks() {
    local start="$1" count="$2" resp
    resp=$(rpc_post "/getwalletsyncdata" \
        "{\"startHeight\":${start},\"startTimestamp\":0,\"blockCount\":${count},\"skipCoinbaseTransactions\":true,\"blockHashCheckpoints\":[]}") \
        || return 1
    [[ "$(printf '%s' "$resp" | jq -r '.status // "FAIL"')" == "OK" ]] || return 1
    printf '%s' "$resp" | jq -r '.items[]? | "\(.blockHeight),\(.blockHash)"' | tr -d '\r' | tr 'A-F' 'a-f'
}

supports_bulk() {
    local out
    out=$(get_sync_blocks 0 1 2>/dev/null) || return 1
    [[ -n "$out" ]]
}

# ── Existing-file handling ────────────────────────────────────────────────────
# Pulls "height,hash" out of either a csv or a CryptoNoteCheckpoints.h, strips
# CR so a Windows-authored file still parses, and lowercases the hash.
# Anything that is neither a checkpoint nor recognisable C++ boilerplate is an
# error, not something to skip: a line silently dropped here is a checkpoint
# silently missing from the file we are about to publish.
extract_entries() {
    local file="$1"
    tr -d '\r' < "$file" | awk -v label="$file" '
        {
            line = $0
            sub(/^[[:space:]]+/, "", line)
            sub(/[[:space:]]+$/, "", line)

            if (line == "") next

            # csv:  height,hash
            if (match(line, /^[0-9]+[[:space:]]*,[[:space:]]*[0-9a-fA-F]{64}$/)) {
                split(line, f, /[[:space:]]*,[[:space:]]*/)
                printf("%d,%s\n", f[1], tolower(f[2]))
                next
            }
            # C++: {height,"hash"},
            if (match(line, /^\{[[:space:]]*[0-9]+[[:space:]]*,[[:space:]]*"[0-9a-fA-F]{64}"[[:space:]]*\}/)) {
                h = line
                sub(/^\{[[:space:]]*/, "", h)
                split(h, f, /[[:space:]]*,[[:space:]]*"/)
                hash = f[2]
                sub(/".*$/, "", hash)
                printf("%d,%s\n", f[1], tolower(hash))
                next
            }
            # Boilerplate around a CryptoNoteCheckpoints.h body.
            if (line ~ /^(\/\/|#|const|struct|namespace|\{|\}|\};|uint32_t|const char)/) next

            printf("ERROR: %s:%d: cannot parse checkpoint line: %s\n", label, NR, $0) > "/dev/stderr"
            exit 1
        }
    '
}

# Rejects anything Checkpoints::loadCheckpointsFromFile would reject, so a broken
# file is caught before it gets extended and republished.
# Prints: "<count> <last height> <first height> <stride>", where stride is the
# uniform gap between entries, or 0 if the file has no single stride.
validate_entries() {
    local file="$1" label="$2"
    awk -F, -v label="$label" '
        {
            if ($2 !~ /^[0-9a-f]{64}$/) {
                printf("ERROR: %s: height %s has a malformed hash %s\n", label, $1, $2) > "/dev/stderr"
                exit 1
            }
            h = $1 + 0
            if (NR == 1) {
                first = h
            } else {
                # addCheckpoint fails the whole load on a duplicate, and the
                # parser is positional, so ordering matters too.
                if (h == prev) { printf("ERROR: %s: duplicate height %d\n", label, h) > "/dev/stderr"; exit 1 }
                if (h <  prev) { printf("ERROR: %s: height %d appears after %d — the file must ascend\n", label, h, prev) > "/dev/stderr"; exit 1 }
                d = h - prev
                if (NR == 2) {
                    stride = d
                } else if (d != stride) {
                    # A per-block file must stay per-block; a hole in it means
                    # the file is truncated or corrupt.
                    if (stride == 1) {
                        printf("ERROR: %s: gap in a per-block file — expected height %d, found %d\n", label, prev + 1, h) > "/dev/stderr"
                        exit 1
                    }
                    # Otherwise the file is simply irregular, which is legal but
                    # rules out the fast range fetch.
                    stride = 0
                }
            }
            prev = h
        }
        END { printf("%d %d %d %d\n", NR, prev + 0, first + 0, (NR < 2 ? 1 : stride)) }
    ' "$file"
}

# ── Output formatting ─────────────────────────────────────────────────────────
# Reads "height,hash" on stdin, writes the requested shape on stdout.
format_stream() {
    case "$MODE" in
        csv)         cat ;;
        raw)         tr ',' ' ' ;;
        append|full) sed 's/^\([0-9]*\),\(.*\)$/        {\1,"\2"},/' ;;
    esac
}

CPP_HEADER='// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace CryptoNote
{
    struct CheckpointData
    {
        uint32_t index;
        const char *blockId;
    };

const std::initializer_list<CheckpointData> CHECKPOINTS = {'

CPP_FOOTER='};
}'

# ── Segment worker ────────────────────────────────────────────────────────────
# Fetches [lo,hi] into $out as "height,hash" lines. Runs in a subshell under
# xargs -P, so it must be self-contained and must fail loudly.
fetch_segment() {
    local lo="$1" hi="$2" out="$3"
    local cursor="$lo" want line height hash expected

    : > "$out"

    if $USE_BULK; then
        while [[ "$cursor" -le "$hi" ]]; do
            want=$(( hi - cursor + 1 ))
            [[ "$want" -gt "$BATCH" ]] && want=$BATCH

            local blocks
            blocks=$(get_sync_blocks "$cursor" "$want") || {
                info "ERROR: getwalletsyncdata failed at height $cursor"
                return 1
            }
            [[ -n "$blocks" ]] || {
                info "ERROR: daemon returned no blocks at height $cursor — it may have been pruned or rewound mid-run"
                return 1
            }

            expected=$cursor
            while IFS=, read -r height hash; do
                [[ -z "$height" ]] && continue
                [[ "$height" -lt "$lo" || "$height" -gt "$hi" ]] && continue
                if [[ "$height" -ne "$expected" ]]; then
                    info "ERROR: gap in the daemon response: expected height $expected, got $height"
                    return 1
                fi
                printf '%s,%s\n' "$height" "$hash" >> "$out"
                expected=$(( expected + 1 ))
            done <<< "$blocks"

            [[ "$expected" -gt "$cursor" ]] || {
                info "ERROR: no usable blocks at height $cursor"
                return 1
            }
            cursor=$expected
        done
    else
        while [[ "$cursor" -le "$hi" ]]; do
            hash=$(get_block_hash "$cursor") || {
                info "ERROR: getblockheaderbyheight failed at height $cursor"
                return 1
            }
            [[ "$hash" =~ ^[0-9a-f]{64}$ ]] || {
                info "ERROR: daemon returned a malformed hash at height $cursor: $hash"
                return 1
            }
            printf '%s,%s\n' "$cursor" "$hash" >> "$out"
            cursor=$(( cursor + STEP ))
        done
    fi
    return 0
}

# Re-invoked by xargs for each segment; everything above is already sourced.
if [[ "${SEGMENT_MODE:-false}" == "true" ]]; then
    die "--_segment is internal and cannot be invoked directly"
fi

# Fill $3 with part.NNNNNN files covering [$1,$2] at $STEP, $WORKERS at a time.
# Used by both the generate and the verify paths.
fetch_into_parts() {
    local from="$1" to="$2" parts="$3" count="$4"
    local seg_count seg_span idx lo hi jobs want got out

    mkdir -p "$parts"

    # Split the range into segments, each an independent chunk of work. Aim for a
    # few segments per worker so a slow one cannot stall the tail, while keeping
    # the part-file count manageable.
    seg_count=$(( WORKERS * 4 ))
    [[ "$seg_count" -lt 1 ]] && seg_count=1
    seg_span=$(( (count + seg_count - 1) / seg_count ))
    [[ "$seg_span" -lt 1 ]] && seg_span=1
    # A bulk segment shorter than one batch would waste the round trip.
    if [[ "$USE_BULK" == "true" && "$seg_span" -lt "$BATCH" ]]; then
        seg_span=$BATCH
    fi

    jobs="$parts/.jobs"
    : > "$jobs"
    idx=0
    lo=$from
    while [[ "$lo" -le "$to" ]]; do
        hi=$(( lo + (seg_span - 1) * STEP ))
        [[ "$hi" -gt "$to" ]] && hi=$to
        printf '%d\t%d\t%s\n' "$lo" "$hi" "$(printf '%s/part.%06d' "$parts" "$idx")" >> "$jobs"
        idx=$(( idx + 1 ))
        lo=$(( hi + STEP ))
    done
    log "Split into $idx segments of up to $seg_span checkpoints, $WORKERS at a time"

    # Export everything the subshells need.
    export HOST PORT TOKEN TIMEOUT BATCH STEP RPC_URL USE_BULK VERBOSE
    export -f fetch_segment get_sync_blocks get_block_hash rpc_post rpc_json info log die

    if ! < "$jobs" xargs -P "$WORKERS" -I{} -d '\n' \
            bash -c 'IFS=$'"'"'\t'"'"' read -r lo hi out <<< "$1"; fetch_segment "$lo" "$hi" "$out"' _ {}
    then
        die "one or more segments failed — nothing was written"
    fi

    # Every segment must have produced exactly the rows it was asked for, so a
    # short read can never be mistaken for a complete range.
    while IFS=$'\t' read -r lo hi out; do
        [[ -f "$out" ]] || die "segment $lo-$hi produced no output — nothing was written"
        want=$(( (hi - lo) / STEP + 1 ))
        got=$(wc -l < "$out" | tr -d ' ')
        [[ "$got" -eq "$want" ]] || die "segment $lo-$hi returned $got of $want checkpoints — nothing was written"
    done < "$jobs"

    rm -f "$jobs"
}

# One worker: read heights (one per line) from $1, write "height,hash" to $2.
fetch_height_list() {
    local list="$1" out="$2" height hash
    : > "$out"
    while read -r height; do
        [[ -n "$height" ]] || continue
        hash=$(get_block_hash "$height") || {
            info "ERROR: getblockheaderbyheight failed at height $height"
            return 1
        }
        [[ "$hash" =~ ^[0-9a-f]{64}$ ]] || {
            info "ERROR: daemon returned a malformed hash at height $height: $hash"
            return 1
        }
        printf '%s,%s\n' "$height" "$hash" >> "$out"
    done < "$list"
    return 0
}

# Fetch an arbitrary, possibly irregular set of heights ($1, one per line,
# ascending) into part files under $2, $WORKERS at a time.
fetch_heights_into_parts() {
    local list="$1" parts="$2" total chunk
    mkdir -p "$parts"
    total=$(wc -l < "$list" | tr -d ' ')
    chunk=$(( (total + (WORKERS * 4) - 1) / (WORKERS * 4) ))
    [[ "$chunk" -lt 1 ]] && chunk=1

    # split -d gives numerically ordered suffixes, so the parts concatenate back
    # in ascending height order.
    rm -f "$parts"/chunk.*
    split -l "$chunk" -d -a 6 "$list" "$parts/chunk."

    export HOST PORT TOKEN TIMEOUT RPC_URL VERBOSE
    export -f fetch_height_list get_block_hash rpc_post rpc_json info log die

    if ! printf '%s\n' "$parts"/chunk.* \
        | xargs -P "$WORKERS" -I{} -d '\n' \
            bash -c 'fetch_height_list "$1" "${1/chunk./part.}"' _ {}
    then
        die "one or more height batches failed — nothing was written"
    fi

    local got
    got=$(cat "$parts"/part.* | wc -l | tr -d ' ')
    [[ "$got" -eq "$total" ]] || die "fetched $got of $total heights — nothing was written"
    rm -f "$parts"/chunk.*
}

# ── Verify mode ───────────────────────────────────────────────────────────────
if $VERIFY; then
    SOURCE="$CHECKPOINT_FILE"
    [[ -z "$SOURCE" && "$OUTPUT_FILE" != "-" ]] && SOURCE="$OUTPUT_FILE"
    [[ -n "$SOURCE" ]] || die "--verify needs a file: pass --checkpoint-file"
    [[ -f "$SOURCE" ]] || die "no such file: $SOURCE"

    WORKDIR=$(mktemp -d)
    # A parse error here must abort: extract_entries reports the offending line.
    if ! extract_entries "$SOURCE" > "$WORKDIR/entries"; then exit 1; fi
    [[ -s "$WORKDIR/entries" ]] || die "$SOURCE contains no checkpoints"
    # Note: a plain `read < <(validate_entries ...)` would throw away the exit
    # status, so a file that fails validation would sail straight through.
    if ! V_STATS=$(validate_entries "$WORKDIR/entries" "$SOURCE"); then
        exit 1   # validate_entries has already explained why on stderr
    fi
    read -r V_COUNT V_LAST V_FIRST V_STRIDE <<< "$V_STATS"

    TIP=$(get_current_height) || die "could not reach the daemon at ${HOST}:${PORT}"
    is_uint "$TIP" || die "daemon returned a nonsensical height: $TIP"

    # Anything above the tip cannot be checked against this node.
    awk -F, -v tip="$TIP" '($1 + 0) <= tip' "$WORKDIR/entries" > "$WORKDIR/checkable"
    CHECKED=$(wc -l < "$WORKDIR/checkable" | tr -d ' ')
    SKIPPED=$(( V_COUNT - CHECKED ))
    [[ "$CHECKED" -gt 0 ]] || die "every checkpoint in $SOURCE is above the daemon tip ($TIP)"

    info "Verifying $CHECKED checkpoints from $SOURCE against a daemon at height $TIP"

    V_LAST=$(tail -n 1 "$WORKDIR/checkable" | cut -d, -f1)

    if [[ "$V_STRIDE" -ge 1 ]]; then
        # The file has one uniform stride, so the daemon's view of the same
        # heights can be pulled with the parallel range fetch and diffed. Without
        # this, verifying a per-block file is one RPC per line — days of work.
        STEP="$V_STRIDE"
        USE_BULK=false
        if [[ "$STEP" -eq 1 ]] && ! $NO_BULK && [[ "$CHECKED" -gt "$BATCH" ]] && supports_bulk; then
            USE_BULK=true
        fi
        fetch_into_parts "$V_FIRST" "$V_LAST" "$WORKDIR/vparts" "$CHECKED"
        cat "$WORKDIR/vparts"/part.* | tr -d '\r' > "$WORKDIR/daemon"

        # Both sides are ascending over identical heights, so a line diff is an
        # exact comparison.
    else
        # Irregular file (mixed strides, e.g. a header that switches from 100 to
        # 1000). Fetch exactly the heights the file names, still in parallel.
        log "$SOURCE has no uniform stride; fetching its heights individually"
        cut -d, -f1 "$WORKDIR/checkable" > "$WORKDIR/heights"
        fetch_heights_into_parts "$WORKDIR/heights" "$WORKDIR/vparts"
        cat "$WORKDIR/vparts"/part.* | tr -d '\r' > "$WORKDIR/daemon"
    fi

    # Both sides are ascending over identical heights, so a line diff is an
    # exact comparison. `|| true` throughout: diff exits non-zero when the files
    # differ, which is the case we are here to report, not a script failure.
    if cmp -s "$WORKDIR/checkable" "$WORKDIR/daemon"; then
        MISMATCH=0
    else
        MISMATCH=$(join -t, -j 1 -o 0,1.2,2.2 "$WORKDIR/checkable" "$WORKDIR/daemon" 2>/dev/null \
                    | awk -F, '$2 != $3' | wc -l | tr -d ' ') || true
        join -t, -j 1 -o 0,1.2,2.2 "$WORKDIR/checkable" "$WORKDIR/daemon" 2>/dev/null \
            | awk -F, '$2 != $3 { printf("  MISMATCH at %s: file has %s, daemon has %s\n", $1, $2, $3) }' \
            | head -20 >&2 || true
        # A height present on one side only is a structural mismatch, not a hash
        # mismatch, so it would slip past the join above.
        if [[ "$MISMATCH" -eq 0 ]]; then
            MISMATCH=$(( CHECKED - $(join -t, -j 1 "$WORKDIR/checkable" "$WORKDIR/daemon" 2>/dev/null | wc -l | tr -d ' ') ))
            info "  note: the daemon did not return every height the file names"
        fi
    fi

    if [[ "$SKIPPED" -gt 0 ]]; then
        info "  note: $SKIPPED checkpoints are above the daemon tip and were skipped"
    fi

    if [[ "$MISMATCH" -gt 0 ]]; then
        info "FAILED: $MISMATCH/$CHECKED checkpoints do not match"
        exit 1
    fi
    info "OK: all $CHECKED checkpoints match the daemon"
    exit 0
fi

# ── Resume from an existing file ──────────────────────────────────────────────
WORKDIR=$(mktemp -d)
RESUME_SOURCE="$CHECKPOINT_FILE"

# Extending checkpoints.csv in place is the common case; don't make the caller
# repeat the path.
if [[ -z "$RESUME_SOURCE" && "$OUTPUT_FILE" != "-" && -f "$OUTPUT_FILE" ]] \
   && [[ "$MODE" == "csv" || "$MODE" == "full" ]]; then
    RESUME_SOURCE="$OUTPUT_FILE"
fi

LAST_HEIGHT=""
if [[ -n "$RESUME_SOURCE" ]]; then
    [[ -f "$RESUME_SOURCE" ]] || die "no such file: $RESUME_SOURCE"
    if ! extract_entries "$RESUME_SOURCE" > "$WORKDIR/existing"; then exit 1; fi
    if [[ -s "$WORKDIR/existing" ]]; then
        # Command substitution, not process substitution: this must abort when
        # the existing file is corrupt rather than silently extending it.
        if ! EXIST_STATS=$(validate_entries "$WORKDIR/existing" "$RESUME_SOURCE"); then
            exit 1   # validate_entries has already explained why on stderr
        fi
        read -r EXIST_COUNT LAST_HEIGHT EXIST_FIRST EXIST_STRIDE <<< "$EXIST_STATS"
        log "Existing file $RESUME_SOURCE holds $EXIST_COUNT checkpoints, last at $LAST_HEIGHT"
        if [[ -z "$START_HEIGHT" ]]; then
            START_HEIGHT=$(( LAST_HEIGHT + STEP ))
            log "Resuming: last checkpoint at $LAST_HEIGHT → starting at $START_HEIGHT"
        elif [[ "$START_HEIGHT" -le "$LAST_HEIGHT" ]]; then
            die "--start $START_HEIGHT would duplicate or reorder entries already in $RESUME_SOURCE (last height $LAST_HEIGHT)"
        fi
    else
        RESUME_SOURCE=""
    fi
fi

# csv and full describe a whole file, so anything already there has to be carried
# forward — otherwise resuming in place silently truncates it. append and raw are
# deltas by definition.
CARRY_FROM=""
if [[ -n "$RESUME_SOURCE" && ( "$MODE" == "csv" || "$MODE" == "full" ) ]]; then
    CARRY_FROM="$WORKDIR/existing"
fi

[[ -z "$START_HEIGHT" ]] && START_HEIGHT=0

# ── Resolve the end height ────────────────────────────────────────────────────
if [[ -z "$END_HEIGHT" ]]; then
    if $DRY_RUN; then
        info "Dry-run: cannot determine current height without RPC. Use --end."
        END_HEIGHT=$(( START_HEIGHT + STEP * 3 ))
    else
        log "Querying daemon for current height..."
        TIP=$(get_current_height) || die "could not reach the daemon at ${HOST}:${PORT}"
        is_uint "$TIP" || die "daemon returned a nonsensical height: $TIP"
        if [[ "$TIP" -lt "$CONFIRMATIONS" ]]; then
            info "Nothing to do: the daemon is only $TIP blocks tall, which is within --confirmations $CONFIRMATIONS"
            exit 0
        fi
        END_HEIGHT=$(( TIP - CONFIRMATIONS ))
        log "Daemon height $TIP, holding back $CONFIRMATIONS confirmations → end $END_HEIGHT"
    fi
fi

# Snap the end onto the step grid so resumed runs stay aligned.
if [[ "$STEP" -gt 1 ]]; then
    END_HEIGHT=$(( END_HEIGHT - ( (END_HEIGHT - START_HEIGHT) % STEP ) ))
fi

if [[ "$END_HEIGHT" -lt "$START_HEIGHT" ]]; then
    info "Nothing to do: start=$START_HEIGHT end=$END_HEIGHT (the daemon may not be ahead of the last checkpoint)"
    exit 0
fi

TOTAL=$(( (END_HEIGHT - START_HEIGHT) / STEP + 1 ))
info "Fetching $TOTAL checkpoints from height $START_HEIGHT to $END_HEIGHT (step=$STEP)"

# ── Choose a fetch strategy ───────────────────────────────────────────────────
USE_BULK=false
if ! $DRY_RUN && ! $NO_BULK && [[ "$STEP" -eq 1 && "$TOTAL" -gt "$BATCH" ]]; then
    if supports_bulk; then
        USE_BULK=true
    else
        info "Note: /getwalletsyncdata is unavailable; falling back to one RPC call per height. This will be slow."
    fi
fi

# ── Fetch ─────────────────────────────────────────────────────────────────────
PARTS_DIR="$WORKDIR/parts"
mkdir -p "$PARTS_DIR"

if $DRY_RUN; then
    ZEROS=$(printf '0%.0s' $(seq 1 64))
    h=$START_HEIGHT
    while [[ "$h" -le "$END_HEIGHT" ]]; do
        printf '%s,%s\n' "$h" "$ZEROS"
        h=$(( h + STEP ))
    done > "$PARTS_DIR/part.000000"
else
    fetch_into_parts "$START_HEIGHT" "$END_HEIGHT" "$PARTS_DIR" "$TOTAL"
fi

# ── Assemble ──────────────────────────────────────────────────────────────────
# Built in a temp file and moved into place, so interrupting a long run can never
# leave a truncated checkpoints.csv where nodes expect a complete one.
STAGED="$WORKDIR/staged"

# Note: every guard below is a full `if`, not `cond && action`. This block is a
# pipeline, so under `set -o pipefail` a trailing guard whose condition is false
# would make the whole group exit non-zero and `set -e` would abort the run.
{
    if [[ "$MODE" == "full" ]]; then printf '%s\n' "$CPP_HEADER"; fi
    {
        if [[ -n "$CARRY_FROM" ]]; then cat "$CARRY_FROM"; fi
        # part.NNNNNN is zero padded, so glob order is numeric order.
        cat "$PARTS_DIR"/part.* 2>/dev/null
    } | format_stream
    if [[ "$MODE" == "full" ]]; then printf '%s\n' "$CPP_FOOTER"; fi
} | tr -d '\r' > "$STAGED"   # never emit CR: Checkpoints.cpp reads the hash with
                             # std::getline(file, hash), which strips \n but keeps
                             # \r. The resulting 65 char string has an odd length,
                             # so Common::fromHex rejects it and the daemon refuses
                             # the entire checkpoint file.

WRITTEN=$(grep -cE '^[[:space:]]*[{]?[0-9]+[,]' "$STAGED" || true)

if [[ "$OUTPUT_FILE" == "-" ]]; then
    cat "$STAGED"
else
    OUT_DIR=$(dirname "$OUTPUT_FILE")
    [[ -d "$OUT_DIR" ]] || mkdir -p "$OUT_DIR"
    mv -f "$STAGED" "$OUTPUT_FILE"
    info "Wrote $WRITTEN checkpoints to $OUTPUT_FILE"
fi

$DRY_RUN && info "Dry-run complete. No real RPC calls were made."

exit 0
