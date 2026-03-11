import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/widgets/pluton_logo.dart';

const _kVersion = '1.0.0';
const _kGithubUrl = 'https://github.com/wrkzcoin/wrkzcoin';
const _kDiscordUrl = 'https://discord.gg/wrkzcoin';
const _kTwitterUrl = 'https://twitter.com/wrkzcoin';
const _kWebsiteUrl = 'https://wrkz.work';

class AboutScreen extends StatelessWidget {
  const AboutScreen({super.key});

  Future<void> _open(String url) async {
    final uri = Uri.parse(url);
    if (await canLaunchUrl(uri)) await launchUrl(uri);
  }

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(28),
      child: Center(
        child: SizedBox(
          width: 500,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              const SizedBox(height: 24),
              const PlutonLogo(size: 56),
              const SizedBox(height: 16),
              Text('PLUTON v2', style: Theme.of(context).textTheme.headlineMedium),
              const SizedBox(height: 4),
              Text('Version $_kVersion — WRKZ Desktop Wallet',
                  style: Theme.of(context).textTheme.bodyMedium),
              const SizedBox(height: 32),

              // Description card
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(20),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('About', style: Theme.of(context).textTheme.titleSmall),
                      const SizedBox(height: 10),
                      const Text(
                        'PLUTON v2 is the official desktop wallet for WrkzCoin (WRKZ), '
                        'a fast and lightweight CryptoNote-based cryptocurrency.\n\n'
                        'Built with Flutter, powered by wallet-api.',
                        style: TextStyle(height: 1.6),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 20),

              // Links card
              Card(
                child: Column(
                  children: [
                    _LinkTile(
                      icon: Icons.code,
                      label: 'GitHub',
                      subtitle: 'View source code and releases',
                      url: _kGithubUrl,
                      onTap: () => _open(_kGithubUrl),
                    ),
                    const Divider(height: 1, indent: 56),
                    _LinkTile(
                      icon: Icons.chat_bubble_outline,
                      label: 'Discord',
                      subtitle: 'Join the community',
                      url: _kDiscordUrl,
                      onTap: () => _open(_kDiscordUrl),
                    ),
                    const Divider(height: 1, indent: 56),
                    _LinkTile(
                      icon: Icons.alternate_email,
                      label: 'Twitter / X',
                      subtitle: 'Follow @wrkzcoin',
                      url: _kTwitterUrl,
                      onTap: () => _open(_kTwitterUrl),
                    ),
                    const Divider(height: 1, indent: 56),
                    _LinkTile(
                      icon: Icons.language,
                      label: 'Website',
                      subtitle: 'wrkz.work',
                      url: _kWebsiteUrl,
                      onTap: () => _open(_kWebsiteUrl),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 20),

              // License / disclaimer
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('License', style: Theme.of(context).textTheme.titleSmall),
                      const SizedBox(height: 8),
                      const Text(
                        'Released under the MIT License.\n'
                        'Use at your own risk. Always back up your seed phrase.',
                        style: TextStyle(fontSize: 12, height: 1.5),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 32),
            ],
          ),
        ),
      ),
    );
  }
}

class _LinkTile extends StatelessWidget {
  final IconData icon;
  final String label;
  final String subtitle;
  final String url;
  final VoidCallback onTap;

  const _LinkTile({
    required this.icon,
    required this.label,
    required this.subtitle,
    required this.url,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return ListTile(
      leading: Icon(icon, size: 20, color: kTextSecondary),
      title: Text(label, style: const TextStyle(fontSize: 14)),
      subtitle: Text(subtitle, style: const TextStyle(fontSize: 12)),
      trailing: const Icon(Icons.open_in_new, size: 14, color: kTextSecondary),
      onTap: onTap,
    );
  }
}
