// GENERATED CODE - DO NOT MODIFY BY HAND
// ignore_for_file: type=lint

import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'app_localizations_de.dart';
import 'app_localizations_en.dart';
import 'app_localizations_es.dart';
import 'app_localizations_fr.dart';
import 'app_localizations_ja.dart';
import 'app_localizations_pt.dart';
import 'app_localizations_ru.dart';
import 'app_localizations_vi.dart';
import 'app_localizations_zh.dart';

/// Callers can lookup localized strings with an instance of S
/// returned by `S.of(context)`.
///
/// Applications need to include `S.delegate()` in their app's
/// `localizationsDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'generated/app_localizations.dart';
///
/// return MaterialApp(
///   localizationsDelegates: S.localizationsDelegates,
///   supportedLocales: S.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you'll need to edit this
/// file.
///
/// First, open your project's ios/Runner.xcworkspace Xcode workspace. Then,
/// in the Project Navigator, open the Info.plist file under the Runner
/// project's Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the S.supportedLocales
/// property.
abstract class S {
  S(String locale)
      : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static S? of(BuildContext context) {
    return Localizations.of<S>(context, S);
  }

  static const LocalizationsDelegate<S> delegate = _SDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates =
      <LocalizationsDelegate<dynamic>>[
    delegate,
    GlobalMaterialLocalizations.delegate,
    GlobalCupertinoLocalizations.delegate,
    GlobalWidgetsLocalizations.delegate,
  ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[
    Locale('en'),
    Locale('de'),
    Locale('es'),
    Locale('fr'),
    Locale('ja'),
    Locale('pt'),
    Locale('ru'),
    Locale('vi'),
    Locale('zh'),
  ];

  /// No description provided for @appTitle.
  ///
  /// In en, this message translates to:
  /// **'PLUTON Mobile'**
  String get appTitle;

  String get tabOverview;
  String get tabReceive;
  String get tabSend;
  String get tabHistory;
  String get tabSettings;

  String get send;
  String get receive;
  String get available;
  String get locked;
  String get total;
  String lockedAmount(String amount);
  String totalAmount(String amount);

  String get recentTransactions;
  String get viewAll;
  String get noTransactionsYet;
  String get noMatchingTransactions;
  String get pending;
  String get justNow;
  String minutesAgo(int count);
  String hoursAgo(int count);
  String daysAgo(int count);
  String get received;
  String get sent;

  String get networkStatus;
  String get node;
  String get status;
  String get connected;
  String get disconnected;
  String get walletHeight;
  String get networkHeight;
  String get peers;
  String get type;
  String get viewOnly;
  String get couldNotFetchStatus;
  String errorPrefix(String message);

  String get seedBackupWarning;
  String get noConnectionToDaemon;
  String syncingPercent(String percent);

  String get yourAddress;
  String get errorLoadingAddress;
  String get integratedAddress;
  String get embedPaymentId;
  String get randomShort;
  String get randomLong;
  String get enterCustomPaymentId;
  String get enterPaymentId;
  String get paymentIdInvalid;
  String get shortPid;
  String get longPid;
  String get share;
  String get copy;

  String get sweepAllFunds;
  String get normalSend;
  String get sweep;
  String get recipientAddress;
  String get scanQr;
  String get amount;
  String availableBalance(String amount);
  String sweepInfo(String amount);
  String get paymentIdOptional;
  String get hexCharacters;
  String get mustBeHex;
  String get recipientRequired;
  String get invalidAddress;
  String get enterValidAmount;
  String get reviewTransaction;
  String get to;
  String get fee;
  String get totalDeducted;
  String get paymentId;
  String get transactionsIrreversible;
  String get back;
  String get confirmAndSend;
  String get transactionSent;
  String get transactionHash;
  String get sendAnother;
  String get scanQrCode;
  String get scannedAddress;
  String get cancel;
  String get useThisAddress;
  String get sweepFailed;

  String get searchPlaceholder;
  String get all;
  String get filterReceived;
  String get filterSent;
  String get hash;
  String get address;
  String get block;
  String get confirmed;

  String get password;
  String get unlock;
  String get switchWallet;
  String get enterPasswordToUnlock;
  String get incorrectPassword;
  String get enterYourPassword;

  String get plutonMobile;
  String get createFirstWalletSubtitle;
  String get selectWalletSubtitle;
  String get yourWallets;
  String get noWalletsYet;
  String get lastOpened;
  String createdDate(String date);
  String get createFirstWallet;
  String get addWallet;
  String get deleteWallet;
  String deleteWalletConfirm(String name);
  String get delete;

  String get createNewWallet;
  String get createNewWalletSubtitle;
  String get importFromSeed;
  String get importFromSeedSubtitle;
  String get importFromKeys;
  String get importFromKeysSubtitle;
  String get viewOnlyWallet;
  String get viewOnlyWalletSubtitle;
  String get createWallet;
  String get importWallet;
  String get walletName;
  String get walletNameHint;
  String get passwordLabel;
  String get enterPassword;
  String get confirmPassword;
  String get seedPhrase;
  String get enterSeedPhrase;
  String get scanHeight;
  String get scanHeightHint;
  String get privateSpendKey;
  String get privateViewKey;
  String get walletAddress;
  String get walletAddressHint;
  String get hexKey;
  String get daemonNode;
  String get custom;
  String get host;
  String get hostHint;
  String get port;
  String get ssl;
  String get walletNameRequired;
  String get passwordRequired;
  String get passwordTooShort;
  String get passwordsDoNotMatch;
  String get seedRequired;
  String get spendKeyRequired;
  String get viewKeyRequired;
  String get addressRequired;
  String get daemonHostRequired;

  String get backupSeedTitle;
  String get backupWarning;
  String get seedPhraseLabel;
  String get privateViewKeyLabel;
  String get privateSpendKeyLabel;
  String get backupConfirmCheck;
  String get continueToWallet;

  String get sectionDaemonNode;
  String get apply;
  String nodeUpdated(String host, int port);
  String get hostRequired;
  String currentWallet(String name);
  String get saveWallet;
  String get walletSaved;
  String saveFailed(String error);
  String get backupSeed;
  String get changePassword;
  String get resetScanHeight;
  String get reset;
  String resetScanConfirm(int height);
  String scanResetTo(int height);
  String resetFailed(String error);
  String get enterPasswordTitle;
  String get confirm;
  String get seedBackup;
  String get seedPhraseColon;
  String get privateViewKeyColon;
  String get iveBackedUp;

  String get currentPasswordLabel;
  String get newPasswordLabel;
  String get confirmNewPasswordLabel;
  String get change;
  String get currentPasswordIncorrect;
  String get newPasswordsDoNotMatch;
  String get passwordChanged;

  String get walletManagement;
  String get switchWalletSubtitle;
  String get manageWallets;
  String get manageWalletsSubtitle;
  String get currentlyOpen;
  String get close;
  String get renameWallet;
  String get newName;
  String get rename;
  String deleteWalletConfirmShort(String name);

  String get security;
  String get biometricUnlock;
  String get biometricSubtitle;
  String get biometricNotAvailable;
  String get autoLock;

  String get appearance;
  String get theme;
  String get themeAuto;
  String get themeLight;
  String get themeDark;

  String get preferences;
  String get transactionNotifications;
  String get notificationsSubtitle;
  String get autosave;
  String get autosaveSubtitle;
  String get scanCoinbaseTx;
  String get scanCoinbaseSubtitle;

  String get dangerZone;
  String get deleteCurrentWallet;
  String get deleteCurrentWalletSubtitle;
  String get deleteWalletTypeCaps;
  String get deleteHint;

  String get language;
  String get selectLanguage;
  String get languageEn;
  String get languageFr;
  String get languageDe;
  String get languageZh;
  String get languageVi;
  String get languageJa;
  String get languageEs;
  String get languagePt;
  String get languageRu;

  String get autoLockImmediately;
  String get autoLock1Min;
  String get autoLock5Min;
  String get autoLockNever;
}

class _SDelegate extends LocalizationsDelegate<S> {
  const _SDelegate();

  @override
  Future<S> load(Locale locale) {
    return SynchronousFuture<S>(lookupS(locale));
  }

  @override
  bool isSupported(Locale locale) => <String>[
        'de',
        'en',
        'es',
        'fr',
        'ja',
        'pt',
        'ru',
        'vi',
        'zh',
      ].contains(locale.languageCode);

  @override
  bool shouldReload(_SDelegate old) => false;
}

S lookupS(Locale locale) {
  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'de':
      return SDe();
    case 'en':
      return SEn();
    case 'es':
      return SEs();
    case 'fr':
      return SFr();
    case 'ja':
      return SJa();
    case 'pt':
      return SPt();
    case 'ru':
      return SRu();
    case 'vi':
      return SVi();
    case 'zh':
      return SZh();
  }

  throw FlutterError(
      'S.delegate failed to load unsupported locale "$locale". This is likely '
      'an issue with the localizations generation tool. Please file an issue '
      'on GitHub with a reproducible sample app and the gen-l10n configuration '
      'that was used.');
}
