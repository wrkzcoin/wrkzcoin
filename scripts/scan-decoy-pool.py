#!/usr/bin/env python3
# Copyright (c) 2018-2026, The WrkzCoin developers
#
# Please see the included LICENSE file for more information.

"""Measure how deep the decoy pool is per denomination, and what that caps a send at.

A CryptoNote input can only be mixed with outputs of the *exact same*
denomination. Building a ring of R for an input of amount d therefore needs at
least R mature outputs of exactly d on chain - R-1 decoys plus your own, and
the daemon may hand your own output back as one of the R it returns, so R is
the floor and not R-1.

Amounts are decomposed into d * 10^k pieces, so a large transfer lands on the
high denominations, and those are exactly the ones that may not have R outputs
in existence yet. One thin denomination fails the whole transaction: the wallet
asks for every input's decoys in a single request and there is no partial
success. That is why a big send is far more likely to fail than a small one.

Today MINIMUM_MIXIN_V6 is 1, so a wallet that cannot build a ring of 8 falls
back down to whatever the chain supports and the send still goes through, at
less privacy. The comment in CryptoNoteConfig.h says the next fork intends to
raise MINIMUM_MIXIN to match MAXIMUM_MIXIN. That closes the fallback, and every
output whose denomination has fewer than 8 mature outputs becomes permanently
unspendable. This script measures how much of the chain that would be, before
such a fork is scheduled rather than after.

It reports three things:

  1. Per denomination, how many mature decoys the daemon can actually serve,
     and whether that denomination survives a fixed ring size.
  2. The ceiling on a single transaction at a given ring size - how many inputs
     fit in a block, times the deepest denomination that can still be mixed.
     Ring size drives this hard: an input costs 112 + 68 * mixin bytes, so
     going from ring 2 to ring 8 cuts the input count by roughly 3.3x.
  3. For a specific amount, which output denominations it decomposes into, and
     whether the receiver will be able to spend them again afterwards.

Usage:
    python scripts/scan-decoy-pool.py --daemon http://127.0.0.1:17856
    python scripts/scan-decoy-pool.py --daemon http://node-fin.wrkz.work:17856 --ring 8
    python scripts/scan-decoy-pool.py --amount 50000000 --ring 8
    python scripts/scan-decoy-pool.py --legacy --probe 128 --json

Probing only counts outputs the daemon would actually serve as decoys, which
excludes anything newer than CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW blocks and
anything still locked - the same filter the wallet hits when it sends.

Remote daemons rate limit to --rpc-max-rpm requests per minute per address
(240 by default), so amounts are batched into one request each and the script
throttles itself. Point it at your own node where you can.

Exit status is 0 if every denomination can support the requested ring size,
1 if any cannot, and 2 if the scan could not be completed.
"""

import argparse
import json
import math
import sys
import time
import urllib.error
import urllib.request

# ---------------------------------------------------------------------------
# Mirrored from src/config/CryptoNoteConfig.h and src/config/WalletConfig.h.
# Keep these in step with the C++ - every ceiling below is derived from them.
# ---------------------------------------------------------------------------

TICKER = "WRKZ"
DECIMAL_POINT = 2
MONEY_SUPPLY = 50_000_000_000_000

# Largest denomination a wallet will create (since MAX_OUTPUT_SIZE_HEIGHT).
MAX_OUTPUT_SIZE_CLIENT = 5_000_000_000_00
# Largest a block will carry at all. Outputs between the two exist on chain
# from before height 800000 and are only reachable with --legacy.
MAX_OUTPUT_SIZE_NODE = 125_000_000_000_00

NORMAL_TX_MAX_OUTPUT_COUNT_V1 = 90

DIFFICULTY_TARGET = 60
MAX_BLOCK_SIZE_INITIAL = 100000
MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR = 100 * 1024
MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR = 365 * 24 * 60 * 60 // DIFFICULTY_TARGET
COINBASE_BLOB_RESERVED_SIZE = 600

MINED_MONEY_UNLOCK_WINDOW = 40

FEE_PER_BYTE_CHUNK_SIZE = 256
FEE_PER_BYTE_CHUNK_SIZE_V2 = 128
MINIMUM_FEE_PER_BYTE_V1 = 500.0 / FEE_PER_BYTE_CHUNK_SIZE
MINIMUM_FEE_PER_BYTE_V2 = 10.0 / FEE_PER_BYTE_CHUNK_SIZE_V2
MINIMUM_FEE_PER_BYTE_V1_HEIGHT = 832000
MINIMUM_FEE_PER_BYTE_V2_HEIGHT = 1500000

