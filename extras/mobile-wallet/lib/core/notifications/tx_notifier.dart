import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';

/// Local notification for incoming transactions.
///
/// The dependency and the Android permissions were already declared but
/// nothing ever called into the plugin, so the Settings "notifications"
/// toggle had no effect.
class TxNotifier {
  TxNotifier._();

  static final TxNotifier instance = TxNotifier._();

  static const _channelId = 'wrkz_incoming_tx';
  static const _channelName = 'Incoming transactions';
  static const _channelDescription =
      'Alerts when WRKZ arrives in this wallet';

  final _plugin = FlutterLocalNotificationsPlugin();
  bool _ready = false;
  int _nextId = 0;

  Future<void> init() async {
    if (_ready) return;
    try {
      await _plugin.initialize(
        const InitializationSettings(
          android: AndroidInitializationSettings('@mipmap/ic_launcher'),
          iOS: DarwinInitializationSettings(
            requestAlertPermission: false,
            requestBadgePermission: false,
            requestSoundPermission: false,
          ),
        ),
      );
      _ready = true;
    } catch (e) {
      debugPrint('[notifications] init failed: $e');
    }
  }

  /// Asks for the runtime permission. Safe to call more than once.
  Future<bool> requestPermission() async {
    await init();
    if (!_ready) return false;
    try {
      if (Platform.isAndroid) {
        final android = _plugin.resolvePlatformSpecificImplementation<
            AndroidFlutterLocalNotificationsPlugin>();
        return await android?.requestNotificationsPermission() ?? false;
      }
      if (Platform.isIOS) {
        final ios = _plugin.resolvePlatformSpecificImplementation<
            IOSFlutterLocalNotificationsPlugin>();
        return await ios?.requestPermissions(alert: true, sound: true) ?? false;
      }
    } catch (e) {
      debugPrint('[notifications] permission request failed: $e');
    }
    return false;
  }

  Future<void> showIncoming(String title, String body) async {
    await init();
    if (!_ready) return;
    try {
      await _plugin.show(
        _nextId++,
        title,
        body,
        const NotificationDetails(
          android: AndroidNotificationDetails(
            _channelId,
            _channelName,
            channelDescription: _channelDescription,
            importance: Importance.defaultImportance,
            priority: Priority.defaultPriority,
          ),
          iOS: DarwinNotificationDetails(),
        ),
      );
    } catch (e) {
      debugPrint('[notifications] show failed: $e');
    }
  }
}
