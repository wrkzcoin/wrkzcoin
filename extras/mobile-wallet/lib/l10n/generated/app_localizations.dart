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

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of S
/// returned by `S.of(context)`.
///
/// Applications need to include `S.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
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
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
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
    Locale('de'),
    Locale('en'),
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

  /// No description provided for @tabOverview.
  ///
  /// In en, this message translates to:
  /// **'Overview'**
  String get tabOverview;

  /// No description provided for @tabReceive.
  ///
  /// In en, this message translates to:
  /// **'Receive'**
  String get tabReceive;

  /// No description provided for @tabSend.
  ///
  /// In en, this message translates to:
  /// **'Send'**
  String get tabSend;

  /// No description provided for @tabHistory.
  ///
  /// In en, this message translates to:
  /// **'History'**
  String get tabHistory;

  /// No description provided for @tabSettings.
  ///
  /// In en, this message translates to:
  /// **'Settings'**
  String get tabSettings;

  /// No description provided for @send.
  ///
  /// In en, this message translates to:
  /// **'Send'**
  String get send;

  /// No description provided for @receive.
  ///
  /// In en, this message translates to:
  /// **'Receive'**
  String get receive;

  /// No description provided for @available.
  ///
  /// In en, this message translates to:
  /// **'Available'**
  String get available;

  /// No description provided for @locked.
  ///
  /// In en, this message translates to:
  /// **'Locked'**
  String get locked;

  /// No description provided for @total.
  ///
  /// In en, this message translates to:
  /// **'Total'**
  String get total;

  /// No description provided for @lockedAmount.
  ///
  /// In en, this message translates to:
  /// **'Locked: {amount}'**
  String lockedAmount(String amount);

  /// No description provided for @totalAmount.
  ///
  /// In en, this message translates to:
  /// **'Total: {amount}'**
  String totalAmount(String amount);

  /// No description provided for @recentTransactions.
  ///
  /// In en, this message translates to:
  /// **'Recent Transactions'**
  String get recentTransactions;

  /// No description provided for @viewAll.
  ///
  /// In en, this message translates to:
  /// **'View all'**
  String get viewAll;

  /// No description provided for @noTransactionsYet.
  ///
  /// In en, this message translates to:
  /// **'No transactions yet'**
  String get noTransactionsYet;

  /// No description provided for @noMatchingTransactions.
  ///
  /// In en, this message translates to:
  /// **'No matching transactions'**
  String get noMatchingTransactions;

  /// No description provided for @pending.
  ///
  /// In en, this message translates to:
  /// **'Pending...'**
  String get pending;

  /// No description provided for @justNow.
  ///
  /// In en, this message translates to:
  /// **'Just now'**
  String get justNow;

  /// No description provided for @minutesAgo.
  ///
  /// In en, this message translates to:
  /// **'{count}m ago'**
  String minutesAgo(int count);

  /// No description provided for @hoursAgo.
  ///
  /// In en, this message translates to:
  /// **'{count}h ago'**
  String hoursAgo(int count);

  /// No description provided for @daysAgo.
  ///
  /// In en, this message translates to:
  /// **'{count}d ago'**
  String daysAgo(int count);

  /// No description provided for @received.
  ///
  /// In en, this message translates to:
  /// **'Received'**
  String get received;

  /// No description provided for @sent.
  ///
  /// In en, this message translates to:
  /// **'Sent'**
  String get sent;

  /// No description provided for @networkStatus.
  ///
  /// In en, this message translates to:
  /// **'Network Status'**
  String get networkStatus;

  /// No description provided for @node.
  ///
  /// In en, this message translates to:
  /// **'Node'**
  String get node;

  /// No description provided for @status.
  ///
  /// In en, this message translates to:
  /// **'Status'**
  String get status;

  /// No description provided for @connected.
  ///
  /// In en, this message translates to:
  /// **'Connected'**
  String get connected;

  /// No description provided for @disconnected.
  ///
  /// In en, this message translates to:
  /// **'Disconnected'**
  String get disconnected;

  /// No description provided for @walletHeight.
  ///
  /// In en, this message translates to:
  /// **'Wallet Height'**
  String get walletHeight;

  /// No description provided for @networkHeight.
  ///
  /// In en, this message translates to:
  /// **'Network Height'**
  String get networkHeight;

  /// No description provided for @peers.
  ///
  /// In en, this message translates to:
  /// **'Peers'**
  String get peers;

  /// No description provided for @type.
  ///
  /// In en, this message translates to:
  /// **'Type'**
  String get type;

  /// No description provided for @viewOnly.
  ///
  /// In en, this message translates to:
  /// **'View-only'**
  String get viewOnly;

  /// No description provided for @couldNotFetchStatus.
  ///
  /// In en, this message translates to:
  /// **'Could not fetch status. Check your node in Settings.'**
  String get couldNotFetchStatus;

  /// No description provided for @errorPrefix.
  ///
  /// In en, this message translates to:
  /// **'Error: {message}'**
  String errorPrefix(String message);

  /// No description provided for @seedBackupWarning.
  ///
  /// In en, this message translates to:
  /// **'Back up your seed phrase in Settings to protect your funds.'**
  String get seedBackupWarning;

  /// No description provided for @noConnectionToDaemon.
  ///
  /// In en, this message translates to:
  /// **'No connection to daemon'**
  String get noConnectionToDaemon;

  /// No description provided for @syncingPercent.
  ///
  /// In en, this message translates to:
  /// **'Syncing {percent}%'**
  String syncingPercent(String percent);

  /// No description provided for @yourAddress.
  ///
  /// In en, this message translates to:
  /// **'Your Address'**
  String get yourAddress;

  /// No description provided for @errorLoadingAddress.
  ///
  /// In en, this message translates to:
  /// **'Error loading address'**
  String get errorLoadingAddress;

  /// No description provided for @integratedAddress.
  ///
  /// In en, this message translates to:
  /// **'Integrated Address'**
  String get integratedAddress;

  /// No description provided for @embedPaymentId.
  ///
  /// In en, this message translates to:
  /// **'Embed a payment ID into your address'**
  String get embedPaymentId;

  /// No description provided for @randomShort.
  ///
  /// In en, this message translates to:
  /// **'Random Short (16)'**
  String get randomShort;

  /// No description provided for @randomLong.
  ///
  /// In en, this message translates to:
  /// **'Random Long (64)'**
  String get randomLong;

  /// No description provided for @enterCustomPaymentId.
  ///
  /// In en, this message translates to:
  /// **'Or enter custom payment ID (16 or 64 hex)'**
  String get enterCustomPaymentId;

  /// No description provided for @enterPaymentId.
  ///
  /// In en, this message translates to:
  /// **'Enter a payment ID'**
  String get enterPaymentId;

  /// No description provided for @paymentIdInvalid.
  ///
  /// In en, this message translates to:
  /// **'Payment ID must be 16 or 64 hex characters'**
  String get paymentIdInvalid;

  /// No description provided for @shortPid.
  ///
  /// In en, this message translates to:
  /// **'Short PID'**
  String get shortPid;

  /// No description provided for @longPid.
  ///
  /// In en, this message translates to:
  /// **'Long PID'**
  String get longPid;

  /// No description provided for @share.
  ///
  /// In en, this message translates to:
  /// **'Share'**
  String get share;

  /// No description provided for @copy.
  ///
  /// In en, this message translates to:
  /// **'Copy'**
  String get copy;

  /// No description provided for @sweepAllFunds.
  ///
  /// In en, this message translates to:
  /// **'Sweep All Funds'**
  String get sweepAllFunds;

  /// No description provided for @normalSend.
  ///
  /// In en, this message translates to:
  /// **'Normal Send'**
  String get normalSend;

  /// No description provided for @sweep.
  ///
  /// In en, this message translates to:
  /// **'Sweep'**
  String get sweep;

  /// No description provided for @recipientAddress.
  ///
  /// In en, this message translates to:
  /// **'Recipient Address'**
  String get recipientAddress;

  /// No description provided for @scanQr.
  ///
  /// In en, this message translates to:
  /// **'Scan QR'**
  String get scanQr;

  /// No description provided for @amount.
  ///
  /// In en, this message translates to:
  /// **'Amount'**
  String get amount;

  /// No description provided for @availableBalance.
  ///
  /// In en, this message translates to:
  /// **'Available: {amount}'**
  String availableBalance(String amount);

  /// No description provided for @sweepInfo.
  ///
  /// In en, this message translates to:
  /// **'Sweep consolidates all UTXOs and sends your entire unlocked balance ({amount}) minus fees.'**
  String sweepInfo(String amount);

  /// No description provided for @paymentIdOptional.
  ///
  /// In en, this message translates to:
  /// **'Payment ID (optional)'**
  String get paymentIdOptional;

  /// No description provided for @hexCharacters.
  ///
  /// In en, this message translates to:
  /// **'16 or 64 hex characters'**
  String get hexCharacters;

  /// No description provided for @mustBeHex.
  ///
  /// In en, this message translates to:
  /// **'Must be 16 or 64 hex characters'**
  String get mustBeHex;

  /// No description provided for @recipientRequired.
  ///
  /// In en, this message translates to:
  /// **'Recipient address is required'**
  String get recipientRequired;

  /// No description provided for @invalidAddress.
  ///
  /// In en, this message translates to:
  /// **'Invalid WRKZ address'**
  String get invalidAddress;

  /// No description provided for @enterValidAmount.
  ///
  /// In en, this message translates to:
  /// **'Enter a valid amount'**
  String get enterValidAmount;

  /// No description provided for @reviewTransaction.
  ///
  /// In en, this message translates to:
  /// **'Review Transaction'**
  String get reviewTransaction;

  /// No description provided for @to.
  ///
  /// In en, this message translates to:
  /// **'To'**
  String get to;

  /// No description provided for @fee.
  ///
  /// In en, this message translates to:
  /// **'Fee'**
  String get fee;

  /// No description provided for @totalDeducted.
  ///
  /// In en, this message translates to:
  /// **'Total Deducted'**
  String get totalDeducted;

  /// No description provided for @paymentId.
  ///
  /// In en, this message translates to:
  /// **'Payment ID'**
  String get paymentId;

  /// No description provided for @transactionsIrreversible.
  ///
  /// In en, this message translates to:
  /// **'Transactions are irreversible. Please verify the details.'**
  String get transactionsIrreversible;

  /// No description provided for @back.
  ///
  /// In en, this message translates to:
  /// **'Back'**
  String get back;

  /// No description provided for @confirmAndSend.
  ///
  /// In en, this message translates to:
  /// **'Confirm & Send'**
  String get confirmAndSend;

  /// No description provided for @transactionSent.
  ///
  /// In en, this message translates to:
  /// **'Transaction Sent!'**
  String get transactionSent;

  /// No description provided for @transactionHash.
  ///
  /// In en, this message translates to:
  /// **'Transaction Hash'**
  String get transactionHash;

  /// No description provided for @sendAnother.
  ///
  /// In en, this message translates to:
  /// **'Send Another'**
  String get sendAnother;

  /// No description provided for @scanQrCode.
  ///
  /// In en, this message translates to:
  /// **'Scan QR Code'**
  String get scanQrCode;

  /// No description provided for @scannedAddress.
  ///
  /// In en, this message translates to:
  /// **'Scanned Address'**
  String get scannedAddress;

  /// No description provided for @cancel.
  ///
  /// In en, this message translates to:
  /// **'Cancel'**
  String get cancel;

  /// No description provided for @useThisAddress.
  ///
  /// In en, this message translates to:
  /// **'Use this address'**
  String get useThisAddress;

  /// No description provided for @sweepFailed.
  ///
  /// In en, this message translates to:
  /// **'Sweep failed'**
  String get sweepFailed;

  /// No description provided for @searchPlaceholder.
  ///
  /// In en, this message translates to:
  /// **'Search by hash, address, payment ID...'**
  String get searchPlaceholder;

  /// No description provided for @all.
  ///
  /// In en, this message translates to:
  /// **'All'**
  String get all;

  /// No description provided for @filterReceived.
  ///
  /// In en, this message translates to:
  /// **'Received'**
  String get filterReceived;

  /// No description provided for @filterSent.
  ///
  /// In en, this message translates to:
  /// **'Sent'**
  String get filterSent;

  /// No description provided for @hash.
  ///
  /// In en, this message translates to:
  /// **'Hash'**
  String get hash;

  /// No description provided for @address.
  ///
  /// In en, this message translates to:
  /// **'Address'**
  String get address;

  /// No description provided for @block.
  ///
  /// In en, this message translates to:
  /// **'Block'**
  String get block;

  /// No description provided for @confirmed.
  ///
  /// In en, this message translates to:
  /// **'Confirmed'**
  String get confirmed;

  /// No description provided for @password.
  ///
  /// In en, this message translates to:
  /// **'Password'**
  String get password;

  /// No description provided for @unlock.
  ///
  /// In en, this message translates to:
  /// **'Unlock'**
  String get unlock;

  /// No description provided for @switchWallet.
  ///
  /// In en, this message translates to:
  /// **'Switch Wallet'**
  String get switchWallet;

  /// No description provided for @enterPasswordToUnlock.
  ///
  /// In en, this message translates to:
  /// **'Enter your password to unlock'**
  String get enterPasswordToUnlock;

  /// No description provided for @incorrectPassword.
  ///
  /// In en, this message translates to:
  /// **'Incorrect password'**
  String get incorrectPassword;

  /// No description provided for @enterYourPassword.
  ///
  /// In en, this message translates to:
  /// **'Enter your password'**
  String get enterYourPassword;

  /// No description provided for @plutonMobile.
  ///
  /// In en, this message translates to:
  /// **'PLUTON Mobile'**
  String get plutonMobile;

  /// No description provided for @createFirstWalletSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Create your first wallet to get started'**
  String get createFirstWalletSubtitle;

  /// No description provided for @selectWalletSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Select a wallet to open'**
  String get selectWalletSubtitle;

  /// No description provided for @yourWallets.
  ///
  /// In en, this message translates to:
  /// **'Your Wallets'**
  String get yourWallets;

  /// No description provided for @noWalletsYet.
  ///
  /// In en, this message translates to:
  /// **'No wallets yet'**
  String get noWalletsYet;

  /// No description provided for @lastOpened.
  ///
  /// In en, this message translates to:
  /// **'Last opened'**
  String get lastOpened;

  /// No description provided for @createdDate.
  ///
  /// In en, this message translates to:
  /// **'Created {date}'**
  String createdDate(String date);

  /// No description provided for @createFirstWallet.
  ///
  /// In en, this message translates to:
  /// **'Create First Wallet'**
  String get createFirstWallet;

  /// No description provided for @addWallet.
  ///
  /// In en, this message translates to:
  /// **'Add Wallet'**
  String get addWallet;

  /// No description provided for @deleteWallet.
  ///
  /// In en, this message translates to:
  /// **'Delete Wallet'**
  String get deleteWallet;

  /// No description provided for @deleteWalletConfirm.
  ///
  /// In en, this message translates to:
  /// **'Delete \"{name}\"?\n\nThis will permanently remove the wallet file and keys. Make sure you have backed up your seed phrase.'**
  String deleteWalletConfirm(String name);

  /// No description provided for @delete.
  ///
  /// In en, this message translates to:
  /// **'Delete'**
  String get delete;

  /// No description provided for @createNewWallet.
  ///
  /// In en, this message translates to:
  /// **'Create New Wallet'**
  String get createNewWallet;

  /// No description provided for @createNewWalletSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Generate a new wallet with a fresh seed phrase'**
  String get createNewWalletSubtitle;

  /// No description provided for @importFromSeed.
  ///
  /// In en, this message translates to:
  /// **'Import from Seed Phrase'**
  String get importFromSeed;

  /// No description provided for @importFromSeedSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Restore wallet using your 25-word mnemonic seed'**
  String get importFromSeedSubtitle;

  /// No description provided for @importFromKeys.
  ///
  /// In en, this message translates to:
  /// **'Import from Private Keys'**
  String get importFromKeys;

  /// No description provided for @importFromKeysSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Restore using spend key and view key'**
  String get importFromKeysSubtitle;

  /// No description provided for @viewOnlyWallet.
  ///
  /// In en, this message translates to:
  /// **'View-Only Wallet'**
  String get viewOnlyWallet;

  /// No description provided for @viewOnlyWalletSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Watch-only wallet using view key and address'**
  String get viewOnlyWalletSubtitle;

  /// No description provided for @createWallet.
  ///
  /// In en, this message translates to:
  /// **'Create Wallet'**
  String get createWallet;

  /// No description provided for @importWallet.
  ///
  /// In en, this message translates to:
  /// **'Import Wallet'**
  String get importWallet;

  /// No description provided for @walletName.
  ///
  /// In en, this message translates to:
  /// **'Wallet Name'**
  String get walletName;

  /// No description provided for @walletNameHint.
  ///
  /// In en, this message translates to:
  /// **'e.g. Main Wallet'**
  String get walletNameHint;

  /// No description provided for @passwordLabel.
  ///
  /// In en, this message translates to:
  /// **'Password'**
  String get passwordLabel;

  /// No description provided for @enterPassword.
  ///
  /// In en, this message translates to:
  /// **'Enter password'**
  String get enterPassword;

  /// No description provided for @confirmPassword.
  ///
  /// In en, this message translates to:
  /// **'Confirm password'**
  String get confirmPassword;

  /// No description provided for @seedPhrase.
  ///
  /// In en, this message translates to:
  /// **'Seed Phrase (25 words)'**
  String get seedPhrase;

  /// No description provided for @enterSeedPhrase.
  ///
  /// In en, this message translates to:
  /// **'Enter your seed phrase...'**
  String get enterSeedPhrase;

  /// No description provided for @scanHeight.
  ///
  /// In en, this message translates to:
  /// **'Scan Height (optional)'**
  String get scanHeight;

  /// No description provided for @scanHeightHint.
  ///
  /// In en, this message translates to:
  /// **'0 = scan from beginning'**
  String get scanHeightHint;

  /// No description provided for @privateSpendKey.
  ///
  /// In en, this message translates to:
  /// **'Private Spend Key'**
  String get privateSpendKey;

  /// No description provided for @privateViewKey.
  ///
  /// In en, this message translates to:
  /// **'Private View Key'**
  String get privateViewKey;

  /// No description provided for @walletAddress.
  ///
  /// In en, this message translates to:
  /// **'Wallet Address'**
  String get walletAddress;

  /// No description provided for @walletAddressHint.
  ///
  /// In en, this message translates to:
  /// **'Wrkz... address'**
  String get walletAddressHint;

  /// No description provided for @hexKey.
  ///
  /// In en, this message translates to:
  /// **'64-char hex'**
  String get hexKey;

  /// No description provided for @daemonNode.
  ///
  /// In en, this message translates to:
  /// **'Daemon Node'**
  String get daemonNode;

  /// No description provided for @custom.
  ///
  /// In en, this message translates to:
  /// **'Custom'**
  String get custom;

  /// No description provided for @host.
  ///
  /// In en, this message translates to:
  /// **'Host'**
  String get host;

  /// No description provided for @hostHint.
  ///
  /// In en, this message translates to:
  /// **'Host / IP'**
  String get hostHint;

  /// No description provided for @port.
  ///
  /// In en, this message translates to:
  /// **'Port'**
  String get port;

  /// No description provided for @ssl.
  ///
  /// In en, this message translates to:
  /// **'SSL'**
  String get ssl;

  /// No description provided for @walletNameRequired.
  ///
  /// In en, this message translates to:
  /// **'Wallet name is required'**
  String get walletNameRequired;

  /// No description provided for @passwordRequired.
  ///
  /// In en, this message translates to:
  /// **'Password is required'**
  String get passwordRequired;

  /// Shown when a chosen wallet password is below the minimum length
  ///
  /// In en, this message translates to:
  /// **'Password must be at least {count} characters'**
  String passwordTooShort(int count);

  /// No description provided for @passwordsDoNotMatch.
  ///
  /// In en, this message translates to:
  /// **'Passwords do not match'**
  String get passwordsDoNotMatch;

  /// No description provided for @seedRequired.
  ///
  /// In en, this message translates to:
  /// **'Seed phrase is required'**
  String get seedRequired;

  /// No description provided for @spendKeyRequired.
  ///
  /// In en, this message translates to:
  /// **'Spend key is required'**
  String get spendKeyRequired;

  /// No description provided for @viewKeyRequired.
  ///
  /// In en, this message translates to:
  /// **'View key is required'**
  String get viewKeyRequired;

  /// No description provided for @addressRequired.
  ///
  /// In en, this message translates to:
  /// **'Address is required'**
  String get addressRequired;

  /// No description provided for @daemonHostRequired.
  ///
  /// In en, this message translates to:
  /// **'Daemon host is required'**
  String get daemonHostRequired;

  /// No description provided for @backupSeedTitle.
  ///
  /// In en, this message translates to:
  /// **'Backup Your Seed'**
  String get backupSeedTitle;

  /// No description provided for @backupWarning.
  ///
  /// In en, this message translates to:
  /// **'Write down your seed phrase and store it safely. If you lose it, your funds are gone forever.'**
  String get backupWarning;

  /// No description provided for @seedPhraseLabel.
  ///
  /// In en, this message translates to:
  /// **'Seed Phrase'**
  String get seedPhraseLabel;

  /// No description provided for @privateViewKeyLabel.
  ///
  /// In en, this message translates to:
  /// **'Private View Key'**
  String get privateViewKeyLabel;

  /// No description provided for @privateSpendKeyLabel.
  ///
  /// In en, this message translates to:
  /// **'Private Spend Key'**
  String get privateSpendKeyLabel;

  /// No description provided for @backupConfirmCheck.
  ///
  /// In en, this message translates to:
  /// **'I have safely backed up my seed phrase'**
  String get backupConfirmCheck;

  /// No description provided for @continueToWallet.
  ///
  /// In en, this message translates to:
  /// **'Continue to Wallet'**
  String get continueToWallet;

  /// No description provided for @sectionDaemonNode.
  ///
  /// In en, this message translates to:
  /// **'Daemon Node'**
  String get sectionDaemonNode;

  /// No description provided for @apply.
  ///
  /// In en, this message translates to:
  /// **'Apply'**
  String get apply;

  /// No description provided for @nodeUpdated.
  ///
  /// In en, this message translates to:
  /// **'Node updated to {host}:{port}'**
  String nodeUpdated(String host, int port);

  /// No description provided for @hostRequired.
  ///
  /// In en, this message translates to:
  /// **'Host is required'**
  String get hostRequired;

  /// No description provided for @currentWallet.
  ///
  /// In en, this message translates to:
  /// **'Current Wallet — {name}'**
  String currentWallet(String name);

  /// No description provided for @saveWallet.
  ///
  /// In en, this message translates to:
  /// **'Save Wallet'**
  String get saveWallet;

  /// No description provided for @walletSaved.
  ///
  /// In en, this message translates to:
  /// **'Wallet saved'**
  String get walletSaved;

  /// No description provided for @saveFailed.
  ///
  /// In en, this message translates to:
  /// **'Save failed: {error}'**
  String saveFailed(String error);

  /// No description provided for @backupSeed.
  ///
  /// In en, this message translates to:
  /// **'Backup Seed'**
  String get backupSeed;

  /// No description provided for @changePassword.
  ///
  /// In en, this message translates to:
  /// **'Change Password'**
  String get changePassword;

  /// No description provided for @resetScanHeight.
  ///
  /// In en, this message translates to:
  /// **'Reset Scan Height'**
  String get resetScanHeight;

  /// No description provided for @reset.
  ///
  /// In en, this message translates to:
  /// **'Reset'**
  String get reset;

  /// No description provided for @resetScanConfirm.
  ///
  /// In en, this message translates to:
  /// **'This will rescan the blockchain from block {height}. This may take a while. Continue?'**
  String resetScanConfirm(int height);

  /// No description provided for @scanResetTo.
  ///
  /// In en, this message translates to:
  /// **'Scan reset to block {height}'**
  String scanResetTo(int height);

  /// No description provided for @resetFailed.
  ///
  /// In en, this message translates to:
  /// **'Reset failed: {error}'**
  String resetFailed(String error);

  /// No description provided for @enterPasswordTitle.
  ///
  /// In en, this message translates to:
  /// **'Enter Password'**
  String get enterPasswordTitle;

  /// No description provided for @confirm.
  ///
  /// In en, this message translates to:
  /// **'Confirm'**
  String get confirm;

  /// No description provided for @seedBackup.
  ///
  /// In en, this message translates to:
  /// **'Seed Backup'**
  String get seedBackup;

  /// No description provided for @seedPhraseColon.
  ///
  /// In en, this message translates to:
  /// **'Seed Phrase:'**
  String get seedPhraseColon;

  /// No description provided for @privateViewKeyColon.
  ///
  /// In en, this message translates to:
  /// **'Private View Key:'**
  String get privateViewKeyColon;

  /// No description provided for @iveBackedUp.
  ///
  /// In en, this message translates to:
  /// **'I\'ve backed up'**
  String get iveBackedUp;

  /// No description provided for @currentPasswordLabel.
  ///
  /// In en, this message translates to:
  /// **'Current password'**
  String get currentPasswordLabel;

  /// No description provided for @newPasswordLabel.
  ///
  /// In en, this message translates to:
  /// **'New password'**
  String get newPasswordLabel;

  /// No description provided for @confirmNewPasswordLabel.
  ///
  /// In en, this message translates to:
  /// **'Confirm new password'**
  String get confirmNewPasswordLabel;

  /// No description provided for @change.
  ///
  /// In en, this message translates to:
  /// **'Change'**
  String get change;

  /// No description provided for @currentPasswordIncorrect.
  ///
  /// In en, this message translates to:
  /// **'Current password is incorrect'**
  String get currentPasswordIncorrect;

  /// No description provided for @newPasswordsDoNotMatch.
  ///
  /// In en, this message translates to:
  /// **'New passwords do not match'**
  String get newPasswordsDoNotMatch;

  /// No description provided for @passwordChanged.
  ///
  /// In en, this message translates to:
  /// **'Password changed'**
  String get passwordChanged;

  /// No description provided for @walletManagement.
  ///
  /// In en, this message translates to:
  /// **'Wallet Management'**
  String get walletManagement;

  /// No description provided for @switchWalletSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Save & close, pick another'**
  String get switchWalletSubtitle;

  /// No description provided for @manageWallets.
  ///
  /// In en, this message translates to:
  /// **'Manage Wallets'**
  String get manageWallets;

  /// No description provided for @manageWalletsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Rename or delete wallets'**
  String get manageWalletsSubtitle;

  /// No description provided for @currentlyOpen.
  ///
  /// In en, this message translates to:
  /// **'(currently open)'**
  String get currentlyOpen;

  /// No description provided for @close.
  ///
  /// In en, this message translates to:
  /// **'Close'**
  String get close;

  /// No description provided for @renameWallet.
  ///
  /// In en, this message translates to:
  /// **'Rename Wallet'**
  String get renameWallet;

  /// No description provided for @newName.
  ///
  /// In en, this message translates to:
  /// **'New name'**
  String get newName;

  /// No description provided for @rename.
  ///
  /// In en, this message translates to:
  /// **'Rename'**
  String get rename;

  /// No description provided for @deleteWalletConfirmShort.
  ///
  /// In en, this message translates to:
  /// **'Delete \"{name}\"? This cannot be undone.'**
  String deleteWalletConfirmShort(String name);

  /// No description provided for @security.
  ///
  /// In en, this message translates to:
  /// **'Security'**
  String get security;

  /// No description provided for @biometricUnlock.
  ///
  /// In en, this message translates to:
  /// **'Biometric Unlock'**
  String get biometricUnlock;

  /// No description provided for @biometricSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Fingerprint / Face ID'**
  String get biometricSubtitle;

  /// No description provided for @biometricNotAvailable.
  ///
  /// In en, this message translates to:
  /// **'Biometric not available'**
  String get biometricNotAvailable;

  /// No description provided for @autoLock.
  ///
  /// In en, this message translates to:
  /// **'Auto-Lock'**
  String get autoLock;

  /// No description provided for @appearance.
  ///
  /// In en, this message translates to:
  /// **'Appearance'**
  String get appearance;

  /// No description provided for @theme.
  ///
  /// In en, this message translates to:
  /// **'Theme'**
  String get theme;

  /// No description provided for @themeAuto.
  ///
  /// In en, this message translates to:
  /// **'Auto'**
  String get themeAuto;

  /// No description provided for @themeLight.
  ///
  /// In en, this message translates to:
  /// **'Light'**
  String get themeLight;

  /// No description provided for @themeDark.
  ///
  /// In en, this message translates to:
  /// **'Dark'**
  String get themeDark;

  /// No description provided for @preferences.
  ///
  /// In en, this message translates to:
  /// **'Preferences'**
  String get preferences;

  /// No description provided for @transactionNotifications.
  ///
  /// In en, this message translates to:
  /// **'Transaction Notifications'**
  String get transactionNotifications;

  /// No description provided for @notificationsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Alert on incoming transactions'**
  String get notificationsSubtitle;

  /// No description provided for @autosave.
  ///
  /// In en, this message translates to:
  /// **'Autosave'**
  String get autosave;

  /// No description provided for @autosaveSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Save after sync, then every 5 minutes'**
  String get autosaveSubtitle;

  /// No description provided for @scanCoinbaseTx.
  ///
  /// In en, this message translates to:
  /// **'Scan Coinbase Transactions'**
  String get scanCoinbaseTx;

  /// No description provided for @scanCoinbaseSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Include miner rewards (off by default)'**
  String get scanCoinbaseSubtitle;

  /// No description provided for @dangerZone.
  ///
  /// In en, this message translates to:
  /// **'Danger Zone'**
  String get dangerZone;

  /// No description provided for @deleteCurrentWallet.
  ///
  /// In en, this message translates to:
  /// **'Delete Current Wallet'**
  String get deleteCurrentWallet;

  /// No description provided for @deleteCurrentWalletSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Permanently remove wallet data'**
  String get deleteCurrentWalletSubtitle;

  /// No description provided for @deleteWalletTypeCaps.
  ///
  /// In en, this message translates to:
  /// **'This will permanently delete the wallet file and keys. Make sure you have backed up your seed phrase.\n\nType DELETE to confirm:'**
  String get deleteWalletTypeCaps;

  /// No description provided for @deleteHint.
  ///
  /// In en, this message translates to:
  /// **'DELETE'**
  String get deleteHint;

  /// No description provided for @language.
  ///
  /// In en, this message translates to:
  /// **'Language'**
  String get language;

  /// No description provided for @selectLanguage.
  ///
  /// In en, this message translates to:
  /// **'Select Language'**
  String get selectLanguage;

  /// No description provided for @languageEn.
  ///
  /// In en, this message translates to:
  /// **'English'**
  String get languageEn;

  /// No description provided for @languageFr.
  ///
  /// In en, this message translates to:
  /// **'Français'**
  String get languageFr;

  /// No description provided for @languageDe.
  ///
  /// In en, this message translates to:
  /// **'Deutsch'**
  String get languageDe;

  /// No description provided for @languageZh.
  ///
  /// In en, this message translates to:
  /// **'中文'**
  String get languageZh;

  /// No description provided for @languageVi.
  ///
  /// In en, this message translates to:
  /// **'Tiếng Việt'**
  String get languageVi;

  /// No description provided for @languageJa.
  ///
  /// In en, this message translates to:
  /// **'日本語'**
  String get languageJa;

  /// No description provided for @languageEs.
  ///
  /// In en, this message translates to:
  /// **'Español'**
  String get languageEs;

  /// No description provided for @languagePt.
  ///
  /// In en, this message translates to:
  /// **'Português'**
  String get languagePt;

  /// No description provided for @languageRu.
  ///
  /// In en, this message translates to:
  /// **'Русский'**
  String get languageRu;

  /// No description provided for @autoLockImmediately.
  ///
  /// In en, this message translates to:
  /// **'Immediately'**
  String get autoLockImmediately;

  /// No description provided for @autoLock1Min.
  ///
  /// In en, this message translates to:
  /// **'1 minute'**
  String get autoLock1Min;

  /// No description provided for @autoLock5Min.
  ///
  /// In en, this message translates to:
  /// **'5 minutes'**
  String get autoLock5Min;

  /// No description provided for @autoLockNever.
  ///
  /// In en, this message translates to:
  /// **'Never'**
  String get autoLockNever;

  /// Added by the wallet review fixes
  ///
  /// In en, this message translates to:
  /// **'This transaction is no longer valid. Go back and create it again.'**
  String get preparedTransactionExpired;

  /// Added by the wallet review fixes
  ///
  /// In en, this message translates to:
  /// **'Type DELETE exactly to confirm.'**
  String get deleteConfirmMismatch;

  /// Added by the wallet review fixes
  ///
  /// In en, this message translates to:
  /// **'You have not confirmed a backup of this wallet\'s seed phrase. Deleting it now means the funds cannot be recovered.'**
  String get seedNotBackedUpWarning;

  /// Added by the wallet review fixes
  ///
  /// In en, this message translates to:
  /// **'WRKZ received'**
  String get wrkzReceived;

  /// Added by the wallet review fixes
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get retry;

  /// Added by the wallet review fixes
  ///
  /// In en, this message translates to:
  /// **'You received {amount}'**
  String youReceivedAmount(String amount);
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
    'that was used.',
  );
}