TRANSACTION_POW_HEIGHT = 1123000
TRANSACTION_POW_HEIGHT_DYN_V1 = 1200000
TRANSACTION_POW_DIFFICULTY_DYN_V1 = 40000
MULTIPLIER_TX_POW_PER_IO_V1 = 1000
MULTIPLIER_TX_POW_FACTORED_OUT_V1 = 4
TRANSACTION_POW_PASS_WITH_FEE = 10000
TRANSACTION_POW_PASS_WITH_FEE_HEIGHT = 1500000

# (height, minimum mixin, maximum mixin, default mixin), highest first.
MIXIN_LIMITS = (
    (4300000, 1, 7, 7),
    (1000000, 1, 1, 1),
    (658500, 1, 3, 3),
    (430000, 0, 7, 3),
    (302400, 3, 7, 3),
    (10000, 0, 30, 3),
)

MIXIN_LIMITS_V6_HEIGHT = 4300000

# Byte model from Utilities::getApproximateMaximumInputCount.
KEY_IMAGE_SIZE = 32
OUTPUT_KEY_SIZE = 32
AMOUNT_SIZE = 8 + 2
GLOBAL_INDEXES_VECTOR_SIZE_SIZE = 1
GLOBAL_INDEXES_INITIAL_VALUE_SIZE = 4
GLOBAL_INDEXES_DIFFERENCE_SIZE = 4
SIGNATURE_SIZE = 64
EXTRA_TAG_SIZE = 1
INPUT_TAG_SIZE = 1
OUTPUT_TAG_SIZE = 1
PUBLIC_KEY_SIZE = 32
TRANSACTION_VERSION_SIZE = 1
TRANSACTION_UNLOCK_TIME_SIZE = 8


# ---------------------------------------------------------------------------
# Consensus arithmetic, mirrored from the C++ so the numbers below are the
# numbers the daemon and wallet will actually use.
# ---------------------------------------------------------------------------


def mixin_range(height):
    """Utilities::getMixinAllowableRange - returns (min, max, default)."""
    for fork_height, minimum, maximum, default in MIXIN_LIMITS:
        if height >= fork_height:
            return minimum, maximum, default

    return 0, None, 3


def planned_ring(height):
    """Largest ring the chain demands now or at any fork already scheduled.

    Defaulting to the *current* era's maximum answers the wrong question
    entirely: before the V6 height the chain still caps mixin at 1, so the
    scan would grade every denomination against a ring of 2, pass everything,
    and say nothing about the ring of 8 that is the reason to run it. Look
    ahead to the forks in the table instead, so the default is the ring the
    chain is heading for rather than the one it has left behind.
    """
    _, maximum, _ = mixin_range(height)

    best = maximum or 0

    for fork_height, _, fork_maximum, _ in MIXIN_LIMITS:
        if fork_height > height and fork_maximum is not None:
            best = max(best, fork_maximum)

    return best + 1


def max_tx_size(height):
    """Utilities::getMaxTxSize."""
    growth = (height * MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR) // MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR

    return min(MAX_BLOCK_SIZE_INITIAL + growth, 125000) - COINBASE_BLOB_RESERVED_SIZE


def input_size(mixin):
    """Serialised size of one key input carrying `mixin` decoys."""
    return (
        INPUT_TAG_SIZE
        + AMOUNT_SIZE
        + KEY_IMAGE_SIZE
        + SIGNATURE_SIZE
        + GLOBAL_INDEXES_VECTOR_SIZE_SIZE
        + GLOBAL_INDEXES_INITIAL_VALUE_SIZE
        + mixin * (GLOBAL_INDEXES_DIFFERENCE_SIZE + SIGNATURE_SIZE)
    )


def max_input_count(size_budget, output_count, mixin):
    """Utilities::getApproximateMaximumInputCount."""
    outputs_size = output_count * (OUTPUT_TAG_SIZE + OUTPUT_KEY_SIZE + AMOUNT_SIZE)
    header_size = TRANSACTION_VERSION_SIZE + TRANSACTION_UNLOCK_TIME_SIZE + EXTRA_TAG_SIZE + PUBLIC_KEY_SIZE

    available = size_budget - header_size - outputs_size

    if available <= 0:
        return 0

    return available // input_size(mixin)


