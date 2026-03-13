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
      );

  /// Sync progress 0.0 → 1.0
  double get syncProgress {
    if (networkBlockCount == 0) return 0.0;
    return (walletBlockCount / networkBlockCount).clamp(0.0, 1.0);
  }
}
