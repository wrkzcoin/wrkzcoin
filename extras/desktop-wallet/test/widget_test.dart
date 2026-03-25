import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:pluton_wallet/app/app.dart';

void main() {
  testWidgets('App smoke test — builds without crashing', (tester) async {
    await tester.pumpWidget(const ProviderScope(child: PlutonApp()));
    // App starts on setup screen (no wallet open yet)
    expect(find.text('PLUTON'), findsWidgets);
  });
}