def transaction_fee(size, height):
    """Utilities::getTransactionFee at the minimum fee per byte."""
    if height < MINIMUM_FEE_PER_BYTE_V1_HEIGHT:
        return 50000

    if height < MINIMUM_FEE_PER_BYTE_V2_HEIGHT:
        chunks = math.ceil(size / FEE_PER_BYTE_CHUNK_SIZE)
        return int(chunks * MINIMUM_FEE_PER_BYTE_V1 * FEE_PER_BYTE_CHUNK_SIZE)

    chunks = math.ceil(size / FEE_PER_BYTE_CHUNK_SIZE_V2)
    return int(chunks * MINIMUM_FEE_PER_BYTE_V2 * FEE_PER_BYTE_CHUNK_SIZE_V2)


def transaction_pow_difficulty(height, inputs, outputs):
    """CryptoNote::transactionPoWDifficulty for a non-fusion transaction."""
    if height < TRANSACTION_POW_HEIGHT:
        return 0

    if height <= TRANSACTION_POW_HEIGHT_DYN_V1:
        return TRANSACTION_POW_DIFFICULTY_DYN_V1

    return TRANSACTION_POW_DIFFICULTY_DYN_V1 + (
        inputs + outputs * MULTIPLIER_TX_POW_FACTORED_OUT_V1
    ) * MULTIPLIER_TX_POW_PER_IO_V1


def split_amount_into_denominations(amount, prevent_too_large_outputs=True):
    """SendTransaction::splitAmountIntoDenominations."""
    split_amounts = []
    multiplier = 1

    while amount > 0:
        denomination = multiplier * (amount % 10)

        if denomination > MAX_OUTPUT_SIZE_CLIENT and prevent_too_large_outputs:
            count = 10
            piece = denomination // 10

            while piece > MAX_OUTPUT_SIZE_CLIENT:
                piece //= 10
                count *= 10

            split_amounts.extend([piece] * count)
        elif denomination != 0:
            split_amounts.append(denomination)

        amount //= 10
        multiplier *= 10

    return split_amounts


def single_destination_ceiling():
    """Smallest amount whose decomposition alone busts the 90 output limit.

    A digit above MAX_OUTPUT_SIZE_CLIENT is chunked into 10, then 100, equal
    pieces, and the second chunking is what overflows. Every lower digit adds
    at most one output each, so the threshold always lands on a d * 10^k.
    """
    multiplier = 1

    while multiplier < 10 ** 19:
        for digit in range(1, 10):
            amount = digit * multiplier

            if len(split_amount_into_denominations(amount)) > NORMAL_TX_MAX_OUTPUT_COUNT_V1:
                return amount

        multiplier *= 10

    return None


def pretty_amounts(ceiling):
    """Every d * 10^k denomination up to and including `ceiling`."""
    amounts = []
    multiplier = 1

    while multiplier <= ceiling:
        for digit in range(1, 10):
            value = digit * multiplier

            if value <= ceiling:
                amounts.append(value)

        multiplier *= 10

    return sorted(amounts)


# ---------------------------------------------------------------------------
# Presentation
# ---------------------------------------------------------------------------


def format_amount(atomic):
    whole, fraction = divmod(int(atomic), 10 ** DECIMAL_POINT)

    return "{:,}.{:0{}d}".format(whole, fraction, DECIMAL_POINT)


def parse_amount(text, atomic):
    """Accept either whole coins ("1500.25") or raw atomic units."""
    text = text.replace(",", "").replace("_", "").strip()

    if atomic:
        return int(text)

    if "." in text:
        whole, _, fraction = text.partition(".")
        fraction = (fraction + "0" * DECIMAL_POINT)[:DECIMAL_POINT]
    else:
        whole, fraction = text, "0" * DECIMAL_POINT

    return int(whole or "0") * 10 ** DECIMAL_POINT + int(fraction or "0")


# ---------------------------------------------------------------------------
# RPC
# ---------------------------------------------------------------------------


