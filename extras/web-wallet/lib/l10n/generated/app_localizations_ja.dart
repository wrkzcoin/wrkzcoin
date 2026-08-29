// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Japanese (`ja`).
class SJa extends S {
  SJa([String locale = 'ja']) : super(locale);

  @override
  String get appTitle => 'PLUTON v2';

  @override
  String get tabOverview => '概要';

  @override
  String get tabReceive => '受取';

  @override
  String get tabTransfer => '送金';

  @override
  String get tabHistory => '履歴';

  @override
  String get tabAddressBook => 'アドレス帳';

  @override
  String get tabSettings => '設定';

  @override
  String get tabAbout => '情報';

  @override
  String get lockWallet => 'ウォレットをロック';

  @override
  String get send => '送金';

  @override
  String get receive => '受取';

  @override
  String get transfer => '送金';

  @override
  String get available => '利用可能';

  @override
  String get locked => 'ロック中';

  @override
  String get total => '合計';

  @override
  String get availableBalance => '利用可能残高';

  @override
  String lockedUnconfirmed(String amount, String ticker) {
    return 'ロック中（未確認）: $amount $ticker';
  }

  @override
  String totalBalance(String amount, String ticker) {
    return '合計: $amount $ticker';
  }

  @override
  String get balanceIncompleteWhileSyncing => '同期中は残高が不完全な場合があります';

  @override
  String errorPrefix(String message) {
    return 'エラー: $message';
  }

  @override
  String get network => 'ネットワーク';

  @override
  String get syncStatus => '同期状態';

  @override
  String get synced => '同期完了';

  @override
  String get syncing => '同期中…';

  @override
  String get walletBlock => 'ウォレットブロック';

  @override
  String get networkBlock => 'ネットワークブロック';

  @override
  String get peers => 'ピア';

  @override
  String get walletType => 'ウォレットの種類';

  @override
  String get viewOnly => '閲覧専用';

  @override
  String get full => 'フル';

  @override
  String get nodeConnectionIssue => 'ノード接続の問題';

  @override
  String get switchNodeInSettings => '設定でノードを切り替える →';

  @override
  String get recentTransactions => '最近のトランザクション';

  @override
  String get viewAll => 'すべて表示 →';

  @override
  String get noTransactionsYet => 'トランザクションはまだありません';

  @override
  String get received => '受取済み';

  @override
  String get sent => '送金済み';

  @override
  String syncingProgress(String pct, int wallet, int network) {
    return '同期中 $pct%（ブロック $wallet / $network）';
  }

  @override
  String get shareAddressSubtitle => 'WRKZを受け取るためにアドレスを共有してください';

  @override
  String get yourAddress => 'あなたのアドレス';

  @override
  String get generateIntegratedAddress => '統合アドレスを生成';

  @override
  String get integratedAddressDescription =>
      'アドレスとペイメントIDを組み合わせます。ランダムボタンで新しいIDを生成するか、下に入力してください。';

  @override
  String get randomShort16 => 'ランダム短縮 (16)';

  @override
  String get randomLong64 => 'ランダム長形 (64)';

  @override
  String get customPaymentIdLabel => 'カスタムペイメントID（16または64文字の16進数）';

  @override
  String get generate => '生成';

  @override
  String get integratedAddress => '統合アドレス';

  @override
  String get paymentIdShort => '短縮 (16)';

  @override
  String get paymentIdLong => '長形 (64)';

  @override
  String paymentIdLabel(String label) {
    return 'ペイメントID · $label';
  }

  @override
  String get enterPaymentIdError => 'ペイメントIDを入力してください（16または64文字の16進数）';

  @override
  String get paymentIdInvalidError => 'ペイメントIDは16または64文字の16進数である必要があります';

  @override
  String get copyAddress => 'アドレスをコピー';

  @override
  String get copyPaymentId => 'ペイメントIDをコピー';

  @override
  String get copy => 'コピー';

  @override
  String get copied => 'コピーしました！';

  @override
  String get sendWrkzToAny => '任意のアドレスにWRKZを送金';

  @override
  String get sweepAllDescription => 'すべての資金をアドレスに送金します（UTXOを統合）';

  @override
  String get sweepAll => '全額スイープ';

  @override
  String get sweepWarning =>
      'スイープはすべてのUTXOを1つの出力に統合します。入力が多すぎてトランザクションが失敗する場合に使用してください。';

