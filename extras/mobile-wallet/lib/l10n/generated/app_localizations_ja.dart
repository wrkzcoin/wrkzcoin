// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Japanese (`ja`).
class SJa extends S {
  SJa([String locale = 'ja']) : super(locale);

  @override
  String get appTitle => 'PLUTON Mobile';

  @override
  String get tabOverview => '概要';

  @override
  String get tabReceive => '受取';

  @override
  String get tabSend => '送金';

  @override
  String get tabHistory => '履歴';

  @override
  String get tabSettings => '設定';

  @override
  String get send => '送金';

  @override
  String get receive => '受取';

  @override
  String get available => '利用可能';

  @override
  String get locked => 'ロック中';

  @override
  String get total => '合計';

  @override
  String lockedAmount(String amount) {
    return 'ロック中: $amount';
  }

  @override
  String totalAmount(String amount) {
    return '合計: $amount';
  }

  @override
  String get recentTransactions => '最近のトランザクション';

  @override
  String get viewAll => 'すべて表示';

  @override
  String get noTransactionsYet => 'トランザクションはまだありません';

  @override
  String get noMatchingTransactions => '一致するトランザクションがありません';

  @override
  String get pending => '保留中...';

  @override
  String get justNow => 'たった今';

  @override
  String minutesAgo(int count) {
    return '$count分前';
  }

  @override
  String hoursAgo(int count) {
    return '$count時間前';
  }

  @override
  String daysAgo(int count) {
    return '$count日前';
  }

  @override
  String get received => '受取済み';

  @override
  String get sent => '送金済み';

  @override
  String get networkStatus => 'ネットワーク状態';

  @override
  String get node => 'ノード';

  @override
  String get status => 'ステータス';

  @override
  String get connected => '接続済み';

  @override
  String get disconnected => '未接続';

  @override
  String get walletHeight => 'ウォレットの高さ';

  @override
  String get networkHeight => 'ネットワークの高さ';

  @override
  String get peers => 'ピア';

  @override
  String get type => '種類';

  @override
  String get viewOnly => '閲覧専用';

  @override
  String get couldNotFetchStatus => 'ステータスを取得できませんでした。設定のノードを確認してください。';

  @override
  String errorPrefix(String message) {
    return 'エラー: $message';
  }

  @override
  String get seedBackupWarning => '資産を守るため、設定でシードフレーズをバックアップしてください。';

  @override
  String get noConnectionToDaemon => 'デーモンへの接続がありません';

  @override
  String syncingPercent(String percent) {
    return '同期中 $percent%';
  }

  @override
  String get yourAddress => 'あなたのアドレス';

  @override
  String get errorLoadingAddress => 'アドレスの読み込みエラー';

  @override
  String get integratedAddress => '統合アドレス';

  @override
  String get embedPaymentId => 'アドレスに支払いIDを埋め込む';

  @override
  String get randomShort => 'ランダム短 (16)';

  @override
  String get randomLong => 'ランダム長 (64)';

  @override
  String get enterCustomPaymentId => 'カスタム支払いID（16または64桁の16進数）を入力';

  @override
  String get enterPaymentId => '支払いIDを入力';

  @override
  String get paymentIdInvalid => '支払いIDは16または64桁の16進数でなければなりません';

  @override
  String get shortPid => '短PID';

  @override
  String get longPid => '長PID';

  @override
  String get share => '共有';

  @override
  String get copy => 'コピー';

  @override
  String get sweepAllFunds => '全資金をスイープ';

  @override
  String get normalSend => '通常送金';

  @override
  String get sweep => 'スイープ';

  @override
  String get recipientAddress => '受取人アドレス';

  @override
  String get scanQr => 'QRをスキャン';

  @override
  String get amount => '金額';

  @override
  String availableBalance(String amount) {
    return '利用可能: $amount';
  }

  @override
  String sweepInfo(String amount) {
    return 'スイープはすべてのUTXOを統合し、ロック解除済み残高（$amount）から手数料を引いた全額を送金します。';
  }

  @override
  String get paymentIdOptional => '支払いID（任意）';

  @override
  String get hexCharacters => '16または64桁の16進数';

  @override
  String get mustBeHex => '16または64桁の16進数でなければなりません';

  @override
  String get recipientRequired => '受取人アドレスは必須です';

  @override
  String get invalidAddress => '無効なWRKZアドレスです';

  @override
  String get enterValidAmount => '有効な金額を入力してください';

  @override
  String get reviewTransaction => 'トランザクションの確認';

  @override
  String get to => '宛先';

  @override
  String get fee => '手数料';

  @override
  String get totalDeducted => '合計控除額';

  @override
  String get paymentId => '支払いID';