class Daemon:
    def __init__(self, url, token, timeout, rpm):
        self.url = url.rstrip("/")
        self.token = token
        self.timeout = timeout
        self.min_interval = 0.0 if rpm <= 0 else 60.0 / rpm
        self.last_call = 0.0
        self.calls = 0

    def call(self, path, payload=None):
        if self.min_interval:
            wait = self.min_interval - (time.monotonic() - self.last_call)

            if wait > 0:
                time.sleep(wait)

        headers = {"Content-Type": "application/json"}

        if self.token:
            headers["X-API-Key"] = self.token

        data = None if payload is None else json.dumps(payload).encode()
        request = urllib.request.Request(self.url + path, data=data, headers=headers)

        self.last_call = time.monotonic()
        self.calls += 1

        with urllib.request.urlopen(request, timeout=self.timeout) as response:
            return json.loads(response.read().decode())

    def info(self):
        return self.call("/info")

    def random_outs(self, amounts, count):
        """Returns {amount: number of mature decoys the daemon served}.

        A denomination the daemon has nothing for is answered with an empty
        list rather than an error, so a missing entry and a zero are the same
        answer and both mean "no ring can be built here".
        """
        response = self.call("/getrandom_outs", {"amounts": list(amounts), "outs_count": count})

        if response.get("status") != "OK":
            raise RuntimeError("getrandom_outs failed: {}".format(response.get("error", response)))

        found = {amount: 0 for amount in amounts}

        for entry in response.get("outs", []):
            found[entry["amount"]] = len(entry.get("outs", []))

        return found


# ---------------------------------------------------------------------------
# The scan
# ---------------------------------------------------------------------------


def classify(found, ring, probe):
    """What this denomination can do, given `found` mature outputs.

    A ring of R needs R outputs in existence: R-1 decoys plus your own, and
    the daemon may return your own as one of the R it hands back.
    """
    if found == 0:
        # Nobody can be holding an output that was never created, so this is
        # an unused denomination and not money anyone is about to lose. Kept
        # separate from DEAD because conflating the two turns an empty part of
        # the amount space into a fork blocker it is not.
        return "EMPTY", "no outputs of this denomination exist - nothing held here to strand"

    if found == 1:
        return "DEAD", "only one output exists - the decoy filter strips your own, leaving none"

    if found < ring:
        return "THIN", "ring {} max - fails once the minimum ring is raised to {}".format(found, ring)

    if found < probe:
        return "OK", "ring {} available, no margin above it".format(found)

    return "OK", "ring {}+ available".format(probe)


def scan(daemon, amounts, ring, probe, batch, verbose):
    results = {}

    for start in range(0, len(amounts), batch):
        chunk = amounts[start:start + batch]

        if verbose:
            print(
                "  probing {}-{} of {} denominations...".format(
                    start + 1, start + len(chunk), len(amounts)
                ),
                file=sys.stderr,
            )

        found = daemon.random_outs(chunk, probe)

        for amount in chunk:
            count = found.get(amount, 0)
            verdict, note = classify(count, ring, probe)
            results[amount] = (count, verdict, note)

    return results


def report_denominations(results, probe, show_all):
    print("Denomination decoy depth")
    print("=" * 108)
    print("{:>16}  {:>22}  {:>7}  {:<6}  {}".format("atomic", TICKER, "decoys", "state", "note"))
    print("-" * 108)

    hidden = 0

    for amount in sorted(results):
        count, verdict, note = results[amount]

        if verdict in ("OK", "EMPTY") and not show_all:
            hidden += 1
            continue

        print(
            "{:>16}  {:>22}  {:>7}  {:<6}  {}".format(
                amount,
                format_amount(amount),
                "{}{}".format(count, "+" if count >= probe else ""),
                verdict,
                note,
            )
        )

    if hidden:
        print("... {} healthy or unused denominations hidden, pass --all to list them".format(hidden))

    print()


