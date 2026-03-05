#!/usr/bin/env bash
# gen_checkpoints.sh — Generate CryptoNote checkpoint data from a running daemon
#
# Usage:
#   ./gen_checkpoints.sh [OPTIONS]
#
# Options:
#   -H, --host HOST         Daemon RPC host (default: 127.0.0.1)
#   -p, --port PORT         Daemon RPC port (default: 17856)
#   -s, --start HEIGHT      First checkpoint height (default: auto from checkpoint file)
#   -e, --end HEIGHT        Last checkpoint height  (default: daemon current height)
#   --step N                Interval between checkpoints (default: 1000)
#   -c, --checkpoint-file F Existing CryptoNoteCheckpoints.h to read last height from
#   -o, --output FILE       Output file (default: stdout)
#   -m, --mode MODE         Output mode: append | full | raw (default: append)
#                             append  — C++ entries only, ready to paste into CHECKPOINTS
#                             full    — complete CryptoNoteCheckpoints.h file
#                             raw     — plain "height hash" lines (one per line)
#   --dry-run               Print what would be queried without making RPC calls
#   -v, --verbose           Print progress to stderr
#   -h, --help              Show this help

set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────────────────────
HOST="127.0.0.1"
PORT=17856
START_HEIGHT=""
END_HEIGHT=""
STEP=1000
CHECKPOINT_FILE=""
OUTPUT_FILE=""
MODE="append"
DRY_RUN=false
VERBOSE=false

# ── Helpers ───────────────────────────────────────────────────────────────────
die()  { echo "ERROR: $*" >&2; exit 1; }
log()  { $VERBOSE && echo "$*" >&2 || true; }
info() { echo "$*" >&2; }

usage() {
    sed -n '2,/^set -/{ /^set -/d; s/^# \{0,1\}//; p }' "$0"
    exit 0
}

# ── Argument parsing ──────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        -H|--host)              HOST="$2";             shift 2 ;;
        -p|--port)              PORT="$2";             shift 2 ;;
        -s|--start)             START_HEIGHT="$2";     shift 2 ;;
        -e|--end)               END_HEIGHT="$2";       shift 2 ;;
        --step)                 STEP="$2";             shift 2 ;;
        -c|--checkpoint-file)   CHECKPOINT_FILE="$2";  shift 2 ;;
        -o|--output)            OUTPUT_FILE="$2";      shift 2 ;;
        -m|--mode)              MODE="$2";             shift 2 ;;
        --dry-run)              DRY_RUN=true;          shift   ;;
        -v|--verbose)           VERBOSE=true;          shift   ;;
        -h|--help)              usage ;;
        *) die "Unknown option: $1" ;;
    esac
done

[[ "$MODE" =~ ^(append|full|raw)$ ]] || die "Invalid mode '$MODE'. Use: append, full, raw"

# ── Dependency check ──────────────────────────────────────────────────────────
command -v curl >/dev/null 2>&1 || die "curl is required but not found"
command -v jq   >/dev/null 2>&1 || die "jq is required but not found"

# ── RPC call ──────────────────────────────────────────────────────────────────
rpc_call() {
    local method="$1"
    local params="$2"
    local url="http://${HOST}:${PORT}/json_rpc"
    local body="{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"${method}\",\"params\":${params}}"

    curl -sS --max-time 10 \
         -H "Content-Type: application/json" \
         -d "$body" \
         "$url"
}

get_block_hash() {
    local height="$1"
    local resp
    resp=$(rpc_call "getblockheaderbyheight" "{\"height\":${height}}")
    local status hash
    status=$(echo "$resp" | jq -r '.result.status // "FAIL"')
    if [[ "$status" != "OK" ]]; then
        local err
        err=$(echo "$resp" | jq -r '.error.message // "unknown error"')
        die "RPC failed at height $height: $err"
    fi
    hash=$(echo "$resp" | jq -r '.result.block_header.hash')
    echo "$hash"
}

get_current_height() {
    local resp
    resp=$(rpc_call "getlastblockheader" "{}")
    local height
    height=$(echo "$resp" | jq -r '.result.block_header.height')
    echo "$height"
}

