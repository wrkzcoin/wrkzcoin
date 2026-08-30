'use strict';

/* Verifies vendor/wrkz-crypto.js against published test vectors, a real
   WrkzCoin address, and the constants in the C++ daemon source.

   The point of this suite is that a browser-side wallet tool is only safe if it
   agrees with the daemon exactly. Where a value can be checked against the C++
   source, it is read out of src/ rather than copied here, so the two cannot
   drift apart silently.

   Run with:  node test/crypto.test.js       (from extras/explorer) */

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const MODULE_PATH = path.join(ROOT, 'vendor', 'wrkz-crypto.js');
const SRC = path.join(ROOT, '..', '..', 'src');

/* Load it the way a browser would: as a plain script against a global object. */
const sandbox = { crypto: require('crypto').webcrypto };
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(MODULE_PATH, 'utf8'), sandbox, { filename: 'wrkz-crypto.js' });
const W = sandbox.WrkzCrypto;

let pass = 0, fail = 0;

function check(name, actual, expected) {
  if (String(actual) === String(expected)) { pass++; console.log(`  ok   ${name}`); }
  else {
    fail++;
    console.log(`  FAIL ${name}\n         got      ${actual}\n         expected ${expected}`);
  }
}

function throws(name, fn, needle) {
  try {
    fn();
    fail++;
    console.log(`  FAIL ${name} — expected an error, got none`);
  } catch (e) {
    if (needle && !e.message.toLowerCase().includes(needle.toLowerCase())) {
      fail++;
      console.log(`  FAIL ${name} — wrong error: ${e.message}`);
    } else { pass++; console.log(`  ok   ${name}`); }
  }
}

/* A real, live WrkzCoin address (the donation address in extras/mobile-wallet). */
const LIVE = 'WrkzTF4cNAHbphuhWvyYjjE4qfVL3FCWTVpa11jSPFUqcuhuQcVa9e9ffSTutP6zSs7HUVTdDZd1aH8HCpxn3Sy52M2dfahwSi';
const enc = new TextEncoder();

console.log('\n— primitives against published vectors —');
check('keccak256("")', W.hex(W.keccak256(new Uint8Array(0))),
  'c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470');
check('keccak256("abc")', W.hex(W.keccak256(enc.encode('abc'))),
  '4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45');
check('keccak256("The quick brown fox jumps over the lazy dog")',
  W.hex(W.keccak256(enc.encode('The quick brown fox jumps over the lazy dog'))),
  '4d741b6f1eb29cb2a9b9911c82f56fa8d73b04959d3d9d222895df6c0b28aa15');
check('crc32("123456789")', W.crc32('123456789').toString(16), 'cbf43926');
check('crc32("")', W.crc32(''), 0);
check('ed25519 base point', W.hex(W.scalarmultBase(W.bigToLe32(1n))),
  '5866666666666666666666666666666666666666666666666666666666666666');
check('ed25519 2G', W.hex(W.scalarmultBase(W.bigToLe32(2n))),
  'c9a3f86aae465f0e56513864510f3997561fa2c9e85ea21dc2292309f3cd6022');
check('sc_check accepts l - 1', W.scCheck(W.bigToLe32(W.L - 1n)), true);
check('sc_check rejects l', W.scCheck(W.bigToLe32(W.L)), false);

console.log('\n— constants agree with src/config —');
{
  const cryptoNoteConfig = fs.readFileSync(path.join(SRC, 'config', 'CryptoNoteConfig.h'), 'utf8');
  const walletConfig = fs.readFileSync(path.join(SRC, 'config', 'WalletConfig.h'), 'utf8');

  const prefix = Number(
    cryptoNoteConfig.match(/CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX\s*=\s*(\d+)/)[1]);
  const standard = Number(walletConfig.match(/standardAddressLength\s*=\s*(\d+)/)[1]);
  const shortPid = Number(walletConfig.match(/shortPaymentIDLength\s*=\s*(\d+)/)[1]);
  const longPid = Number(walletConfig.match(/longPaymentIDLength\s*=\s*(\d+)/)[1]);

  check('ADDRESS_PREFIX', W.ADDRESS_PREFIX, prefix);
  check('STANDARD_ADDRESS_LENGTH', W.STANDARD_ADDRESS_LENGTH, standard);
  check('SHORT_PAYMENT_ID_LENGTH', W.SHORT_PAYMENT_ID_LENGTH, shortPid);
  check('LONG_PAYMENT_ID_LENGTH', W.LONG_PAYMENT_ID_LENGTH, longPid);
  check('INTEGRATED_ADDRESS_LENGTH', W.INTEGRATED_ADDRESS_LENGTH,
    standard + Math.floor((shortPid * 11) / 8));
  check('INTEGRATED_ADDRESS_LENGTH_LONG', W.INTEGRATED_ADDRESS_LENGTH_LONG,
    standard + Math.floor((longPid * 11) / 8));
}

console.log('\n— a real address round-trips exactly —');
const live = W.decodeAddress(LIVE);
check('prefix', live.prefix, W.ADDRESS_PREFIX);
check('not integrated', live.isIntegrated, false);
check('re-encoding is byte-identical',
  W.publicKeysToAddress(live.publicSpendKey, live.publicViewKey), LIVE);
check('spend key is on the curve', W.isValidPublicKey(W.unhex(live.publicSpendKey)), true);
check('view key is on the curve', W.isValidPublicKey(W.unhex(live.publicViewKey)), true);