def report_ceilings(results, height, ring, output_count):
    """The headline: what one transaction can carry at each ring size."""
    budget = max_tx_size(height)

    print("Transaction ceilings at height {:,} (max tx size {:,} bytes)".format(height, budget))
    print("=" * 108)

    rings = sorted({2, ring, 8})

    print(
        "{:>6}  {:>9}  {:>12}  {:>22}  {:>26}  {:>10}".format(
            "ring", "b/input", "max inputs", "deepest usable", "max per tx (" + TICKER + ")", "fee"
        )
    )
    print("-" * 108)

    for candidate in rings:
        mixin = candidate - 1
        inputs = max_input_count(budget, output_count, mixin)

        viable = [amount for amount, (count, _, _) in results.items() if count >= candidate]
        deepest = max(viable) if viable else 0

        if inputs and deepest:
            fee = transaction_fee(budget, height)
            total = inputs * deepest
            # Past the money supply the product stops describing anything real,
            # so say that rather than print a number no wallet could ever hold.
            ceiling = "> supply" if total > MONEY_SUPPLY else format_amount(total)
        else:
            fee = 0
            ceiling = "n/a"

        print(
            "{:>6}  {:>9}  {:>12,}  {:>22}  {:>26}  {:>10}".format(
                candidate,
                input_size(mixin),
                inputs,
                format_amount(deepest) if deepest else "none",
                ceiling,
                format_amount(fee) if fee else "n/a",
            )
        )

    print()
    print("  'deepest usable' is the largest denomination with enough outputs to build that")
    print("  ring. 'max per tx' assumes you hold that many inputs at it - a ceiling, not a")
    print("  balance. A wallet holding smaller denominations hits the input count first.")

    # The other end of the transaction: the decomposition has to fit in 90
    # outputs, and that bites long before the input count does.
    overflow = single_destination_ceiling()

    if overflow:
        print()
        print(
            "  Output count: a single destination of {} {} or more decomposes into".format(
                format_amount(overflow), TICKER
            )
        )
        print(
            "  {} outputs and is rejected by the {} output limit, whatever the ring size.".format(
                len(split_amount_into_denominations(overflow)), NORMAL_TX_MAX_OUTPUT_COUNT_V1
            )
        )

    mixin = ring - 1
    inputs = max_input_count(budget, output_count, mixin)
    outputs = min(NORMAL_TX_MAX_OUTPUT_COUNT_V1, output_count)
    difficulty = transaction_pow_difficulty(height, inputs, outputs)
    fee = transaction_fee(budget, height)

    if height >= TRANSACTION_POW_PASS_WITH_FEE_HEIGHT:
        print()
        print(
            "  A full {:,} byte transaction pays {} {} at the minimum fee rate.".format(
                budget, format_amount(fee), TICKER
            )
        )

        if fee < TRANSACTION_POW_PASS_WITH_FEE:
            print(
                "  That is below the {} {} that skips tx PoW, so it must still grind".format(
                    format_amount(TRANSACTION_POW_PASS_WITH_FEE), TICKER
                )
            )
            print(
                "  difficulty {:,} of cn_upx. Overpay to {} {} to skip it.".format(
                    difficulty, format_amount(TRANSACTION_POW_PASS_WITH_FEE), TICKER
                )
            )
        else:
            print("  That clears the tx PoW fee bypass.")

    print()


def report_amount(results, height, amount, ring, sender_ring):
    """What sending `amount` produces, and whether it stays spendable."""
    print("Sending {} {}".format(format_amount(amount), TICKER))
    print("=" * 108)

    denominations = split_amount_into_denominations(amount)

    print("  decomposes into {} output(s)".format(len(denominations)))

    if len(denominations) > NORMAL_TX_MAX_OUTPUT_COUNT_V1:
        print(
            "  REJECTED: over the {} output limit before change is even added.".format(
                NORMAL_TX_MAX_OUTPUT_COUNT_V1
            )
        )
        print("  Split this across several transactions.")
        print()
        return False

    headroom = NORMAL_TX_MAX_OUTPUT_COUNT_V1 - len(denominations)
    print("  {} of the {} output slots left for change".format(headroom, NORMAL_TX_MAX_OUTPUT_COUNT_V1))
    print()

    print("  Outputs the receiver ends up holding:")
    print("  {:>16}  {:>22}  {:>6}  {:>7}  {}".format("atomic", TICKER, "count", "decoys", "spendable later?"))
    print("  " + "-" * 104)

    tally = {}
    for denomination in denominations:
        tally[denomination] = tally.get(denomination, 0) + 1

    at_risk = False

    for denomination in sorted(tally):
        count = tally[denomination]
        found, verdict, _ = results.get(denomination, (None, "?", ""))

        if found is None:
            note = "not probed (rerun without --legacy filtering)"
        elif found == 0:
            note = "NO - no decoys exist, receiver cannot spend this"
            at_risk = True
        elif found == 1:
            note = "NO - one output on chain, the receiver's own"
            at_risk = True
        elif found < ring:
            note = "ONLY AT RING {} - dies if the minimum ring becomes {}".format(found, ring)
            at_risk = True
        else:
            note = "yes"

        print(
            "  {:>16}  {:>22}  {:>6}  {:>7}  {}".format(
                denomination,
                format_amount(denomination),
                count,
                "-" if found is None else found,
                note,
            )
        )

    print()

    # How many inputs the sender needs, at best.
    viable = [d for d, (c, _, _) in results.items() if c >= sender_ring]
    deepest = max(viable) if viable else 0
    budget = max_tx_size(height)
    per_tx = max_input_count(budget, len(denominations) + 1, sender_ring - 1)

    if deepest and per_tx:
        needed = math.ceil(amount / deepest)
        transactions = math.ceil(needed / per_tx)

        print(
            "  At ring {}, best case you need {:,} input(s) of {} {}".format(
                sender_ring, needed, format_amount(deepest), TICKER
            )
        )
        print(
            "  and {:,} fit per transaction, so at least {} transaction(s).".format(per_tx, transactions)
        )
        print("  A wallet holding smaller denominations needs proportionally more.")
        print()

    return not at_risk