  @override
  String get transactionsIrreversible => 'トランザクションは取り消せません。内容をよく確認してください。';

  @override
  String get back => '戻る';

  @override
  String get confirmAndSend => '確認して送金';

  @override
  String get transactionSent => 'トランザクションを送信しました！';

  @override
  String get transactionHash => 'トランザクションハッシュ';

  @override
  String get sendAnother => '別の送金';

  @override
  String get scanQrCode => 'QRコードをスキャン';

  @override
  String get scannedAddress => 'スキャンしたアドレス';

  @override
  String get cancel => 'キャンセル';

  @override
  String get useThisAddress => 'このアドレスを使用';

  @override
  String get sweepFailed => 'スイープに失敗しました';

  @override
  String get searchPlaceholder => 'ハッシュ、アドレス、支払いIDで検索...';

  @override
  String get all => 'すべて';

  @override
  String get filterReceived => '受取済み';

  @override
  String get filterSent => '送金済み';

  @override
  String get hash => 'ハッシュ';

  @override
  String get address => 'アドレス';

  @override
  String get block => 'ブロック';

  @override
  String get confirmed => '確認済み';

  @override
  String get password => 'パスワード';

  @override
  String get unlock => 'ロック解除';

  @override
  String get switchWallet => 'ウォレット切替';

  @override
  String get enterPasswordToUnlock => 'ロック解除するパスワードを入力してください';

  @override
  String get incorrectPassword => 'パスワードが正しくありません';

  @override
  String get enterYourPassword => 'パスワードを入力してください';

  @override
  String get plutonMobile => 'PLUTON Mobile';

  @override
  String get createFirstWalletSubtitle => '最初のウォレットを作成して始めましょう';

  @override
  String get selectWalletSubtitle => '開くウォレットを選択してください';

  @override
  String get yourWallets => 'あなたのウォレット';

  @override
  String get noWalletsYet => 'ウォレットはまだありません';

  @override
  String get lastOpened => '最後に開いた日時';

  @override
  String createdDate(String date) {
    return '$dateに作成';
  }

  @override
  String get createFirstWallet => '最初のウォレットを作成';

  @override
  String get addWallet => 'ウォレットを追加';

  @override
  String get deleteWallet => 'ウォレットを削除';

  @override
  String deleteWalletConfirm(String name) {
    return '「$name」を削除しますか？\n\nウォレットファイルと鍵が完全に削除されます。シードフレーズのバックアップを確認してください。';
  }

  @override
  String get delete => '削除';

  @override
  String get createNewWallet => '新しいウォレットを作成';

  @override
  String get createNewWalletSubtitle => '新しいシードフレーズでウォレットを生成する';

  @override
  String get importFromSeed => 'シードフレーズからインポート';

  @override
  String get importFromSeedSubtitle => '25ワードのニーモニックシードでウォレットを復元する';

  @override
  String get importFromKeys => '秘密鍵からインポート';

  @override
  String get importFromKeysSubtitle => 'スペンド鍵とビュー鍵を使って復元する';

  @override
  String get viewOnlyWallet => '閲覧専用ウォレット';

  @override
  String get viewOnlyWalletSubtitle => 'ビュー鍵とアドレスを使ったウォッチ専用ウォレット';

  @override
  String get createWallet => 'ウォレットを作成';

  @override
  String get importWallet => 'ウォレットをインポート';

  @override
  String get walletName => 'ウォレット名';

  @override
  String get walletNameHint => '例: メインウォレット';

  @override
  String get passwordLabel => 'パスワード';

  @override
  String get enterPassword => 'パスワードを入力';

  @override
  String get confirmPassword => 'パスワードを確認';

  @override
  String get seedPhrase => 'シードフレーズ（25ワード）';

  @override
  String get enterSeedPhrase => 'シードフレーズを入力...';

  @override
  String get scanHeight => 'スキャン高さ（任意）';

  @override
  String get scanHeightHint => '0 = 最初からスキャン';

  @override
  String get privateSpendKey => '秘密スペンド鍵';

  @override
  String get privateViewKey => '秘密ビュー鍵';

  @override
  String get walletAddress => 'ウォレットアドレス';

  @override
  String get walletAddressHint => 'Wrkz... アドレス';

  @override
  String get hexKey => '64文字の16進数';

  @override
  String get daemonNode => 'デーモンノード';

  @override
  String get custom => 'カスタム';

  @override
  String get host => 'ホスト';

  @override
  String get hostHint => 'ホスト / IP';

  @override
  String get port => 'ポート';

  @override
  String get ssl => 'SSL';

  @override
  String get walletNameRequired => 'ウォレット名は必須です';

  @override
  String get passwordRequired => 'パスワードは必須です';

