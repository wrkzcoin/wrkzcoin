# Encrypted Payment IDs

Short payment IDs are encrypted so that only the sender and the receiver can
read them. Long payment IDs remain plaintext and are readable by anyone with a
copy of the chain.

This page covers what changed, what it means if you run an exchange, a pool or
a tip bot, and how to migrate.

## The two kinds of payment ID

| | Length | On chain | Who can read it | Searchable |
|---|---|---|---|---|
| Long | 64 hex chars (32 bytes) | plaintext | anyone | yes |
| Short | 16 hex chars (8 bytes) | encrypted | sender and receiver only | no |

Nothing about long payment IDs has changed. If you use them today, you can keep
using them and no action is required.

## Why short payment IDs are encrypted

A payment ID is normally an account number. Publishing it in the clear on a
public ledger means anyone can see which deposits belong to the same customer,
tie separate payments together, and correlate that with anything else they know.
Encrypting it removes that: the value on chain is meaningless to an observer,
and the receiver recovers the real one using a key only they hold.

This is the same construction Monero uses, and it is byte for byte compatible
with theirs.

## How it works

Both parties independently arrive at the same shared secret.

```
Sender   D = 8 * r * A     r = transaction private key
                           A = receiver's public view key

Receiver D = 8 * a * R     a = receiver's private view key
                           R = transaction public key

         k  = keccak256(D || 0x8d)
         ct = payment_id XOR k[0..8]
```

Because `8*r*A` and `8*a*R` are the same curve point, both sides derive the same
keystream. Nobody else can: computing `D` requires one of the two private keys.

The operation is a XOR, so encrypting and decrypting are the same call.

The ciphertext is written into `tx_extra` under extra nonce sub-tag `0x02`.

### Wire format

Inside the `tx_extra` nonce field (top level tag `0x02`):

| Sub-tag | Meaning | Payload |
|---|---|---|
| `0x00` | Long payment ID, plaintext | 32 bytes |
| `0x01` | Short payment ID, plaintext | 8 bytes — **legacy, see below** |
| `0x02` | Short payment ID, encrypted | 8 bytes |
| `0x7f` | Arbitrary data | varint length, then bytes |

### About sub-tag `0x01`

An earlier release shipped sub-tag `0x01` under the name "encrypted payment ID".
It was never encrypted — the bytes were written in the clear. Nothing on chain
is known to have used it.

Rather than change the meaning of `0x01` (which would make old software read a
wrong payment ID and credit the wrong account), encrypted payment IDs use a new
sub-tag `0x02`.

An **old daemon** does not recognise `0x02` and reports no payment ID at all,
which is the safe failure — an operator investigates a missing payment ID, but
silently accepts a wrong one.

An **old wallet talking to an upgraded daemon** is the one case that is not
self-announcing: wallets do not parse `tx_extra` themselves, they take whatever
the daemon reports, so such a wallet will display the ciphertext as though it
were a payment ID. This cannot be prevented for software already deployed. In
practice the window is harmless, because nothing can create a `0x02` field until
upgraded wallets exist — but it is the reason to finish the wallet rollout
rather than leaving it half done.

`0x01` is still skipped correctly during parsing so surrounding fields decode,
but it is no longer reported as a payment ID and is no longer created.

Before deploying, confirm nothing on your chain used it:

```
python scripts/scan-payment-id-tags.py --daemon http://127.0.0.1:17856
```

Exit status is 0 if no legacy short payment IDs exist.

## Consequences you need to know about

**A short payment ID means one destination.** The ciphertext is encrypted to one
receiver's view key, so only that receiver can decrypt it. A transaction with a
short payment ID and more than one destination is refused with
`SHORT_PAYMENT_ID_NEEDS_SINGLE_DESTINATION`. Change outputs do not count. Use a
long payment ID if you need to pay several addresses at once.

**Short payment IDs cannot be searched for.** There is no way to ask a daemon or
a block explorer "show me the transaction with payment ID X" — the value on
chain is ciphertext, and it differs per transaction. Attribution has to happen
in the wallet that holds the view key. Long payment IDs are unaffected.