console.log('\n— the word list is identical to src/mnemonics/WordList.h —');
{
  const header = fs.readFileSync(path.join(SRC, 'mnemonics', 'WordList.h'), 'utf8');
  const start = header.indexOf('English = {') + 'English = {'.length;
  const cppWords = header.slice(start, header.indexOf('};', start))
    .match(/"([a-z]+)"/g).map(w => w.slice(1, -1));

  const js = fs.readFileSync(MODULE_PATH, 'utf8');
  const embedded = js.match(/const MNEMONIC_WORDS = \(([\s\S]*?)\)\.split\(' '\);/);
  const jsWords = embedded[1].match(/'([^']*)'/g)
    .map(s => s.slice(1, -1)).join('').split(' ').filter(Boolean);

  check('word count', jsWords.length, cppWords.length);
  check('count is 1626', cppWords.length, 1626);
  check('every word matches, in order', jsWords.join(',') === cppWords.join(','), true);
  check('word index 0 round-trips',
    W.keyToMnemonic(W.bigToLe32(0n)).split(' ')[0], cppWords[0]);
  check('word index 1625 round-trips',
    W.keyToMnemonic(W.bigToLe32(BigInt(cppWords.length - 1))).split(' ')[0],
    cppWords[cppWords.length - 1]);
}

console.log('\n— integrated addresses, both payment ID lengths —');
const shortAddr = W.createIntegratedAddress(LIVE, 'a1b2c3d4e5f60718');
const longAddr = W.createIntegratedAddress(LIVE, 'f'.repeat(63) + '0');
check('short form length', shortAddr.length, W.INTEGRATED_ADDRESS_LENGTH);
check('long form length', longAddr.length, W.INTEGRATED_ADDRESS_LENGTH_LONG);

const dShort = W.decodeAddress(shortAddr);
const dLong = W.decodeAddress(longAddr);
check('short payment ID survives', dShort.paymentId, 'a1b2c3d4e5f60718');
check('short payment ID type', dShort.paymentIdType, 'short');
check('short base address', dShort.baseAddress, LIVE);
check('long payment ID survives', dLong.paymentId, 'f'.repeat(63) + '0');
check('long payment ID type', dLong.paymentIdType, 'long');
check('long base address', dLong.baseAddress, LIVE);
check('flagged as integrated', dShort.isIntegrated, true);
check('keys are unchanged by integration', dShort.publicSpendKey, live.publicSpendKey);

console.log('\n— invalid input is rejected —');
throws('corrupted checksum', () => W.decodeAddress(LIVE.slice(0, 97) + 'X'), 'checksum');
throws('empty address', () => W.decodeAddress(''), 'empty');
throws('payment ID of the wrong length', () => W.createIntegratedAddress(LIVE, 'abcd'), 'must be 16 or 64');
throws('non-hex payment ID', () => W.createIntegratedAddress(LIVE, 'z'.repeat(16)), 'hexadecimal');
throws('integrating an integrated address',
  () => W.createIntegratedAddress(shortAddr, 'a'.repeat(16)), 'already an integrated');
throws('unreduced private spend key',
  () => W.fromPrivateSpendKey(W.bigToLe32(W.L)), 'not a valid ed25519 scalar');
throws('mnemonic of the wrong length', () => W.mnemonicToKey('abbey abducts'), '25 words');
throws('mnemonic with an unknown word',
  () => W.mnemonicToKey(('abbey '.repeat(24) + 'notaword').trim()), 'not in the English');
throws('empty import', () => W.importWallet('   '), '25-word mnemonic');

console.log('\n— generate, export, re-import (300 wallets) —');
{
  let broken = null;
  for (let i = 0; i < 300 && !broken; i++) {
    const w = W.createWallet();

    if (w.address.length !== W.STANDARD_ADDRESS_LENGTH) broken = 'address length';
    else if (!w.address.startsWith('Wrkz')) broken = 'address prefix text';
    else if (w.mnemonic.split(' ').length !== 25) broken = 'mnemonic word count';
    else {
      const viaSeed = W.importWallet(w.mnemonic);
      const viaKey = W.importWallet(w.privateSpendKey);
      if (viaSeed.address !== w.address) broken = 'mnemonic import address';
      else if (viaKey.address !== w.address) broken = 'spend key import address';
      else if (viaSeed.privateSpendKey !== w.privateSpendKey) broken = 'spend key round-trip';
      else if (viaSeed.privateViewKey !== w.privateViewKey) broken = 'view key round-trip';
      else {
        const integrated = W.createIntegratedAddress(w.address, '0123456789abcdef');
        const d = W.decodeAddress(integrated);
        if (d.baseAddress !== w.address) broken = 'integrated base address';
        else if (d.paymentId !== '0123456789abcdef') broken = 'integrated payment ID';
      }
    }
  }
  check('300 wallets survive a full round-trip', broken || 'clean', 'clean');
}

console.log('\n— key derivation matches generateViewFromSpend —');
{
  const w = W.createWallet();
  const spend = W.unhex(w.privateSpendKey);
  check('private view key == sc_reduce32(keccak(spend))',
    w.privateViewKey, W.hex(W.scReduce(W.keccak256(spend))));
  check('public view key == view * G',
    w.publicViewKey, W.hex(W.scalarmultBase(W.unhex(w.privateViewKey))));
  check('public spend key == spend * G',
    w.publicSpendKey, W.hex(W.scalarmultBase(spend)));
}

console.log(`\n${pass} passed, ${fail} failed`);
process.exit(fail === 0 ? 0 : 1);
