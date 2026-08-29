// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Chinese (`zh`).
class SZh extends S {
  SZh([String locale = 'zh']) : super(locale);

  @override
  String get appTitle => 'PLUTON v2';

  @override
  String get tabOverview => '概览';

  @override
  String get tabReceive => '接收';

  @override
  String get tabTransfer => '转账';

  @override
  String get tabHistory => '历史';

  @override
  String get tabAddressBook => '地址簿';

  @override
  String get tabSettings => '设置';

  @override
  String get tabAbout => '关于';

  @override
  String get lockWallet => '锁定钱包';

  @override
  String get send => '发送';

  @override
  String get receive => '接收';

  @override
  String get transfer => '转账';

  @override
  String get available => '可用';

  @override
  String get locked => '锁定';

  @override
  String get total => '总计';

  @override
  String get availableBalance => '可用余额';

  @override
  String lockedUnconfirmed(String amount, String ticker) {
    return '锁定（未确认）：$amount $ticker';
  }

  @override
  String totalBalance(String amount, String ticker) {
    return '总计：$amount $ticker';
  }

  @override
  String get balanceIncompleteWhileSyncing => '同步期间余额可能不完整';

  @override
  String errorPrefix(String message) {
    return '错误：$message';
  }

  @override
  String get network => '网络';

  @override
  String get syncStatus => '同步状态';

  @override
  String get synced => '已同步';

  @override
  String get syncing => '同步中…';

  @override
  String get walletBlock => '钱包区块';

  @override
  String get networkBlock => '网络区块';

  @override
  String get peers => '节点';

  @override
  String get walletType => '钱包类型';

  @override
  String get viewOnly => '仅查看';

  @override
  String get full => '完整';

  @override
  String get nodeConnectionIssue => '节点连接问题';

  @override
  String get switchNodeInSettings => '在设置中切换节点 →';

  @override
  String get recentTransactions => '最近交易';

  @override
  String get viewAll => '查看全部 →';

  @override
  String get noTransactionsYet => '暂无交易';

  @override
  String get received => '已接收';

  @override
  String get sent => '已发送';

  @override
  String syncingProgress(String pct, int wallet, int network) {
    return '同步中 $pct%（区块 $wallet / $network）';
  }

  @override
  String get shareAddressSubtitle => '分享您的地址以接收 WRKZ';

  @override
  String get yourAddress => '您的地址';

  @override
  String get generateIntegratedAddress => '生成集成地址';

  @override
  String get integratedAddressDescription =>
      '将您的地址与付款 ID 组合。使用随机按钮生成新 ID，或在下方输入您自己的 ID。';

  @override
  String get randomShort16 => '随机短码 (16)';

  @override
  String get randomLong64 => '随机长码 (64)';

  @override
  String get customPaymentIdLabel => '自定义付款 ID（16 或 64 个十六进制字符）';

  @override
  String get generate => '生成';

  @override
  String get integratedAddress => '集成地址';

  @override
  String get paymentIdShort => '短码 (16)';

  @override
  String get paymentIdLong => '长码 (64)';

  @override
  String paymentIdLabel(String label) {
    return '付款 ID · $label';
  }

  @override
  String get enterPaymentIdError => '请输入付款 ID（16 或 64 个十六进制字符）';

  @override
  String get paymentIdInvalidError => '付款 ID 必须为 16 或 64 个十六进制字符';

  @override
  String get copyAddress => '复制地址';

  @override
  String get copyPaymentId => '复制付款 ID';

  @override
  String get copy => '复制';

  @override
  String get copied => '已复制！';

  @override
  String get sendWrkzToAny => '发送 WRKZ 到任意地址';

  @override
  String get sweepAllDescription => '将所有资金发送到一个地址（合并 UTXO）';

  @override
  String get sweepAll => '全部清扫';

  @override
  String get sweepWarning => '清扫会将所有 UTXO 合并为一个输出。当交易因输入过多而失败时使用此功能。';

  @override
  String sweepAvailableBalance(String amount, String ticker) {
    return '可用：$amount $ticker（将发送全部余额减去手续费）';
  }

  @override
  String get destinationAddress => '目标地址';

  @override
  String get addressBook => '地址簿';

  @override
  String get sweepAllFunds => '清扫所有资金';

  @override
  String get recipientAddress => '收款地址';

  @override
  String get amount => '金额';

  @override
  String get paymentIdOptional => '付款 ID（可选）';

  @override
  String get hexCharacters => '16 或 64 个十六进制字符';

  @override
  String get reviewTransaction => '审核交易';

  @override
  String get reviewAndConfirm => '审核并确认';

  @override
  String get to => '收款方';

  @override
  String get fee => '手续费';

  @override
  String get totalDeducted => '总扣除额';

  @override
  String get paymentId => '付款 ID';

  @override
  String get transactionsIrreversible => '交易不可逆。请在确认前核实地址。';

  @override
  String get back => '返回';

  @override
  String get confirmAndSend => '确认并发送';

