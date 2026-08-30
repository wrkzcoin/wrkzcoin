// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Chinese (`zh`).
class SZh extends S {
  SZh([String locale = 'zh']) : super(locale);

  @override
  String get appTitle => 'PLUTON Mobile';

  @override
  String get tabOverview => '概览';

  @override
  String get tabReceive => '收款';

  @override
  String get tabSend => '转账';

  @override
  String get tabHistory => '记录';

  @override
  String get tabSettings => '设置';

  @override
  String get send => '转账';

  @override
  String get receive => '收款';

  @override
  String get available => '可用';

  @override
  String get locked => '已锁定';

  @override
  String get total => '总计';

  @override
  String lockedAmount(String amount) {
    return '已锁定：$amount';
  }

  @override
  String totalAmount(String amount) {
    return '总计：$amount';
  }

  @override
  String get recentTransactions => '最近交易';

  @override
  String get viewAll => '查看全部';

  @override
  String get noTransactionsYet => '暂无交易记录';

  @override
  String get noMatchingTransactions => '无匹配交易';

  @override
  String get pending => '处理中...';

  @override
  String get justNow => '刚刚';

  @override
  String minutesAgo(int count) {
    return '$count分钟前';
  }

  @override
  String hoursAgo(int count) {
    return '$count小时前';
  }

  @override
  String daysAgo(int count) {
    return '$count天前';
  }

  @override
  String get received => '已收款';

  @override
  String get sent => '已转账';

  @override
  String get networkStatus => '网络状态';

  @override
  String get node => '节点';

  @override
  String get status => '状态';

  @override
  String get connected => '已连接';

  @override
  String get disconnected => '已断开';

  @override
  String get walletHeight => '钱包高度';

  @override
  String get networkHeight => '网络高度';

  @override
  String get peers => '节点数';

  @override
  String get type => '类型';

  @override
  String get viewOnly => '仅查看';

  @override
  String get couldNotFetchStatus => '无法获取状态，请在设置中检查您的节点。';

  @override
  String errorPrefix(String message) {
    return '错误：$message';
  }

  @override
  String get seedBackupWarning => '请在设置中备份您的助记词以保护资金安全。';

  @override
  String get noConnectionToDaemon => '无法连接到守护进程';

  @override
  String syncingPercent(String percent) {
    return '同步中 $percent%';
  }

  @override
  String get yourAddress => '您的地址';

  @override
  String get errorLoadingAddress => '加载地址失败';

  @override
  String get integratedAddress => '集成地址';

  @override
  String get embedPaymentId => '在地址中嵌入支付ID';

  @override
  String get randomShort => '随机短码（16位）';

  @override
  String get randomLong => '随机长码（64位）';

  @override
  String get enterCustomPaymentId => '或输入自定义支付ID（16或64位十六进制）';

  @override
  String get enterPaymentId => '输入支付ID';

  @override
  String get paymentIdInvalid => '支付ID必须为16或64位十六进制字符';

  @override
  String get shortPid => '短支付ID';

  @override
  String get longPid => '长支付ID';

  @override
  String get share => '分享';

  @override
  String get copy => '复制';

  @override
  String get sweepAllFunds => '归集全部资金';

  @override
  String get normalSend => '普通转账';

  @override
  String get sweep => '归集';

  @override
  String get recipientAddress => '收款地址';

  @override
  String get scanQr => '扫描 QR';

  @override
  String get amount => '金额';

  @override
  String availableBalance(String amount) {
    return '可用：$amount';
  }

  @override
  String sweepInfo(String amount) {
    return '归集将合并所有UTXO，并发送您全部已解锁余额（$amount）减去手续费。';
  }

  @override
  String get paymentIdOptional => '支付ID（可选）';

  @override
  String get hexCharacters => '16或64位十六进制字符';

  @override
  String get mustBeHex => '必须为16或64位十六进制字符';

  @override
  String get recipientRequired => '收款地址不能为空';

  @override
  String get invalidAddress => '无效的 WRKZ 地址';

  @override
  String get enterValidAmount => '请输入有效金额';

  @override
  String get reviewTransaction => '确认交易';

  @override
  String get to => '收款方';

  @override
  String get fee => '手续费';

  @override
  String get totalDeducted => '合计扣除';

  @override
  String get paymentId => '支付ID';

  @override
  String get transactionsIrreversible => '交易不可撤销，请仔细核对详情。';

  @override
  String get back => '返回';

  @override
  String get confirmAndSend => '确认并发送';

  @override
  String get transactionSent => '交易已发送！';

  @override
  String get transactionHash => '交易哈希';

  @override
  String get sendAnother => '再次转账';

  @override
  String get scanQrCode => '扫描 QR 码';

  @override
  String get scannedAddress => '已扫描地址';

  @override
  String get cancel => '取消';