  @override
  String passwordTooShort(int count) {
    return 'パスワードは$count文字以上でなければなりません';
  }

  @override
  String get passwordsDoNotMatch => 'パスワードが一致しません';

  @override
  String get seedRequired => 'シードフレーズは必須です';

  @override
  String get spendKeyRequired => 'スペンド鍵は必須です';

  @override
  String get viewKeyRequired => 'ビュー鍵は必須です';

  @override
  String get addressRequired => 'アドレスは必須です';

  @override
  String get daemonHostRequired => 'デーモンホストは必須です';

  @override
  String get backupSeedTitle => 'シードのバックアップ';

  @override
  String get backupWarning => 'シードフレーズを書き留め、安全な場所に保管してください。失った場合、資金は永久に失われます。';

  @override
  String get seedPhraseLabel => 'シードフレーズ';

  @override
  String get privateViewKeyLabel => '秘密ビュー鍵';

  @override
  String get privateSpendKeyLabel => '秘密スペンド鍵';

  @override
  String get backupConfirmCheck => 'シードフレーズを安全にバックアップしました';

  @override
  String get continueToWallet => 'ウォレットへ進む';

  @override
  String get sectionDaemonNode => 'デーモンノード';

  @override
  String get apply => '適用';

  @override
  String nodeUpdated(String host, int port) {
    return 'ノードを $host:$port に更新しました';
  }

  @override
  String get hostRequired => 'ホストは必須です';

  @override
  String currentWallet(String name) {
    return '現在のウォレット — $name';
  }

  @override
  String get saveWallet => 'ウォレットを保存';

  @override
  String get walletSaved => 'ウォレットを保存しました';

  @override
  String saveFailed(String error) {
    return '保存に失敗しました: $error';
  }

  @override
  String get backupSeed => 'シードのバックアップ';

  @override
  String get changePassword => 'パスワードを変更';

  @override
  String get resetScanHeight => 'スキャン高さをリセット';

  @override
  String get reset => 'リセット';

  @override
  String resetScanConfirm(int height) {
    return 'ブロック $height からブロックチェーンを再スキャンします。時間がかかる場合があります。続けますか？';
  }

  @override
  String scanResetTo(int height) {
    return 'スキャンをブロック $height にリセットしました';
  }

  @override
  String resetFailed(String error) {
    return 'リセットに失敗しました: $error';
  }

  @override
  String get enterPasswordTitle => 'パスワードを入力';

  @override
  String get confirm => '確認';

  @override
  String get seedBackup => 'シードバックアップ';

  @override
  String get seedPhraseColon => 'シードフレーズ:';

  @override
  String get privateViewKeyColon => '秘密ビュー鍵:';

  @override
  String get iveBackedUp => 'バックアップ完了';

  @override
  String get currentPasswordLabel => '現在のパスワード';

  @override
  String get newPasswordLabel => '新しいパスワード';

  @override
  String get confirmNewPasswordLabel => '新しいパスワードを確認';

  @override
  String get change => '変更';

  @override
  String get currentPasswordIncorrect => '現在のパスワードが正しくありません';

  @override
  String get newPasswordsDoNotMatch => '新しいパスワードが一致しません';

  @override
  String get passwordChanged => 'パスワードを変更しました';

  @override
  String get walletManagement => 'ウォレット管理';

  @override
  String get switchWalletSubtitle => '保存して閉じ、別のウォレットを選択';

  @override
  String get manageWallets => 'ウォレットを管理';

  @override
  String get manageWalletsSubtitle => 'ウォレットの名前変更または削除';

  @override
  String get currentlyOpen => '（現在開いています）';

  @override
  String get close => '閉じる';

  @override
  String get renameWallet => 'ウォレットの名前を変更';

  @override
  String get newName => '新しい名前';

  @override
  String get rename => '名前を変更';

  @override
  String deleteWalletConfirmShort(String name) {
    return '「$name」を削除しますか？この操作は取り消せません。';
  }

  @override
  String get security => 'セキュリティ';

  @override
  String get biometricUnlock => '生体認証ロック解除';

  @override
  String get biometricSubtitle => '指紋 / 顔認証';

  @override
  String get biometricNotAvailable => '生体認証は利用できません';

  @override
  String get autoLock => '自動ロック';

  @override
  String get appearance => '外観';

  @override
  String get theme => 'テーマ';

  @override
  String get themeAuto => '自動';

  @override
  String get themeLight => 'ライト';

  @override
  String get themeDark => 'ダーク';

  @override
  String get preferences => '環境設定';

  @override
  String get transactionNotifications => 'トランザクション通知';

  @override
  String get notificationsSubtitle => '入金時にアラートを表示';

  @override
  String get autosave => '自動保存';