  @override
  String sweepAvailableBalance(String amount, String ticker) {
    return '利用可能: $amount $ticker（手数料を差し引いた全残高が送金されます）';
  }

  @override
  String get destinationAddress => '送金先アドレス';

  @override
  String get addressBook => 'アドレス帳';

  @override
  String get sweepAllFunds => '全額スイープ';

  @override
  String get recipientAddress => '受取人アドレス';

  @override
  String get amount => '金額';

  @override
  String get paymentIdOptional => 'ペイメントID（任意）';

  @override
  String get hexCharacters => '16または64文字の16進数';

  @override
  String get reviewTransaction => 'トランザクションを確認';

  @override
  String get reviewAndConfirm => '確認して送金';

  @override
  String get to => '宛先';

  @override
  String get fee => '手数料';

  @override
  String get totalDeducted => '差し引き合計';

  @override
  String get paymentId => 'ペイメントID';

  @override
  String get transactionsIrreversible => 'トランザクションは取り消せません。送金前にアドレスを確認してください。';

  @override
  String get back => '戻る';

  @override
  String get confirmAndSend => '確認して送金';

  @override
  String get transactionSent => '送金完了！';

  @override
  String get transactionBroadcast => 'トランザクションがネットワークにブロードキャストされました。';

  @override
  String get transactionHash => 'トランザクションハッシュ';

  @override
  String get sendAnother => 'もう一度送金';

  @override
  String get enterDestinationAddress => '送金先アドレスを入力してください';

  @override
  String get enterValidAmount => '有効な金額を入力してください';

  @override
  String computingPow(int seconds) {
    return 'PoW計算中... $seconds秒';
  }

  @override
  String get stepFillDetails => '詳細を入力';

  @override
  String get stepReview => '確認';

  @override
  String get stepDone => '完了';

  @override
  String get sweepFailed => 'スイープに失敗しました';

  @override
  String get addressBookTitle => 'アドレス帳';

  @override
  String get transactionHistory => 'トランザクション履歴';

  @override
  String get searchByHash => 'ハッシュ、アドレス、ペイメントIDで検索…';

  @override
  String get all => 'すべて';

  @override
  String get filterReceived => '受取済み';

  @override
  String get filterSent => '送金済み';

  @override
  String get refresh => '更新';

  @override
  String get noTransactionsFound => 'トランザクションが見つかりません';

  @override
  String get confirmed => '確認済み';

  @override
  String get pending => '保留中';

  @override
  String get hash => 'ハッシュ';

  @override
  String get address => 'アドレス';

  @override
  String get block => 'ブロック';

  @override
  String showingRange(int start, int end, int total) {
    return '$start–$end / $total 件を表示中';
  }

  @override
  String get previous => '前へ';

  @override
  String get next => '次へ';

  @override
  String get walletLocked => 'ウォレットはロックされています';

  @override
  String get enterPasswordToContinue => '続行するにはウォレットのパスワードを入力してください';

  @override
  String get password => 'パスワード';

  @override
  String get incorrectPassword => 'パスワードが正しくありません';

  @override
  String get unlock => 'ロック解除';

  @override
  String get closeWalletInstead => '代わりにウォレットを閉じる';

  @override
  String get closeWallet => 'ウォレットを閉じる';

  @override
  String get closeWalletDescription => 'ウォレットを保存して閉じます。\n\nログイン画面に戻ります。';

  @override
  String get cancel => 'キャンセル';

  @override
  String get welcomeToPluton => 'PLUTON v2 へようこそ';

  @override
  String get selectOptionToStart => 'オプションを選択して開始してください';

  @override
  String get createNewWallet => '新しいウォレットを作成';

  @override
  String get openExistingWallet => '既存のウォレットを開く';

  @override
  String get importFromSeed => 'シードフレーズからインポート';

  @override
  String get importFromKeys => '秘密鍵からインポート';

  @override
  String get openWallet => 'ウォレットを開く';

  @override
  String get importFromSeedTitle => 'シードからインポート';

  @override
  String get importFromKeysTitle => '鍵からインポート';

  @override
  String get saveWalletTo => 'ウォレットの保存先';

  @override
  String get walletFile => 'ウォレットファイル';

  @override
  String get walletPassword => 'ウォレットパスワード';

  @override
  String get mnemonicSeedPhrase => 'ニーモニックシードフレーズ';