  @override
  String get useThisAddress => '使用此地址';

  @override
  String get sweepFailed => '归集失败';

  @override
  String get searchPlaceholder => '按哈希、地址、支付ID搜索...';

  @override
  String get all => '全部';

  @override
  String get filterReceived => '收款';

  @override
  String get filterSent => '转账';

  @override
  String get hash => '哈希';

  @override
  String get address => '地址';

  @override
  String get block => '区块';

  @override
  String get confirmed => '已确认';

  @override
  String get password => '密码';

  @override
  String get unlock => '解锁';

  @override
  String get switchWallet => '切换钱包';

  @override
  String get enterPasswordToUnlock => '输入密码以解锁';

  @override
  String get incorrectPassword => '密码错误';

  @override
  String get enterYourPassword => '输入您的密码';

  @override
  String get plutonMobile => 'PLUTON Mobile';

  @override
  String get createFirstWalletSubtitle => '创建您的第一个钱包以开始使用';

  @override
  String get selectWalletSubtitle => '选择一个钱包以打开';

  @override
  String get yourWallets => '您的钱包';

  @override
  String get noWalletsYet => '暂无钱包';

  @override
  String get lastOpened => '上次打开';

  @override
  String createdDate(String date) {
    return '创建于 $date';
  }

  @override
  String get createFirstWallet => '创建第一个钱包';

  @override
  String get addWallet => '添加钱包';

  @override
  String get deleteWallet => '删除钱包';

  @override
  String deleteWalletConfirm(String name) {
    return '删除“$name”？\n\n这将永久移除钱包文件和密钥，请确保已备份您的助记词。';
  }

  @override
  String get delete => '删除';

  @override
  String get createNewWallet => '创建新钱包';

  @override
  String get createNewWalletSubtitle => '生成一个带有全新助记词的钱包';

  @override
  String get importFromSeed => '从助记词导入';

  @override
  String get importFromSeedSubtitle => '使用25个单词的助记词恢复钱包';

  @override
  String get importFromKeys => '从私钥导入';

  @override
  String get importFromKeysSubtitle => '使用花费密钥和查看密钥恢复钱包';

  @override
  String get viewOnlyWallet => '仅查看钱包';

  @override
  String get viewOnlyWalletSubtitle => '使用查看密钥和地址的只读钱包';

  @override
  String get createWallet => '创建钱包';

  @override
  String get importWallet => '导入钱包';

  @override
  String get walletName => '钱包名称';

  @override
  String get walletNameHint => '例如：主钱包';

  @override
  String get passwordLabel => '密码';

  @override
  String get enterPassword => '输入密码';

  @override
  String get confirmPassword => '确认密码';

  @override
  String get seedPhrase => '助记词（25个单词）';

  @override
  String get enterSeedPhrase => '输入您的助记词...';

  @override
  String get scanHeight => '扫描高度（可选）';

  @override
  String get scanHeightHint => '0 = 从头开始扫描';

  @override
  String get privateSpendKey => '私有花费密钥';

  @override
  String get privateViewKey => '私有查看密钥';

  @override
  String get walletAddress => '钱包地址';

  @override
  String get walletAddressHint => 'Wrkz... 地址';

  @override
  String get hexKey => '64位十六进制';

  @override
  String get daemonNode => '守护节点';

  @override
  String get custom => '自定义';

  @override
  String get host => '主机';

  @override
  String get hostHint => '主机 / IP';

  @override
  String get port => '端口';

  @override
  String get ssl => 'SSL';

  @override
  String get walletNameRequired => '钱包名称不能为空';

  @override
  String get passwordRequired => '密码不能为空';

  @override
  String passwordTooShort(int count) {
    return '密码至少需要$count个字符';
  }

  @override
  String get passwordsDoNotMatch => '两次密码不一致';

  @override
  String get seedRequired => '助记词不能为空';

  @override
  String get spendKeyRequired => '花费密钥不能为空';

  @override
  String get viewKeyRequired => '查看密钥不能为空';

  @override
  String get addressRequired => '地址不能为空';

  @override
  String get daemonHostRequired => '守护进程主机不能为空';

  @override
  String get backupSeedTitle => '备份助记词';

  @override
  String get backupWarning => '请抄写您的助记词并妥善保管。如果丢失，您的资金将永久无法找回。';

  @override
  String get seedPhraseLabel => '助记词';

  @override
  String get privateViewKeyLabel => '私有查看密钥';

  @override
  String get privateSpendKeyLabel => '私有花费密钥';

  @override
  String get backupConfirmCheck => '我已安全备份助记词';

  @override
  String get continueToWallet => '继续进入钱包';

  @override
  String get sectionDaemonNode => '守护节点';

  @override
  String get apply => '应用';

