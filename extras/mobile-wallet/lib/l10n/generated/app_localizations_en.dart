// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class SEn extends S {
  SEn([String locale = 'en']) : super(locale);

  @override
  String get appTitle => 'PLUTON Mobile';

  @override
  String get tabOverview => 'Overview';

  @override
  String get tabReceive => 'Receive';

  @override
  String get tabSend => 'Send';

  @override
  String get tabHistory => 'History';

  @override
  String get tabSettings => 'Settings';

  @override
  String get send => 'Send';

  @override
  String get receive => 'Receive';

  @override
  String get available => 'Available';

  @override
  String get locked => 'Locked';

  @override
  String get total => 'Total';

  @override
  String lockedAmount(String amount) {
    return 'Locked: $amount';
  }

  @override
  String totalAmount(String amount) {
    return 'Total: $amount';
  }

  @override
  String get recentTransactions => 'Recent Transactions';

  @override
  String get viewAll => 'View all';

  @override
  String get noTransactionsYet => 'No transactions yet';

  @override
  String get noMatchingTransactions => 'No matching transactions';

  @override
  String get pending => 'Pending...';

  @override
  String get justNow => 'Just now';

  @override
  String minutesAgo(int count) {
    return '${count}m ago';
  }

  @override
  String hoursAgo(int count) {
    return '${count}h ago';
  }

  @override
  String daysAgo(int count) {
    return '${count}d ago';
  }

  @override
  String get received => 'Received';

  @override
  String get sent => 'Sent';

  @override
  String get networkStatus => 'Network Status';

  @override
  String get node => 'Node';

  @override
  String get status => 'Status';

  @override
  String get connected => 'Connected';

  @override
  String get disconnected => 'Disconnected';

  @override
  String get walletHeight => 'Wallet Height';

  @override
  String get networkHeight => 'Network Height';

  @override
  String get peers => 'Peers';

  @override
  String get type => 'Type';

  @override
  String get viewOnly => 'View-only';

  @override
  String get couldNotFetchStatus =>
      'Could not fetch status. Check your node in Settings.';

  @override
  String errorPrefix(String message) {
    return 'Error: $message';
  }

  @override
  String get seedBackupWarning =>
      'Back up your seed phrase in Settings to protect your funds.';

  @override
  String get noConnectionToDaemon => 'No connection to daemon';

  @override
  String syncingPercent(String percent) {
    return 'Syncing $percent%';
  }

  @override
  String get yourAddress => 'Your Address';

  @override
  String get errorLoadingAddress => 'Error loading address';

  @override
  String get integratedAddress => 'Integrated Address';

  @override
  String get embedPaymentId => 'Embed a payment ID into your address';

  @override
  String get randomShort => 'Random Short (16)';

  @override
  String get randomLong => 'Random Long (64)';

  @override
  String get enterCustomPaymentId =>
      'Or enter custom payment ID (16 or 64 hex)';

  @override
  String get enterPaymentId => 'Enter a payment ID';

  @override
  String get paymentIdInvalid => 'Payment ID must be 16 or 64 hex characters';

  @override
  String get shortPid => 'Short PID';

  @override
  String get longPid => 'Long PID';

  @override
  String get share => 'Share';

  @override
  String get copy => 'Copy';

  @override
  String get sweepAllFunds => 'Sweep All Funds';

  @override
  String get normalSend => 'Normal Send';

  @override
  String get sweep => 'Sweep';

  @override
  String get recipientAddress => 'Recipient Address';

  @override
  String get scanQr => 'Scan QR';

  @override
  String get amount => 'Amount';

  @override
  String availableBalance(String amount) {
    return 'Available: $amount';
  }

  @override
  String sweepInfo(String amount) {
    return 'Sweep consolidates all UTXOs and sends your entire unlocked balance ($amount) minus fees.';
  }

  @override
  String get paymentIdOptional => 'Payment ID (optional)';

  @override
  String get hexCharacters => '16 or 64 hex characters';

  @override
  String get mustBeHex => 'Must be 16 or 64 hex characters';

  @override
  String get recipientRequired => 'Recipient address is required';

  @override
  String get invalidAddress => 'Invalid WRKZ address';

  @override
  String get enterValidAmount => 'Enter a valid amount';

  @override
  String get reviewTransaction => 'Review Transaction';

  @override
  String get to => 'To';

  @override
  String get fee => 'Fee';

  @override
  String get totalDeducted => 'Total Deducted';

  @override
  String get paymentId => 'Payment ID';

  @override
  String get transactionsIrreversible =>
      'Transactions are irreversible. Please verify the details.';

  @override
  String get back => 'Back';

  @override
  String get confirmAndSend => 'Confirm & Send';

  @override
  String get transactionSent => 'Transaction Sent!';

  @override
  String get transactionHash => 'Transaction Hash';

  @override
  String get sendAnother => 'Send Another';

  @override
  String get scanQrCode => 'Scan QR Code';

  @override
  String get scannedAddress => 'Scanned Address';

  @override
  String get cancel => 'Cancel';

  @override
  String get useThisAddress => 'Use this address';

  @override
  String get sweepFailed => 'Sweep failed';

  @override
  String get searchPlaceholder => 'Search by hash, address, payment ID...';

  @override
  String get all => 'All';

  @override
  String get filterReceived => 'Received';

  @override
  String get filterSent => 'Sent';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Address';

  @override
  String get block => 'Block';

  @override
  String get confirmed => 'Confirmed';

  @override
  String get password => 'Password';

  @override
  String get unlock => 'Unlock';

  @override
  String get switchWallet => 'Switch Wallet';

  @override
  String get enterPasswordToUnlock => 'Enter your password to unlock';

  @override
  String get incorrectPassword => 'Incorrect password';

  @override
  String get enterYourPassword => 'Enter your password';

  @override
  String get plutonMobile => 'PLUTON Mobile';

  @override
  String get createFirstWalletSubtitle =>
      'Create your first wallet to get started';

  @override
  String get selectWalletSubtitle => 'Select a wallet to open';

  @override
  String get yourWallets => 'Your Wallets';

  @override
  String get noWalletsYet => 'No wallets yet';

  @override
  String get lastOpened => 'Last opened';

  @override
  String createdDate(String date) {
    return 'Created $date';
  }

  @override
  String get createFirstWallet => 'Create First Wallet';

  @override
  String get addWallet => 'Add Wallet';

  @override
  String get deleteWallet => 'Delete Wallet';

  @override
  String deleteWalletConfirm(String name) {
    return 'Delete \"$name\"?\n\nThis will permanently remove the wallet file and keys. Make sure you have backed up your seed phrase.';
  }

  @override
  String get delete => 'Delete';

  @override
  String get createNewWallet => 'Create New Wallet';

  @override
  String get createNewWalletSubtitle =>
      'Generate a new wallet with a fresh seed phrase';

  @override
  String get importFromSeed => 'Import from Seed Phrase';

  @override
  String get importFromSeedSubtitle =>
      'Restore wallet using your 25-word mnemonic seed';

  @override
  String get importFromKeys => 'Import from Private Keys';

  @override
  String get importFromKeysSubtitle => 'Restore using spend key and view key';

  @override
  String get viewOnlyWallet => 'View-Only Wallet';

  @override
  String get viewOnlyWalletSubtitle =>
      'Watch-only wallet using view key and address';

  @override
  String get createWallet => 'Create Wallet';

  @override
  String get importWallet => 'Import Wallet';

  @override
  String get walletName => 'Wallet Name';

  @override
  String get walletNameHint => 'e.g. Main Wallet';

  @override
  String get passwordLabel => 'Password';

  @override
  String get enterPassword => 'Enter password';

  @override
  String get confirmPassword => 'Confirm password';

  @override
  String get seedPhrase => 'Seed Phrase (25 words)';

  @override
  String get enterSeedPhrase => 'Enter your seed phrase...';

  @override
  String get scanHeight => 'Scan Height (optional)';

  @override
  String get scanHeightHint => '0 = scan from beginning';

  @override
  String get privateSpendKey => 'Private Spend Key';

  @override
  String get privateViewKey => 'Private View Key';

  @override
  String get walletAddress => 'Wallet Address';

  @override
  String get walletAddressHint => 'Wrkz... address';

  @override
  String get hexKey => '64-char hex';

  @override
  String get daemonNode => 'Daemon Node';

  @override
  String get custom => 'Custom';

  @override
  String get host => 'Host';

  @override
  String get hostHint => 'Host / IP';

  @override
  String get port => 'Port';

  @override
  String get ssl => 'SSL';

  @override
  String get walletNameRequired => 'Wallet name is required';

  @override
  String get passwordRequired => 'Password is required';

  @override
  String passwordTooShort(int count) {
    return 'Password must be at least $count characters';
  }

  @override
  String get passwordsDoNotMatch => 'Passwords do not match';

  @override
  String get seedRequired => 'Seed phrase is required';

  @override
  String get spendKeyRequired => 'Spend key is required';

  @override
  String get viewKeyRequired => 'View key is required';

  @override
  String get addressRequired => 'Address is required';

  @override
  String get daemonHostRequired => 'Daemon host is required';

  @override
  String get backupSeedTitle => 'Backup Your Seed';

  @override
  String get backupWarning =>
      'Write down your seed phrase and store it safely. If you lose it, your funds are gone forever.';

  @override
  String get seedPhraseLabel => 'Seed Phrase';

  @override
  String get privateViewKeyLabel => 'Private View Key';

  @override
  String get privateSpendKeyLabel => 'Private Spend Key';

  @override
  String get backupConfirmCheck => 'I have safely backed up my seed phrase';

  @override
  String get continueToWallet => 'Continue to Wallet';

  @override
  String get sectionDaemonNode => 'Daemon Node';

  @override
  String get apply => 'Apply';

  @override
  String nodeUpdated(String host, int port) {
    return 'Node updated to $host:$port';
  }

  @override
  String get hostRequired => 'Host is required';

  @override
  String currentWallet(String name) {
    return 'Current Wallet — $name';
  }

  @override
  String get saveWallet => 'Save Wallet';

  @override
  String get walletSaved => 'Wallet saved';

  @override
  String saveFailed(String error) {
    return 'Save failed: $error';
  }

  @override
  String get backupSeed => 'Backup Seed';

  @override
  String get changePassword => 'Change Password';

  @override
  String get resetScanHeight => 'Reset Scan Height';

  @override
  String get reset => 'Reset';

  @override
  String resetScanConfirm(int height) {
    return 'This will rescan the blockchain from block $height. This may take a while. Continue?';
  }

  @override
  String scanResetTo(int height) {
    return 'Scan reset to block $height';
  }

  @override
  String resetFailed(String error) {
    return 'Reset failed: $error';
  }

  @override
  String get enterPasswordTitle => 'Enter Password';

  @override
  String get confirm => 'Confirm';

  @override
  String get seedBackup => 'Seed Backup';

  @override
  String get seedPhraseColon => 'Seed Phrase:';

  @override
  String get privateViewKeyColon => 'Private View Key:';

  @override
  String get iveBackedUp => 'I\'ve backed up';

  @override
  String get currentPasswordLabel => 'Current password';

  @override
  String get newPasswordLabel => 'New password';

  @override
  String get confirmNewPasswordLabel => 'Confirm new password';

  @override
  String get change => 'Change';

  @override
  String get currentPasswordIncorrect => 'Current password is incorrect';

  @override
  String get newPasswordsDoNotMatch => 'New passwords do not match';

  @override
  String get passwordChanged => 'Password changed';

  @override
  String get walletManagement => 'Wallet Management';

  @override
  String get switchWalletSubtitle => 'Save & close, pick another';

  @override
  String get manageWallets => 'Manage Wallets';

  @override
  String get manageWalletsSubtitle => 'Rename or delete wallets';

  @override
  String get currentlyOpen => '(currently open)';

  @override
  String get close => 'Close';

  @override
  String get renameWallet => 'Rename Wallet';

  @override
  String get newName => 'New name';

  @override
  String get rename => 'Rename';

  @override
  String deleteWalletConfirmShort(String name) {
    return 'Delete \"$name\"? This cannot be undone.';
  }

  @override
  String get security => 'Security';

  @override
  String get biometricUnlock => 'Biometric Unlock';

  @override
  String get biometricSubtitle => 'Fingerprint / Face ID';

  @override
  String get biometricNotAvailable => 'Biometric not available';

  @override
  String get autoLock => 'Auto-Lock';

  @override
  String get appearance => 'Appearance';

  @override
  String get theme => 'Theme';

  @override
  String get themeAuto => 'Auto';

  @override
  String get themeLight => 'Light';

  @override
  String get themeDark => 'Dark';

  @override
  String get preferences => 'Preferences';

  @override
  String get transactionNotifications => 'Transaction Notifications';

  @override
  String get notificationsSubtitle => 'Alert on incoming transactions';

  @override
  String get autosave => 'Autosave';

  @override
  String get autosaveSubtitle => 'Save after sync, then every 5 minutes';

  @override
  String get scanCoinbaseTx => 'Scan Coinbase Transactions';

  @override
  String get scanCoinbaseSubtitle => 'Include miner rewards (off by default)';

  @override
  String get dangerZone => 'Danger Zone';

  @override
  String get deleteCurrentWallet => 'Delete Current Wallet';

  @override
  String get deleteCurrentWalletSubtitle => 'Permanently remove wallet data';

  @override
  String get deleteWalletTypeCaps =>
      'This will permanently delete the wallet file and keys. Make sure you have backed up your seed phrase.\n\nType DELETE to confirm:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get language => 'Language';

  @override
  String get selectLanguage => 'Select Language';

  @override
  String get languageEn => 'English';

  @override
  String get languageFr => 'Français';

  @override
  String get languageDe => 'Deutsch';

  @override
  String get languageZh => '中文';

  @override
  String get languageVi => 'Tiếng Việt';

  @override
  String get languageJa => '日本語';

  @override
  String get languageEs => 'Español';

  @override
  String get languagePt => 'Português';

  @override
  String get languageRu => 'Русский';

  @override
  String get autoLockImmediately => 'Immediately';

  @override
  String get autoLock1Min => '1 minute';

  @override
  String get autoLock5Min => '5 minutes';

  @override
  String get autoLockNever => 'Never';

  @override
  String get preparedTransactionExpired =>
      'This transaction is no longer valid. Go back and create it again.';

  @override
  String get deleteConfirmMismatch => 'Type DELETE exactly to confirm.';

  @override
  String get seedNotBackedUpWarning =>
      'You have not confirmed a backup of this wallet\'s seed phrase. Deleting it now means the funds cannot be recovered.';

  @override
  String get wrkzReceived => 'WRKZ received';

  @override
  String get retry => 'Retry';

  @override
  String youReceivedAmount(String amount) {
    return 'You received $amount';
  }

  @override
  String get ringSize => 'Ring size';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Ring size reduced to $actual (normally $normal). The amounts being sent do not have enough outputs on the chain to form a full ring, so this transaction is less private than usual.';
  }

  @override
  String get liteNodeTitle => 'Lite node';

  @override
  String liteNodeServesFrom(int height) {
    return 'This node only holds blocks from $height onward. Transactions before that block cannot be found through it.';
  }

  @override
  String liteNodeMissesHistory(int nodeHeight, int walletHeight) {
    return 'This node starts at block $nodeHeight, but this wallet starts at block $walletHeight. Anything received in between is invisible here, so the balance shown may be too low. Connect a node holding the whole chain to see it.';
  }

  @override
  String liteNodeSyncStalled(int wallet, int node) {
    return 'Sync stopped at block $wallet. This node holds nothing below block $node, so the blocks in between cannot be downloaded from it. The balance is incomplete until you connect a node holding the whole chain.';
  }

  @override
  String get liteNodeRescanRefusedTitle =>
      'This node cannot rescan that far back';

  @override
  String liteNodeRescanRefused(int height) {
    return 'The connected node is a lite node holding no block data below $height. Rescanning from lower than that would drop transactions this wallet has already found, with no way to find them again here. Nothing has been changed.';
  }

  @override
  String liteNodeRescanFromInstead(int height) {
    return 'Rescan from $height instead';
  }

  @override
  String liteNodeRescanHint(int height) {
    return 'The connected node can only rescan from block $height or above.';
  }

  @override
  String get nodeServesFromLabel => 'Serves blocks from';

  @override
  String get nodeFullChain => 'Full chain';

  @override
  String get localNodeMobileFuture =>
      'Running the node on the phone itself is planned, but not available yet — a node needs several GB of storage and hours of syncing. Until then, point this wallet at a node you run yourself.';

  @override
  String get syncStoppedTitle => 'Sync stopped';

  @override
  String syncGapStalled(int covered, int servesFrom) {
    return 'Sync stopped at block $covered. The node it was talking to answers only from block $servesFrom upward, so the blocks in between cannot be downloaded from it. The balance is incomplete until you connect a node holding the whole chain.';
  }

  @override
  String get txPowServerSection => 'Transaction PoW Server';

  @override
  String get txPowServerUse => 'Use an external PoW server';

  @override
  String get txPowServerSubtitle =>
      'Send the transaction proof of work to a server instead of computing it on this device. If the server does not respond, this device\'s CPU is used.';

  @override
  String get txPowServerSaved => 'PoW server settings saved';

  @override
  String get txPowServerInvalid => 'Enter a valid host and port';

  @override
  String get txPowServerTest => 'Test';

  @override
  String txPowServerTestOk(int ms, int threads, int queue, int capacity) {
    return 'Server reachable in $ms ms: $threads threads, $queue of $capacity queue slots in use';
  }

  @override
  String txPowServerTestFailed(String error) {
    return 'Server not reachable: $error';
  }

  @override
  String get nodeTest => 'Test';

  @override
  String nodeTestOk(int ms, int height, int peers) {
    return 'Reachable in $ms ms: height $height, $peers peers';
  }

  @override
  String nodeTestSyncing(int ms, int height, int networkHeight) {
    return 'Reachable in $ms ms, but the node is still syncing: height $height of $networkHeight';
  }

  @override
  String nodeTestFailed(String error) {
    return 'Node not reachable: $error';
  }
}