  @override
  String get transactionSent => '交易已发送！';

  @override
  String get transactionBroadcast => '您的交易已广播到网络。';

  @override
  String get transactionHash => '交易哈希';

  @override
  String get sendAnother => '再发一笔';

  @override
  String get enterDestinationAddress => '请输入目标地址';

  @override
  String get enterValidAmount => '请输入有效金额';

  @override
  String computingPow(int seconds) {
    return '正在计算 PoW... $seconds秒';
  }

  @override
  String get stepFillDetails => '填写详情';

  @override
  String get stepReview => '审核';

  @override
  String get stepDone => '完成';

  @override
  String get sweepFailed => '清扫失败';

  @override
  String get addressBookTitle => '地址簿';

  @override
  String get transactionHistory => '交易历史';

  @override
  String get searchByHash => '按哈希、地址或付款 ID 搜索…';

  @override
  String get all => '全部';

  @override
  String get filterReceived => '已接收';

  @override
  String get filterSent => '已发送';

  @override
  String get refresh => '刷新';

  @override
  String get noTransactionsFound => '未找到交易';

  @override
  String get confirmed => '已确认';

  @override
  String get pending => '待确认';

  @override
  String get hash => '哈希';

  @override
  String get address => '地址';

  @override
  String get block => '区块';

  @override
  String showingRange(int start, int end, int total) {
    return '显示 $start–$end，共 $total';
  }

  @override
  String get previous => '上一页';

  @override
  String get next => '下一页';

  @override
  String get walletLocked => '钱包已锁定';

  @override
  String get enterPasswordToContinue => '输入钱包密码以继续';

  @override
  String get password => '密码';

  @override
  String get incorrectPassword => '密码错误';

  @override
  String get unlock => '解锁';

  @override
  String get closeWalletInstead => '改为关闭钱包';

  @override
  String get closeWallet => '关闭钱包';

  @override
  String get closeWalletDescription => '这将保存并关闭钱包。\n\n您将返回到登录页面。';

  @override
  String get cancel => '取消';

  @override
  String get welcomeToPluton => '欢迎使用 PLUTON v2';

  @override
  String get selectOptionToStart => '选择一个选项以开始';

  @override
  String get createNewWallet => '创建新钱包';

  @override
  String get openExistingWallet => '打开现有钱包';

  @override
  String get importFromSeed => '通过助记词导入';

  @override
  String get importFromKeys => '通过私钥导入';

  @override
  String get openWallet => '打开钱包';

  @override
  String get importFromSeedTitle => '从助记词导入';

  @override
  String get importFromKeysTitle => '从私钥导入';

  @override
  String get saveWalletTo => '钱包保存到';

  @override
  String get walletFile => '钱包文件';

  @override
  String get walletPassword => '钱包密码';

  @override
  String get mnemonicSeedPhrase => '助记词';

  @override
  String get scanFromHeight => '从高度扫描（0 = 完整扫描）';

  @override
  String get daemonHost => '守护进程主机';

  @override
  String get port => '端口';

  @override
  String get continueButton => '继续';

  @override
  String get browse => '浏览';

  @override
  String get backupWarning => '继续前请备份您的钱包。\n这些密钥丢失后无法恢复。';

  @override
  String get yourWalletAddress => '您的钱包地址';

  @override
  String get seedPhrase25Words => '助记词（25 个单词）';

  @override
  String get privateViewKey => '私有查看密钥';

  @override
  String get privateSpendKey => '私有消费密钥';

  @override
  String get seedBackupConfirm => '我已将助记词和私钥抄写在安全的地方。';

  @override
  String get backedUpContinue => '我已备份钱包 — 继续';

  @override
  String get settings => '设置';

  @override
  String get sectionDaemonNode => '守护进程节点';

  @override
  String get nodeDescription => '连接到本地或远程守护进程节点。更改立即生效。';

  @override
  String get hostIpAddress => '主机 / IP 地址';

  @override
  String get ssl => 'SSL';

  @override
  String get apply => '应用';

  @override
  String get nodeUpdatedSuccess => '节点更新成功';

  @override
  String get nodeUnreachable => '无法连接当前节点。请在下方输入新的节点地址并点击应用。';

  @override
  String get sectionWallet => '钱包';

  @override
  String get saveWallet => '保存钱包';

  @override
  String get saveWalletSubtitle => '将当前状态写入磁盘';

  @override
  String get walletSaved => '钱包已保存';

  @override
  String get exportToJson => '导出为 JSON';

  @override
  String get exportToJsonSubtitle => '将钱包数据保存为 JSON 文件';

  @override
  String get exportJsonTitle => '导出钱包 JSON';

  @override
  String exportedTo(String path) {
    return '已导出到 $path';
  }

  @override
  String exportFailed(String error) {
    return '导出失败：$error';
  }

  @override
  String get resetScanHeight => '重置扫描高度';

  @override
  String get resetScanHeightSubtitle => '从指定高度重新扫描区块链';