  @override
  String nodeUpdated(String host, int port) {
    return '节点已更新为 $host:$port';
  }

  @override
  String get hostRequired => '主机不能为空';

  @override
  String currentWallet(String name) {
    return '当前钱包 — $name';
  }

  @override
  String get saveWallet => '保存钱包';

  @override
  String get walletSaved => '钱包已保存';

  @override
  String saveFailed(String error) {
    return '保存失败：$error';
  }

  @override
  String get backupSeed => '备份助记词';

  @override
  String get changePassword => '更改密码';

  @override
  String get resetScanHeight => '重置扫描高度';

  @override
  String get reset => '重置';

  @override
  String resetScanConfirm(int height) {
    return '这将从区块 $height 重新扫描区块链，可能需要一段时间。是否继续？';
  }

  @override
  String scanResetTo(int height) {
    return '扫描已重置到区块 $height';
  }

  @override
  String resetFailed(String error) {
    return '重置失败：$error';
  }

  @override
  String get enterPasswordTitle => '输入密码';

  @override
  String get confirm => '确认';

  @override
  String get seedBackup => '助记词备份';

  @override
  String get seedPhraseColon => '助记词：';

  @override
  String get privateViewKeyColon => '私有查看密钥：';

  @override
  String get iveBackedUp => '已完成备份';

  @override
  String get currentPasswordLabel => '当前密码';

  @override
  String get newPasswordLabel => '新密码';

  @override
  String get confirmNewPasswordLabel => '确认新密码';

  @override
  String get change => '更改';

  @override
  String get currentPasswordIncorrect => '当前密码不正确';

  @override
  String get newPasswordsDoNotMatch => '两次新密码不一致';

  @override
  String get passwordChanged => '密码已更改';

  @override
  String get walletManagement => '钱包管理';

  @override
  String get switchWalletSubtitle => '保存并关闭，选择其他钱包';

  @override
  String get manageWallets => '管理钱包';

  @override
  String get manageWalletsSubtitle => '重命名或删除钱包';

  @override
  String get currentlyOpen => '（当前已打开）';

  @override
  String get close => '关闭';

  @override
  String get renameWallet => '重命名钱包';

  @override
  String get newName => '新名称';

  @override
  String get rename => '重命名';

  @override
  String deleteWalletConfirmShort(String name) {
    return '删除“$name”？此操作无法撤销。';
  }

  @override
  String get security => '安全';

  @override
  String get biometricUnlock => '生物识别解锁';

  @override
  String get biometricSubtitle => '指纹 / 面容ID';

  @override
  String get biometricNotAvailable => '生物识别不可用';

  @override
  String get autoLock => '自动锁定';

  @override
  String get appearance => '外观';

  @override
  String get theme => '主题';

  @override
  String get themeAuto => '自动';

  @override
  String get themeLight => '浅色';

  @override
  String get themeDark => '深色';

  @override
  String get preferences => '偏好设置';

  @override
  String get transactionNotifications => '交易通知';

  @override
  String get notificationsSubtitle => '收到入账交易时提醒';

  @override
  String get autosave => '自动保存';

  @override
  String get autosaveSubtitle => '同步后保存，之后每5分钟保存一次';

  @override
  String get scanCoinbaseTx => '扫描Coinbase交易';

  @override
  String get scanCoinbaseSubtitle => '包括矿工奖励（默认关闭）';

  @override
  String get dangerZone => '危险区域';

  @override
  String get deleteCurrentWallet => '删除当前钱包';

  @override
  String get deleteCurrentWalletSubtitle => '永久删除钱包数据';

  @override
  String get deleteWalletTypeCaps =>
      '这将永久删除钱包文件和密钥，请确保已备份助记词。\n\n输入 DELETE 以确认：';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get language => '语言';

  @override
  String get selectLanguage => '选择语言';

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
  String get autoLockImmediately => '立即';

  @override
  String get autoLock1Min => '1分钟';

  @override
  String get autoLock5Min => '5分钟';

  @override
  String get autoLockNever => '从不';

  @override
  String get preparedTransactionExpired => '该交易已失效，请返回重新创建。';

  @override
  String get deleteConfirmMismatch => '请准确输入 DELETE 以确认。';

  @override
  String get seedNotBackedUpWarning => '您尚未确认备份该钱包的助记词。现在删除将无法找回资金。';

  @override
  String get wrkzReceived => '已收到 WRKZ';

  @override
  String get retry => '重试';

  @override
  String youReceivedAmount(String amount) {
    return '您收到了 $amount';
  }

  @override
  String get ringSize => '环签名大小';

  @override
  String ringSizeReduced(int actual, int normal) {
    return '环签名大小已降至 $actual（通常为 $normal）。链上没有足够的输出来为所发送的金额构成完整的环，因此此交易的隐私性低于平常。';
  }
}