def main():
    parser = argparse.ArgumentParser(
        description="Measure decoy pool depth per denomination and the send ceilings it implies.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Exit 0 if every denomination supports the requested ring, 1 if any does not, 2 on error.",
    )

    parser.add_argument(
        "--daemon",
        default="http://127.0.0.1:17856",
        help="daemon RPC base URL (default: %(default)s)",
    )
    parser.add_argument("--token", default=None, help="RPC access token, sent as X-API-Key")
    parser.add_argument("--timeout", type=float, default=30.0, help="per request timeout in seconds")
    parser.add_argument(
        "--rpm",
        type=int,
        default=120,
        help="throttle to this many requests per minute; remote daemons allow 240 (default: %(default)s)",
    )
    parser.add_argument(
        "--ring",
        type=int,
        default=None,
        help="ring size to test against (default: the chain's current maximum + 1)",
    )
    parser.add_argument(
        "--probe",
        type=int,
        default=32,
        help="ask for this many decoys per denomination, to see margin above the ring (default: %(default)s)",
    )
    parser.add_argument(
        "--batch",
        type=int,
        default=16,
        help="denominations per RPC request (default: %(default)s)",
    )
    parser.add_argument(
        "--outputs",
        type=int,
        default=2,
        help="output count to assume when sizing a transaction (default: %(default)s)",
    )
    parser.add_argument(
        "--amount",
        default=None,
        help="also check whether this amount can be sent and re-spent, in whole " + TICKER,
    )
    parser.add_argument(
        "--atomic",
        action="store_true",
        help="read --amount as atomic units instead of whole " + TICKER,
    )
    parser.add_argument(
        "--legacy",
        action="store_true",
        help="also probe denominations above MAX_OUTPUT_SIZE_CLIENT, which only pre-800000 blocks hold",
    )
    parser.add_argument("--all", action="store_true", help="list healthy denominations too")
    parser.add_argument("--json", action="store_true", help="emit machine readable output instead")
    parser.add_argument("--verbose", action="store_true", help="log progress to stderr")

    args = parser.parse_args()

    daemon = Daemon(args.daemon, args.token, args.timeout, args.rpm)

    try:
        info = daemon.info()
    except (urllib.error.URLError, OSError, ValueError) as error:
        print("Could not reach {}: {}".format(args.daemon, error), file=sys.stderr)
        return 2

    height = int(info.get("network_height") or info.get("height") or 0)

    if height == 0:
        print("Daemon reported no height, refusing to guess the consensus rules.", file=sys.stderr)
        return 2

    minimum, maximum, default = mixin_range(height)
    ring = args.ring if args.ring else planned_ring(height)
    probe = max(args.probe, ring)

    ceiling = MAX_OUTPUT_SIZE_NODE if args.legacy else MAX_OUTPUT_SIZE_CLIENT
    amounts = pretty_amounts(ceiling)

    if not args.json:
        print()
        print("Node    {}  ({})".format(args.daemon, info.get("version", "unknown version")))
        print(
            "Height  {:,} of {:,}{}".format(
                int(info.get("height", 0)), height, "" if info.get("synced") else "   NOT SYNCED"
            )
        )
        current_ring = (maximum or 0) + 1

        print(
            "Mixin   min {}, max {}, default {}   (chain allows ring {} to {} today)".format(
                minimum, maximum, default, minimum + 1, current_ring
            )
        )
        print(
            "Testing ring {}{}".format(
                ring,
                "" if ring <= current_ring else "   <- ahead of the chain, which is the point",
            )
        )

        if height < MIXIN_LIMITS_V6_HEIGHT:
            print(
                "Fork    V6 ring 8 activates at {:,} - {:,} blocks away (~{:.1f} days)".format(
                    MIXIN_LIMITS_V6_HEIGHT,
                    MIXIN_LIMITS_V6_HEIGHT - height,
                    (MIXIN_LIMITS_V6_HEIGHT - height) * DIFFICULTY_TARGET / 86400.0,
                )
            )
        else:
            print("Fork    V6 ring 8 active since {:,}".format(MIXIN_LIMITS_V6_HEIGHT))

        print(
            "Probe   {} denominations, {} decoys each, ignoring outputs newer than {} blocks".format(
                len(amounts), probe, MINED_MONEY_UNLOCK_WINDOW
            )
        )
        print()

    try:
        results = scan(daemon, amounts, ring, probe, args.batch, args.verbose)
    except urllib.error.HTTPError as error:
        if error.code == 429:
            print("Rate limited by the daemon. Lower --rpm and retry.", file=sys.stderr)
        elif error.code == 401:
            print("Unauthorized. This daemon needs --token.", file=sys.stderr)
        else:
            print("RPC error {}: {}".format(error.code, error.reason), file=sys.stderr)
        return 2
    except (urllib.error.URLError, OSError, ValueError, RuntimeError) as error:
        print("Scan failed: {}".format(error), file=sys.stderr)
        return 2

    dead = [a for a, (_, verdict, _) in results.items() if verdict == "DEAD"]
    thin = [a for a, (_, verdict, _) in results.items() if verdict == "THIN"]
    empty = [a for a, (_, verdict, _) in results.items() if verdict == "EMPTY"]

    amount = parse_amount(args.amount, args.atomic) if args.amount else None

    if args.json:
        print(
            json.dumps(
                {
                    "daemon": args.daemon,
                    "height": height,
                    "ring_tested": ring,
                    "probe": probe,
                    "mixin": {"minimum": minimum, "maximum": maximum, "default": default},
                    "max_tx_size": max_tx_size(height),
                    "max_inputs": {
                        str(r): max_input_count(max_tx_size(height), args.outputs, r - 1)
                        for r in sorted({2, ring, 8})
                    },
                    "denominations": {
                        str(a): {"decoys": c, "state": v, "note": n} for a, (c, v, n) in sorted(results.items())
                    },
                    "dead": sorted(dead),
                    "thin": sorted(thin),
                    "empty": sorted(empty),
                    "amount": None
                    if amount is None
                    else {
                        "atomic": amount,
                        "outputs": split_amount_into_denominations(amount),
                    },
                },
                indent=2,
            )
        )
        return 1 if dead or thin else 0

    report_denominations(results, probe, args.all)
    report_ceilings(results, height, ring, args.outputs)

    if amount is not None:
        report_amount(results, height, amount, ring, ring)

    healthy = len(results) - len(dead) - len(thin) - len(empty)
    shallowest = min(
        (count for count, verdict, _ in results.values() if verdict == "OK"), default=0
    )

    print("Summary")
    print("=" * 108)
    print("  {} denominations probed, {} requests".format(len(results), daemon.calls))
    print("  {} healthy at ring {}".format(healthy, ring))
    print(
        "  {} THIN - hold outputs but fewer than {}, so they need the fallback".format(len(thin), ring)
    )
    print("  {} DEAD - a single output, unspendable at any ring size".format(len(dead)))
    print("  {} EMPTY - no outputs exist, so nothing is held there".format(len(empty)))

    if healthy:
        print()
        print(
            "  Thinnest healthy denomination has {} outputs, {} above the {} a ring of".format(
                shallowest, shallowest - ring, ring
            )
        )
        print("  {} needs. That is the margin the fork actually rests on.".format(ring))

    print()

    if dead or thin:
        print(
            "  Raising MINIMUM_MIXIN to {} would strand the {} DEAD and {} THIN denominations".format(
                ring - 1, len(dead), len(thin)
            )
        )
        print("  above. Measure the value sitting in them before scheduling that fork -")
        print("  there is no migration path once it activates.")
        print()
        print("  EMPTY denominations are not a blocker: an output that was never created")
        print("  cannot be stranded.")
    else:
        print("  No denomination holding coins would be stranded by a fixed ring of {}.".format(ring))

    print()

    return 1 if dead or thin else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(2)
