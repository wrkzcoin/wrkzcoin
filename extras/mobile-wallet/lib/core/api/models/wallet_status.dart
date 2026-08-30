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
  });

  double get syncProgress {
    if (networkBlockCount == 0) return 0.0;
    return (walletBlockCount / networkBlockCount).clamp(0.0, 1.0);
  }

  factory WalletStatus.fromJson(Map<String, dynamic> json) => WalletStatus(
        walletBlockCount:
            (json['walletBlockCount'] as num?)?.toInt() ?? 0,
        localDaemonBlockCount:
            (json['localDaemonBlockCount'] as num?)?.toInt() ?? 0,
        networkBlockCount:
            (json['networkBlockCount'] as num?)?.toInt() ?? 0,
        isDaemonSynced: json['isDaemonSynced'] as bool? ?? false,
        isWalletSynced: json['isWalletSynced'] as bool? ?? false,
        isOutOfSync: json['isOutOfSync'] as bool? ?? true,
        peerCount: (json['peerCount'] as num?)?.toInt() ?? 0,
        hashrate: (json['hashrate'] as num?)?.toInt() ?? 0,
        isViewWallet: json['isViewWallet'] as bool? ?? false,
        subWalletCount:
            (json['subWalletCount'] as num?)?.toInt() ?? 1,
        minMixin: (json['minMixin'] as num?)?.toInt() ?? 0,
        maxMixin: (json['maxMixin'] as num?)?.toInt() ?? 0,
        defaultMixin: (json['defaultMixin'] as num?)?.toInt() ?? 0,
      );
}
