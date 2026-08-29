import 'dart:convert';
import 'dart:io';

import 'package:path_provider/path_provider.dart';

import '../config/app_config.dart';

// ── model ────────────────────────────────────────────────────────────────────

class WalletEntry {
  final String caption;
  final String filename;
  final DateTime createdAt;

  const WalletEntry({
    required this.caption,
    required this.filename,
    required this.createdAt,
  });

  Map<String, dynamic> toJson() => {
        'caption': caption,
        'filename': filename,
        'createdAt': createdAt.toIso8601String(),
      };

  factory WalletEntry.fromJson(Map<String, dynamic> json) => WalletEntry(
        caption: json['caption'] as String? ?? '',
        filename: json['filename'] as String? ?? '',
        createdAt: DateTime.tryParse(json['createdAt'] as String? ?? '') ??
            DateTime.now(),
      );
}

// ── registry ─────────────────────────────────────────────────────────────────

class WalletRegistry {
  List<WalletEntry> _wallets = [];
  String? _lastOpened;
  String? _walletsDir;

  List<WalletEntry> get wallets => List.unmodifiable(_wallets);
  String? get lastOpened => _lastOpened;

  /// Initializes the registry — call once at app start.
  Future<void> init() async {
    final docsDir = await getApplicationDocumentsDirectory();
    _walletsDir = '${docsDir.path}/${AppConfig.walletsSubdir}';
    final dir = Directory(_walletsDir!);
    if (!await dir.exists()) {
      await dir.create(recursive: true);
    }
    await _load();
  }

  String get walletsDir {
    if (_walletsDir == null) throw StateError('WalletRegistry not initialized');
    return _walletsDir!;
  }

  /// Full path to a wallet file.
  String getWalletPath(String filename) =>
      '$walletsDir/$filename${AppConfig.walletFileExtension}';

  /// Add a new wallet. Returns the created entry.
  Future<WalletEntry> addWallet(String caption) async {
    final filename = _sanitizeCaption(caption);
    final entry = WalletEntry(
      caption: caption,
      filename: filename,
      createdAt: DateTime.now(),
    );
    _wallets.add(entry);
    await _save();
    return entry;
  }

  /// Rename a wallet's caption.
  Future<void> renameWallet(String filename, String newCaption) async {
    final idx = _wallets.indexWhere((w) => w.filename == filename);
    if (idx == -1) return;
    _wallets[idx] = WalletEntry(
      caption: newCaption,
      filename: filename,
      createdAt: _wallets[idx].createdAt,
    );
    await _save();
  }

  /// Delete a wallet entry and its files from disk.
  Future<void> deleteWallet(String filename) async {
    _wallets.removeWhere((w) => w.filename == filename);
    if (_lastOpened == filename) _lastOpened = null;
    await _save();

    // Delete wallet files.
    final walletFile = File(getWalletPath(filename));
    final keysFile = File('${getWalletPath(filename)}.keys');
    if (await walletFile.exists()) await walletFile.delete();
    if (await keysFile.exists()) await keysFile.delete();
  }

  /// Set the last-opened wallet filename.
  Future<void> setLastOpened(String? filename) async {
    _lastOpened = filename;
    await _save();
  }

  /// Find entry by filename.
  WalletEntry? findByFilename(String filename) {
    try {
      return _wallets.firstWhere((w) => w.filename == filename);
    } catch (_) {
      return null;
    }
  }

  // ── private ──────────────────────────────────────────────────────────────

  File get _registryFile =>
      File('$walletsDir/${AppConfig.registryFilename}');

  Future<void> _load() async {
    final file = _registryFile;
    if (!await file.exists()) {
      _wallets = [];
      _lastOpened = null;
      await _adoptOrphanWalletFiles();
      return;
    }
    try {
      final json = jsonDecode(await file.readAsString()) as Map<String, dynamic>;
      _wallets = (json['wallets'] as List<dynamic>?)
              ?.map((e) => WalletEntry.fromJson(e as Map<String, dynamic>))
              .toList() ??
          [];
      _lastOpened = json['lastOpened'] as String?;
    } catch (_) {
      // A corrupt index must not look like "you have no wallets" — the wallet
      // files themselves are the real data. Preserve the bad file for
      // forensics, then rebuild the list from what is on disk.
      _wallets = [];
      _lastOpened = null;
      try {
        await file.rename('${file.path}.corrupt-'
            '${DateTime.now().millisecondsSinceEpoch}');
      } catch (_) {
        // Non-fatal: recovery below matters more than keeping the copy.
      }
    }
    await _adoptOrphanWalletFiles();
  }

  /// Adds registry entries for any `*.wallet` file in the wallets directory
  /// that the index does not know about.
  Future<void> _adoptOrphanWalletFiles() async {
    try {
      final dir = Directory(walletsDir);
      if (!await dir.exists()) return;
      final known = _wallets.map((w) => w.filename).toSet();
      var recovered = false;
      await for (final entity in dir.list(followLinks: false)) {
        if (entity is! File) continue;
        final name = entity.uri.pathSegments.last;
        if (!name.endsWith(AppConfig.walletFileExtension)) continue;
        final filename = name.substring(
            0, name.length - AppConfig.walletFileExtension.length);
        if (filename.isEmpty || known.contains(filename)) continue;
        final stat = await entity.stat();
        _wallets.add(WalletEntry(
          caption: filename,
          filename: filename,
          createdAt: stat.modified,
        ));
        known.add(filename);
        recovered = true;
      }
      if (recovered) await _save();
    } catch (_) {
      // Recovery is best effort; never let it stop the app from starting.
    }
  }

  Future<void> _save() async {
    final json = {
      'wallets': _wallets.map((w) => w.toJson()).toList(),
      'lastOpened': _lastOpened,
    };
    await _registryFile.writeAsString(
      const JsonEncoder.withIndent('  ').convert(json),
    );
  }

  /// Sanitize caption into a safe filename. Deduplicates if needed.
  String _sanitizeCaption(String caption) {
    var name = caption
        .toLowerCase()
        .trim()
        .replaceAll(RegExp(r'[^a-z0-9_\- ]'), '')
        .replaceAll(RegExp(r'\s+'), '_');
    if (name.isEmpty) name = 'wallet';

    // Deduplicate.
    var candidate = name;
    var suffix = 2;
    while (_wallets.any((w) => w.filename == candidate)) {
      candidate = '${name}_$suffix';
      suffix++;
    }
    return candidate;
  }
}
