class WalletStatus {
  final int walletBlockCount;
  final int localDaemonBlockCount;
  final int networkBlockCount;
  final bool isDaemonSynced;
  final bool isWalletSynced;
  final bool isOutOfSync;
  final int peerCount;
  final int hashrate;
  final bool isViewWallet;
  final int subWalletCount;

  /// The ring sizes the network expects at the current height. Zero against a
  /// wallet_capi too old to report them, which every reader must treat as
  /// "unknown" rather than as a real limit.
  final int minMixin;
  final int maxMixin;
  final int defaultMixin;

  /// The lowest height the connected daemon holds anything for. Zero means it
  /// holds the whole chain. Non-zero means it is a lite node: nothing below
  /// this height can be found through it however far back a scan is started,
  /// so a wallet older than this shows an incomplete balance. See LITENODE.md.
  final int daemonLiteStartHeight;

  /// Sync has deliberately stopped because this wallet has already scanned
  /// past a height the daemon cannot serve from. The balance is incomplete
  /// and stays that way until a daemon holding the range is connected.
  final bool isSyncStalledByLiteNode;

  /// The lowest height this wallet was ever told to scan from — where its
  /// funds can start, not where it has got to. Zero when the wallet was
  /// created from a timestamp instead; then [walletSyncStartTimestamp] holds
  /// it and no height comparison is possible.
  final int walletSyncStartHeight;
  final int walletSyncStartTimestamp;

  const WalletStatus({
    required this.walletBlockCount,
    required this.localDaemonBlockCount,
    required this.networkBlockCount,
    required this.isDaemonSynced,
    required this.isWalletSynced,
    required this.isOutOfSync,
    required this.peerCount,
    required this.hashrate,
    required this.isViewWallet,
    required this.subWalletCount,
    this.minMixin = 0,
    this.maxMixin = 0,
    this.defaultMixin = 0,
    this.daemonLiteStartHeight = 0,
    this.isSyncStalledByLiteNode = false,
    this.walletSyncStartHeight = 0,
    this.walletSyncStartTimestamp = 0,
  });

  factory WalletStatus.fromJson(Map<String, dynamic> json) => WalletStatus(
        walletBlockCount: (json['walletBlockCount'] as num).toInt(),
        localDaemonBlockCount: (json['localDaemonBlockCount'] as num).toInt(),
        networkBlockCount: (json['networkBlockCount'] as num).toInt(),
        isDaemonSynced: json['isDaemonSynced'] as bool,
        isWalletSynced: json['isWalletSynced'] as bool,
        isOutOfSync: json['isOutOfSync'] as bool,
        peerCount: (json['peerCount'] as num).toInt(),
        hashrate: (json['hashrate'] as num).toInt(),
        isViewWallet: json['isViewWallet'] as bool,
        subWalletCount: (json['subWalletCount'] as num).toInt(),
        minMixin: (json['minMixin'] as num?)?.toInt() ?? 0,
        maxMixin: (json['maxMixin'] as num?)?.toInt() ?? 0,
        defaultMixin: (json['defaultMixin'] as num?)?.toInt() ?? 0,
        daemonLiteStartHeight:
            (json['daemonLiteStartHeight'] as num?)?.toInt() ?? 0,
        isSyncStalledByLiteNode:
            json['isSyncStalledByLiteNode'] as bool? ?? false,
        walletSyncStartHeight:
            (json['walletSyncStartHeight'] as num?)?.toInt() ?? 0,
        walletSyncStartTimestamp:
            (json['walletSyncStartTimestamp'] as num?)?.toInt() ?? 0,
      );

  /// Sync progress 0.0 → 1.0
  double get syncProgress {
    if (networkBlockCount == 0) return 0.0;
    return (walletBlockCount / networkBlockCount).clamp(0.0, 1.0);
  }

  /// The connected daemon is a lite node and holds no block data below
  /// [daemonLiteStartHeight].
  bool get isLiteNode => daemonLiteStartHeight > 0;

  /// The earliest block this wallet could hold funds in.
  ///
  /// Null when the wallet was created from a timestamp that has not been
  /// resolved to a height yet — nothing can be compared against a lite node's
  /// start height in that case, so callers must not guess.
  int? get walletEarliestHeight =>
      (walletSyncStartHeight == 0 && walletSyncStartTimestamp > 0)
          ? null
          : walletSyncStartHeight;

  /// The connected lite node starts above the wallet's own start height, so
  /// transactions between the two are invisible and the balance reads low.
  bool get liteNodeMissesWalletHistory {
    final earliest = walletEarliestHeight;
    return isLiteNode &&
        earliest != null &&
        daemonLiteStartHeight > earliest;
  }
}
