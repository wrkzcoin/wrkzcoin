'use strict';

/* ═══════════════════════════════════════════════════════════════════════════
   wrkz-crypto.js — self-contained WrkzCoin address / key primitives

   Pure JavaScript. No dependencies, no network access, no WebAssembly.
   Every routine mirrors a specific function in the WrkzCoin daemon source, so
   addresses and keys produced in the browser are bit-identical to the ones the
   native wallet produces:

     keccak256             <- Crypto::cn_fast_hash          src/crypto/keccak.c
     b58Encode / b58Decode <- Tools::Base58::encode/decode   src/common/Base58.cpp
     encodeAddr/decodeAddr <- Tools::Base58::encode_addr / decode_addr
     scReduce              <- sc_reduce / sc_reduce32        src/crypto/crypto-ops.c
     scCheck               <- sc_check
     scalarmultBase        <- ge_scalarmult_base
     isValidPublicKey      <- crypto_ops::check_key          src/crypto/crypto.cpp
     viewFromSpend         <- crypto_ops::generateViewFromSpend
     randomSpendKey        <- crypto_ops::generate_keys / random_scalar
     keyToMnemonic         <- Mnemonics::PrivateKeyToMnemonic  src/mnemonics/Mnemonics.cpp
     mnemonicToKey         <- Mnemonics::MnemonicToPrivateKey
     crc32                 <- CRC32::crc32                   src/mnemonics/CRC32.h
     createIntegratedAddress <- Utilities::createIntegratedAddress
                                                             src/utilities/Addresses.cpp
     decodeAddress         <- Utilities::parseAccountAddressString
                              + Utilities::extractIntegratedAddressData

   Exposed as window.WrkzCrypto.
   ═══════════════════════════════════════════════════════════════════════════ */

