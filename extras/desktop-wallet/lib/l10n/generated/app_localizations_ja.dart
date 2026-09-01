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
  String get browse => '参照…';

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
    return 'バージョン $version — WRKZ デスクトップウォレット';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2はWrkzCoin（WRKZ）の公式デスクトップウォレットです。WRKZは高速で軽量なCryptoNoteベースの暗号通貨です。\n\nFlutterで構築され、wallet-apiを使用しています。';

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
  String get preparedTransactionExpired => 'この取引は無効になりました。戻って作成し直してください。';

  @override
  String get deleteConfirmMismatch => '確認するにはDELETEと正確に入力してください。';

  @override
  String get unlockNeedsReopen =>
      'このデバイスではパスワードを検証できません。「ウォレットを閉じる」を使ってから、ウォレットを開き直してください。';

  @override
  String get exportJsonWarningTitle => '暗号化されていないウォレットをエクスポートしますか？';

  @override
  String get exportJsonWarningBody =>
      'エクスポートされたファイルには、プライベートビューキーとプライベートスペンドキーが平文で含まれます。読んだ人は誰でもあなたの資金を使えます。\n\n自分が管理する保存先にのみ保存し、用が済んだらすぐに削除してください。';

  @override
  String passwordTooShort(int count) {
    return 'パスワードは$count文字以上でなければなりません';
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
  String get sectionLocalNode => 'ローカルライトノード';

  @override
  String get localNodeDescription =>
      'このコンピューターでノードを動かし、リモートサーバーの代わりにそれと同期します。ライトノードはウォレットに必要なものだけを保存しますが、チェーン全体を一度はダウンロードします。';

  @override
  String get localNodeSetUp => 'ローカルノードを設定';

  @override
  String get localNodeStart => '開始';

  @override
  String get localNodeStop => '停止';

  @override
  String get localNodeUse => 'このノードを使う';

  @override
  String get localNodeDelete => 'ノードデータを削除';

  @override
  String get localNodeStateStopped => '停止中';

  @override
  String get localNodeStateStarting => '起動中';

  @override
  String get localNodeStateSyncing => '同期中';

  @override
  String get localNodeStateReady => '同期済み';

  @override
  String get localNodeStateFailed => '失敗';

  @override
  String localNodeProgress(int height, int network) {
    return 'ブロック $height / $network';
  }

  @override
  String localNodePeers(int count) {
    return '$count ピア';
  }

  @override
  String get localNodeNotReadyYet =>
      'ローカルノードはまだ追いついておらず、ウォレットに応答できません。バックグラウンドで同期を続けます。準備できるまでリモートノードを使い、その後で切り替えてください。';

  @override
  String get localNodeInUse => 'ウォレットはこのノードに接続しています。';

  @override
  String localNodeBinaryMissing(String name) {
    return '$name が見つかりません。デーモンの実行ファイルをウォレット実行ファイルの隣、または隣接する「sidecar」フォルダーに置いて、もう一度お試しください。';
  }

  @override
  String get localNodeSetupTitle => 'ローカルライトノードを設定';

  @override
  String get localNodeSetupCost =>
      '始める前に:\n• 約 6 GB のディスク容量が必要で、チェーン全体を一度ダウンロードします。\n• 初回同期には数時間かかります。バックグラウンドで続行し、その間はリモートノードを使えます。\n• 下の開始ブロック高は変更できません。後で変えるにはノードを削除し、最初から同期し直す必要があります。';

  @override
  String get localNodeStartHeightLabel => '開始ブロック高';

  @override
  String localNodeStartHeightHelp(int height) {
    return 'この高さより下のブロックはダウンロードして検証したうえで、後続ブロックに必要なインデックスだけを残します。このウォレットの開始ブロック高 ($height) 以下にしてください。';
  }

  @override
  String localNodeStartHeightTooHigh(int height) {
    return 'このウォレットの開始ブロック高 ($height) より高い値です。このノードでは古い取引を表示できなくなります。';
  }

  @override
  String get localNodeCreate => 'ノードを作成';

  @override
  String get localNodeDeleteTitle => 'ローカルノードのデータを削除しますか？';

  @override
  String get localNodeDeleteWarning =>
      'ノードを停止し、そのブロックチェーンデータベースをディスクから完全に削除します。ウォレット、シード、資金には影響しませんが、新しいローカルノードは一からの同期になり数時間かかります。';

  @override
  String get localNodeDeleted => 'ローカルノードを削除しました。';

  @override
  String localNodeDiskUsage(String size) {
    return 'ディスク使用量 $size';
  }

  @override
  String get nodePresetRemote => 'リモートノード';

  @override
  String get nodePresetLocal => 'ローカルライトノード';

  @override
  String get savingWallet => 'ウォレットを保存しています…';

  @override
  String get savingWalletBody =>
      'PLUTON がウォレットをディスクに書き込んでいます。大きなウォレットでは少し時間がかかることがあります。';

  @override
  String get shutdownTakingLong =>
      '予想より時間がかかっています。ここで終了すると今回の保存は失われます。ディスク上のウォレットファイルはそのままなので破損はしませんが、前回の保存以降の内容はなくなります。';

  @override
  String get quitAnyway => '強制終了';

  @override
  String get stillRunningInTray =>
      'PLUTON は通知領域で実行中です。アイコンをクリックすると再表示できます。終了するにはアイコンを右クリックして「終了」を選択してください。';

  @override
  String get localNodeDataFolder => 'データフォルダ';

  @override
  String get localNodeDataFolderHelp =>
      'ここに約 6 GB が書き込まれます。空き容量のあるドライブを選んでください。';

  @override
  String get localNodeDataFolderInUse =>
      'そのフォルダにはすでに他のファイルがあります。空のフォルダか新しいフォルダを選んでください。';

  @override
  String get localNodeStartHeightRequired =>
      '完全なブロックを保持し始める高さを入力してください。0 より大きい必要があります。ライトノードはジェネシスブロックからは始められません。';

  @override
  String get nodeExitTitle => 'ローカルノードはまだ実行中です';

  @override
  String get nodeExitBodySyncing =>
      '初回同期がまだ終わっていません。数時間かかり、ノードが動いている間だけ進みます。ただし実行したままにすると、PLUTON を閉じた後もディスクとネットワークを使い続けます。';

  @override
  String get nodeExitBodySynced =>
      'ネットワークに追いついています。実行したままにすればその状態を保てますが、PLUTON を閉じても CPU・帯域・ディスクを使います。停止すると次回に短い追いつきが必要です。';

  @override
  String get nodeExitKeep => '実行したままにする';

  @override
  String get nodeExitStop => '停止する';

  @override
  String get nodeExitChangeLater => '後から設定の「ローカルライトノード」で変更できます。';

  @override
  String get rememberMyChoice => 'この選択を覚えておく';

  @override
  String get shutdownStoppingNode => 'ローカルノードを停止しています…';

  @override
  String get shutdownStoppingNodeBody =>
      'データベースを書き出させています。次回起動時に先行書き込みログを再生しなくて済みます。';

  @override
  String get nodeExitPolicyLabel => 'ウォレットを閉じるとき';

  @override
  String get nodeExitPolicyAsk => '毎回尋ねる';

  @override
  String get nodeExitPolicyKeep => 'ノードを実行したままにする';

  @override
  String get nodeExitPolicyStop => 'ノードを停止する';

  @override
  String get localNodeStillRunningBody =>
      'ローカルライトノードはバックグラウンドで実行中です。停止するには PLUTON を開いてください。';
}
