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
  /// **'PLUTON v2'**
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

  /// No description provided for @tabTransfer.
  ///
  /// In en, this message translates to:
  /// **'Transfer'**
  String get tabTransfer;

  /// No description provided for @tabHistory.
  ///
  /// In en, this message translates to:
  /// **'History'**
  String get tabHistory;

  /// No description provided for @tabAddressBook.
  ///
  /// In en, this message translates to:
  /// **'Address Book'**
  String get tabAddressBook;

  /// No description provided for @tabSettings.
  ///
  /// In en, this message translates to:
  /// **'Settings'**
  String get tabSettings;

  /// No description provided for @tabAbout.
  ///
  /// In en, this message translates to:
  /// **'About'**
  String get tabAbout;

  /// No description provided for @lockWallet.
  ///
  /// In en, this message translates to:
  /// **'Lock Wallet'**
  String get lockWallet;

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

  /// No description provided for @transfer.
  ///
  /// In en, this message translates to:
  /// **'Transfer'**
  String get transfer;

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

  /// No description provided for @availableBalance.
  ///
  /// In en, this message translates to:
  /// **'Available Balance'**
  String get availableBalance;

  /// No description provided for @lockedUnconfirmed.
  ///
  /// In en, this message translates to:
  /// **'Locked (unconfirmed): {amount} {ticker}'**
  String lockedUnconfirmed(String amount, String ticker);

  /// No description provided for @totalBalance.
  ///
  /// In en, this message translates to:
  /// **'Total: {amount} {ticker}'**
  String totalBalance(String amount, String ticker);

  /// No description provided for @balanceIncompleteWhileSyncing.
  ///
  /// In en, this message translates to:
  /// **'Balance may be incomplete while syncing'**
  String get balanceIncompleteWhileSyncing;

  /// No description provided for @errorPrefix.
  ///
  /// In en, this message translates to:
  /// **'Error: {message}'**
  String errorPrefix(String message);

  /// No description provided for @network.
  ///
  /// In en, this message translates to:
  /// **'Network'**
  String get network;

  /// No description provided for @syncStatus.
  ///
  /// In en, this message translates to:
  /// **'Sync status'**
  String get syncStatus;

  /// No description provided for @synced.
  ///
  /// In en, this message translates to:
  /// **'Synced'**
  String get synced;

  /// No description provided for @syncing.
  ///
  /// In en, this message translates to:
  /// **'Syncing…'**
  String get syncing;

  /// No description provided for @walletBlock.
  ///
  /// In en, this message translates to:
  /// **'Wallet block'**
  String get walletBlock;

  /// No description provided for @networkBlock.
  ///
  /// In en, this message translates to:
  /// **'Network block'**
  String get networkBlock;

  /// No description provided for @peers.
  ///
  /// In en, this message translates to:
  /// **'Peers'**
  String get peers;

  /// No description provided for @walletType.
  ///
  /// In en, this message translates to:
  /// **'Wallet type'**
  String get walletType;

  /// No description provided for @viewOnly.
  ///
  /// In en, this message translates to:
  /// **'View-only'**
  String get viewOnly;

  /// No description provided for @full.
  ///
  /// In en, this message translates to:
  /// **'Full'**
  String get full;

  /// No description provided for @nodeConnectionIssue.
  ///
  /// In en, this message translates to:
  /// **'Node connection issue'**
  String get nodeConnectionIssue;

  /// No description provided for @switchNodeInSettings.
  ///
  /// In en, this message translates to:
  /// **'Switch node in Settings →'**
  String get switchNodeInSettings;

  /// No description provided for @recentTransactions.
  ///
  /// In en, this message translates to:
  /// **'Recent Transactions'**
  String get recentTransactions;

  /// No description provided for @viewAll.
  ///
  /// In en, this message translates to:
  /// **'View all →'**
  String get viewAll;

  /// No description provided for @noTransactionsYet.
  ///
  /// In en, this message translates to:
  /// **'No transactions yet'**
  String get noTransactionsYet;

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

  /// No description provided for @syncingProgress.
  ///
  /// In en, this message translates to:
  /// **'Syncing {pct}% (block {wallet} / {network})'**
  String syncingProgress(String pct, int wallet, int network);

  /// No description provided for @shareAddressSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Share your address to receive WRKZ'**
  String get shareAddressSubtitle;

  /// No description provided for @yourAddress.
  ///
  /// In en, this message translates to:
  /// **'Your Address'**
  String get yourAddress;

  /// No description provided for @generateIntegratedAddress.
  ///
  /// In en, this message translates to:
  /// **'Generate Integrated Address'**
  String get generateIntegratedAddress;

  /// No description provided for @integratedAddressDescription.
  ///
  /// In en, this message translates to:
  /// **'Combine your address with a payment ID. Use the random buttons for a new ID, or enter your own below.'**
  String get integratedAddressDescription;

  /// No description provided for @randomShort16.
  ///
  /// In en, this message translates to:
  /// **'Random Short (16)'**
  String get randomShort16;

  /// No description provided for @randomLong64.
  ///
  /// In en, this message translates to:
  /// **'Random Long (64)'**
  String get randomLong64;

  /// No description provided for @customPaymentIdLabel.
  ///
  /// In en, this message translates to:
  /// **'Custom payment ID (16 or 64 hex chars)'**
  String get customPaymentIdLabel;

  /// No description provided for @generate.
  ///
  /// In en, this message translates to:
  /// **'Generate'**
  String get generate;

  /// No description provided for @integratedAddress.
  ///
  /// In en, this message translates to:
  /// **'Integrated Address'**
  String get integratedAddress;

  /// No description provided for @paymentIdShort.
  ///
  /// In en, this message translates to:
  /// **'Short (16)'**
  String get paymentIdShort;

  /// No description provided for @paymentIdLong.
  ///
  /// In en, this message translates to:
  /// **'Long (64)'**
  String get paymentIdLong;

  /// No description provided for @paymentIdLabel.
  ///
  /// In en, this message translates to:
  /// **'Payment ID · {label}'**
  String paymentIdLabel(String label);

  /// No description provided for @enterPaymentIdError.
  ///
  /// In en, this message translates to:
  /// **'Enter a payment ID (16 or 64 hex chars)'**
  String get enterPaymentIdError;

  /// No description provided for @paymentIdInvalidError.
  ///
  /// In en, this message translates to:
  /// **'Payment ID must be 16 or 64 hex characters'**
  String get paymentIdInvalidError;

  /// No description provided for @copyAddress.
  ///
  /// In en, this message translates to:
  /// **'Copy address'**
  String get copyAddress;

  /// No description provided for @copyPaymentId.
  ///
  /// In en, this message translates to:
  /// **'Copy payment ID'**
  String get copyPaymentId;

  /// No description provided for @copy.
  ///
  /// In en, this message translates to:
  /// **'Copy'**
  String get copy;

  /// No description provided for @copied.
  ///
  /// In en, this message translates to:
  /// **'Copied!'**
  String get copied;

  /// No description provided for @sendWrkzToAny.
  ///
  /// In en, this message translates to:
  /// **'Send WRKZ to any address'**
  String get sendWrkzToAny;

  /// No description provided for @sweepAllDescription.
  ///
  /// In en, this message translates to:
  /// **'Send all funds to an address (consolidates UTXOs)'**
  String get sweepAllDescription;

  /// No description provided for @sweepAll.
  ///
  /// In en, this message translates to:
  /// **'Sweep all'**
  String get sweepAll;

  /// No description provided for @sweepWarning.
  ///
  /// In en, this message translates to:
  /// **'Sweep consolidates all UTXOs into one output. Use this when transactions fail due to too many inputs.'**
  String get sweepWarning;

  /// No description provided for @sweepAvailableBalance.
  ///
  /// In en, this message translates to:
  /// **'Available: {amount} {ticker} (entire balance will be sent minus fee)'**
  String sweepAvailableBalance(String amount, String ticker);

  /// No description provided for @destinationAddress.
  ///
  /// In en, this message translates to:
  /// **'Destination address'**
  String get destinationAddress;

  /// No description provided for @addressBook.
  ///
  /// In en, this message translates to:
  /// **'Address book'**
  String get addressBook;

  /// No description provided for @sweepAllFunds.
  ///
  /// In en, this message translates to:
  /// **'Sweep All Funds'**
  String get sweepAllFunds;

  /// No description provided for @recipientAddress.
  ///
  /// In en, this message translates to:
  /// **'Recipient address'**
  String get recipientAddress;

  /// No description provided for @amount.
  ///
  /// In en, this message translates to:
  /// **'Amount'**
  String get amount;

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

  /// No description provided for @reviewTransaction.
  ///
  /// In en, this message translates to:
  /// **'Review Transaction'**
  String get reviewTransaction;

  /// No description provided for @reviewAndConfirm.
  ///
  /// In en, this message translates to:
  /// **'Review & Confirm'**
  String get reviewAndConfirm;

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
  /// **'Total deducted'**
  String get totalDeducted;

  /// No description provided for @paymentId.
  ///
  /// In en, this message translates to:
  /// **'Payment ID'**
  String get paymentId;

  /// No description provided for @transactionsIrreversible.
  ///
  /// In en, this message translates to:
  /// **'Transactions are irreversible. Verify the address before confirming.'**
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

  /// No description provided for @transactionBroadcast.
  ///
  /// In en, this message translates to:
  /// **'Your transaction has been broadcast to the network.'**
  String get transactionBroadcast;

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

  /// No description provided for @enterDestinationAddress.
  ///
  /// In en, this message translates to:
  /// **'Enter a destination address'**
  String get enterDestinationAddress;

  /// No description provided for @enterValidAmount.
  ///
  /// In en, this message translates to:
  /// **'Enter a valid amount'**
  String get enterValidAmount;

  /// No description provided for @computingPow.
  ///
  /// In en, this message translates to:
  /// **'Computing PoW... {seconds}s'**
  String computingPow(int seconds);

  /// No description provided for @stepFillDetails.
  ///
  /// In en, this message translates to:
  /// **'Fill Details'**
  String get stepFillDetails;

  /// No description provided for @stepReview.
  ///
  /// In en, this message translates to:
  /// **'Review'**
  String get stepReview;

  /// No description provided for @stepDone.
  ///
  /// In en, this message translates to:
  /// **'Done'**
  String get stepDone;

  /// No description provided for @sweepFailed.
  ///
  /// In en, this message translates to:
  /// **'Sweep failed'**
  String get sweepFailed;

  /// No description provided for @addressBookTitle.
  ///
  /// In en, this message translates to:
  /// **'Address Book'**
  String get addressBookTitle;

  /// No description provided for @transactionHistory.
  ///
  /// In en, this message translates to:
  /// **'Transaction History'**
  String get transactionHistory;

  /// No description provided for @searchByHash.
  ///
  /// In en, this message translates to:
  /// **'Search by hash, address or payment ID…'**
  String get searchByHash;

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

  /// No description provided for @refresh.
  ///
  /// In en, this message translates to:
  /// **'Refresh'**
  String get refresh;

  /// No description provided for @noTransactionsFound.
  ///
  /// In en, this message translates to:
  /// **'No transactions found'**
  String get noTransactionsFound;

  /// No description provided for @confirmed.
  ///
  /// In en, this message translates to:
  /// **'Confirmed'**
  String get confirmed;

  /// No description provided for @pending.
  ///
  /// In en, this message translates to:
  /// **'Pending'**
  String get pending;

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

  /// No description provided for @showingRange.
  ///
  /// In en, this message translates to:
  /// **'Showing {start}–{end} of {total}'**
  String showingRange(int start, int end, int total);

  /// No description provided for @previous.
  ///
  /// In en, this message translates to:
  /// **'Previous'**
  String get previous;

  /// No description provided for @next.
  ///
  /// In en, this message translates to:
  /// **'Next'**
  String get next;

  /// No description provided for @walletLocked.
  ///
  /// In en, this message translates to:
  /// **'Wallet Locked'**
  String get walletLocked;

  /// No description provided for @enterPasswordToContinue.
  ///
  /// In en, this message translates to:
  /// **'Enter your wallet password to continue'**
  String get enterPasswordToContinue;

  /// No description provided for @password.
  ///
  /// In en, this message translates to:
  /// **'Password'**
  String get password;

  /// No description provided for @incorrectPassword.
  ///
  /// In en, this message translates to:
  /// **'Incorrect password'**
  String get incorrectPassword;

  /// No description provided for @unlock.
  ///
  /// In en, this message translates to:
  /// **'Unlock'**
  String get unlock;

  /// No description provided for @closeWalletInstead.
  ///
  /// In en, this message translates to:
  /// **'Close wallet instead'**
  String get closeWalletInstead;

  /// No description provided for @closeWallet.
  ///
  /// In en, this message translates to:
  /// **'Close Wallet'**
  String get closeWallet;

  /// No description provided for @closeWalletDescription.
  ///
  /// In en, this message translates to:
  /// **'This will save and close the wallet.\n\nYou will be returned to the login screen.'**
  String get closeWalletDescription;

  /// No description provided for @cancel.
  ///
  /// In en, this message translates to:
  /// **'Cancel'**
  String get cancel;

  /// No description provided for @welcomeToPluton.
  ///
  /// In en, this message translates to:
  /// **'Welcome to PLUTON v2'**
  String get welcomeToPluton;

  /// No description provided for @selectOptionToStart.
  ///
  /// In en, this message translates to:
  /// **'Select an option to get started'**
  String get selectOptionToStart;

  /// No description provided for @createNewWallet.
  ///
  /// In en, this message translates to:
  /// **'Create New Wallet'**
  String get createNewWallet;

  /// No description provided for @openExistingWallet.
  ///
  /// In en, this message translates to:
  /// **'Open Existing Wallet'**
  String get openExistingWallet;

  /// No description provided for @importFromSeed.
  ///
  /// In en, this message translates to:
  /// **'Import from Seed Phrase'**
  String get importFromSeed;

  /// No description provided for @importFromKeys.
  ///
  /// In en, this message translates to:
  /// **'Import from Private Keys'**
  String get importFromKeys;

  /// No description provided for @openWallet.
  ///
  /// In en, this message translates to:
  /// **'Open Wallet'**
  String get openWallet;

  /// No description provided for @importFromSeedTitle.
  ///
  /// In en, this message translates to:
  /// **'Import from Seed'**
  String get importFromSeedTitle;

  /// No description provided for @importFromKeysTitle.
  ///
  /// In en, this message translates to:
  /// **'Import from Keys'**
  String get importFromKeysTitle;

  /// No description provided for @saveWalletTo.
  ///
  /// In en, this message translates to:
  /// **'Save wallet to'**
  String get saveWalletTo;

  /// No description provided for @walletFile.
  ///
  /// In en, this message translates to:
  /// **'Wallet file'**
  String get walletFile;

  /// No description provided for @walletPassword.
  ///
  /// In en, this message translates to:
  /// **'Wallet password'**
  String get walletPassword;

  /// No description provided for @mnemonicSeedPhrase.
  ///
  /// In en, this message translates to:
  /// **'Mnemonic Seed Phrase'**
  String get mnemonicSeedPhrase;

  /// No description provided for @scanFromHeight.
  ///
  /// In en, this message translates to:
  /// **'Scan from height (0 = full scan)'**
  String get scanFromHeight;

  /// No description provided for @daemonHost.
  ///
  /// In en, this message translates to:
  /// **'Daemon host'**
  String get daemonHost;

  /// No description provided for @port.
  ///
  /// In en, this message translates to:
  /// **'Port'**
  String get port;

  /// No description provided for @continueButton.
  ///
  /// In en, this message translates to:
  /// **'Continue'**
  String get continueButton;

  /// No description provided for @browse.
  ///
  /// In en, this message translates to:
  /// **'Browse'**
  String get browse;

  /// No description provided for @backupWarning.
  ///
  /// In en, this message translates to:
  /// **'Back up your wallet before continuing.\nThese keys cannot be recovered if lost.'**
  String get backupWarning;

  /// No description provided for @yourWalletAddress.
  ///
  /// In en, this message translates to:
  /// **'Your Wallet Address'**
  String get yourWalletAddress;

  /// No description provided for @seedPhrase25Words.
  ///
  /// In en, this message translates to:
  /// **'Seed Phrase (25 words)'**
  String get seedPhrase25Words;

  /// No description provided for @privateViewKey.
  ///
  /// In en, this message translates to:
  /// **'Private View Key'**
  String get privateViewKey;

  /// No description provided for @privateSpendKey.
  ///
  /// In en, this message translates to:
  /// **'Private Spend Key'**
  String get privateSpendKey;

  /// No description provided for @seedBackupConfirm.
  ///
  /// In en, this message translates to:
  /// **'I have written down my seed phrase and private keys in a safe place.'**
  String get seedBackupConfirm;

  /// No description provided for @backedUpContinue.
  ///
  /// In en, this message translates to:
  /// **'I\'ve backed up my wallet — Continue'**
  String get backedUpContinue;

  /// No description provided for @settings.
  ///
  /// In en, this message translates to:
  /// **'Settings'**
  String get settings;

  /// No description provided for @sectionDaemonNode.
  ///
  /// In en, this message translates to:
  /// **'Daemon Node'**
  String get sectionDaemonNode;

  /// No description provided for @nodeDescription.
  ///
  /// In en, this message translates to:
  /// **'Connect to a local or remote daemon node. Changes take effect immediately.'**
  String get nodeDescription;

  /// No description provided for @hostIpAddress.
  ///
  /// In en, this message translates to:
  /// **'Host / IP address'**
  String get hostIpAddress;

  /// No description provided for @ssl.
  ///
  /// In en, this message translates to:
  /// **'SSL'**
  String get ssl;

  /// No description provided for @apply.
  ///
  /// In en, this message translates to:
  /// **'Apply'**
  String get apply;

  /// No description provided for @nodeUpdatedSuccess.
  ///
  /// In en, this message translates to:
  /// **'Node updated successfully'**
  String get nodeUpdatedSuccess;

  /// No description provided for @nodeUnreachable.
  ///
  /// In en, this message translates to:
  /// **'Cannot reach the current node. Enter a new node address below and tap Apply.'**
  String get nodeUnreachable;

  /// No description provided for @sectionWallet.
  ///
  /// In en, this message translates to:
  /// **'Wallet'**
  String get sectionWallet;

  /// No description provided for @saveWallet.
  ///
  /// In en, this message translates to:
  /// **'Save Wallet'**
  String get saveWallet;

  /// No description provided for @saveWalletSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Flush current state to disk'**
  String get saveWalletSubtitle;

  /// No description provided for @walletSaved.
  ///
  /// In en, this message translates to:
  /// **'Wallet saved'**
  String get walletSaved;

  /// No description provided for @exportToJson.
  ///
  /// In en, this message translates to:
  /// **'Export to JSON'**
  String get exportToJson;

  /// No description provided for @exportToJsonSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Save wallet data as a JSON file'**
  String get exportToJsonSubtitle;

  /// No description provided for @exportJsonTitle.
  ///
  /// In en, this message translates to:
  /// **'Export wallet JSON'**
  String get exportJsonTitle;

  /// No description provided for @exportedTo.
  ///
  /// In en, this message translates to:
  /// **'Exported to {path}'**
  String exportedTo(String path);

  /// No description provided for @exportFailed.
  ///
  /// In en, this message translates to:
  /// **'Export failed: {error}'**
  String exportFailed(String error);

  /// No description provided for @resetScanHeight.
  ///
  /// In en, this message translates to:
  /// **'Reset Scan Height'**
  String get resetScanHeight;

  /// No description provided for @resetScanHeightSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Rescan blockchain from a specific height'**
  String get resetScanHeightSubtitle;

  /// No description provided for @resetScanHeightDescription.
  ///
  /// In en, this message translates to:
  /// **'Enter a block height to rescan from. Use 0 for a full rescan.'**
  String get resetScanHeightDescription;

  /// No description provided for @scanHeight.
  ///
  /// In en, this message translates to:
  /// **'Scan height'**
  String get scanHeight;

  /// No description provided for @reset.
  ///
  /// In en, this message translates to:
  /// **'Reset'**
  String get reset;

  /// No description provided for @autosave.
  ///
  /// In en, this message translates to:
  /// **'Autosave'**
  String get autosave;

  /// No description provided for @autosaveSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Save wallet to disk after sync and every 5 minutes'**
  String get autosaveSubtitle;

  /// No description provided for @scanCoinbaseTx.
  ///
  /// In en, this message translates to:
  /// **'Scan Coinbase Transactions'**
  String get scanCoinbaseTx;

  /// No description provided for @scanCoinbaseSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Include miner rewards when syncing (off by default)'**
  String get scanCoinbaseSubtitle;

  /// No description provided for @sectionAppearance.
  ///
  /// In en, this message translates to:
  /// **'Appearance'**
  String get sectionAppearance;

  /// No description provided for @theme.
  ///
  /// In en, this message translates to:
  /// **'Theme'**
  String get theme;

  /// No description provided for @themeSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Choose app colour scheme'**
  String get themeSubtitle;

  /// No description provided for @themeSystem.
  ///
  /// In en, this message translates to:
  /// **'System'**
  String get themeSystem;

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

  /// No description provided for @sectionNotifications.
  ///
  /// In en, this message translates to:
  /// **'Notifications'**
  String get sectionNotifications;

  /// No description provided for @incomingTxAlerts.
  ///
  /// In en, this message translates to:
  /// **'Incoming Transaction Alerts'**
  String get incomingTxAlerts;

  /// No description provided for @incomingTxAlertsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Show a desktop notification when WRKZ is received'**
  String get incomingTxAlertsSubtitle;

  /// No description provided for @sectionDebugLogs.
  ///
  /// In en, this message translates to:
  /// **'Debug & Logs'**
  String get sectionDebugLogs;

  /// No description provided for @logLevel.
  ///
  /// In en, this message translates to:
  /// **'Log Level'**
  String get logLevel;

  /// No description provided for @logLevelSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Controls wallet library verbosity'**
  String get logLevelSubtitle;

  /// No description provided for @viewLogs.
  ///
  /// In en, this message translates to:
  /// **'View Logs'**
  String get viewLogs;

  /// No description provided for @viewLogsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Live wallet library log output'**
  String get viewLogsSubtitle;

  /// No description provided for @walletLogs.
  ///
  /// In en, this message translates to:
  /// **'Wallet Logs'**
  String get walletLogs;

  /// No description provided for @logEntries.
  ///
  /// In en, this message translates to:
  /// **'{count} entries'**
  String logEntries(int count);

  /// No description provided for @autoScroll.
  ///
  /// In en, this message translates to:
  /// **'Auto-scroll'**
  String get autoScroll;

  /// No description provided for @copyAll.
  ///
  /// In en, this message translates to:
  /// **'Copy all'**
  String get copyAll;

  /// No description provided for @clear.
  ///
  /// In en, this message translates to:
  /// **'Clear'**
  String get clear;

  /// No description provided for @close.
  ///
  /// In en, this message translates to:
  /// **'Close'**
  String get close;

  /// No description provided for @noLogsYet.
  ///
  /// In en, this message translates to:
  /// **'No logs yet. Set a log level above Disabled to see output.'**
  String get noLogsYet;

  /// No description provided for @logsCopied.
  ///
  /// In en, this message translates to:
  /// **'Logs copied to clipboard'**
  String get logsCopied;

  /// No description provided for @sectionDangerZone.
  ///
  /// In en, this message translates to:
  /// **'Danger Zone'**
  String get sectionDangerZone;

  /// No description provided for @deleteWalletData.
  ///
  /// In en, this message translates to:
  /// **'Delete Wallet Data'**
  String get deleteWalletData;

  /// No description provided for @deleteWalletDataSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Permanently remove wallet file from disk'**
  String get deleteWalletDataSubtitle;

  /// No description provided for @deleteWalletWarning.
  ///
  /// In en, this message translates to:
  /// **'This will permanently delete your wallet file from disk.\n\nMake sure you have backed up your seed phrase and private keys before proceeding. This action cannot be undone.'**
  String get deleteWalletWarning;

  /// No description provided for @iUnderstandContinue.
  ///
  /// In en, this message translates to:
  /// **'I understand, continue'**
  String get iUnderstandContinue;

  /// No description provided for @finalConfirmation.
  ///
  /// In en, this message translates to:
  /// **'Final Confirmation'**
  String get finalConfirmation;

  /// No description provided for @typeDeleteToConfirm.
  ///
  /// In en, this message translates to:
  /// **'Type DELETE to confirm:'**
  String get typeDeleteToConfirm;

  /// No description provided for @deleteHint.
  ///
  /// In en, this message translates to:
  /// **'DELETE'**
  String get deleteHint;

  /// No description provided for @deletePermanently.
  ///
  /// In en, this message translates to:
  /// **'Delete permanently'**
  String get deletePermanently;

  /// No description provided for @aboutTitle.
  ///
  /// In en, this message translates to:
  /// **'About'**
  String get aboutTitle;

  /// No description provided for @versionInfo.
  ///
  /// In en, this message translates to:
  /// **'Version {version} — WRKZ Web Wallet'**
  String versionInfo(String version);

  /// No description provided for @aboutDescription.
  ///
  /// In en, this message translates to:
  /// **'PLUTON v2 is the official web wallet for WrkzCoin (WRKZ), a fast and lightweight CryptoNote-based cryptocurrency.\n\nBuilt with Flutter, powered by wallet-api.'**
  String get aboutDescription;

  /// No description provided for @github.
  ///
  /// In en, this message translates to:
  /// **'GitHub'**
  String get github;

  /// No description provided for @githubSubtitle.
  ///
  /// In en, this message translates to:
  /// **'View source code and releases'**
  String get githubSubtitle;

  /// No description provided for @discord.
  ///
  /// In en, this message translates to:
  /// **'Discord'**
  String get discord;

  /// No description provided for @discordSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Join the community'**
  String get discordSubtitle;

  /// No description provided for @twitterX.
  ///
  /// In en, this message translates to:
  /// **'Twitter / X'**
  String get twitterX;

  /// No description provided for @twitterXSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Follow @wrkzcoin'**
  String get twitterXSubtitle;

  /// No description provided for @website.
  ///
  /// In en, this message translates to:
  /// **'Website'**
  String get website;

  /// No description provided for @websiteSubtitle.
  ///
  /// In en, this message translates to:
  /// **'wrkz.work'**
  String get websiteSubtitle;

  /// No description provided for @license.
  ///
  /// In en, this message translates to:
  /// **'License'**
  String get license;

  /// No description provided for @licenseText.
  ///
  /// In en, this message translates to:
  /// **'Released under the MIT License.\nUse at your own risk. Always back up your seed phrase.'**
  String get licenseText;

  /// No description provided for @addButton.
  ///
  /// In en, this message translates to:
  /// **'Add'**
  String get addButton;

  /// No description provided for @noSavedAddresses.
  ///
  /// In en, this message translates to:
  /// **'No saved addresses'**
  String get noSavedAddresses;

  /// No description provided for @tapAddToSave.
  ///
  /// In en, this message translates to:
  /// **'Tap Add to save a frequently used address.'**
  String get tapAddToSave;

  /// No description provided for @addAddress.
  ///
  /// In en, this message translates to:
  /// **'Add Address'**
  String get addAddress;

  /// No description provided for @nameLabel.
  ///
  /// In en, this message translates to:
  /// **'Name / label'**
  String get nameLabel;

  /// No description provided for @addressLabel.
  ///
  /// In en, this message translates to:
  /// **'Address'**
  String get addressLabel;

  /// No description provided for @noteOptional.
  ///
  /// In en, this message translates to:
  /// **'Note (optional)'**
  String get noteOptional;

  /// No description provided for @nameAndAddressRequired.
  ///
  /// In en, this message translates to:
  /// **'Name and address are required'**
  String get nameAndAddressRequired;

  /// No description provided for @invalidWrkzAddress.
  ///
  /// In en, this message translates to:
  /// **'Invalid WRKZ address. Must be 98 (standard), 120 (short integrated), or 186 (long integrated) characters starting with \"Wrkz\".'**
  String get invalidWrkzAddress;

  /// No description provided for @save.
  ///
  /// In en, this message translates to:
  /// **'Save'**
  String get save;

  /// No description provided for @editEntry.
  ///
  /// In en, this message translates to:
  /// **'Edit Entry'**
  String get editEntry;

  /// No description provided for @deleteEntry.
  ///
  /// In en, this message translates to:
  /// **'Delete Entry'**
  String get deleteEntry;

  /// No description provided for @removeFromAddressBook.
  ///
  /// In en, this message translates to:
  /// **'Remove \"{name}\" from your address book?'**
  String removeFromAddressBook(String name);

  /// No description provided for @delete.
  ///
  /// In en, this message translates to:
  /// **'Delete'**
  String get delete;

  /// No description provided for @edit.
  ///
  /// In en, this message translates to:
  /// **'Edit'**
  String get edit;

  /// No description provided for @wrkzReceived.
  ///
  /// In en, this message translates to:
  /// **'WRKZ Received'**
  String get wrkzReceived;

  /// No description provided for @youReceivedAmount.
  ///
  /// In en, this message translates to:
  /// **'You received {amount}'**
  String youReceivedAmount(String amount);

  /// No description provided for @show.
  ///
  /// In en, this message translates to:
  /// **'Show'**
  String get show;

  /// No description provided for @exit.
  ///
  /// In en, this message translates to:
  /// **'Exit'**
  String get exit;

  /// No description provided for @plutonWallet.
  ///
  /// In en, this message translates to:
  /// **'PLUTON Wallet'**
  String get plutonWallet;

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

  /// No description provided for @chooseLanguage.
  ///
  /// In en, this message translates to:
  /// **'Choose your preferred language'**
  String get chooseLanguage;

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

  /// Page not found
  ///
  /// In en, this message translates to:
  /// **'Page not found'**
  String get pageNotFound;

  /// Back to wallet
  ///
  /// In en, this message translates to:
  /// **'Back to wallet'**
  String get backToWallet;

  /// Hide
  ///
  /// In en, this message translates to:
  /// **'Hide'**
  String get hide;

  /// MAX
  ///
  /// In en, this message translates to:
  /// **'MAX'**
  String get max;

  /// View in explorer
  ///
  /// In en, this message translates to:
  /// **'View in explorer'**
  String get viewInExplorer;

  /// Pending confirmation
  ///
  /// In en, this message translates to:
  /// **'Pending confirmation'**
  String get pendingConfirmation;

  /// A WRKZ address starts with “Wrkz”
  ///
  /// In en, this message translates to:
  /// **'A WRKZ address starts with “Wrkz”'**
  String get addressWrongPrefix;

  /// Wrong length — a WRKZ address is 98 characters (120 or 186 if integrated)
  ///
  /// In en, this message translates to:
  /// **'Wrong length — a WRKZ address is 98 characters (120 or 186 if integrated)'**
  String get addressWrongLength;

  /// Contains characters that are not valid in an address (0, O, I and l are never used)
  ///
  /// In en, this message translates to:
  /// **'Contains characters that are not valid in an address (0, O, I and l are never used)'**
  String get addressBadCharacters;

  /// Already saved as “{name}”
  ///
  /// In en, this message translates to:
  /// **'Already saved as “{name}”'**
  String addressAlreadySaved(String name);

  /// Amount must be greater than zero
  ///
  /// In en, this message translates to:
  /// **'Amount must be greater than zero'**
  String get amountMustBePositive;

  /// At most {places} decimal places
  ///
  /// In en, this message translates to:
  /// **'At most {places} decimal places'**
  String amountTooManyDecimals(int places);

  /// Amount is too large
  ///
  /// In en, this message translates to:
  /// **'Amount is too large'**
  String get amountTooLarge;

  /// More than your available balance ({amount} {ticker}, plus fee)
  ///
  /// In en, this message translates to:
  /// **'More than your available balance ({amount} {ticker}, plus fee)'**
  String amountExceedsBalance(String amount, String ticker);

  /// Network fee
  ///
  /// In en, this message translates to:
  /// **'Network fee'**
  String get networkFee;

  /// Fast
  ///
  /// In en, this message translates to:
  /// **'Fast'**
  String get feeFast;

  /// Economy
  ///
  /// In en, this message translates to:
  /// **'Economy'**
  String get feeEconomy;

  /// Pays {amount} {ticker} to skip the transaction proof of work. Sends in seconds.
  ///
  /// In en, this message translates to:
  /// **'Pays {amount} {ticker} to skip the transaction proof of work. Sends in seconds.'**
  String feeFastHint(String amount, String ticker);

  /// Pays the network minimum, but your browser must compute the transaction proof of work —...
  ///
  /// In en, this message translates to:
  /// **'Pays the network minimum, but your browser must compute the transaction proof of work — this can take several minutes.'**
  String get feeEconomyHint;

  /// This address already contains a payment ID — leave the payment ID field empty.
  ///
  /// In en, this message translates to:
  /// **'This address already contains a payment ID — leave the payment ID field empty.'**
  String get paymentIdWithIntegrated;

  /// This sends your entire balance, minus fees, to:
  ///
  /// In en, this message translates to:
  /// **'This sends your entire balance, minus fees, to:'**
  String get sweepConfirmBody;

  /// Too many attempts — try again in {seconds}s
  ///
  /// In en, this message translates to:
  /// **'Too many attempts — try again in {seconds}s'**
  String tooManyAttempts(int seconds);

  /// Security
  ///
  /// In en, this message translates to:
  /// **'Security'**
  String get sectionSecurity;

  /// Auto-lock
  ///
  /// In en, this message translates to:
  /// **'Auto-lock'**
  String get autoLock;

  /// Lock the wallet after a period of inactivity
  ///
  /// In en, this message translates to:
  /// **'Lock the wallet after a period of inactivity'**
  String get autoLockSubtitle;

  /// Never
  ///
  /// In en, this message translates to:
  /// **'Never'**
  String get autoLockNever;

  /// {minutes} min
  ///
  /// In en, this message translates to:
  /// **'{minutes} min'**
  String autoLockMinutes(int minutes);

  /// Confirm password
  ///
  /// In en, this message translates to:
  /// **'Confirm password'**
  String get confirmPassword;

  /// Confirm new password
  ///
  /// In en, this message translates to:
  /// **'Confirm new password'**
  String get confirmPasswordField;

  /// New password
  ///
  /// In en, this message translates to:
  /// **'New password'**
  String get newPassword;

  /// Change Password
  ///
  /// In en, this message translates to:
  /// **'Change Password'**
  String get changePassword;

  /// Re-encrypt this wallet with a new password
  ///
  /// In en, this message translates to:
  /// **'Re-encrypt this wallet with a new password'**
  String get changePasswordSubtitle;

  /// Enter your current wallet password.
  ///
  /// In en, this message translates to:
  /// **'Enter your current wallet password.'**
  String get changePasswordPurpose;

  /// Password changed
  ///
  /// In en, this message translates to:
  /// **'Password changed'**
  String get passwordChanged;

  /// Passwords do not match
  ///
  /// In en, this message translates to:
  /// **'Passwords do not match'**
  String get passwordsDoNotMatch;

  /// Use a password of at least {length} characters
  ///
  /// In en, this message translates to:
  /// **'Use a password of at least {length} characters'**
  String passwordTooShort(int length);

  /// Seed Phrase & Private Keys
  ///
  /// In en, this message translates to:
  /// **'Seed Phrase & Private Keys'**
  String get showSeedAndKeys;

  /// Reveal your recovery seed and keys (asks for your password)
  ///
  /// In en, this message translates to:
  /// **'Reveal your recovery seed and keys (asks for your password)'**
  String get showSeedAndKeysSubtitle;

  /// Your seed phrase and private keys will be shown on screen.
  ///
  /// In en, this message translates to:
  /// **'Your seed phrase and private keys will be shown on screen.'**
  String get revealSecretsPurpose;

  /// Anyone with these can spend your funds. Make sure nobody can see your screen, and never...
  ///
  /// In en, this message translates to:
  /// **'Anyone with these can spend your funds. Make sure nobody can see your screen, and never share them — no support channel will ever ask for them.'**
  String get secretsWarning;

  /// Tap to reveal
  ///
  /// In en, this message translates to:
  /// **'Tap to reveal'**
  String get tapToReveal;

  /// This is a view-only wallet — it has no seed phrase or spend key.
  ///
  /// In en, this message translates to:
  /// **'This is a view-only wallet — it has no seed phrase or spend key.'**
  String get viewOnlyNoSeed;

  /// Confirm your backup
  ///
  /// In en, this message translates to:
  /// **'Confirm your backup'**
  String get verifySeedTitle;

  /// Type the requested words from the phrase you just wrote down.
  ///
  /// In en, this message translates to:
  /// **'Type the requested words from the phrase you just wrote down.'**
  String get verifySeedSubtitle;

  /// Word #{number}
  ///
  /// In en, this message translates to:
  /// **'Word #{number}'**
  String wordNumber(int number);

  /// Ask me different words
  ///
  /// In en, this message translates to:
  /// **'Ask me different words'**
  String get askDifferentWords;

  /// Download Wallet File
  ///
  /// In en, this message translates to:
  /// **'Download Wallet File'**
  String get downloadWalletFile;

  /// Save an encrypted backup of this wallet to your device
  ///
  /// In en, this message translates to:
  /// **'Save an encrypted backup of this wallet to your device'**
  String get downloadWalletFileSubtitle;

  /// Encrypted wallet file downloaded — keep it somewhere safe.
  ///
  /// In en, this message translates to:
  /// **'Encrypted wallet file downloaded — keep it somewhere safe.'**
  String get walletFileDownloaded;

  /// This file is NOT encrypted. It contains your private keys in plain text — anyone who op...
  ///
  /// In en, this message translates to:
  /// **'This file is NOT encrypted. It contains your private keys in plain text — anyone who opens it can spend your funds.\n\nOnly save it somewhere you control, and delete it when you are done.'**
  String get exportJsonWarning;

  /// Could not save the wallet to browser storage: {error}
  ///
  /// In en, this message translates to:
  /// **'Could not save the wallet to browser storage: {error}'**
  String autosaveFailed(String error);

  /// Wallet data deleted
  ///
  /// In en, this message translates to:
  /// **'Wallet data deleted'**
  String get walletDataDeleted;

  /// Delete failed: {error}
  ///
  /// In en, this message translates to:
  /// **'Delete failed: {error}'**
  String deleteFailed(String error);

  /// Your browser is blocking notifications for this site. Enable them in site settings.
  ///
  /// In en, this message translates to:
  /// **'Your browser is blocking notifications for this site. Enable them in site settings.'**
  String get notificationsBlocked;

  /// Enter a name for this wallet
  ///
  /// In en, this message translates to:
  /// **'Enter a name for this wallet'**
  String get walletNameRequired;

  /// Enter a daemon host
  ///
  /// In en, this message translates to:
  /// **'Enter a daemon host'**
  String get daemonHostRequired;

  /// Port must be between 1 and 65535
  ///
  /// In en, this message translates to:
  /// **'Port must be between 1 and 65535'**
  String get invalidPort;

  /// Private spend key must be 64 hexadecimal characters
  ///
  /// In en, this message translates to:
  /// **'Private spend key must be 64 hexadecimal characters'**
  String get invalidSpendKey;

  /// Private view key must be 64 hexadecimal characters
  ///
  /// In en, this message translates to:
  /// **'Private view key must be 64 hexadecimal characters'**
  String get invalidViewKey;

  /// A seed phrase has {expected} words — you entered {actual}
  ///
  /// In en, this message translates to:
  /// **'A seed phrase has {expected} words — you entered {actual}'**
  String seedWordCount(int expected, int actual);
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
