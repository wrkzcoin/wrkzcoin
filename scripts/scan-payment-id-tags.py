#!/usr/bin/env python3
# Copyright (c) 2018-2026, The WrkzCoin developers
#
# Please see the included LICENSE file for more information.

"""Count which payment ID tags actually appear in tx_extra on chain.

Encrypted short payment IDs reuse the extra nonce sub-tag space. Before we
retire the legacy plaintext short payment ID (0x01) we need to know whether
anything ever used it, because a wallet handed a payment ID it cannot read
would credit the wrong account.

This walks the chain through a daemon's /getrawblocks endpoint, pulls tx_extra
out of every non-coinbase transaction, and reports what it found. Coinbase
transactions are skipped - they never carry a payment ID.

Usage:
    python scripts/scan-payment-id-tags.py --daemon http://127.0.0.1:17856
    python scripts/scan-payment-id-tags.py --start-height 4000000 --batch 200

Exit status is 0 if no legacy short payment IDs were found, 1 if any were.
"""

import argparse
import json
import sys
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
NONCE_ENCRYPTED_SHORT_PAYMENT_ID = 0x02
NONCE_ARBITRARY_DATA = 0x7F

TAG_NAMES = {
    NONCE_LONG_PAYMENT_ID: "long plaintext (0x00, 32 bytes)",
    NONCE_SHORT_PAYMENT_ID: "short plaintext (0x01, 8 bytes) [LEGACY]",
    NONCE_ENCRYPTED_SHORT_PAYMENT_ID: "short encrypted (0x02, 8 bytes)",
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


def scan_nonce(nonce, counts):
    """Walk an extra nonce, counting payment ID sub-tags."""
    r = Reader(nonce)

    while r.remaining() > 0:
        tag = r.byte()

        if tag == NONCE_LONG_PAYMENT_ID and r.remaining() >= 32:
            r.take(32)
            counts[NONCE_LONG_PAYMENT_ID] += 1
        elif tag in (NONCE_SHORT_PAYMENT_ID, NONCE_ENCRYPTED_SHORT_PAYMENT_ID) and r.remaining() >= 8:
            r.take(8)
            counts[tag] += 1
        elif tag == NONCE_ARBITRARY_DATA:
            r.take(r.varint())
        else:
            # Unknown sub-tag with no length prefix. We cannot know how far to
            # skip, so stop rather than report a misparse as a payment ID.
            return


def scan_extra(extra, counts):
    """Walk tx_extra, descending into the nonce field."""
    r = Reader(extra)

    while r.remaining() > 0:
        tag = r.byte()

        if tag == TX_EXTRA_TAG_PADDING:
            return  # padding runs to the end
        elif tag == TX_EXTRA_TAG_PUBKEY:
            r.take(32)
        elif tag == TX_EXTRA_NONCE:
            scan_nonce(r.take(r.varint()), counts)
        elif tag == TX_EXTRA_MERGE_MINING_TAG:
            r.take(r.varint())
        elif tag == TX_EXTRA_TRANSACTION_POW_NONCE:
            r.take(8)
        else:
            return  # unknown top level tag, cannot skip safely


def rpc(daemon, path, payload, timeout):
    request = urllib.request.Request(
        daemon.rstrip("/") + path,
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
    )

    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--daemon", default="http://127.0.0.1:17856", help="daemon RPC address")
    parser.add_argument("--start-height", type=int, default=0, help="height to start scanning from")
    parser.add_argument("--end-height", type=int, default=0, help="height to stop at (0 = chain tip)")
    parser.add_argument("--batch", type=int, default=100, help="blocks per request")
    parser.add_argument("--timeout", type=int, default=60, help="request timeout in seconds")
    args = parser.parse_args()

    counts = {
        NONCE_LONG_PAYMENT_ID: 0,
        NONCE_SHORT_PAYMENT_ID: 0,
        NONCE_ENCRYPTED_SHORT_PAYMENT_ID: 0,
    }

    height = args.start_height
    transactions = 0
    undecodable = 0
    top = None

    print("Scanning %s from height %d..." % (args.daemon, height))

    while True:
        if args.end_height and height >= args.end_height:
            break

        count = args.batch

        if args.end_height:
            count = min(count, args.end_height - height)

        try:
            body = rpc(args.daemon, "/getrawblocks", {"startHeight": height, "blockCount": count}, args.timeout)
        except (urllib.error.URLError, OSError) as error:
            print("\nRequest failed at height %d: %s" % (height, error), file=sys.stderr)
            return 2

        items = body.get("items", [])

        if body.get("topBlock"):
            top = body["topBlock"].get("height")

        if not items:
            break

        for item in items:
            for tx_hex in item.get("transactions", []):
                transactions += 1

                try:
                    scan_extra(extract_extra(bytes.fromhex(tx_hex)), counts)
                except (ValueError, IndexError):
                    undecodable += 1

        height += len(items)

        if top:
            print("  height %d / %d  (%d transactions)" % (height, top, transactions), end="\r", flush=True)

    print(" " * 70, end="\r")
    print("\nScanned to height %d, %d non-coinbase transactions.\n" % (height, transactions))

    for tag in (NONCE_LONG_PAYMENT_ID, NONCE_SHORT_PAYMENT_ID, NONCE_ENCRYPTED_SHORT_PAYMENT_ID):
        print("  %-45s %d" % (TAG_NAMES[tag] + ":", counts[tag]))

    if undecodable:
        print("\n  %d transactions could not be decoded (counted as unknown)." % undecodable)

    legacy = counts[NONCE_SHORT_PAYMENT_ID]

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