**A wallet restored from seed loses payment IDs on transactions it sent.** The
payment ID was encrypted to the *receiver*, so the sender cannot recover it from
chain data alone. Wallets keep a local copy at send time, so this only shows up
after restoring from seed into a fresh wallet file.

Note the wallet reports *nothing* in that case rather than guessing. Decrypting
a payment ID meant for someone else does not fail — it quietly yields eight
bytes of noise that look just like a real payment ID — so the wallet suppresses
it instead. The cost is that a payment you sent to yourself also loses its
payment ID after a seed restore. Received payments are unaffected; those are
exactly the ones your view key can read.

**The wallet service (`wrkz-service`) cannot send short payment IDs.** It builds
`tx_extra` before transaction keys exist, so it has nothing to encrypt against.
It rejects short payment IDs with `BAD_PAYMENT_ID`. Long payment IDs work as
before. Use `wrkz-wallet-api` if you need short payment IDs.

**Block explorers show ciphertext.** The daemon reports
`txDetails.paymentIdEncrypted` alongside `txDetails.paymentId` so explorers can
label the value instead of presenting it as readable.

## Rolling it out

The order matters. The daemon parses `tx_extra`, not the wallet, so a wallet
talking to an old daemon receives no payment ID at all.

1. **Upgrade daemons first**, including every public node your users connect to.
   Wait for coverage before step 2.
2. **Then upgrade wallets.** A wallet on an old daemon will not see encrypted
   payment IDs.
3. **Then hand out new integrated addresses**, if you want to use short payment
   IDs.

There is no consensus change and no fork. `tx_extra` content is not validated by
consensus — only its total size — so old nodes relay, validate and mine these
transactions exactly as before. No `FORK_HEIGHTS` entry, no block version bump,
no risk of a chain split. What is required is a coordinated software rollout in
the order above.

## Which should you use?

**If you are an exchange or a pool**, you have three options.

*Keep long payment IDs.* Nothing changes, everything keeps working. Your
customers' deposit identifiers stay publicly linkable.

*Move to short encrypted payment IDs.* Same operational model you have now — one
deposit address plus a per-customer identifier, the pattern you already run for
XRP, XLM and BNB. Requires a wallet upgrade and reissuing deposit addresses. You
lose the ability to look a deposit up by payment ID on an explorer.

*Move to one address per customer.* `POST /addresses/create` on `wrkz-wallet-api`
generates deterministic sub-wallet addresses that share a single view key, are
recoverable from the seed, and scan in constant time no matter how many you
have. Each customer gets an ordinary address, so nothing links your depositors
to each other — a stronger property than any payment ID scheme gives you,
because it removes the shared deposit address as a correlation point.

This last one needs no payment ID at all and works with every wallet on the
network today. It is the option we recommend if you can manage a set of
addresses rather than a single one.

## For wallet implementers

The scheme is small enough to reimplement. Test vectors, verified against an
independent from-scratch implementation:

```
receiver private view key  8d31b6a2b0e2d1c6f4e0b4d6b0b7d0a0e6b1b6e2e0b6d1a0c6b4e0d1b6a2b00d
receiver public view key   86f65b62d4e7443f150d4955100e159eca8b18a59fc8aa9667d6b905236e684a
transaction private key    0c1b6a2b0e2d1c6f4e0b4d6b0b7d0a0e6b1b6e2e0b6d1a0c6b4e0d1b6a2b0d0f
transaction public key     ec2dcf9ba1572f2d211ad676b5820ac09b35029ca8d51ccd704b8398d86e8a33

payment ID (plaintext)     1122334455667788
payment ID (encrypted)     5a3c5490ba23c021
```

Encrypting from either side must produce the same ciphertext, and encrypting
twice must return the plaintext. Note the hash is **original** Keccak-256, with
`0x01` padding — not the NIST SHA3-256 variant, which pads with `0x06` and will
give you a different answer.

`src/cryptotest` checks all of this; run `cryptotest` to verify an
implementation against the same vectors.
