import 'package:flutter/material.dart';
import '../../core/api/models/wallet_status.dart';
import '../../l10n/generated/app_localizations.dart';
import '../theme/app_theme.dart';

/// Standing notice shown whenever the wallet is talking to a lite node.
///
/// A lite node answers a scan from its own start height whatever it is asked
/// for, so a wallet older than that node reads a balance that is simply too
/// low with nothing on screen to say why. This is that "why", and it stays up
/// for as long as the wallet is pointed at such a node — including when the
/// wallet is fully synced, which is exactly when the wrong number looks most
/// trustworthy. Renders nothing against a daemon holding the whole chain.
///
/// See LITENODE.md.
class LiteNodeBanner extends StatelessWidget {
  final WalletStatus status;

  /// Set on screens that already name the connected node, to drop the leading
  /// "Lite node" label and keep the line short.
  final bool compact;

  const LiteNodeBanner({super.key, required this.status, this.compact = false});

  @override
  Widget build(BuildContext context) {
    if (!status.isLiteNode && !status.isSyncStalledByLiteNode) {
      return const SizedBox.shrink();
    }

    final tr = S.of(context);
    final node = status.daemonLiteStartHeight;
    final walletStart = status.walletEarliestHeight ?? 0;

    final Color colour;
    final IconData icon;
    final String text;

    if (status.isSyncStalledByLiteNode) {
      // Scanned past a range this node cannot serve. Sync has stopped on
      // purpose; the balance is incomplete and stays that way here.
      colour = kError;
      icon = Icons.error_outline;
      text = tr?.liteNodeSyncStalled(status.walletBlockCount, node) ??
          'Sync stopped at block ${status.walletBlockCount}. This node holds '
              'nothing below block $node, so the blocks in between cannot be '
              'downloaded from it. The balance is incomplete until you connect '
              'a node holding the whole chain.';
    } else if (status.liteNodeMissesWalletHistory) {
      // The node starts above where this wallet does, so what is on screen
      // can be missing everything received in between.
      colour = kWarning;
      icon = Icons.warning_amber_rounded;
      text = tr?.liteNodeMissesHistory(node, walletStart) ??
          'This node starts at block $node, but this wallet starts at block '
              '$walletStart. Anything received in between is invisible here, so '
              'the balance shown may be too low. Connect a node holding the '
              'whole chain to see it.';
    } else {
      // A lite node that does cover this wallet. Nothing is wrong, but it is
      // worth saying what this node cannot answer for.
      colour = kAccent;
      icon = Icons.info_outline;
      text = tr?.liteNodeServesFrom(node) ??
          'This node only holds blocks from $node onward. Transactions before '
              'that block cannot be found through it.';
    }

    return Container(
      width: double.infinity,
      color: colour.withAlpha(25),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 7),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, size: 15, color: colour),
          const SizedBox(width: 8),
          if (!compact) ...[
            Text(
              tr?.liteNodeTitle ?? 'Lite node',
              style: TextStyle(
                  color: colour, fontSize: 12, fontWeight: FontWeight.w600),
            ),
            const SizedBox(width: 8),
          ],
          Expanded(
            child: Text(
              text,
              style: TextStyle(color: colour, fontSize: 12, height: 1.35),
            ),
          ),
        ],
      ),
    );
  }
}