# ── Auto-detect START from existing checkpoint file ───────────────────────────
if [[ -z "$START_HEIGHT" ]]; then
    if [[ -n "$CHECKPOINT_FILE" && -f "$CHECKPOINT_FILE" ]]; then
        last=$(grep -oP '^\s*\{\K[0-9]+(?=\s*,)' "$CHECKPOINT_FILE" | tail -1)
        if [[ -n "$last" ]]; then
            START_HEIGHT=$(( last + STEP ))
            log "Auto start: last checkpoint at $last → starting at $START_HEIGHT"
        fi
    fi
    [[ -z "$START_HEIGHT" ]] && die "Cannot auto-detect start height. Use --start or --checkpoint-file."
fi

# ── Auto-detect END from daemon ───────────────────────────────────────────────
if [[ -z "$END_HEIGHT" ]]; then
    if $DRY_RUN; then
        info "Dry-run: cannot determine current height without RPC. Use --end."
        END_HEIGHT=$(( START_HEIGHT + STEP * 3 ))   # show a few dummy rows
    else
        log "Querying daemon for current height..."
        END_HEIGHT=$(get_current_height)
        log "Current daemon height: $END_HEIGHT"
    fi
fi

# Round END_HEIGHT down to the nearest multiple of STEP
END_HEIGHT=$(( (END_HEIGHT / STEP) * STEP ))

[[ "$END_HEIGHT" -lt "$START_HEIGHT" ]] && {
    info "Nothing to do: start=$START_HEIGHT end=$END_HEIGHT (daemon may not be ahead of last checkpoint)"
    exit 0
}

TOTAL=$(( (END_HEIGHT - START_HEIGHT) / STEP + 1 ))
info "Fetching $TOTAL checkpoints from height $START_HEIGHT to $END_HEIGHT (step=$STEP)"

# ── Output setup ──────────────────────────────────────────────────────────────
exec_output() {
    if [[ -n "$OUTPUT_FILE" ]]; then
        cat > "$OUTPUT_FILE"
    else
        cat
    fi
}

# ── License / header for 'full' mode ─────────────────────────────────────────
HEADER='// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
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
#include <initializer_list>

namespace CryptoNote
{
    struct CheckpointData
    {
        uint32_t index;
        const char *blockId;
    };

const std::initializer_list<CheckpointData> CHECKPOINTS = {'

FOOTER='};'

# ── Main fetch loop ───────────────────────────────────────────────────────────
generate_entries() {
    local h="$START_HEIGHT"
    local count=0

    while [[ "$h" -le "$END_HEIGHT" ]]; do
        count=$(( count + 1 ))
        if $DRY_RUN; then
            hash="0000000000000000000000000000000000000000000000000000000000000000"
        else
            log "[$count/$TOTAL] height $h..."
            hash=$(get_block_hash "$h")
        fi

        case "$MODE" in
            append|full) printf '        {%d,"%s"},\n' "$h" "$hash" ;;
            raw)         printf '%d %s\n' "$h" "$hash" ;;
        esac

        h=$(( h + STEP ))
    done
}

# ── Assemble output ───────────────────────────────────────────────────────────
# Note: C++11 allows a trailing comma on the last initializer-list entry,
# so all generated lines safely carry one — no post-processing needed.
{
    if [[ "$MODE" == "full" ]]; then
        echo "$HEADER"
        if [[ -n "$CHECKPOINT_FILE" && -f "$CHECKPOINT_FILE" ]]; then
            # Re-emit existing entries (lines matching the checkpoint pattern)
            grep -P '^\s*\{[0-9]+,"[0-9a-f]+"\},' "$CHECKPOINT_FILE" || true
        fi
        generate_entries
        echo "$FOOTER"
    else
        generate_entries
    fi
} | exec_output

if $DRY_RUN; then
    info "Dry-run complete. No real RPC calls were made."
fi

if [[ -n "$OUTPUT_FILE" ]]; then
    info "Written to: $OUTPUT_FILE"
fi