(function (global) {

  // ─── COIN CONSTANTS ─────────────────────────────────────────────────────────

  /* src/config/CryptoNoteConfig.h */
  const ADDRESS_PREFIX = 999730;

  /* src/config/WalletConfig.h */
  const STANDARD_ADDRESS_LENGTH        = 98;
  const SHORT_PAYMENT_ID_LENGTH        = 16;
  const LONG_PAYMENT_ID_LENGTH         = 64;
  const INTEGRATED_ADDRESS_LENGTH      = 120;   // 98 + (16 * 11 / 8)
  const INTEGRATED_ADDRESS_LENGTH_LONG = 186;   // 98 + (64 * 11 / 8)

  // ─── HEX / BYTE HELPERS ─────────────────────────────────────────────────────

  function hex(u8) {
    let s = '';
    for (let i = 0; i < u8.length; i++) s += u8[i].toString(16).padStart(2, '0');
    return s;
  }

  function unhex(h) {
    if (h.length % 2 !== 0) throw new Error('Hex string has an odd length.');
    const out = new Uint8Array(h.length >> 1);
    for (let i = 0; i < out.length; i++) {
      const b = parseInt(h.substr(i * 2, 2), 16);
      if (Number.isNaN(b)) throw new Error('Invalid hex string.');
      out[i] = b;
    }
    return out;
  }

  /* CryptoNote stores a payment ID inside an integrated address as the ASCII
     text of the hex string, not as the decoded bytes — createIntegratedAddress
     in src/utilities/Addresses.cpp concatenates `paymentID + keys` directly. */
  function asciiToBytes(s) {
    const out = new Uint8Array(s.length);
    for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xff;
    return out;
  }

  function bytesToAscii(u8) {
    let s = '';
    for (let i = 0; i < u8.length; i++) s += String.fromCharCode(u8[i]);
    return s;
  }

  const isHex = (s, len) =>
    typeof s === 'string' && s.length === len && /^[0-9a-fA-F]+$/.test(s);

  // ─── KECCAK-256 ─────────────────────────────────────────────────────────────
  // Original Keccak padding (0x01), NOT the SHA-3 variant (0x06). This is what
  // CryptoNote calls cn_fast_hash; every address checksum depends on it.

  const KECCAK_RC = [
    0x0000000000000001n, 0x0000000000008082n, 0x800000000000808an, 0x8000000080008000n,
    0x000000000000808bn, 0x0000000080000001n, 0x8000000080008081n, 0x8000000000008009n,
    0x000000000000008an, 0x0000000000000088n, 0x0000000080008009n, 0x000000008000000an,
    0x000000008000808bn, 0x800000000000008bn, 0x8000000000008089n, 0x8000000000008003n,
    0x8000000000008002n, 0x8000000000000080n, 0x000000000000800an, 0x800000008000000an,
    0x8000000080008081n, 0x8000000000008080n, 0x0000000080000001n, 0x8000000080008008n,
  ];

  const KECCAK_ROT = [
    [0, 36, 3, 41, 18], [1, 44, 10, 45, 2], [62, 6, 43, 15, 61],
    [28, 55, 25, 21, 56], [27, 20, 39, 8, 14],
  ];

  const U64 = (1n << 64n) - 1n;

  const rotl64 = (x, n) => (n === 0 ? x : ((x << BigInt(n)) | (x >> BigInt(64 - n))) & U64);

  function keccakF1600(A) {
    for (let round = 0; round < 24; round++) {
      /* theta */
      const C = [0, 1, 2, 3, 4].map(x => A[x][0] ^ A[x][1] ^ A[x][2] ^ A[x][3] ^ A[x][4]);
      const D = [0, 1, 2, 3, 4].map(x => C[(x + 4) % 5] ^ rotl64(C[(x + 1) % 5], 1));
      for (let x = 0; x < 5; x++) for (let y = 0; y < 5; y++) A[x][y] ^= D[x];

      /* rho + pi */
      const B = [[], [], [], [], []];
      for (let x = 0; x < 5; x++) {
        for (let y = 0; y < 5; y++) B[y][(2 * x + 3 * y) % 5] = rotl64(A[x][y], KECCAK_ROT[x][y]);
      }

      /* chi */
      for (let x = 0; x < 5; x++) {
        for (let y = 0; y < 5; y++) {
          A[x][y] = B[x][y] ^ ((~B[(x + 1) % 5][y] & U64) & B[(x + 2) % 5][y]);
        }
      }

      /* iota */
      A[0][0] ^= KECCAK_RC[round];
    }
  }

  function keccak256(bytes) {
    const RATE = 136;   // 1088 bits
    const A = [0, 1, 2, 3, 4].map(() => [0n, 0n, 0n, 0n, 0n]);

    const padded = new Uint8Array(Math.ceil((bytes.length + 1) / RATE) * RATE);
    padded.set(bytes);
    padded[bytes.length] = 0x01;
    padded[padded.length - 1] |= 0x80;

    for (let off = 0; off < padded.length; off += RATE) {
      for (let i = 0; i < RATE / 8; i++) {
        let lane = 0n;
        for (let j = 7; j >= 0; j--) lane = (lane << 8n) | BigInt(padded[off + i * 8 + j]);
        A[i % 5][(i / 5) | 0] ^= lane;
      }
      keccakF1600(A);
    }

    const out = new Uint8Array(32);
    for (let i = 0; i < 4; i++) {
      let lane = A[i % 5][(i / 5) | 0];
      for (let j = 0; j < 8; j++) { out[i * 8 + j] = Number(lane & 0xffn); lane >>= 8n; }
    }
    return out;
  }

  // ─── ED25519 ────────────────────────────────────────────────────────────────
  // Only what CryptoNote key derivation needs: scalar reduction mod l,
  // scalar-multiplying the base point, and point decompression for validation.

  const P     = (1n << 255n) - 19n;
  const L     = 2n ** 252n + 27742317777372353535851937790883648493n;
  const ED_D  = 37095705934669439343138083508754565189542113879843219016388785533085940283555n;

  const fmod = a => ((a % P) + P) % P;

  function fpow(base, exp) {
    let r = 1n, b = fmod(base);
    while (exp > 0n) { if (exp & 1n) r = r * b % P; b = b * b % P; exp >>= 1n; }
    return r;
  }

  const finv    = a => fpow(a, P - 2n);
  const SQRT_M1 = fpow(2n, (P - 1n) / 4n);

  /* Extended twisted-Edwards coordinates: [X, Y, Z, T] */
  const ED_ZERO = [0n, 1n, 1n, 0n];

  function recoverX(y, sign) {
    const y2 = fmod(y * y);
    const u  = fmod(y2 - 1n);
    const v  = fmod(ED_D * y2 + 1n);
    const v3 = fmod(v * v * v);
    const v7 = fmod(v3 * v3 * v);
    let x = fmod(u * v3 % P * fpow(fmod(u * v7), (P - 5n) / 8n));
    if (fmod(v * x * x) === fmod(-u)) x = fmod(x * SQRT_M1);
    if (fmod(v * x * x) !== u) return null;             // not on the curve
    if ((x & 1n) !== sign) x = fmod(-x);
    return x;
  }

  function edAdd(A, B) {
    const [X1, Y1, Z1, T1] = A;
    const [X2, Y2, Z2, T2] = B;
    const a = fmod((Y1 - X1) * (Y2 - X2));
    const b = fmod((Y1 + X1) * (Y2 + X2));
    const c = fmod(2n * T1 * T2 * ED_D);
    const d = fmod(2n * Z1 * Z2);
    const e = b - a, f = d - c, g = d + c, h = b + a;
    return [fmod(e * f), fmod(g * h), fmod(f * g), fmod(e * h)];
  }

  function edMul(k, point) {
    let q = ED_ZERO, n = point;
    while (k > 0n) { if (k & 1n) q = edAdd(q, n); n = edAdd(n, n); k >>= 1n; }
    return q;
  }

  const ED_BASE = (() => {
    const y = fmod(4n * finv(5n));
    const x = recoverX(y, 0n);
    return [x, y, 1n, fmod(x * y)];
  })();

  function edCompress(point) {
    const zi = finv(point[2]);
    const x = fmod(point[0] * zi), y = fmod(point[1] * zi);
    let n = y | ((x & 1n) << 255n);
    const out = new Uint8Array(32);
    for (let i = 0; i < 32; i++) { out[i] = Number(n & 0xffn); n >>= 8n; }
    return out;
  }

  function edDecompress(b) {
    if (b.length !== 32) return null;
    let n = 0n;
    for (let i = 31; i >= 0; i--) n = (n << 8n) | BigInt(b[i]);
    const sign = (n >> 255n) & 1n;
    const y = n & ((1n << 255n) - 1n);
    if (y >= P) return null;
    const x = recoverX(y, sign);
    return x === null ? null : [x, y, 1n, fmod(x * y)];
  }

  function leToBig(b) {
    let n = 0n;
    for (let i = b.length - 1; i >= 0; i--) n = (n << 8n) | BigInt(b[i]);
    return n;
  }

  function bigToLe32(n) {
    const out = new Uint8Array(32);
    for (let i = 0; i < 32; i++) { out[i] = Number(n & 0xffn); n >>= 8n; }
    return out;
  }

  /* sc_reduce (64-byte input) and sc_reduce32 (32-byte input) are both
     "interpret little-endian, reduce mod l, re-encode little-endian". */
  const scReduce = bytes => bigToLe32(leToBig(bytes) % L);

  /* sc_check — is this scalar already canonical (< l)? The daemon rejects
     private keys that are not; see validatePrivateKey in
     src/errors/ValidateParameters.cpp. */
  const scCheck = bytes => bytes.length === 32 && leToBig(bytes) < L;

  /* ge_scalarmult_base */
  const scalarmultBase = secret => edCompress(edMul(leToBig(secret) % L, ED_BASE));

  /* ge_frombytes_vartime() == 0, i.e. Crypto::check_key */
  const isValidPublicKey = key => edDecompress(key) !== null;

  // ─── BASE58 (CryptoNote block encoding) ─────────────────────────────────────
  // Data is chunked into 8-byte blocks, each encoded as 11 base58 characters.
  // src/common/Base58.cpp

  const B58_ALPHABET = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';
  const B58_ENCODED_BLOCK_SIZES = [0, 2, 3, 5, 6, 7, 9, 10, 11];        // bytes -> chars
  const B58_DECODED_BLOCK_SIZES = [0, 0, 1, 2, 0, 3, 4, 5, 0, 6, 7];    // chars -> bytes (0 = invalid)

  function b58EncodeBlock(block) {
    let n = 0n;
    for (let i = 0; i < block.length; i++) n = (n << 8n) | BigInt(block[i]);
    const size = B58_ENCODED_BLOCK_SIZES[block.length];
    const out = new Array(size).fill(B58_ALPHABET[0]);
    for (let i = size - 1; n > 0n; i--) { out[i] = B58_ALPHABET[Number(n % 58n)]; n /= 58n; }
    return out.join('');
  }

  function b58DecodeBlock(str, outLen) {
    let n = 0n;
    for (const c of str) {
      const i = B58_ALPHABET.indexOf(c);
      if (i < 0) throw new Error(`Invalid base58 character: "${c}".`);
      n = n * 58n + BigInt(i);
    }
    if (n >= (1n << BigInt(8 * outLen))) throw new Error('Base58 block overflow.');
    const out = new Uint8Array(outLen);
    for (let i = outLen - 1; i >= 0; i--) { out[i] = Number(n & 0xffn); n >>= 8n; }
    return out;
  }

  function b58Encode(bytes) {
    const full = Math.floor(bytes.length / 8), rem = bytes.length % 8;
    let s = '';
    for (let i = 0; i < full; i++) s += b58EncodeBlock(bytes.subarray(i * 8, i * 8 + 8));
    if (rem) s += b58EncodeBlock(bytes.subarray(full * 8));
    return s;
  }

  function b58Decode(str) {
    const full = Math.floor(str.length / 11), tail = str.length % 11;
    if (tail > 0 && B58_DECODED_BLOCK_SIZES[tail] === 0) {
      throw new Error(`Invalid base58 length (${str.length} characters).`);
    }
    const out = new Uint8Array(full * 8 + B58_DECODED_BLOCK_SIZES[tail]);
    let off = 0;
    for (let i = 0; i < full; i++) {
      out.set(b58DecodeBlock(str.slice(i * 11, i * 11 + 11), 8), off);
      off += 8;
    }
    if (tail) out.set(b58DecodeBlock(str.slice(full * 11), B58_DECODED_BLOCK_SIZES[tail]), off);
    return out;
  }

  // ─── VARINT ─────────────────────────────────────────────────────────────────

  function varintEncode(n) {
    if (!Number.isSafeInteger(n) || n < 0) throw new Error('Varint out of range.');
    const out = [];
    while (n > 0x7f) { out.push((n & 0x7f) | 0x80); n = Math.floor(n / 128); }
    out.push(n & 0x7f);
    return Uint8Array.from(out);
  }

  function varintDecode(bytes) {
    let n = 0, shift = 0, i = 0;
    for (;;) {
      if (i >= bytes.length) throw new Error('Truncated varint.');
      const c = bytes[i++];
      n += (c & 0x7f) * Math.pow(2, shift);
      if (!(c & 0x80)) break;
      shift += 7;
      if (shift > 56) throw new Error('Varint is too long.');
    }
    return [n, i];
  }

  // ─── ADDRESS ENCODE / DECODE ────────────────────────────────────────────────
  // Tools::Base58::encode_addr / decode_addr — varint(prefix) + payload, with
  // the first 4 bytes of cn_fast_hash(varint + payload) appended as a checksum.

  function encodeAddr(tag, payload) {
    const prefix = varintEncode(tag);
    const buf = new Uint8Array(prefix.length + payload.length);
    buf.set(prefix);
    buf.set(payload, prefix.length);

    const checksum = keccak256(buf).subarray(0, 4);
    const full = new Uint8Array(buf.length + 4);
    full.set(buf);
    full.set(checksum, buf.length);
    return b58Encode(full);
  }

  function decodeAddr(address) {
    if (typeof address !== 'string' || address.length === 0) {
      throw new Error('Address is empty.');
    }

    const raw = b58Decode(address.trim());
    if (raw.length <= 4) throw new Error('Address is too short.');

    const body = raw.subarray(0, raw.length - 4);
    const checksum = raw.subarray(raw.length - 4);
    const expected = keccak256(body).subarray(0, 4);
    for (let i = 0; i < 4; i++) {
      if (checksum[i] !== expected[i]) throw new Error('Address checksum does not match.');
    }

    const [tag, read] = varintDecode(body);
    return { prefix: tag, data: body.subarray(read) };
  }

  // ─── ADDRESS-LEVEL API ──────────────────────────────────────────────────────

  /* Decodes any WrkzCoin address, standard or integrated, and reports which it
     is. Mirrors Utilities::parseAccountAddressString together with
     Utilities::extractIntegratedAddressData. */
  function decodeAddress(address) {
    const { prefix, data } = decodeAddr(address);

    if (prefix !== ADDRESS_PREFIX) {
      throw new Error(`Wrong address prefix: got ${prefix}, expected ${ADDRESS_PREFIX}.`);
    }

    const paymentIdLength = data.length - 64;
    if (paymentIdLength !== 0
        && paymentIdLength !== SHORT_PAYMENT_ID_LENGTH
        && paymentIdLength !== LONG_PAYMENT_ID_LENGTH) {
      throw new Error('Address payload is not a valid length.');
    }

    const paymentId = paymentIdLength > 0 ? bytesToAscii(data.subarray(0, paymentIdLength)) : '';
    if (paymentIdLength > 0 && !isHex(paymentId, paymentIdLength)) {
      throw new Error('Integrated address contains a payment ID that is not hexadecimal.');
    }

    const keys = data.subarray(paymentIdLength);
    const publicSpendKey = keys.subarray(0, 32);
    const publicViewKey  = keys.subarray(32, 64);

    if (!isValidPublicKey(publicSpendKey)) throw new Error('Public spend key is not a valid curve point.');
    if (!isValidPublicKey(publicViewKey))  throw new Error('Public view key is not a valid curve point.');

    return {
      prefix,
      isIntegrated: paymentIdLength > 0,
      paymentId,
      paymentIdType: paymentIdLength === SHORT_PAYMENT_ID_LENGTH ? 'short'
                   : paymentIdLength === LONG_PAYMENT_ID_LENGTH  ? 'long'
                   : null,
      /* For an integrated address, the underlying standard address. */
      baseAddress: encodeAddr(ADDRESS_PREFIX, keys),
      publicSpendKey: hex(publicSpendKey),
      publicViewKey:  hex(publicViewKey),
    };
  }

  /* Utilities::publicKeysToAddress */
  function publicKeysToAddress(publicSpendKeyHex, publicViewKeyHex) {
    if (!isHex(publicSpendKeyHex, 64)) throw new Error('Public spend key must be 64 hex characters.');
    if (!isHex(publicViewKeyHex, 64))  throw new Error('Public view key must be 64 hex characters.');
    const payload = new Uint8Array(64);
    payload.set(unhex(publicSpendKeyHex));
    payload.set(unhex(publicViewKeyHex), 32);
    return encodeAddr(ADDRESS_PREFIX, payload);
  }

  /* Utilities::createIntegratedAddress */
  function createIntegratedAddress(address, paymentId) {
    paymentId = (paymentId || '').trim();

    if (paymentId.length !== SHORT_PAYMENT_ID_LENGTH
        && paymentId.length !== LONG_PAYMENT_ID_LENGTH) {
      throw new Error(
        `Payment ID must be ${SHORT_PAYMENT_ID_LENGTH} or ${LONG_PAYMENT_ID_LENGTH} `
        + `hex characters (this one is ${paymentId.length}).`);
    }
    if (!/^[0-9a-fA-F]+$/.test(paymentId)) {
      throw new Error('Payment ID must contain only hexadecimal characters.');
    }

    const decoded = decodeAddress((address || '').trim());
    if (decoded.isIntegrated) {
      throw new Error('That is already an integrated address — supply a standard address instead.');
    }

    const pidBytes = asciiToBytes(paymentId);
    const keys = unhex(decoded.publicSpendKey + decoded.publicViewKey);
    const payload = new Uint8Array(pidBytes.length + keys.length);
    payload.set(pidBytes);
    payload.set(keys, pidBytes.length);

    return encodeAddr(ADDRESS_PREFIX, payload);
  }

  // ─── MNEMONIC SEEDS ─────────────────────────────────────────────────────────
  // Electrum-style 24 words + 1 checksum word, over the 1626-word English list
  // in src/mnemonics/WordList.h. The list is chosen so 1626^3 > 2^32, meaning
  // every 4-byte group of the key maps to exactly three words.

  const MNEMONIC_WORDS = (
    'abbey abducts ability ablaze abnormal abort abrasive absorb abyss academy aces aching acidic ' +
    'acoustic acquire across actress acumen adapt addicted adept adhesive adjust adopt adrenalin ' +
    'adult adventure aerial afar affair afield afloat afoot afraid after against agenda aggravate ' +
    'agile aglow agnostic agony agreed ahead aided ailments aimless airport aisle ajar akin alarms ' +
    'album alchemy alerts algebra alkaline alley almost aloof alpine already also altitude alumni ' +
    'always amaze ambush amended amidst ammo amnesty among amply amused anchor android anecdote ' +
    'angled ankle annoyed answers antics anvil anxiety anybody apart apex aphid aplomb apology apply ' +
    'apricot aptitude aquarium arbitrary archer ardent arena argue arises army around arrow arsenic ' +
    'artistic ascend ashtray aside asked asleep aspire assorted asylum athlete atlas atom atrium ' +
    'attire auburn auctions audio august aunt austere autumn avatar avidly avoid awakened awesome ' +
    'awful awkward awning awoken axes axis axle aztec azure baby bacon badge baffles bagpipe bailed ' +
    'bakery balding bamboo banjo baptism basin batch bawled bays because beer befit begun behind ' +
    'being below bemused benches berries bested betting bevel beware beyond bias bicycle bids ' +
    'bifocals biggest bikini bimonthly binocular biology biplane birth biscuit bite biweekly blender ' +
    'blip bluntly boat bobsled bodies bogeys boil boldly bomb border boss both bounced bovine bowling ' +
    'boxes boyfriend broken brunt bubble buckets budget buffet bugs building bulb bumper bunch ' +
    'business butter buying buzzer bygones byline bypass cabin cactus cadets cafe cage cajun cake ' +
    'calamity camp candy casket catch cause cavernous cease cedar ceiling cell cement cent certain ' +
    'chlorine chrome cider cigar cinema circle cistern citadel civilian claim click clue coal cobra ' +
    'cocoa code coexist coffee cogs cohesive coils colony comb cool copy corrode costume cottage ' +
    'cousin cowl criminal cube cucumber cuddled cuffs cuisine cunning cupcake custom cycling cylinder ' +
    'cynical dabbing dads daft dagger daily damp dangerous dapper darted dash dating dauntless dawn ' +
    'daytime dazed debut decay dedicated deepest deftly degrees dehydrate deity dejected delayed ' +
    'demonstrate dented deodorant depth desk devoid dewdrop dexterity dialect dice diet different ' +
    'digit dilute dime dinner diode diplomat directed distance ditch divers dizzy doctor dodge does ' +
    'dogs doing dolphin domestic donuts doorway dormant dosage dotted double dove down dozen dreams ' +
    'drinks drowning drunk drying dual dubbed duckling dude duets duke dullness dummy dunes duplex ' +
    'duration dusted duties dwarf dwelt dwindling dying dynamite dyslexic each eagle earth easy ' +
    'eating eavesdrop eccentric echo eclipse economics ecstatic eden edgy edited educated eels ' +
    'efficient eggs egotistic eight either eject elapse elbow eldest eleven elite elope else eluded ' +
    'emails ember emerge emit emotion empty emulate energy enforce enhanced enigma enjoy enlist ' +
    'enmity enough enraged ensign entrance envy epoxy equip erase erected erosion error eskimos ' +
    'espionage essential estate etched eternal ethics etiquette evaluate evenings evicted evolved ' +
    'examine excess exhale exit exotic exquisite extra exult fabrics factual fading fainted faked ' +
    'fall family fancy farming fatal faulty fawns faxed fazed feast february federal feel feline ' +
    'females fences ferry festival fetches fever fewest fiat fibula fictional fidget fierce fifteen ' +
    'fight films firm fishing fitting five fixate fizzle fleet flippant flying foamy focus foes foggy ' +
    'foiled folding fonts foolish fossil fountain fowls foxes foyer framed friendly frown fruit ' +
    'frying fudge fuel fugitive fully fuming fungal furnished fuselage future fuzzy gables gadget ' +
    'gags gained galaxy gambit gang gasp gather gauze gave gawk gaze gearbox gecko geek gels gemstone ' +
    'general geometry germs gesture getting geyser ghetto ghost giant giddy gifts gigantic gills ' +
    'gimmick ginger girth giving glass gleeful glide gnaw gnome goat goblet godfather goes goggles ' +
    'going goldfish gone goodbye gopher gorilla gossip gotten gourmet governing gown greater grunt ' +
    'guarded guest guide gulp gumball guru gusts gutter guys gymnast gypsy gyrate habitat hacksaw ' +
    'haggled hairy hamburger happens hashing hatchet haunted having hawk haystack hazard hectare ' +
    'hedgehog heels hefty height hemlock hence heron hesitate hexagon hickory hiding highway hijack ' +
    'hiker hills himself hinder hippo hire history hitched hive hoax hobby hockey hoisting hold ' +
    'honked hookup hope hornet hospital hotel hounded hover howls hubcaps huddle huge hull humid ' +
    'hunter hurried husband huts hybrid hydrogen hyper iceberg icing icon identity idiom idled idols ' +
    'igloo ignore iguana illness imagine imbalance imitate impel inactive inbound incur industrial ' +
    'inexact inflamed ingested initiate injury inkling inline inmate innocent inorganic input inquest ' +
    'inroads insult intended inundate invoke inwardly ionic irate iris irony irritate island isolated ' +
    'issued italics itches items itinerary itself ivory jabbed jackets jaded jagged jailed jamming ' +
    'january jargon jaunt javelin jaws jazz jeans jeers jellyfish jeopardy jerseys jester jetting ' +
    'jewels jigsaw jingle jittery jive jobs jockey jogger joining joking jolted jostle journal joyous ' +
    'jubilee judge juggled juicy jukebox july jump junk jury justice juvenile kangaroo karate keep ' +
    'kennel kept kernels kettle keyboard kickoff kidneys king kiosk kisses kitchens kiwi knapsack ' +
    'knee knife knowledge knuckle koala laboratory ladder lagoon lair lakes lamb language laptop ' +
    'large last later launching lava lawsuit layout lazy lectures ledge leech left legion leisure ' +
    'lemon lending leopard lesson lettuce lexicon liar library licks lids lied lifestyle light ' +
    'likewise lilac limits linen lion lipstick liquid listen lively loaded lobster locker lodge lofty ' +
    'logic loincloth long looking lopped lordship losing lottery loudly love lower loyal lucky ' +
    'luggage lukewarm lullaby lumber lunar lurk lush luxury lymph lynx lyrics macro madness magically ' +
    'mailed major makeup malady mammal maps masterful match maul maverick maximum mayor maze meant ' +
    'mechanic medicate meeting megabyte melting memoir menu merger mesh metro mews mice midst mighty ' +
    'mime mirror misery mittens mixture moat mobile mocked mohawk moisture molten moment money moon ' +
    'mops morsel mostly motherly mouth movement mowing much muddy muffin mugged mullet mumble mundane ' +
    'muppet mural musical muzzle myriad mystery myth nabbing nagged nail names nanny napkin narrate ' +
    'nasty natural nautical navy nearby necklace needed negative neither neon nephew nerves nestle ' +
    'network neutral never newt nexus nibs niche niece nifty nightly nimbly nineteen nirvana nitrogen ' +
    'nobody nocturnal nodes noises nomad noodles northern nostril noted nouns novelty nowhere nozzle ' +
    'nuance nucleus nudged nugget nuisance null number nuns nurse nutshell nylon oaks oars oasis ' +
    'oatmeal obedient object obliged obnoxious observant obtains obvious occur ocean october odds ' +
    'odometer offend often oilfield ointment okay older olive olympics omega omission omnibus onboard ' +
    'oncoming oneself ongoing onion online onslaught onto onward oozed opacity opened opposite ' +
    'optical opus orange orbit orchid orders organs origin ornament orphans oscar ostrich otherwise ' +
    'otter ouch ought ounce ourselves oust outbreak oval oven owed owls owner oxidant oxygen oyster ' +
    'ozone pact paddles pager pairing palace pamphlet pancakes paper paradise pastry patio pause ' +
    'pavements pawnshop payment peaches pebbles peculiar pedantic peeled pegs pelican pencil people ' +
    'pepper perfect pests petals phase pheasants phone phrases physics piano picked pierce pigment ' +
    'piloted pimple pinched pioneer pipeline pirate pistons pitched pivot pixels pizza playful pledge ' +
    'pliers plotting plus plywood poaching pockets podcast poetry point poker polar ponies pool ' +
    'popular portents possible potato pouch poverty powder pram present pride problems pruned prying ' +
    'psychic public puck puddle puffin pulp pumpkins punch puppy purged push putty puzzled pylons ' +
    'pyramid python queen quick quote rabbits racetrack radar rafts rage railway raking rally ramped ' +
    'randomly rapid rarest rash rated ravine rays razor react rebel recipe reduce reef refer regular ' +
    'reheat reinvest rejoices rekindle relic remedy renting reorder repent request reruns rest return ' +
    'reunion revamp rewind rhino rhythm ribbon richly ridges rift rigid rims ringing riots ripped ' +
    'rising ritual river roared robot rockets rodent rogue roles romance roomy roped roster rotate ' +
    'rounded rover rowboat royal ruby rudely ruffled rugged ruined ruling rumble runway rural rustled ' +
    'ruthless sabotage sack sadness safety saga sailor sake salads sample sanity sapling sarcasm sash ' +
    'satin saucepan saved sawmill saxophone sayings scamper scenic school science scoop scrub scuba ' +
    'seasons second sedan seeded segments seismic selfish semifinal sensible september sequence ' +
    'serving session setup seventh sewage shackles shelter shipped shocking shrugged shuffled shyness ' +
    'siblings sickness sidekick sieve sifting sighting silk simplest sincerely sipped siren situated ' +
    'sixteen sizes skater skew skirting skulls skydive slackens sleepless slid slower slug smash ' +
    'smelting smidgen smog smuggled snake sneeze sniff snout snug soapy sober soccer soda software ' +
    'soggy soil solved somewhere sonic soothe soprano sorry southern sovereign sowed soya space ' +
    'speedy sphere spiders splendid spout sprig spud spying square stacking stellar stick stockpile ' +
    'strained stunning stylishly subtly succeed suddenly suede suffice sugar suitcase sulking summon ' +
    'sunken superior surfer sushi suture swagger swept swiftly sword swung syllabus symptoms syndrome ' +
    'syringe system taboo tacit tadpoles tagged tail taken talent tamper tanks tapestry tarnished ' +
    'tasked tattoo taunts tavern tawny taxi teardrop technical tedious teeming tell template tender ' +
    'tepid tequila terminal testing tether textbook thaw theatrics thirsty thorn threaten thumbs ' +
    'thwart ticket tidy tiers tiger tilt timber tinted tipsy tirade tissue titans toaster tobacco ' +
    'today toenail toffee together toilet token tolerant tomorrow tonic toolbox topic torch tossed ' +
    'total touchy towel toxic toyed trash trendy tribal trolling truth trying tsunami tubes tucks ' +
    'tudor tuesday tufts tugs tuition tulips tumbling tunnel turnip tusks tutor tuxedo twang tweezers ' +
    'twice twofold tycoon typist tyrant ugly ulcers ultimate umbrella umpire unafraid unbending uncle ' +
    'under uneven unfit ungainly unhappy union unjustly unknown unlikely unmask unnoticed unopened ' +
    'unplugs unquoted unrest unsafe until unusual unveil unwind unzip upbeat upcoming update upgrade ' +
    'uphill upkeep upload upon upper upright upstairs uptight upwards urban urchins urgent usage ' +
    'useful usher using usual utensils utility utmost utopia uttered vacation vague vain value ' +
    'vampire vane vapidly vary vastness vats vaults vector veered vegan vehicle vein velvet venomous ' +
    'verification vessel veteran vexed vials vibrate victim video viewpoint vigilant viking village ' +
    'vinegar violin vipers virtual visited vitals vivid vixen vocal vogue voice volcano vortex voted ' +
    'voucher vowels voyage vulture wade waffle wagtail waist waking wallets wanted warped washing ' +
    'water waveform waxing wayside weavers website wedge weekday weird welders went wept were western ' +
    'wetsuit whale when whipped whole wickets width wield wife wiggle wildly winter wipeout wiring ' +
    'wise withdrawn wives wizard wobbly woes woken wolf womanly wonders woozy worry wounded woven ' +
    'wrap wrist wrong yacht yahoo yanks yard yawning yearbook yellow yesterday yeti yields yodel yoga ' +
    'younger yoyo zapped zeal zebra zero zesty zigzags zinger zippers zodiac zombie zones zoom'
  ).split(' ');

  const MNEMONIC_WORD_INDEX = (() => {
    const m = new Map();
    for (let i = 0; i < MNEMONIC_WORDS.length; i++) m.set(MNEMONIC_WORDS[i], i);
    return m;
  })();

  /* CRC32::crc32 — standard CRC-32 (IEEE), used only to pick the checksum word */
  const CRC32_TABLE = (() => {
    const t = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
      t[n] = c >>> 0;
    }
    return t;
  })();

  function crc32(str) {
    let c = 0xFFFFFFFF;
    for (let i = 0; i < str.length; i++) {
      c = (c >>> 8) ^ CRC32_TABLE[(str.charCodeAt(i) ^ c) & 0xff];
    }
    return (c ^ 0xFFFFFFFF) >>> 0;
  }

  /* Mnemonics::GetChecksumWord — first 3 characters of each of the 24 words,
     CRC32'd, modulo the word count. */
  function checksumWord(words) {
    let trimmed = '';
    for (const w of words) trimmed += w.slice(0, 3);
    return words[crc32(trimmed) % words.length];
  }

  /* Mnemonics::PrivateKeyToMnemonic */
  function keyToMnemonic(privateSpendKey) {
    if (privateSpendKey.length !== 32) throw new Error('Private spend key must be 32 bytes.');

    const n = MNEMONIC_WORDS.length;
    const view = new DataView(privateSpendKey.buffer, privateSpendKey.byteOffset, 32);
    const words = [];

    for (let i = 0; i < 31; i += 4) {
      const val = view.getUint32(i, true);                 // little-endian uint32
      const w1 = val % n;
      const w2 = (Math.floor(val / n) + w1) % n;
      const w3 = (Math.floor(Math.floor(val / n) / n) + w2) % n;
      words.push(MNEMONIC_WORDS[w1], MNEMONIC_WORDS[w2], MNEMONIC_WORDS[w3]);
    }

    words.push(checksumWord(words));
    return words.join(' ');
  }

  /* Mnemonics::MnemonicToPrivateKey */
  function mnemonicToKey(mnemonic) {
    const words = String(mnemonic || '').trim().toLowerCase().split(/\s+/).filter(Boolean);

    if (words.length !== 25) {
      const plural = words.length === 1 ? 'word' : 'words';
      throw new Error(
        `The mnemonic seed should be 25 words long, but it is ${words.length} ${plural}.`);
    }

    for (const w of words) {
      if (!MNEMONIC_WORD_INDEX.has(w)) {
        throw new Error(`The mnemonic seed contains a word that is not in the English word list: "${w}".`);
      }
    }

    if (words[24] !== checksumWord(words.slice(0, 24))) {
      throw new Error('The mnemonic seed has an invalid checksum word.');
    }

    const n = MNEMONIC_WORDS.length;
    const key = new Uint8Array(32);
    const view = new DataView(key.buffer);

    for (let i = 0, off = 0; i < 24; i += 3, off += 4) {
      const w1 = MNEMONIC_WORD_INDEX.get(words[i]);
      const w2 = MNEMONIC_WORD_INDEX.get(words[i + 1]);
      const w3 = MNEMONIC_WORD_INDEX.get(words[i + 2]);
      const val = w1 + n * (((n - w1) + w2) % n) + n * n * (((n - w2) + w3) % n);
      if (val % n !== w1) throw new Error('The mnemonic seed is not valid.');
      view.setUint32(off, val >>> 0, true);
    }

    return key;
  }

  // ─── KEY GENERATION / DERIVATION ────────────────────────────────────────────

  /* crypto_ops::generate_keys -> random_scalar: 64 CSPRNG bytes, then sc_reduce.
     Uses the platform CSPRNG only; there is no fallback to Math.random. */
  function randomSpendKey() {
    const webcrypto = global.crypto || global.msCrypto;
    if (!webcrypto || typeof webcrypto.getRandomValues !== 'function') {
      throw new Error(
        'This browser does not expose a cryptographically secure random number '
        + 'generator (crypto.getRandomValues), so keys cannot be generated safely.');
    }
    const buf = new Uint8Array(64);
    webcrypto.getRandomValues(buf);
    return scReduce(buf);
  }

  /* crypto_ops::generateViewFromSpend — note this hashes the RAW private spend
     key bytes and reduces afterwards, which is why callers must reject keys
     that are not already canonical (see fromPrivateSpendKey below). */
  const viewFromSpend = privateSpendKey => scReduce(keccak256(privateSpendKey));

  /* Builds the full key set + address from a private spend key, exactly as
     WalletBackend does when creating or importing a wallet. */
  function fromPrivateSpendKey(privateSpendKey) {
    if (privateSpendKey.length !== 32) throw new Error('Private spend key must be 32 bytes.');
    if (!scCheck(privateSpendKey)) {
      throw new Error(
        'That private spend key is not a valid ed25519 scalar (it is not reduced '
        + 'modulo the group order). The daemon would reject it too.');
    }

    const privateViewKey = viewFromSpend(privateSpendKey);
    const publicSpendKey = scalarmultBase(privateSpendKey);
    const publicViewKey  = scalarmultBase(privateViewKey);

    const payload = new Uint8Array(64);
    payload.set(publicSpendKey);
    payload.set(publicViewKey, 32);

    return {
      address:         encodeAddr(ADDRESS_PREFIX, payload),
      mnemonic:        keyToMnemonic(privateSpendKey),
      privateSpendKey: hex(privateSpendKey),
      privateViewKey:  hex(privateViewKey),
      publicSpendKey:  hex(publicSpendKey),
      publicViewKey:   hex(publicViewKey),
    };
  }

  /* A brand new wallet — WalletBackend::createWallet */
  const createWallet = () => fromPrivateSpendKey(randomSpendKey());

  /* WalletBackend::importWalletFromSeed */
  const fromMnemonic = mnemonic => fromPrivateSpendKey(mnemonicToKey(mnemonic));

  /* Accepts either a 25-word mnemonic seed or a 64-character private spend key. */
  function importWallet(input) {
    const trimmed = String(input || '').trim();
    if (!trimmed) throw new Error('Enter a 25-word mnemonic seed or a 64-character private spend key.');
    if (isHex(trimmed, 64)) return fromPrivateSpendKey(unhex(trimmed.toLowerCase()));
    return fromMnemonic(trimmed);
  }

  // ─── EXPORTS ────────────────────────────────────────────────────────────────

  global.WrkzCrypto = {
    /* constants */
    ADDRESS_PREFIX,
    STANDARD_ADDRESS_LENGTH,
    SHORT_PAYMENT_ID_LENGTH,
    LONG_PAYMENT_ID_LENGTH,
    INTEGRATED_ADDRESS_LENGTH,
    INTEGRATED_ADDRESS_LENGTH_LONG,

    /* wallets */
    createWallet,
    importWallet,
    fromMnemonic,
    fromPrivateSpendKey,
    randomSpendKey,
    viewFromSpend,

    /* addresses */
    decodeAddress,
    createIntegratedAddress,
    publicKeysToAddress,

    /* mnemonics */
    keyToMnemonic,
    mnemonicToKey,

    /* low level, exposed for tests and for the Check Transaction tool */
    keccak256,
    b58Encode,
    b58Decode,
    encodeAddr,
    decodeAddr,
    scReduce,
    scCheck,
    scalarmultBase,
    isValidPublicKey,
    edDecompress,
    leToBig,
    bigToLe32,
    varintEncode,
    varintDecode,
    crc32,
    hex,
    unhex,
    isHex,
    L,
  };

})(typeof globalThis !== 'undefined' ? globalThis : this);
