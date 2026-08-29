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

  // Defensive reads throughout: a transient bridge failure yields an empty map,
  // and hard casts turned that into a TypeError that replaced the whole
  // overview with "type 'Null' is not a subtype of type 'num'".
  factory WalletStatus.fromJson(Map<String, dynamic> json) => WalletStatus(
        walletBlockCount: (json['walletBlockCount'] as num? ?? 0).toInt(),
        localDaemonBlockCount: (json['localDaemonBlockCount'] as num? ?? 0).toInt(),
        networkBlockCount: (json['networkBlockCount'] as num? ?? 0).toInt(),
        isDaemonSynced: json['isDaemonSynced'] as bool? ?? false,
        isWalletSynced: json['isWalletSynced'] as bool? ?? false,
        isOutOfSync: json['isOutOfSync'] as bool? ?? false,
        peerCount: (json['peerCount'] as num? ?? 0).toInt(),
        hashrate: (json['hashrate'] as num? ?? 0).toInt(),
        isViewWallet: json['isViewWallet'] as bool? ?? false,
        subWalletCount: (json['subWalletCount'] as num? ?? 0).toInt(),
      );

  /// Sync progress 0.0 -> 1.0
  double get syncProgress {
    if (networkBlockCount == 0) return 0.0;
    return (walletBlockCount / networkBlockCount).clamp(0.0, 1.0);
  }

  @override
  bool operator ==(Object other) =>
      other is WalletStatus &&
      other.walletBlockCount == walletBlockCount &&
      other.localDaemonBlockCount == localDaemonBlockCount &&
      other.networkBlockCount == networkBlockCount &&
      other.isDaemonSynced == isDaemonSynced &&
      other.isWalletSynced == isWalletSynced &&
      other.isOutOfSync == isOutOfSync &&
      other.peerCount == peerCount &&
      other.hashrate == hashrate &&
      other.isViewWallet == isViewWallet &&
      other.subWalletCount == subWalletCount;

  @override
  int get hashCode => Object.hash(
        walletBlockCount,
        localDaemonBlockCount,
        networkBlockCount,
        isDaemonSynced,
        isWalletSynced,
        isOutOfSync,
        peerCount,
        hashrate,
        isViewWallet,
        subWalletCount,
      );
}
