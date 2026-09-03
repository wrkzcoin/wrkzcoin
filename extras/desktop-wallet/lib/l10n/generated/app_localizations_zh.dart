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
  String get browse => '浏览…';

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
    return '版本 $version — WRKZ 桌面钱包';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 是 WrkzCoin (WRKZ) 的官方桌面钱包，WrkzCoin 是一种基于 CryptoNote 的快速轻量加密货币。\n\n使用 Flutter 构建，由 wallet-api 驱动。';

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
  String get preparedTransactionExpired => '该交易已失效，请返回重新创建。';

  @override
  String get deleteConfirmMismatch => '请准确输入 DELETE 以确认。';

  @override
  String get unlockNeedsReopen => '无法在此设备上验证密码。请使用“改为关闭钱包”，然后重新打开钱包。';

  @override
  String get exportJsonWarningTitle => '导出未加密的钱包？';

  @override
  String get exportJsonWarningBody =>
      '导出的文件以明文包含您的私有查看密钥和私有花费密钥。任何读取该文件的人都能动用您的资金。\n\n请仅保存到您掌控的存储中，并在使用完毕后立即删除。';

  @override
  String passwordTooShort(int count) {
    return '密码至少需要$count个字符';
  }

  @override
  String get ringSize => '环签名大小';

  @override
  String ringSizeReduced(int actual, int normal) {
    return '环签名大小已降至 $actual（通常为 $normal）。链上没有足够的输出来为所发送的金额构成完整的环，因此此交易的隐私性低于平常。';
  }

  @override
  String get liteNodeTitle => '轻节点';

  @override
  String liteNodeServesFrom(int height) {
    return '此节点仅保存从 $height 起的区块。该区块之前的交易无法通过它找到。';
  }

  @override
  String liteNodeMissesHistory(int nodeHeight, int walletHeight) {
    return '此节点从区块 $nodeHeight 开始，而此钱包从区块 $walletHeight 开始。两者之间收到的款项在这里不可见，因此显示的余额可能偏低。请连接保存完整链的节点以查看。';
  }

  @override
  String liteNodeSyncStalled(int wallet, int node) {
    return '同步已在区块 $wallet 停止。此节点不保存区块 $node 以下的任何数据，因此无法从它下载中间的区块。在连接保存完整链的节点之前，余额并不完整。';
  }

  @override
  String get liteNodeRescanRefusedTitle => '此节点无法回溯到那么早重新扫描';

  @override
  String liteNodeRescanRefused(int height) {
    return '当前连接的是轻节点，不保存 $height 以下的区块数据。从更低处重新扫描会丢弃钱包已找到的交易，且在此无法再次找回。未做任何更改。';
  }

  @override
  String liteNodeRescanFromInstead(int height) {
    return '改为从 $height 重新扫描';
  }

  @override
  String liteNodeRescanHint(int height) {
    return '当前连接的节点只能从区块 $height 或更高处重新扫描。';
  }

  @override
  String get nodeServesFromLabel => '提供区块起始于';

  @override
  String get nodeFullChain => '完整链';

  @override
  String get sectionLocalNode => '本地轻节点';

  @override
  String get localNodeDescription =>
      '在这台电脑上运行节点并与它同步，而不是使用远程服务器。轻节点只保存钱包需要的数据，但仍需完整下载一次全链。';

  @override
  String get localNodeSetUp => '设置本地节点';

  @override
  String get localNodeStart => '启动';

  @override
  String get localNodeStop => '停止';

  @override
  String get localNodeUse => '使用此节点';

  @override
  String get localNodeDelete => '删除节点数据';

  @override
  String get localNodeStateStopped => '已停止';

  @override
  String get localNodeStateStarting => '启动中';

  @override
  String get localNodeStateSyncing => '同步中';

  @override
  String get localNodeStateReady => '已同步';

  @override
  String get localNodeStateFailed => '失败';

  @override
  String localNodeProgress(int height, int network) {
    return '区块 $height / $network';
  }

  @override
  String localNodePeers(int count) {
    return '$count 个节点连接';
  }

  @override
  String get localNodeNotReadyYet =>
      '本地节点仍在追赶进度，尚不能为钱包提供服务。它会在后台继续同步——请先使用远程节点，就绪后再切换。';

  @override
  String get localNodeInUse => '钱包已连接到此节点。';

  @override
  String localNodeBinaryMissing(String name) {
    return '未找到 $name。请将守护进程可执行文件放在钱包程序旁边，或放入旁边的“sidecar”文件夹，然后重试。';
  }

  @override
  String get localNodeSetupTitle => '设置本地轻节点';

  @override
  String get localNodeSetupCost =>
      '开始之前请注意：\n• 需要约 6 GB 磁盘空间，并会完整下载一次全链。\n• 首次同步需要数小时，会在后台继续，其间你仍可使用远程节点。\n• 下方的起始高度不可更改。以后要改，只能删除节点并从头重新同步。';

  @override
  String get localNodeStartHeightLabel => '起始高度';

  @override
  String localNodeStartHeightHelp(int height) {
    return '低于此高度的区块仍会下载并校验，但只保留后续区块所需的索引。请设为不高于此钱包自身的起始高度（$height）。';
  }

  @override
  String localNodeStartHeightTooHigh(int height) {
    return '高于此钱包的起始高度（$height）。该节点将永远无法显示此钱包更早的交易。';
  }

  @override
  String get localNodeCreate => '创建节点';

  @override
  String get localNodeDeleteTitle => '删除本地节点数据？';

  @override
  String get localNodeDeleteWarning =>
      '这会停止节点并从磁盘上永久删除其区块链数据库。你的钱包、助记词和资金不受影响，但新的本地节点需要从零重新同步，耗时数小时。';

  @override
  String get localNodeDeleted => '已删除本地节点。';

  @override
  String localNodeDiskUsage(String size) {
    return '占用磁盘 $size';
  }

  @override
  String get nodePresetRemote => '远程节点';

  @override
  String get nodePresetLocal => '本地轻节点';

  @override
  String get savingWallet => '正在保存钱包…';

  @override
  String get savingWalletBody => 'PLUTON 正在将钱包写入磁盘。钱包较大时可能需要一点时间。';

  @override
  String get shutdownTakingLong =>
      '耗时超出预期。此时退出会丢失本次保存——磁盘上的钱包文件保持原样，不会损坏，但上次保存之后的改动会丢失。';

  @override
  String get quitAnyway => '仍要退出';

  @override
  String get stillRunningInTray => 'PLUTON 仍在系统托盘中运行。点击托盘图标可重新打开，或右键点击并选择“退出”。';

  @override
  String get localNodeDataFolder => '数据文件夹';

  @override
  String get localNodeDataFolderHelp => '这里将写入约 6 GB。请选择空间充足的磁盘。';

  @override
  String get localNodeDataFolderInUse => '该文件夹已有其他文件。请选择空文件夹或新建一个。';

  @override
  String get localNodeStartHeightRequired =>
      '请输入从哪个高度开始保留完整区块。必须大于零——轻节点无法从创世区块开始。';

  @override
  String get nodeExitTitle => '本地节点仍在运行';

  @override
  String get nodeExitBodySyncing =>
      '它的首次同步尚未完成。这需要数小时，且只有节点运行时才会推进——但保持运行的节点在 PLUTON 关闭后仍会占用磁盘和网络。';

  @override
  String get nodeExitBodySynced =>
      '它已与网络同步。保持运行可以维持这一状态，但在 PLUTON 关闭期间仍会占用 CPU、带宽和磁盘；停止它则下次需要短暂追赶。';

  @override
  String get nodeExitKeep => '保持运行';

  @override
  String get nodeExitStop => '停止';

  @override
  String get nodeExitChangeLater => '以后可在设置的“本地轻节点”中更改。';

  @override
  String get rememberMyChoice => '记住我的选择';

  @override
  String get shutdownStoppingNode => '正在停止本地节点…';

  @override
  String get shutdownStoppingNodeBody => '让它写完数据库，下次启动就不用重放预写日志。';

  @override
  String get nodeExitPolicyLabel => '关闭钱包时';

  @override
  String get nodeExitPolicyAsk => '询问我';

  @override
  String get nodeExitPolicyKeep => '保持节点运行';

  @override
  String get nodeExitPolicyStop => '停止节点';

  @override
  String get localNodeStillRunningBody => '本地轻节点仍在后台运行。打开 PLUTON 可以停止它。';

  @override
  String get syncStoppedTitle => '同步已停止';

  @override
  String syncGapStalled(int covered, int servesFrom) {
    return '同步在区块 $covered 停止。所连接的节点只从区块 $servesFrom 开始响应，因此无法从它下载中间的区块。在您连接一个保存完整链的节点之前，余额都是不完整的。';
  }

  @override
  String get localNodeNotReadyTitle => '该节点尚未就绪';

  @override
  String localNodeNotReadyBody(int behind) {
    return '本地节点仍落后网络 $behind 个区块。现在将钱包指向它，同步会停止直到它追上，余额会缺少它尚未到达的部分——而且屏幕上没有任何说明。继续使用远程节点没有任何损失；无论如何节点都在继续同步。';
  }

  @override
  String get switchAnyway => '仍要切换';

  @override
  String get switchToRemoteNode => '切换到远程节点';

  @override
  String get nodeWillServeFromLabel => '将从此高度提供区块';

  @override
  String get txPowServerSection => '交易 PoW 服务器';

  @override
  String get txPowServerUse => '使用外部 PoW 服务器';

  @override
  String get txPowServerSubtitle =>
      '将交易的工作量证明交由服务器计算，而不是在本设备上计算。如果服务器无响应，将使用本设备的 CPU。';

  @override
  String get txPowServerSaved => 'PoW 服务器设置已保存';

  @override
  String get txPowServerInvalid => '请输入有效的主机和端口';
}
