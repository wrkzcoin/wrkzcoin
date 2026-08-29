import 'package:flutter_test/flutter_test.dart';
import 'package:pluton_web_wallet/shared/utils/address_validator.dart';

/// Structurally valid addresses of each supported length. Base58 only, and
/// prefixed the way every WRKZ address is.
String _address(int length) {
  const filler = 'aBcDeFgHjKmNpQrStUvWxYz23456789';
  final body = StringBuffer(kAddressPrefix);
  var i = 0;
  while (body.length < length) {
    body.write(filler[i++ % filler.length]);
  }
  return body.toString().substring(0, length);
}

void main() {
  final standard = _address(kStandardAddressLength);
  final integrated = _address(kIntegratedAddressLength);
  final integratedLong = _address(kIntegratedAddressLengthLong);

  group('validateAddress', () {
    test('accepts all three valid lengths', () {
      expect(validateAddress(standard), isNull);
      expect(validateAddress(integrated), isNull);
      expect(validateAddress(integratedLong), isNull);
    });

    test('trims surrounding whitespace', () {
      expect(validateAddress('  $standard  '), isNull);
    });

    test('rejects empty input', () {
      expect(validateAddress(''), AddressError.empty);
      expect(validateAddress('   '), AddressError.empty);
    });

    test('rejects a wrong prefix', () {
      final wrong = 'Zzzz${standard.substring(4)}';
      expect(validateAddress(wrong), AddressError.badPrefix);
    });

    test('rejects a wrong length', () {
      expect(validateAddress(_address(97)), AddressError.badLength);
      expect(validateAddress(_address(99)), AddressError.badLength);
      expect(validateAddress(_address(150)), AddressError.badLength);
    });

    test('rejects characters outside the base58 alphabet', () {
      // 0, O, I and l are deliberately absent from base58 — they are exactly
      // the characters people mistype for o, 0, l and 1.
      for (final bad in ['0', 'O', 'I', 'l']) {
        final addr = standard.substring(0, standard.length - 1) + bad;
        expect(validateAddress(addr), AddressError.badCharacters,
            reason: 'should reject "$bad"');
      }
    });
  });

  group('isIntegratedAddress', () {
    test('true only for the integrated lengths', () {
      expect(isIntegratedAddress(standard), isFalse);
      expect(isIntegratedAddress(integrated), isTrue);
      expect(isIntegratedAddress(integratedLong), isTrue);
    });
  });

  group('isValidPaymentId', () {
    test('empty is allowed — payment IDs are optional', () {
      expect(isValidPaymentId(''), isTrue);
      expect(isValidPaymentId('   '), isTrue);
    });

    test('accepts 16 and 64 hex characters', () {
      expect(isValidPaymentId('0123456789abcdef'), isTrue);
      expect(isValidPaymentId('AB' * 8), isTrue);
      expect(isValidPaymentId('0123456789abcdef' * 4), isTrue);
    });

    test('rejects wrong lengths and non-hex', () {
      expect(isValidPaymentId('0123456789abcde'), isFalse); // 15
      expect(isValidPaymentId('0123456789abcdef0'), isFalse); // 17
      expect(isValidPaymentId('0123456789abcdeg'), isFalse); // 'g'
    });
  });

  group('display helpers', () {
    test('shortenAddress keeps both ends', () {
      final short = shortenAddress(standard);
      expect(short, startsWith(standard.substring(0, 12)));
      expect(short, endsWith(standard.substring(standard.length - 8)));
      expect(short.length, lessThan(standard.length));
    });

    test('shortenAddress leaves short strings alone', () {
      expect(shortenAddress('Wrkz123'), 'Wrkz123');
    });

    test('chunkAddress covers the whole address exactly once', () {
      final chunks = chunkAddress(standard);
      expect(chunks.join(), standard);
      expect(chunks.every((c) => c.length <= 14), isTrue);
    });
  });
}
