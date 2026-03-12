import 'package:flutter/services.dart';

/// Light haptic — copy, toggle, minor action.
void hapticLight() => HapticFeedback.lightImpact();

/// Medium haptic — button press, pull-to-refresh complete.
void hapticMedium() => HapticFeedback.mediumImpact();

/// Heavy haptic — tx sent, tx received, wallet created.
void hapticHeavy() => HapticFeedback.heavyImpact();

/// Error haptic — wrong password, validation failure.
void hapticError() => HapticFeedback.vibrate();

/// Selection haptic — tab switch, picker change.
void hapticSelection() => HapticFeedback.selectionClick();
