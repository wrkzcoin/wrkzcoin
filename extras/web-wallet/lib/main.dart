import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'app/app.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // No window_manager or local_notifier on web — just run the app
  runApp(const ProviderScope(child: PlutonWebApp()));
}
