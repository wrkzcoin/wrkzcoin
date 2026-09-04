// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class SEn extends S {
  SEn([String locale = 'en']) : super(locale);

  @override
  String get appTitle => 'PLUTON v2';

  @override
  String get tabOverview => 'Overview';

  @override
  String get tabReceive => 'Receive';

  @override
  String get tabTransfer => 'Transfer';

  @override
  String get tabHistory => 'History';

  @override
  String get tabAddressBook => 'Address Book';

  @override
  String get tabSettings => 'Settings';

  @override
  String get tabAbout => 'About';

  @override
  String get lockWallet => 'Lock Wallet';

  @override
  String get send => 'Send';

  @override
  String get receive => 'Receive';

  @override
  String get transfer => 'Transfer';

  @override
  String get available => 'Available';

  @override
  String get locked => 'Locked';

  @override
  String get total => 'Total';

  @override
  String get availableBalance => 'Available Balance';

  @override
  String lockedUnconfirmed(String amount, String ticker) {
    return 'Locked (unconfirmed): $amount $ticker';
  }

  @override
  String totalBalance(String amount, String ticker) {
    return 'Total: $amount $ticker';
  }

  @override
  String get balanceIncompleteWhileSyncing =>
      'Balance may be incomplete while syncing';

  @override
  String errorPrefix(String message) {
    return 'Error: $message';
  }

  @override
  String get network => 'Network';

  @override
  String get syncStatus => 'Sync status';

  @override
  String get synced => 'Synced';

  @override
  String get syncing => 'Syncing…';

  @override
  String get walletBlock => 'Wallet block';

  @override
  String get networkBlock => 'Network block';

  @override
  String get peers => 'Peers';

  @override
  String get walletType => 'Wallet type';

  @override
  String get viewOnly => 'View-only';

  @override
  String get full => 'Full';

  @override
  String get nodeConnectionIssue => 'Node connection issue';

  @override
  String get switchNodeInSettings => 'Switch node in Settings →';

  @override
  String get recentTransactions => 'Recent Transactions';

  @override
  String get viewAll => 'View all →';

  @override
  String get noTransactionsYet => 'No transactions yet';

  @override
  String get received => 'Received';

  @override
  String get sent => 'Sent';

  @override
  String syncingProgress(String pct, int wallet, int network) {
    return 'Syncing $pct% (block $wallet / $network)';
  }

  @override
  String get shareAddressSubtitle => 'Share your address to receive WRKZ';

  @override
  String get yourAddress => 'Your Address';

  @override
  String get generateIntegratedAddress => 'Generate Integrated Address';

  @override
  String get integratedAddressDescription =>
      'Combine your address with a payment ID. Use the random buttons for a new ID, or enter your own below.';

  @override
  String get randomShort16 => 'Random Short (16)';

  @override
  String get randomLong64 => 'Random Long (64)';

  @override
  String get customPaymentIdLabel => 'Custom payment ID (16 or 64 hex chars)';

  @override
  String get generate => 'Generate';

  @override
  String get integratedAddress => 'Integrated Address';

  @override
  String get paymentIdShort => 'Short (16)';

  @override
  String get paymentIdLong => 'Long (64)';

  @override
  String paymentIdLabel(String label) {
    return 'Payment ID · $label';
  }

  @override
  String get enterPaymentIdError => 'Enter a payment ID (16 or 64 hex chars)';

  @override
  String get paymentIdInvalidError =>
      'Payment ID must be 16 or 64 hex characters';

  @override
  String get copyAddress => 'Copy address';

  @override
  String get copyPaymentId => 'Copy payment ID';

  @override
  String get copy => 'Copy';

  @override
  String get copied => 'Copied!';

  @override
  String get sendWrkzToAny => 'Send WRKZ to any address';

  @override
  String get sweepAllDescription =>
      'Send all funds to an address (consolidates UTXOs)';

  @override
  String get sweepAll => 'Sweep all';

  @override
  String get sweepWarning =>
      'Sweep consolidates all UTXOs into one output. Use this when transactions fail due to too many inputs.';

  @override
  String sweepAvailableBalance(String amount, String ticker) {
    return 'Available: $amount $ticker (entire balance will be sent minus fee)';
  }

  @override
  String get destinationAddress => 'Destination address';

  @override
  String get addressBook => 'Address book';

  @override
  String get sweepAllFunds => 'Sweep All Funds';

  @override
  String get recipientAddress => 'Recipient address';

  @override
  String get amount => 'Amount';

  @override
  String get paymentIdOptional => 'Payment ID (optional)';

  @override
  String get hexCharacters => '16 or 64 hex characters';

  @override
  String get reviewTransaction => 'Review Transaction';

  @override
  String get reviewAndConfirm => 'Review & Confirm';

  @override
  String get to => 'To';

  @override
  String get fee => 'Fee';

  @override
  String get totalDeducted => 'Total deducted';

  @override
  String get paymentId => 'Payment ID';

  @override
  String get transactionsIrreversible =>
      'Transactions are irreversible. Verify the address before confirming.';

  @override
  String get back => 'Back';

  @override
  String get confirmAndSend => 'Confirm & Send';

  @override
  String get transactionSent => 'Transaction Sent!';

  @override
  String get transactionBroadcast =>
      'Your transaction has been broadcast to the network.';

  @override
  String get transactionHash => 'Transaction Hash';

  @override
  String get sendAnother => 'Send Another';

  @override
  String get enterDestinationAddress => 'Enter a destination address';

  @override
  String get enterValidAmount => 'Enter a valid amount';

  @override
  String computingPow(int seconds) {
    return 'Computing PoW... ${seconds}s';
  }

  @override
  String get stepFillDetails => 'Fill Details';

  @override
  String get stepReview => 'Review';

  @override
  String get stepDone => 'Done';

  @override
  String get sweepFailed => 'Sweep failed';

  @override
  String get addressBookTitle => 'Address Book';

  @override
  String get transactionHistory => 'Transaction History';

  @override
  String get searchByHash => 'Search by hash, address or payment ID…';

  @override
  String get all => 'All';

  @override
  String get filterReceived => 'Received';

  @override
  String get filterSent => 'Sent';

  @override
  String get refresh => 'Refresh';

  @override
  String get noTransactionsFound => 'No transactions found';

  @override
  String get confirmed => 'Confirmed';

  @override
  String get pending => 'Pending';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Address';

  @override
  String get block => 'Block';

  @override
  String showingRange(int start, int end, int total) {
    return 'Showing $start–$end of $total';
  }

  @override
  String get previous => 'Previous';

  @override
  String get next => 'Next';

  @override
  String get walletLocked => 'Wallet Locked';

  @override
  String get enterPasswordToContinue =>
      'Enter your wallet password to continue';

  @override
  String get password => 'Password';

  @override
  String get incorrectPassword => 'Incorrect password';

  @override
  String get unlock => 'Unlock';

  @override
  String get closeWalletInstead => 'Close wallet instead';

  @override
  String get closeWallet => 'Close Wallet';

  @override
  String get closeWalletDescription =>
      'This will save and close the wallet.\n\nYou will be returned to the login screen.';

  @override
  String get cancel => 'Cancel';

  @override
  String get welcomeToPluton => 'Welcome to PLUTON v2';

  @override
  String get selectOptionToStart => 'Select an option to get started';

  @override
  String get createNewWallet => 'Create New Wallet';

  @override
  String get openExistingWallet => 'Open Existing Wallet';

  @override
  String get importFromSeed => 'Import from Seed Phrase';

  @override
  String get importFromKeys => 'Import from Private Keys';

  @override
  String get openWallet => 'Open Wallet';

  @override
  String get importFromSeedTitle => 'Import from Seed';

  @override
  String get importFromKeysTitle => 'Import from Keys';

  @override
  String get saveWalletTo => 'Save wallet to';

  @override
  String get walletFile => 'Wallet file';

  @override
  String get walletPassword => 'Wallet password';

  @override
  String get mnemonicSeedPhrase => 'Mnemonic Seed Phrase';

  @override
  String get scanFromHeight => 'Scan from height (0 = full scan)';

  @override
  String get daemonHost => 'Daemon host';

  @override
  String get port => 'Port';

  @override
  String get continueButton => 'Continue';

  @override
  String get browse => 'Browse…';

  @override
  String get backupWarning =>
      'Back up your wallet before continuing.\nThese keys cannot be recovered if lost.';

  @override
  String get yourWalletAddress => 'Your Wallet Address';

  @override
  String get seedPhrase25Words => 'Seed Phrase (25 words)';

  @override
  String get privateViewKey => 'Private View Key';

  @override
  String get privateSpendKey => 'Private Spend Key';

  @override
  String get seedBackupConfirm =>
      'I have written down my seed phrase and private keys in a safe place.';

  @override
  String get backedUpContinue => 'I\'ve backed up my wallet — Continue';

  @override
  String get settings => 'Settings';

  @override
  String get sectionDaemonNode => 'Daemon Node';

  @override
  String get nodeDescription =>
      'Connect to a local or remote daemon node. Changes take effect immediately.';

  @override
  String get hostIpAddress => 'Host / IP address';

  @override
  String get ssl => 'SSL';

  @override
  String get apply => 'Apply';

  @override
  String get nodeUpdatedSuccess => 'Node updated successfully';

  @override
  String get nodeUnreachable =>
      'Cannot reach the current node. Enter a new node address below and tap Apply.';

  @override
  String get sectionWallet => 'Wallet';

  @override
  String get saveWallet => 'Save Wallet';

  @override
  String get saveWalletSubtitle => 'Flush current state to disk';

  @override
  String get walletSaved => 'Wallet saved';

  @override
  String get exportToJson => 'Export to JSON';

  @override
  String get exportToJsonSubtitle => 'Save wallet data as a JSON file';

  @override
  String get exportJsonTitle => 'Export wallet JSON';

  @override
  String exportedTo(String path) {
    return 'Exported to $path';
  }

  @override
  String exportFailed(String error) {
    return 'Export failed: $error';
  }

  @override
  String get resetScanHeight => 'Reset Scan Height';

  @override
  String get resetScanHeightSubtitle =>
      'Rescan blockchain from a specific height';

  @override
  String get resetScanHeightDescription =>
      'Enter a block height to rescan from. Use 0 for a full rescan.';

  @override
  String get scanHeight => 'Scan height';

  @override
  String get reset => 'Reset';

  @override
  String get autosave => 'Autosave';

  @override
  String get autosaveSubtitle =>
      'Save wallet to disk after sync and every 5 minutes';

  @override
  String get scanCoinbaseTx => 'Scan Coinbase Transactions';

  @override
  String get scanCoinbaseSubtitle =>
      'Include miner rewards when syncing (off by default)';

  @override
  String get sectionAppearance => 'Appearance';

  @override
  String get theme => 'Theme';

  @override
  String get themeSubtitle => 'Choose app colour scheme';

  @override
  String get themeSystem => 'System';

  @override
  String get themeLight => 'Light';

  @override
  String get themeDark => 'Dark';

  @override
  String get sectionNotifications => 'Notifications';

  @override
  String get incomingTxAlerts => 'Incoming Transaction Alerts';

  @override
  String get incomingTxAlertsSubtitle =>
      'Show a desktop notification when WRKZ is received';

  @override
  String get sectionDebugLogs => 'Debug & Logs';

  @override
  String get logLevel => 'Log Level';

  @override
  String get logLevelSubtitle => 'Controls wallet library verbosity';

  @override
  String get viewLogs => 'View Logs';

  @override
  String get viewLogsSubtitle => 'Live wallet library log output';

  @override
  String get walletLogs => 'Wallet Logs';

  @override
  String logEntries(int count) {
    return '$count entries';
  }

  @override
  String get autoScroll => 'Auto-scroll';

  @override
  String get copyAll => 'Copy all';

  @override
  String get clear => 'Clear';

  @override
  String get close => 'Close';

  @override
  String get noLogsYet =>
      'No logs yet. Set a log level above Disabled to see output.';

  @override
  String get logsCopied => 'Logs copied to clipboard';

  @override
  String get sectionDangerZone => 'Danger Zone';

  @override
  String get deleteWalletData => 'Delete Wallet Data';

  @override
  String get deleteWalletDataSubtitle =>
      'Permanently remove wallet file from disk';

  @override
  String get deleteWalletWarning =>
      'This will permanently delete your wallet file from disk.\n\nMake sure you have backed up your seed phrase and private keys before proceeding. This action cannot be undone.';

  @override
  String get iUnderstandContinue => 'I understand, continue';

  @override
  String get finalConfirmation => 'Final Confirmation';

  @override
  String get typeDeleteToConfirm => 'Type DELETE to confirm:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get deletePermanently => 'Delete permanently';

  @override
  String get aboutTitle => 'About';

  @override
  String versionInfo(String version) {
    return 'Version $version — WRKZ Desktop Wallet';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 is the official desktop wallet for WrkzCoin (WRKZ), a fast and lightweight CryptoNote-based cryptocurrency.\n\nBuilt with Flutter, powered by wallet-api.';

  @override
  String get github => 'GitHub';

  @override
  String get githubSubtitle => 'View source code and releases';

  @override
  String get discord => 'Discord';

  @override
  String get discordSubtitle => 'Join the community';

  @override
  String get twitterX => 'Twitter / X';

  @override
  String get twitterXSubtitle => 'Follow @wrkzcoin';

  @override
  String get website => 'Website';

  @override
  String get websiteSubtitle => 'wrkz.work';

  @override
  String get license => 'License';

  @override
  String get licenseText =>
      'Released under the MIT License.\nUse at your own risk. Always back up your seed phrase.';

  @override
  String get addButton => 'Add';

  @override
  String get noSavedAddresses => 'No saved addresses';

  @override
  String get tapAddToSave => 'Tap Add to save a frequently used address.';

  @override
  String get addAddress => 'Add Address';

  @override
  String get nameLabel => 'Name / label';

  @override
  String get addressLabel => 'Address';

  @override
  String get noteOptional => 'Note (optional)';

  @override
  String get nameAndAddressRequired => 'Name and address are required';

  @override
  String get invalidWrkzAddress =>
      'Invalid WRKZ address. Must be 98 (standard), 120 (short integrated), or 186 (long integrated) characters starting with \"Wrkz\".';

  @override
  String get save => 'Save';

  @override
  String get editEntry => 'Edit Entry';

  @override
  String get deleteEntry => 'Delete Entry';

  @override
  String removeFromAddressBook(String name) {
    return 'Remove \"$name\" from your address book?';
  }

  @override
  String get delete => 'Delete';

  @override
  String get edit => 'Edit';

  @override
  String get wrkzReceived => 'WRKZ Received';

  @override
  String youReceivedAmount(String amount) {
    return 'You received $amount';
  }

  @override
  String get show => 'Show';

  @override
  String get exit => 'Exit';

  @override
  String get plutonWallet => 'PLUTON Wallet';

  @override
  String get language => 'Language';

  @override
  String get selectLanguage => 'Select Language';

  @override
  String get chooseLanguage => 'Choose your preferred language';

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
  String get preparedTransactionExpired =>
      'This transaction is no longer valid. Go back and create it again.';

  @override
  String get deleteConfirmMismatch => 'Type DELETE exactly to confirm.';

  @override
  String get unlockNeedsReopen =>
      'Cannot verify the password on this device. Use “Close wallet instead” and open the wallet again.';

  @override
  String get exportJsonWarningTitle => 'Export unencrypted wallet?';

  @override
  String get exportJsonWarningBody =>
      'The exported file contains your private view key and private spend keys in plain text. Anyone who reads it can spend your funds.\n\nSave it only to storage you control, and delete it as soon as you are done.';

  @override
  String passwordTooShort(int count) {
    return 'Password must be at least $count characters';
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
  String get sectionLocalNode => 'Local Lite Node';

  @override
  String get localNodeDescription =>
      'Run a node on this computer and sync against it instead of a remote server. A lite node stores only what a wallet needs, but it still downloads the whole chain once.';

  @override
  String get localNodeSetUp => 'Set up local node';

  @override
  String get localNodeStart => 'Start';

  @override
  String get localNodeStop => 'Stop';

  @override
  String get localNodeUse => 'Use this node';

  @override
  String get localNodeDelete => 'Delete node data';

  @override
  String get localNodeStateStopped => 'Stopped';

  @override
  String get localNodeStateStarting => 'Starting';

  @override
  String get localNodeStateSyncing => 'Syncing';

  @override
  String get localNodeStateReady => 'Synced';

  @override
  String get localNodeStateFailed => 'Failed';

  @override
  String localNodeProgress(int height, int network) {
    return 'Block $height of $network';
  }

  @override
  String localNodePeers(int count) {
    return '$count peers';
  }

  @override
  String get localNodeNotReadyYet =>
      'The local node is still catching up and cannot serve the wallet yet. It keeps syncing in the background — stay on a remote node until it is ready, then switch over.';

  @override
  String get localNodeInUse => 'The wallet is connected to this node.';

  @override
  String localNodeBinaryMissing(String name) {
    return '$name was not found. Place the daemon binary next to the wallet executable, or in a \'sidecar\' folder beside it, and try again.';
  }

  @override
  String get localNodeSetupTitle => 'Set up a local lite node';

  @override
  String get localNodeSetupCost =>
      'Before you start:\n• Around 6 GB of disk space, and the whole chain is downloaded once.\n• The first sync takes hours. It continues in the background and you can keep using a remote node meanwhile.\n• The start height below is permanent. Changing it later means deleting the node and syncing again from nothing.';

  @override
  String get localNodeStartHeightLabel => 'Start height';

  @override
  String localNodeStartHeightHelp(int height) {
    return 'Blocks below this height are downloaded and checked, then only the index later blocks need is kept. Keep it at or below this wallet\'s own start height ($height).';
  }

  @override
  String localNodeStartHeightTooHigh(int height) {
    return 'Higher than this wallet\'s start height ($height). The node would never be able to show this wallet\'s older transactions.';
  }

  @override
  String get localNodeCreate => 'Create node';

  @override
  String get localNodeDeleteTitle => 'Delete local node data?';

  @override
  String get localNodeDeleteWarning =>
      'This stops the node and permanently deletes its blockchain database from disk. Your wallet, its seed and its funds are not touched — but a new local node starts syncing again from nothing, which takes hours.';

  @override
  String get localNodeDeleted => 'Local node deleted.';

  @override
  String localNodeDiskUsage(String size) {
    return '$size on disk';
  }

  @override
  String get nodePresetRemote => 'Remote node';

  @override
  String get nodePresetLocal => 'Local lite node';

  @override
  String get savingWallet => 'Saving your wallet…';

  @override
  String get savingWalletBody =>
      'PLUTON is writing your wallet to disk. This can take a moment on a large wallet.';

  @override
  String get shutdownTakingLong =>
      'This is taking longer than expected. Quitting now loses this save — the wallet on disk stays as it was, so nothing is damaged, but anything since the last save is gone.';

  @override
  String get quitAnyway => 'Quit anyway';

  @override
  String get stillRunningInTray =>
      'PLUTON is still running in the system tray. Click the tray icon to bring it back, or right-click it and choose Exit to quit.';

  @override
  String get localNodeDataFolder => 'Data folder';

  @override
  String get localNodeDataFolderHelp =>
      'Around 6 GB is written here. Pick a drive with room for it.';

  @override
  String get localNodeDataFolderInUse =>
      'That folder already holds other files. Choose an empty folder, or a new one.';

  @override
  String get localNodeStartHeightRequired =>
      'Enter the height to keep full blocks from. It has to be above zero — a lite node cannot start at the genesis block.';

  @override
  String get nodeExitTitle => 'The local node is still running';

  @override
  String get nodeExitBodySyncing =>
      'Its first sync has not finished. That takes hours, and it only makes progress while the node is running — but a node left running keeps using the disk and the network after PLUTON has closed.';

  @override
  String get nodeExitBodySynced =>
      'It is level with the network. Leaving it running keeps it that way and keeps using CPU, bandwidth and the disk while PLUTON is closed; stopping it costs a short catch-up next time.';

  @override
  String get nodeExitKeep => 'Leave it running';

  @override
  String get nodeExitStop => 'Stop it';

  @override
  String get nodeExitChangeLater =>
      'Changeable later in Settings, under Local Lite Node.';

  @override
  String get rememberMyChoice => 'Remember my choice';

  @override
  String get shutdownStoppingNode => 'Stopping the local node…';

  @override
  String get shutdownStoppingNodeBody =>
      'Letting it flush its database, so the next start does not have to replay the write-ahead log.';

  @override
  String get nodeExitPolicyLabel => 'When the wallet closes';

  @override
  String get nodeExitPolicyAsk => 'Ask me';

  @override
  String get nodeExitPolicyKeep => 'Leave the node running';

  @override
  String get nodeExitPolicyStop => 'Stop the node';

  @override
  String get localNodeStillRunningBody =>
      'The local lite node is still running in the background. Open PLUTON to stop it.';

  @override
  String get syncStoppedTitle => 'Sync stopped';

  @override
  String syncGapStalled(int covered, int servesFrom) {
    return 'Sync stopped at block $covered. The node it was talking to answers only from block $servesFrom upward, so the blocks in between cannot be downloaded from it. The balance is incomplete until you connect a node holding the whole chain.';
  }

  @override
  String get localNodeNotReadyTitle => 'This node is not ready';

  @override
  String localNodeNotReadyBody(int behind) {
    return 'The local node is still $behind blocks behind the network. Point the wallet at it now and the sync will park until it catches up, showing a balance that is missing anything it has not reached — with nothing on screen to say why. Staying on the remote node costs nothing; the node keeps syncing either way.';
  }

  @override
  String get switchAnyway => 'Switch anyway';

  @override
  String get switchToRemoteNode => 'Switch to the remote node';

  @override
  String get nodeWillServeFromLabel => 'Will serve blocks from';

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
  String get nodeInvalid => 'Enter a valid host and port';

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
