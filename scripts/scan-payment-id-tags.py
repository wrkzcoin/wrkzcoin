#!/usr/bin/env python3
# Copyright (c) 2018-2026, The WrkzCoin developers
#
# Please see the included LICENSE file for more information.

"""Count which payment ID tags actually appear in tx_extra on chain.

Encrypted short payment IDs live in the extra nonce sub-tag space, so picking
a sub-tag for them is only safe if nothing else on chain already uses it. This
answers both halves of that: which sub-tags carry a legacy plaintext short
payment ID, and which sub-tags are free.

A wallet handed a payment ID it cannot read credits the wrong account, so
neither half is a question to answer by assumption. Sub-tag 0x02 was assumed
free and turned out not to be.

This walks the chain through a daemon's /getrawblocks endpoint, pulls tx_extra
out of every non-coinbase transaction, and reports what it found. Coinbase
transactions are skipped - they never carry a payment ID.

Usage:
    python scripts/scan-payment-id-tags.py --daemon http://127.0.0.1:17856
    python scripts/scan-payment-id-tags.py --start-height 4000000 --verbose

Scanning from genesis works but downloads the whole chain, which takes hours.
Sub-tag 0x01 could only ever have been written by a build that already had it,
so --start-height set to the block where that release went live answers the
same question in a fraction of the time.

Exit status is 0 if no legacy short payment IDs were found, 1 if any were, and
2 if the scan could not be completed.
"""

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

# tx_extra top level tags
TX_EXTRA_TAG_PADDING = 0x00
TX_EXTRA_TAG_PUBKEY = 0x01
TX_EXTRA_NONCE = 0x02
TX_EXTRA_MERGE_MINING_TAG = 0x03
TX_EXTRA_TRANSACTION_POW_NONCE = 0x04

# extra nonce sub-tags
NONCE_LONG_PAYMENT_ID = 0x00
NONCE_SHORT_PAYMENT_ID = 0x01
NONCE_SHORT_PAYMENT_ID_PRERELEASE = 0x02
NONCE_ENCRYPTED_SHORT_PAYMENT_ID = 0x03
NONCE_ARBITRARY_DATA = 0x7F

SHORT_PAYMENT_ID_TAGS = (
    NONCE_SHORT_PAYMENT_ID,
    NONCE_SHORT_PAYMENT_ID_PRERELEASE,
    NONCE_ENCRYPTED_SHORT_PAYMENT_ID,
)

# Both 0x01 and 0x02 carry a plaintext short payment ID. Neither is reported
# by current software, so either one appearing in a new block means something
# is still running a build from before the encrypted scheme.
LEGACY_TAGS = (NONCE_SHORT_PAYMENT_ID, NONCE_SHORT_PAYMENT_ID_PRERELEASE)

TAG_NAMES = {
    NONCE_LONG_PAYMENT_ID: "long plaintext (0x00, 32 bytes)",
    NONCE_SHORT_PAYMENT_ID: "short plaintext (0x01, 8 bytes) [LEGACY]",
    NONCE_SHORT_PAYMENT_ID_PRERELEASE: "short plaintext (0x02, 8 bytes) [LEGACY, pre-release]",
    NONCE_ENCRYPTED_SHORT_PAYMENT_ID: "short encrypted (0x03, 8 bytes)",
}


class Reader:
    """Minimal cursor over a bytes object, with CryptoNote varint support."""

    def __init__(self, data):
        self.data = data
        self.pos = 0

    def remaining(self):
        return len(self.data) - self.pos

    def byte(self):
        if self.remaining() < 1:
            raise ValueError("truncated")
        value = self.data[self.pos]
        self.pos += 1
        return value

    def varint(self):
        value = 0
        shift = 0
        while True:
            b = self.byte()
            value |= (b & 0x7F) << shift
            if not b & 0x80:
                return value
            shift += 7
            if shift > 63:
                raise ValueError("varint too long")

    def take(self, count):
        if self.remaining() < count:
            raise ValueError("truncated")
        chunk = self.data[self.pos:self.pos + count]
        self.pos += count
        return chunk