  @override
  String get scanFromHeight => 'スキャン開始ブロック高（0 = フルスキャン）';

  @override
  String get daemonHost => 'デーモンホスト';

  @override
  String get port => 'ポート';

  @override
  String get continueButton => '続行';

  @override
  String get browse => '参照';

  @override
  String get backupWarning => '続行する前にウォレットをバックアップしてください。\nこれらの鍵は紛失すると復元できません。';

  @override
  String get yourWalletAddress => 'あなたのウォレットアドレス';

  @override
  String get seedPhrase25Words => 'シードフレーズ（25語）';

  @override
  String get privateViewKey => '秘密ビューキー';

  @override
  String get privateSpendKey => '秘密スペンドキー';

  @override
  String get seedBackupConfirm => 'シードフレーズと秘密鍵を安全な場所に書き留めました。';

  @override
  String get backedUpContinue => 'ウォレットをバックアップしました — 続行';

  @override
  String get settings => '設定';

  @override
  String get sectionDaemonNode => 'デーモンノード';

  @override
  String get nodeDescription => 'ローカルまたはリモートのデーモンノードに接続します。変更は即座に反映されます。';

  @override
  String get hostIpAddress => 'ホスト / IPアドレス';

  @override
  String get ssl => 'SSL';

  @override
  String get apply => '適用';

  @override
  String get nodeUpdatedSuccess => 'ノードが正常に更新されました';

  @override
  String get nodeUnreachable =>
      '現在のノードに接続できません。下に新しいノードアドレスを入力して「適用」をタップしてください。';

  @override
  String get sectionWallet => 'ウォレット';

  @override
  String get saveWallet => 'ウォレットを保存';

  @override
  String get saveWalletSubtitle => '現在の状態をディスクに保存';

  @override
  String get walletSaved => 'ウォレットを保存しました';

  @override
  String get exportToJson => 'JSONにエクスポート';

  @override
  String get exportToJsonSubtitle => 'ウォレットデータをJSONファイルとして保存';

  @override
  String get exportJsonTitle => 'ウォレットJSONエクスポート';

  @override
  String exportedTo(String path) {
    return '$path にエクスポートしました';
  }

  @override
  String exportFailed(String error) {
    return 'エクスポートに失敗しました: $error';
  }

  @override
  String get resetScanHeight => 'スキャン高をリセット';

  @override
  String get resetScanHeightSubtitle => '指定したブロック高からブロックチェーンを再スキャン';

  @override
  String get resetScanHeightDescription =>
      '再スキャンを開始するブロック高を入力してください。フルスキャンの場合は0を使用してください。';

  @override
  String get scanHeight => 'スキャン高';

  @override
  String get reset => 'リセット';

  @override
  String get autosave => '自動保存';

  @override
  String get autosaveSubtitle => '同期後および5分ごとにウォレットをディスクに保存';

  @override
  String get scanCoinbaseTx => 'コインベーストランザクションをスキャン';

  @override
  String get scanCoinbaseSubtitle => '同期時にマイナー報酬を含める（デフォルトではオフ）';

  @override
  String get sectionAppearance => '外観';

  @override
  String get theme => 'テーマ';

  @override
  String get themeSubtitle => 'アプリの配色を選択';

  @override
  String get themeSystem => 'システム';

  @override
  String get themeLight => 'ライト';

  @override
  String get themeDark => 'ダーク';

  @override
  String get sectionNotifications => '通知';

  @override
  String get incomingTxAlerts => '入金トランザクションの通知';

  @override
  String get incomingTxAlertsSubtitle => 'WRKZを受信したときにデスクトップ通知を表示';

  @override
  String get sectionDebugLogs => 'デバッグとログ';

  @override
  String get logLevel => 'ログレベル';

  @override
  String get logLevelSubtitle => 'ウォレットライブラリの詳細度を制御';

  @override
  String get viewLogs => 'ログを表示';

  @override
  String get viewLogsSubtitle => 'ウォレットライブラリのリアルタイムログ出力';

  @override
  String get walletLogs => 'ウォレットログ';

  @override
  String logEntries(int count) {
    return '$count 件のエントリ';
  }

  @override
  String get autoScroll => '自動スクロール';

  @override
  String get copyAll => 'すべてコピー';

  @override
  String get clear => 'クリア';

  @override
  String get close => '閉じる';

  @override
  String get noLogsYet => 'ログはまだありません。出力を表示するにはログレベルを「無効」以上に設定してください。';

