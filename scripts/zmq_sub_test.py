#!/usr/bin/env python3
"""
Minimal ZMQ subscriber for Wrkz daemon testing.

Usage:
  python scripts/zmq_sub_test.py --endpoint tcp://127.0.0.1:17857
  python scripts/zmq_sub_test.py --endpoint tcp://127.0.0.1:17857 --topics hashblock chain_main

Requires:
  pip install pyzmq
"""

import argparse
import json
import signal
import sys
from datetime import datetime, timezone

import zmq


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Subscribe to Wrkz daemon ZMQ topics.")
    parser.add_argument(
        "--endpoint",
        default="tcp://127.0.0.1:17857",
        help="ZMQ PUB endpoint (default: tcp://127.0.0.1:17857)",
    )
    parser.add_argument(
        "--topics",
        nargs="+",
        default=["hashblock", "chain_main"],
        help="Topics to subscribe to (default: hashblock chain_main)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    ctx = zmq.Context()
    sock = ctx.socket(zmq.SUB)

    for topic in args.topics:
        sock.setsockopt_string(zmq.SUBSCRIBE, topic)

    sock.connect(args.endpoint)
    print(f"[+] Connected to {args.endpoint}")
    print(f"[+] Subscribed topics: {', '.join(args.topics)}")
    print("[+] Waiting for messages... (Ctrl+C to stop)")

    running = True

    def stop_handler(_sig, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop_handler)
    signal.signal(signal.SIGTERM, stop_handler)

    try:
        while running:
            try:
                topic = sock.recv_string(flags=zmq.NOBLOCK)
                payload = sock.recv_string(flags=zmq.NOBLOCK)
            except zmq.Again:
                continue

            ts = datetime.now(timezone.utc).isoformat()
            print(f"\n[{ts}] topic={topic}")

            try:
                obj = json.loads(payload)
                print(json.dumps(obj, indent=2, sort_keys=True))
            except json.JSONDecodeError:
                print(payload)
    finally:
        sock.close(0)
        ctx.term()
        print("\n[+] Stopped.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