def extract_extra(blob):
    """Pull tx_extra out of a serialised transaction.

    Layout is version, unlock time, inputs, outputs, then extra. We only need
    to walk far enough to reach extra, so signatures are ignored.
    """
    r = Reader(blob)

    r.varint()  # version
    r.varint()  # unlock time

    for _ in range(r.varint()):  # inputs
        tag = r.byte()
        if tag == 0xFF:  # coinbase
            r.varint()  # height
        elif tag == 0x02:  # key input
            r.varint()  # amount
            for _ in range(r.varint()):  # output index offsets
                r.varint()
            r.take(32)  # key image
        else:
            raise ValueError("unknown input tag 0x%02x" % tag)

    for _ in range(r.varint()):  # outputs
        r.varint()  # amount
        tag = r.byte()
        if tag != 0x02:  # key output
            raise ValueError("unknown output tag 0x%02x" % tag)
        r.take(32)  # key

    return r.take(r.varint())


def scan_nonce(nonce, counts, findings=None, height=None, extra=None, census=None):
    """Walk an extra nonce, counting payment ID sub-tags.

    field_index matters for diagnosis. The nonce sub-field format carries no
    length prefix for payment IDs, so once we walk past something we do not
    model we are just reading bytes and calling them tags. A hit at field 0 is
    a real payment ID; a hit further in may be an artifact of walking over
    unstructured data, and is recorded as such rather than trusted.
    """
    r = Reader(nonce)
    field_index = 0

    while r.remaining() > 0:
        offset = r.pos
        tag = r.byte()

        # Census of the first sub-tag in every nonce. Only field 0 is
        # trustworthy - past that we may be reading data as tags - but field 0
        # is enough to answer "which sub-tags are actually in use on chain",
        # which is what picking a free one safely depends on.
        if field_index == 0 and census is not None:
            census[tag] = census.get(tag, 0) + 1

        if tag == NONCE_LONG_PAYMENT_ID and r.remaining() >= 32:
            r.take(32)
            counts[NONCE_LONG_PAYMENT_ID] += 1
        elif tag in SHORT_PAYMENT_ID_TAGS and r.remaining() >= 8:
            value = r.take(8)
            counts[tag] += 1

            if findings is not None:
                findings.append({
                    "height": height,
                    "tag": tag,
                    "field_index": field_index,
                    "offset": offset,
                    "value": value.hex(),
                    "nonce": nonce.hex(),
                    "extra": extra.hex() if extra is not None else "",
                })
        elif tag == NONCE_ARBITRARY_DATA:
            r.take(r.varint())
        else:
            # Unknown sub-tag with no length prefix. We cannot know how far to
            # skip, so stop rather than report a misparse as a payment ID.
            return

        field_index += 1


def scan_extra(extra, counts, findings=None, height=None, census=None):
    """Walk tx_extra, descending into the nonce field."""
    r = Reader(extra)

    while r.remaining() > 0:
        tag = r.byte()

        if tag == TX_EXTRA_TAG_PADDING:
            return  # padding runs to the end
        elif tag == TX_EXTRA_TAG_PUBKEY:
            r.take(32)
        elif tag == TX_EXTRA_NONCE:
            scan_nonce(r.take(r.varint()), counts, findings, height, extra, census)
        elif tag == TX_EXTRA_MERGE_MINING_TAG:
            r.take(r.varint())
        elif tag == TX_EXTRA_TRANSACTION_POW_NONCE:
            r.take(8)
        else:
            return  # unknown top level tag, cannot skip safely


def rpc(daemon, path, payload, timeout):
    """POST when there is a payload, GET when there is not."""
    data = None if payload is None else json.dumps(payload).encode()

    request = urllib.request.Request(
        daemon.rstrip("/") + path,
        data=data,
        headers={"Content-Type": "application/json"},
    )

    with urllib.request.urlopen(request, timeout=timeout) as response:
        body = response.read()
        return json.loads(body.decode()), len(body)