  @override
  String get logsCopied => 'ログをクリップボードにコピーしました';

  @override
  String get sectionDangerZone => '危険ゾーン';

  @override
  String get deleteWalletData => 'ウォレットデータを削除';

  @override
  String get deleteWalletDataSubtitle => 'ウォレットファイルをディスクから完全に削除';

  @override
  String get deleteWalletWarning =>
      'ウォレットファイルがディスクから完全に削除されます。\n\n続行する前に、シードフレーズと秘密鍵をバックアップしていることを確認してください。この操作は元に戻せません。';

  @override
  String get iUnderstandContinue => '理解しました、続行します';

  @override
  String get finalConfirmation => '最終確認';

  @override
  String get typeDeleteToConfirm => '確認するにはDELETEと入力してください：';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get deletePermanently => '完全に削除';

  @override
  String get aboutTitle => '情報';

  @override
  String versionInfo(String version) {
    return 'バージョン $version — WRKZ ウェブウォレット';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2はWrkzCoin（WRKZ）の公式ウェブウォレットです。WRKZは高速で軽量なCryptoNoteベースの暗号通貨です。\n\nFlutterで構築され、wallet-apiを使用しています。';

  @override
  String get github => 'GitHub';

  @override
  String get githubSubtitle => 'ソースコードとリリースを表示';

  @override
  String get discord => 'Discord';

  @override
  String get discordSubtitle => 'コミュニティに参加';

  @override
  String get twitterX => 'Twitter / X';

  @override
  String get twitterXSubtitle => '@wrkzcoinをフォロー';

  @override
  String get website => 'ウェブサイト';

  @override
  String get websiteSubtitle => 'wrkz.work';

  @override
  String get license => 'ライセンス';

  @override
  String get licenseText =>
      'MITライセンスの下で公開されています。\n自己責任でご利用ください。シードフレーズは必ずバックアップしてください。';

  @override
  String get addButton => '追加';

  @override
  String get noSavedAddresses => '保存されたアドレスはありません';

  @override
  String get tapAddToSave => '「追加」をタップしてよく使うアドレスを保存してください。';

  @override
  String get addAddress => 'アドレスを追加';

  @override
  String get nameLabel => '名前 / ラベル';

  @override
  String get addressLabel => 'アドレス';

  @override
  String get noteOptional => 'メモ（任意）';

  @override
  String get nameAndAddressRequired => '名前とアドレスは必須です';

  @override
  String get invalidWrkzAddress =>
      '無効なWRKZアドレスです。「Wrkz」で始まる98文字（標準）、120文字（短縮統合）、または186文字（長形統合）である必要があります。';

  @override
  String get save => '保存';

  @override
  String get editEntry => 'エントリを編集';

  @override
  String get deleteEntry => 'エントリを削除';

  @override
  String removeFromAddressBook(String name) {
    return '「$name」をアドレス帳から削除しますか？';
  }

  @override
  String get delete => '削除';

  @override
  String get edit => '編集';

  @override
  String get wrkzReceived => 'WRKZを受信しました';

  @override
  String youReceivedAmount(String amount) {
    return '$amount を受信しました';
  }

  @override
  String get show => '表示';

  @override
  String get exit => '終了';

  @override
  String get plutonWallet => 'PLUTONウォレット';

  @override
  String get language => '言語';

  @override
  String get selectLanguage => '言語を選択';

  @override
  String get chooseLanguage => 'お好みの言語を選択してください';

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
  String get pageNotFound => 'Page not found';

  @override
  String get backToWallet => 'Back to wallet';

  @override
  String get hide => 'Hide';

  @override
  String get max => 'MAX';

  @override
  String get viewInExplorer => 'View in explorer';

  @override
  String get pendingConfirmation => 'Pending confirmation';

  @override
  String get addressWrongPrefix => 'A WRKZ address starts with “Wrkz”';

  @override
  String get addressWrongLength =>
      'Wrong length — a WRKZ address is 98 characters (120 or 186 if integrated)';

  @override
  String get addressBadCharacters =>
      'Contains characters that are not valid in an address (0, O, I and l are never used)';

  @override
  String addressAlreadySaved(String name) {
    return 'Already saved as “$name”';
  }

  @override
  String get amountMustBePositive => 'Amount must be greater than zero';

  @override
  String amountTooManyDecimals(int places) {
    return 'At most $places decimal places';
  }

  @override
  String get amountTooLarge => 'Amount is too large';

  @override
  String amountExceedsBalance(String amount, String ticker) {
    return 'More than your available balance ($amount $ticker, plus fee)';
  }

  @override
  String get networkFee => 'Network fee';

  @override
  String get feeFast => 'Fast';

  @override
  String get feeEconomy => 'Economy';

  @override
  String feeFastHint(String amount, String ticker) {
    return 'Pays $amount $ticker to skip the transaction proof of work. Sends in seconds.';
  }

  @override
  String get feeEconomyHint =>
      'Pays the network minimum, but your browser must compute the transaction proof of work — this can take several minutes.';

  @override
  String get paymentIdWithIntegrated =>
      'This address already contains a payment ID — leave the payment ID field empty.';

  @override
  String get sweepConfirmBody =>
      'This sends your entire balance, minus fees, to:';

  @override
  String tooManyAttempts(int seconds) {
    return 'Too many attempts — try again in ${seconds}s';
  }

  @override
  String get sectionSecurity => 'Security';

  @override
  String get autoLock => 'Auto-lock';

  @override
  String get autoLockSubtitle => 'Lock the wallet after a period of inactivity';

  @override
  String get autoLockNever => 'Never';

  @override
  String autoLockMinutes(int minutes) {
    return '$minutes min';
  }

  @override
  String get confirmPassword => 'Confirm password';

  @override
  String get confirmPasswordField => 'Confirm new password';

  @override
  String get newPassword => 'New password';

  @override
  String get changePassword => 'Change Password';

  @override
  String get changePasswordSubtitle =>
      'Re-encrypt this wallet with a new password';

  @override
  String get changePasswordPurpose => 'Enter your current wallet password.';

  @override
  String get passwordChanged => 'Password changed';

  @override
  String get passwordsDoNotMatch => 'Passwords do not match';

  @override
  String passwordTooShort(int length) {
    return 'Use a password of at least $length characters';
  }

  @override
  String get showSeedAndKeys => 'Seed Phrase & Private Keys';

  @override
  String get showSeedAndKeysSubtitle =>
      'Reveal your recovery seed and keys (asks for your password)';

  @override
  String get revealSecretsPurpose =>
      'Your seed phrase and private keys will be shown on screen.';

  @override
  String get secretsWarning =>
      'Anyone with these can spend your funds. Make sure nobody can see your screen, and never share them — no support channel will ever ask for them.';

  @override
  String get tapToReveal => 'Tap to reveal';

  @override
  String get viewOnlyNoSeed =>
      'This is a view-only wallet — it has no seed phrase or spend key.';

  @override
  String get verifySeedTitle => 'Confirm your backup';

  @override
  String get verifySeedSubtitle =>
      'Type the requested words from the phrase you just wrote down.';

  @override
  String wordNumber(int number) {
    return 'Word #$number';
  }

  @override
  String get askDifferentWords => 'Ask me different words';

  @override
  String get downloadWalletFile => 'Download Wallet File';

  @override
  String get downloadWalletFileSubtitle =>
      'Save an encrypted backup of this wallet to your device';

  @override
  String get walletFileDownloaded =>
      'Encrypted wallet file downloaded — keep it somewhere safe.';

  @override
  String get exportJsonWarning =>
      'This file is NOT encrypted. It contains your private keys in plain text — anyone who opens it can spend your funds.\n\nOnly save it somewhere you control, and delete it when you are done.';

  @override
  String autosaveFailed(String error) {
    return 'Could not save the wallet to browser storage: $error';
  }

  @override
  String get walletDataDeleted => 'Wallet data deleted';

  @override
  String deleteFailed(String error) {
    return 'Delete failed: $error';
  }

  @override
  String get notificationsBlocked =>
      'Your browser is blocking notifications for this site. Enable them in site settings.';

  @override
  String get walletNameRequired => 'Enter a name for this wallet';

  @override
  String get daemonHostRequired => 'Enter a daemon host';

  @override
  String get invalidPort => 'Port must be between 1 and 65535';

  @override
  String get invalidSpendKey =>
      'Private spend key must be 64 hexadecimal characters';

  @override
  String get invalidViewKey =>
      'Private view key must be 64 hexadecimal characters';

  @override
  String seedWordCount(int expected, int actual) {
    return 'A seed phrase has $expected words — you entered $actual';
  }
}