  @override
  String get resetScanHeightDescription => '输入要从哪个区块高度开始重新扫描。输入 0 进行完整重新扫描。';

  @override
  String get scanHeight => '扫描高度';

  @override
  String get reset => '重置';

  @override
  String get autosave => '自动保存';

  @override
  String get autosaveSubtitle => '在同步后和每 5 分钟将钱包保存到磁盘';

  @override
  String get scanCoinbaseTx => '扫描 Coinbase 交易';

  @override
  String get scanCoinbaseSubtitle => '同步时包含矿工奖励（默认关闭）';

  @override
  String get sectionAppearance => '外观';

  @override
  String get theme => '主题';

  @override
  String get themeSubtitle => '选择应用配色方案';

  @override
  String get themeSystem => '跟随系统';

  @override
  String get themeLight => '浅色';

  @override
  String get themeDark => '深色';

  @override
  String get sectionNotifications => '通知';

  @override
  String get incomingTxAlerts => '收款提醒';

  @override
  String get incomingTxAlertsSubtitle => '收到 WRKZ 时显示桌面通知';

  @override
  String get sectionDebugLogs => '调试与日志';

  @override
  String get logLevel => '日志级别';

  @override
  String get logLevelSubtitle => '控制钱包库的详细程度';

  @override
  String get viewLogs => '查看日志';

  @override
  String get viewLogsSubtitle => '实时钱包库日志输出';

  @override
  String get walletLogs => '钱包日志';

  @override
  String logEntries(int count) {
    return '$count 条记录';
  }

  @override
  String get autoScroll => '自动滚动';

  @override
  String get copyAll => '全部复制';

  @override
  String get clear => '清除';

  @override
  String get close => '关闭';

  @override
  String get noLogsYet => '暂无日志。将日志级别设置为“禁用”以上即可查看输出。';

  @override
  String get logsCopied => '日志已复制到剪贴板';

  @override
  String get sectionDangerZone => '危险操作';

  @override
  String get deleteWalletData => '删除钱包数据';

  @override
  String get deleteWalletDataSubtitle => '从磁盘永久删除钱包文件';

  @override
  String get deleteWalletWarning =>
      '这将从磁盘永久删除您的钱包文件。\n\n请确保在继续之前已备份助记词和私钥。此操作无法撤销。';

  @override
  String get iUnderstandContinue => '我已了解，继续';

  @override
  String get finalConfirmation => '最终确认';

  @override
  String get typeDeleteToConfirm => '输入 DELETE 以确认：';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get deletePermanently => '永久删除';

  @override
  String get aboutTitle => '关于';

  @override
  String versionInfo(String version) {
    return '版本 $version — WRKZ 网页钱包';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 是 WrkzCoin (WRKZ) 的官方网页钱包，WrkzCoin 是一种基于 CryptoNote 的快速轻量加密货币。\n\n使用 Flutter 构建，由 wallet-api 驱动。';

  @override
  String get github => 'GitHub';

  @override
  String get githubSubtitle => '查看源代码和发行版';

  @override
  String get discord => 'Discord';

  @override
  String get discordSubtitle => '加入社区';

  @override
  String get twitterX => 'Twitter / X';

  @override
  String get twitterXSubtitle => '关注 @wrkzcoin';

  @override
  String get website => '网站';

  @override
  String get websiteSubtitle => 'wrkz.work';

  @override
  String get license => '许可证';

  @override
  String get licenseText => '基于 MIT 许可证发布。\n使用风险自负。请务必备份您的助记词。';

  @override
  String get addButton => '添加';

  @override
  String get noSavedAddresses => '没有已保存的地址';

  @override
  String get tapAddToSave => '点击“添加”以保存常用地址。';

  @override
  String get addAddress => '添加地址';

  @override
  String get nameLabel => '名称 / 标签';

  @override
  String get addressLabel => '地址';

  @override
  String get noteOptional => '备注（可选）';

  @override
  String get nameAndAddressRequired => '名称和地址为必填项';

  @override
  String get invalidWrkzAddress =>
      '无效的 WRKZ 地址。必须以 \"Wrkz\" 开头，长度为 98（标准）、120（短集成）或 186（长集成）个字符。';

  @override
  String get save => '保存';

  @override
  String get editEntry => '编辑条目';

  @override
  String get deleteEntry => '删除条目';

  @override
  String removeFromAddressBook(String name) {
    return '从地址簿中移除 \"$name\"？';
  }

  @override
  String get delete => '删除';

  @override
  String get edit => '编辑';

  @override
  String get wrkzReceived => '收到 WRKZ';

  @override
  String youReceivedAmount(String amount) {
    return '您收到了 $amount';
  }

  @override
  String get show => '显示';

  @override
  String get exit => '退出';

  @override
  String get plutonWallet => 'PLUTON 钱包';

  @override
  String get language => '语言';

  @override
  String get selectLanguage => '选择语言';

  @override
  String get chooseLanguage => '选择您偏好的语言';

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