def format_duration(seconds):
    seconds = int(seconds)

    if seconds < 60:
        return "%ds" % seconds

    if seconds < 3600:
        return "%dm %02ds" % (seconds // 60, seconds % 60)

    return "%dh %02dm" % (seconds // 3600, (seconds % 3600) // 60)


def format_bytes(count):
    for unit in ("B", "KB", "MB", "GB"):
        if count < 1024 or unit == "GB":
            return "%.1f %s" % (count, unit)
        count /= 1024.0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--daemon", default="http://127.0.0.1:17856", help="daemon RPC address")
    parser.add_argument("--start-height", type=int, default=0, help="height to start scanning from")
    parser.add_argument("--end-height", type=int, default=0, help="height to stop at (0 = chain tip)")
    parser.add_argument(
        "--batch", type=int, default=1000, help="blocks per request (the daemon caps this at --rpc-max-block-count)")
    parser.add_argument("--timeout", type=int, default=120, help="request timeout in seconds")
    parser.add_argument("--verbose", action="store_true", help="print a line for every request")
    args = parser.parse_args()

    counts = {tag: 0 for tag in (NONCE_LONG_PAYMENT_ID,) + SHORT_PAYMENT_ID_TAGS}

    # Ask the node how tall the chain is, so progress has a denominator. The
    # blocks response only carries a top block once the scan reaches the tip,
    # which is far too late to be useful as a progress indicator.
    try:
        info, _ = rpc(args.daemon, "/height", None, args.timeout)
        tip = int(info.get("height", 0)) - 1
    except (urllib.error.URLError, OSError, ValueError) as error:
        print("Could not reach the daemon at %s: %s" % (args.daemon, error), file=sys.stderr)
        return 2

    if tip < 0:
        print("Daemon reported no blocks.", file=sys.stderr)
        return 2

    end = min(args.end_height, tip) if args.end_height else tip

    print("Scanning %s" % args.daemon)
    print("  chain tip:    %d" % tip)
    print("  scan range:   %d to %d  (%d blocks)" % (args.start_height, end, max(0, end - args.start_height + 1)))
    print("  batch size:   %d blocks per request" % args.batch)

    if args.start_height == 0:
        print("\n  Note: scanning from genesis downloads the whole chain and will take a")
        print("  while. Sub-tag 0x01 can only exist in blocks mined after the release")
        print("  that introduced it, so --start-height will get you the same answer")
        print("  much faster.")

    print()

    height = args.start_height
    transactions = 0
    undecodable = 0
    downloaded = 0
    requests = 0
    findings = []
    census = {}
    started = time.time()
    last_print = 0.0
    interactive = sys.stdout.isatty()

    def progress(final=False):
        elapsed = max(0.001, time.time() - started)
        done = height - args.start_height
        total = max(1, end - args.start_height + 1)
        rate = done / elapsed
        remaining = max(0, total - done) / rate if rate > 0 else 0

        # height sits one past the last block we scanned, so clamp for display
        # rather than reporting 2501/2500 at the end of a finished scan.
        line = "  %d/%d  %5.1f%%  |  %d txs  |  %s  |  %.0f blk/s  |  eta %s" % (
            min(height, end), end, min(100.0, 100.0 * done / total), transactions,
            format_bytes(downloaded), rate, format_duration(remaining))

        if interactive and not final:
            # Pad to whatever the previous line was, so a shorter line cannot
            # leave a tail of the old one behind it.
            print(line.ljust(progress.width), end="\r", flush=True)
            progress.width = len(line)
        else:
            print(line.ljust(progress.width), flush=True)
            progress.width = 0

    progress.width = 0

    while height <= end:
        count = args.batch

        if end - height + 1 < count:
            count = end - height + 1

        try:
            body, size = rpc(
                args.daemon, "/getrawblocks", {"startHeight": height, "blockCount": count}, args.timeout)
        except (urllib.error.URLError, OSError) as error:
            if interactive:
                print()
            print("\nRequest failed at height %d: %s" % (height, error), file=sys.stderr)
            print("Scanned %d..%d before failing." % (args.start_height, height), file=sys.stderr)
            return 2

        requests += 1
        downloaded += size

        items = body.get("items", [])

        # An empty response means the node has nothing further to give us. That
        # is expected at the tip, and a problem anywhere else - say so rather
        # than reporting a clean result for a scan that stopped early.
        if not items:
            if height <= end:
                if interactive:
                    print()
                print(
                    "\nThe daemon returned no blocks at height %d, below the requested end %d.\n"
                    "The scan is incomplete, so its result cannot be trusted. The node may\n"
                    "still be syncing - check /height and re-run from this height."
                    % (height, end),
                    file=sys.stderr)
                return 2
            break

        for offset, item in enumerate(items):
            for tx_hex in item.get("transactions", []):
                transactions += 1

                try:
                    scan_extra(extract_extra(bytes.fromhex(tx_hex)), counts, findings, height + offset, census)
                except (ValueError, IndexError):
                    undecodable += 1

        # getrawblocks returns a contiguous run starting at the height we asked
        # for, but it may be short of what we requested because the response is
        # capped by a byte budget. Advance by what we actually received.
        height += len(items)

        if args.verbose:
            print("  request %d: %d blocks from %d, %s, %d txs so far"
                  % (requests, len(items), height - len(items), format_bytes(size), transactions), flush=True)
        elif time.time() - last_print > 1.0:
            last_print = time.time()
            progress()

    progress(final=True)

    print("\nScanned %d blocks (%d..%d) in %s, %d non-coinbase transactions, %s downloaded.\n"
          % (max(0, height - args.start_height), args.start_height, max(args.start_height, height - 1),
             format_duration(time.time() - started), transactions, format_bytes(downloaded)))

    for tag in (NONCE_LONG_PAYMENT_ID,) + SHORT_PAYMENT_ID_TAGS:
        print("  %-55s %d" % (TAG_NAMES[tag] + ":", counts[tag]))

    if undecodable:
        print("\n  %d transactions could not be decoded (counted as unknown)." % undecodable)

    if census:
        print("\nEvery extra nonce sub-tag seen in first position:\n")

        for tag in sorted(census):
            known = {
                0x00: "long plaintext payment ID",
                0x01: "short plaintext payment ID (legacy)",
                0x02: "short plaintext payment ID (pre-release)",
                0x03: "short encrypted payment ID",
                0x7F: "arbitrary data",
            }.get(tag, "unknown - not written by this codebase")

            print("  0x%02x  %8d   %s" % (tag, census[tag], known))

        free = [t for t in range(0x00, 0x80) if t not in census]

        print("\n  Lowest sub-tags with no occurrences on chain: %s"
              % ", ".join("0x%02x" % t for t in free[:8]))
        print("  Any new sub-tag must come from that list, and 0x7f is arbitrary data.")

    if findings:
        # A short payment ID that is not the first field in its nonce was found
        # by walking over bytes we do not model, so the "tag" may just be data.
        suspect = [f for f in findings if f["field_index"] != 0]

        print("\nEvery short payment ID found, with where it sat in the nonce:\n")

        for f in findings:
            print("  height %-9d %s  field %d at offset %d%s"
                  % (f["height"], TAG_NAMES[f["tag"]].split(" (")[0], f["field_index"], f["offset"],
                     "   <-- SUSPECT" if f["field_index"] != 0 else ""))
            print("    value %s" % f["value"])
            print("    nonce %s" % f["nonce"])
            print("    extra %s" % f["extra"])
            print()

        if suspect:
            print("  %d of these were not the first field in their nonce. The nonce sub-field" % len(suspect))
            print("  format has no length prefix for payment IDs, so once the walk passes")
            print("  something it does not model it is reading raw bytes and calling them")
            print("  tags. Treat those as probable false positives, not real payment IDs.")
            print()

    legacy = sum(counts[tag] for tag in LEGACY_TAGS)

    if legacy:
        print(
            "\nFOUND %d legacy plaintext short payment IDs.\n"
            "These would stop being reported once tag 0x01 is retired. Do not\n"
            "ship the change until you know who created them." % legacy
        )
        return 1

    print("\nNo legacy plaintext short payment IDs found - safe to retire tag 0x01.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