  @override
  String get autosaveSubtitle => '同期後に保存し、その後5分ごとに保存';

  @override
  String get scanCoinbaseTx => 'コインベーストランザクションをスキャン';

  @override
  String get scanCoinbaseSubtitle => 'マイナー報酬を含める（デフォルトはオフ）';

  @override
  String get dangerZone => '危険ゾーン';

  @override
  String get deleteCurrentWallet => '現在のウォレットを削除';

  @override
  String get deleteCurrentWalletSubtitle => 'ウォレットデータを完全に削除する';

  @override
  String get deleteWalletTypeCaps =>
      'ウォレットファイルと鍵が完全に削除されます。シードフレーズのバックアップを確認してください。\n\n確認するにはDELETEと入力してください:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get language => '言語';

  @override
  String get selectLanguage => '言語を選択';

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
  String get autoLockImmediately => 'すぐに';

  @override
  String get autoLock1Min => '1分';

  @override
  String get autoLock5Min => '5分';

  @override
  String get autoLockNever => 'しない';

  @override
  String get preparedTransactionExpired => 'この取引は無効になりました。戻って作成し直してください。';

  @override
  String get deleteConfirmMismatch => '確認するにはDELETEと正確に入力してください。';

  @override
  String get seedNotBackedUpWarning =>
      'このウォレットのシードフレーズのバックアップが確認されていません。今削除すると資金を復元できません。';

  @override
  String get wrkzReceived => 'WRKZを受け取りました';

  @override
  String get retry => '再試行';

  @override
  String youReceivedAmount(String amount) {
    return '$amount を受け取りました';
  }

  @override
  String get ringSize => 'リングサイズ';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'リングサイズが $actual に縮小されました（通常は $normal）。送金する金額に対して、チェーン上に完全なリングを構成するのに十分な出力がないため、この取引は通常よりも匿名性が低くなります。';
  }

  @override
  String get liteNodeTitle => 'ライトノード';

  @override
  String liteNodeServesFrom(int height) {
    return 'このノードはブロック $height 以降しか保持していません。それより前の取引はこのノードでは見つけられません。';
  }

  @override
  String liteNodeMissesHistory(int nodeHeight, int walletHeight) {
    return 'このノードはブロック $nodeHeight から始まりますが、このウォレットはブロック $walletHeight から始まります。その間に受け取った分はここでは見えないため、表示される残高が実際より少ない可能性があります。チェーン全体を保持するノードに接続してください。';
  }

  @override
  String liteNodeSyncStalled(int wallet, int node) {
    return 'ブロック $wallet で同期を停止しました。このノードはブロック $node より下を保持していないため、その間のブロックを取得できません。チェーン全体を保持するノードに接続するまで残高は不完全です。';
  }

  @override
  String get liteNodeRescanRefusedTitle => 'このノードではそこまで遡って再スキャンできません';

  @override
  String liteNodeRescanRefused(int height) {
    return '接続中のノードはライトノードで、$height より下のブロックデータを持っていません。それより低い位置から再スキャンすると、すでに見つかっている取引が失われ、ここでは二度と見つけられません。何も変更していません。';
  }

  @override
  String liteNodeRescanFromInstead(int height) {
    return '代わりに $height から再スキャン';
  }

  @override
  String liteNodeRescanHint(int height) {
    return '接続中のノードはブロック $height 以降からしか再スキャンできません。';
  }

  @override
  String get nodeServesFromLabel => '提供ブロック開始位置';

  @override
  String get nodeFullChain => 'チェーン全体';

  @override
  String get localNodeMobileFuture =>
      '端末上でノードを動かす機能は計画中ですが、まだ利用できません。ノードには数 GB の保存容量と数時間の同期が必要です。それまでは自分で運用するノードを指定してください。';

  @override
  String get syncStoppedTitle => '同期を停止しました';

  @override
  String syncGapStalled(int covered, int servesFrom) {
    return 'ブロック $covered で同期が停止しました。接続先のノードはブロック $servesFrom 以降しか応答しないため、その間のブロックを取得できません。全チェーンを保持するノードに接続するまで、残高は不完全です。';
  }

  @override
  String get txPowServerSection => 'トランザクション PoW サーバー';

  @override
  String get txPowServerUse => '外部 PoW サーバーを使用';

  @override
  String get txPowServerSubtitle =>
      'トランザクションのプルーフ・オブ・ワークをこの端末で計算せず、サーバーに任せます。サーバーが応答しない場合は、この端末の CPU を使用します。';

  @override
  String get txPowServerSaved => 'PoW サーバーの設定を保存しました';

  @override
  String get txPowServerInvalid => '有効なホストとポートを入力してください';
}
