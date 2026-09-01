// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for German (`de`).
class SDe extends S {
  SDe([String locale = 'de']) : super(locale);

  @override
  String get appTitle => 'PLUTON Mobile';

  @override
  String get tabOverview => 'Übersicht';

  @override
  String get tabReceive => 'Empfangen';

  @override
  String get tabSend => 'Senden';

  @override
  String get tabHistory => 'Verlauf';

  @override
  String get tabSettings => 'Einstellungen';

  @override
  String get send => 'Senden';

  @override
  String get receive => 'Empfangen';

  @override
  String get available => 'Verfügbar';

  @override
  String get locked => 'Gesperrt';

  @override
  String get total => 'Gesamt';

  @override
  String lockedAmount(String amount) {
    return 'Gesperrt: $amount';
  }

  @override
  String totalAmount(String amount) {
    return 'Gesamt: $amount';
  }

  @override
  String get recentTransactions => 'Letzte Transaktionen';

  @override
  String get viewAll => 'Alle anzeigen';

  @override
  String get noTransactionsYet => 'Noch keine Transaktionen';

  @override
  String get noMatchingTransactions => 'Keine passenden Transaktionen';

  @override
  String get pending => 'Ausstehend...';

  @override
  String get justNow => 'Gerade eben';

  @override
  String minutesAgo(int count) {
    return 'Vor ${count}m';
  }

  @override
  String hoursAgo(int count) {
    return 'Vor ${count}h';
  }

  @override
  String daysAgo(int count) {
    return 'Vor ${count}T';
  }

  @override
  String get received => 'Empfangen';

  @override
  String get sent => 'Gesendet';

  @override
  String get networkStatus => 'Netzwerkstatus';

  @override
  String get node => 'Node';

  @override
  String get status => 'Status';

  @override
  String get connected => 'Verbunden';

  @override
  String get disconnected => 'Getrennt';

  @override
  String get walletHeight => 'Wallet-Höhe';

  @override
  String get networkHeight => 'Netzwerkhöhe';

  @override
  String get peers => 'Peers';

  @override
  String get type => 'Typ';

  @override
  String get viewOnly => 'Nur anzeigen';

  @override
  String get couldNotFetchStatus =>
      'Status konnte nicht abgerufen werden. Überprüfen Sie Ihren Node in den Einstellungen.';

  @override
  String errorPrefix(String message) {
    return 'Fehler: $message';
  }

  @override
  String get seedBackupWarning =>
      'Sichern Sie Ihre Seed-Phrase in den Einstellungen, um Ihr Guthaben zu schützen.';

  @override
  String get noConnectionToDaemon => 'Keine Verbindung zum Daemon';

  @override
  String syncingPercent(String percent) {
    return 'Synchronisierung $percent%';
  }

  @override
  String get yourAddress => 'Ihre Adresse';

  @override
  String get errorLoadingAddress => 'Fehler beim Laden der Adresse';

  @override
  String get integratedAddress => 'Integrierte Adresse';

  @override
  String get embedPaymentId => 'Eine Zahlungs-ID in Ihre Adresse einbetten';

  @override
  String get randomShort => 'Zufällig Kurz (16)';

  @override
  String get randomLong => 'Zufällig Lang (64)';

  @override
  String get enterCustomPaymentId =>
      'Oder benutzerdefinierte Zahlungs-ID eingeben (16 oder 64 Hex)';

  @override
  String get enterPaymentId => 'Zahlungs-ID eingeben';

  @override
  String get paymentIdInvalid =>
      'Die Zahlungs-ID muss 16 oder 64 Hex-Zeichen lang sein';

  @override
  String get shortPid => 'Kurze PID';

  @override
  String get longPid => 'Lange PID';

  @override
  String get share => 'Teilen';

  @override
  String get copy => 'Kopieren';

  @override
  String get sweepAllFunds => 'Alle Guthaben zusammenführen';

  @override
  String get normalSend => 'Normales Senden';

  @override
  String get sweep => 'Zusammenführen';

  @override
  String get recipientAddress => 'Empfängeradresse';

  @override
  String get scanQr => 'QR scannen';

  @override
  String get amount => 'Betrag';

  @override
  String availableBalance(String amount) {
    return 'Verfügbar: $amount';
  }

  @override
  String sweepInfo(String amount) {
    return 'Zusammenführen konsolidiert alle UTXOs und sendet Ihr gesamtes freigegebenes Guthaben ($amount) abzüglich Gebühren.';
  }

  @override
  String get paymentIdOptional => 'Zahlungs-ID (optional)';

  @override
  String get hexCharacters => '16 oder 64 Hex-Zeichen';

  @override
  String get mustBeHex => 'Muss 16 oder 64 Hex-Zeichen sein';

  @override
  String get recipientRequired => 'Empfängeradresse ist erforderlich';

  @override
  String get invalidAddress => 'Ungültige WRKZ-Adresse';

  @override
  String get enterValidAmount => 'Geben Sie einen gültigen Betrag ein';

  @override
  String get reviewTransaction => 'Transaktion prüfen';

  @override
  String get to => 'An';

  @override
  String get fee => 'Gebühr';

  @override
  String get totalDeducted => 'Gesamt abgezogen';

  @override
  String get paymentId => 'Zahlungs-ID';

  @override
  String get transactionsIrreversible =>
      'Transaktionen sind unwiderruflich. Bitte überprüfen Sie die Details.';

  @override
  String get back => 'Zurück';

  @override
  String get confirmAndSend => 'Bestätigen & Senden';

  @override
  String get transactionSent => 'Transaktion gesendet!';

  @override
  String get transactionHash => 'Transaktions-Hash';

  @override
  String get sendAnother => 'Weitere senden';

  @override
  String get scanQrCode => 'QR-Code scannen';

  @override
  String get scannedAddress => 'Gescannte Adresse';

  @override
  String get cancel => 'Abbrechen';

  @override
  String get useThisAddress => 'Diese Adresse verwenden';

  @override
  String get sweepFailed => 'Zusammenführen fehlgeschlagen';

  @override
  String get searchPlaceholder => 'Nach Hash, Adresse, Zahlungs-ID suchen...';

  @override
  String get all => 'Alle';

  @override
  String get filterReceived => 'Empfangen';

  @override
  String get filterSent => 'Gesendet';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Adresse';

  @override
  String get block => 'Block';

  @override
  String get confirmed => 'Bestätigt';

  @override
  String get password => 'Passwort';

  @override
  String get unlock => 'Entsperren';

  @override
  String get switchWallet => 'Wallet wechseln';

  @override
  String get enterPasswordToUnlock =>
      'Geben Sie Ihr Passwort zum Entsperren ein';

  @override
  String get incorrectPassword => 'Falsches Passwort';

  @override
  String get enterYourPassword => 'Geben Sie Ihr Passwort ein';

  @override
  String get plutonMobile => 'PLUTON Mobile';

  @override
  String get createFirstWalletSubtitle =>
      'Erstellen Sie Ihre erste Wallet, um zu beginnen';

  @override
  String get selectWalletSubtitle => 'Wählen Sie eine Wallet zum Öffnen';

  @override
  String get yourWallets => 'Ihre Wallets';

  @override
  String get noWalletsYet => 'Noch keine Wallets';

  @override
  String get lastOpened => 'Zuletzt geöffnet';

  @override
  String createdDate(String date) {
    return 'Erstellt $date';
  }

  @override
  String get createFirstWallet => 'Erste Wallet erstellen';

  @override
  String get addWallet => 'Wallet hinzufügen';

  @override
  String get deleteWallet => 'Wallet löschen';

  @override
  String deleteWalletConfirm(String name) {
    return '\"$name\" löschen?\n\nDies entfernt die Wallet-Datei und die Schlüssel dauerhaft. Stellen Sie sicher, dass Sie Ihre Seed-Phrase gesichert haben.';
  }

  @override
  String get delete => 'Löschen';

  @override
  String get createNewWallet => 'Neue Wallet erstellen';

  @override
  String get createNewWalletSubtitle =>
      'Eine neue Wallet mit einer frischen Seed-Phrase generieren';

  @override
  String get importFromSeed => 'Aus Seed-Phrase importieren';

  @override
  String get importFromSeedSubtitle =>
      'Wallet mithilfe Ihrer 25-Wort-Mnemonik wiederherstellen';

  @override
  String get importFromKeys => 'Aus privaten Schlüsseln importieren';

  @override
  String get importFromKeysSubtitle =>
      'Mit Spend-Key und View-Key wiederherstellen';

  @override
  String get viewOnlyWallet => 'Nur-Lese-Wallet';

  @override
  String get viewOnlyWalletSubtitle =>
      'Beobachtungs-Wallet mit View-Key und Adresse';

  @override
  String get createWallet => 'Wallet erstellen';

  @override
  String get importWallet => 'Wallet importieren';

  @override
  String get walletName => 'Wallet-Name';

  @override
  String get walletNameHint => 'z. B. Haupt-Wallet';

  @override
  String get passwordLabel => 'Passwort';

  @override
  String get enterPassword => 'Passwort eingeben';

  @override
  String get confirmPassword => 'Passwort bestätigen';

  @override
  String get seedPhrase => 'Seed-Phrase (25 Wörter)';

  @override
  String get enterSeedPhrase => 'Geben Sie Ihre Seed-Phrase ein...';

  @override
  String get scanHeight => 'Scan-Höhe (optional)';

  @override
  String get scanHeightHint => '0 = vom Anfang scannen';

  @override
  String get privateSpendKey => 'Privater Spend-Key';

  @override
  String get privateViewKey => 'Privater View-Key';

  @override
  String get walletAddress => 'Wallet-Adresse';

  @override
  String get walletAddressHint => 'Wrkz... Adresse';

  @override
  String get hexKey => '64-stelliges Hex';

  @override
  String get daemonNode => 'Daemon-Node';

  @override
  String get custom => 'Benutzerdefiniert';

  @override
  String get host => 'Host';

  @override
  String get hostHint => 'Host / IP';

  @override
  String get port => 'Port';

  @override
  String get ssl => 'SSL';

  @override
  String get walletNameRequired => 'Wallet-Name ist erforderlich';

  @override
  String get passwordRequired => 'Passwort ist erforderlich';

  @override
  String passwordTooShort(int count) {
    return 'Das Passwort muss mindestens $count Zeichen lang sein';
  }

  @override
  String get passwordsDoNotMatch => 'Passwörter stimmen nicht überein';

  @override
  String get seedRequired => 'Seed-Phrase ist erforderlich';

  @override
  String get spendKeyRequired => 'Spend-Key ist erforderlich';

  @override
  String get viewKeyRequired => 'View-Key ist erforderlich';

  @override
  String get addressRequired => 'Adresse ist erforderlich';

  @override
  String get daemonHostRequired => 'Daemon-Host ist erforderlich';

  @override
  String get backupSeedTitle => 'Seed sichern';

  @override
  String get backupWarning =>
      'Schreiben Sie Ihre Seed-Phrase auf und bewahren Sie sie sicher auf. Wenn Sie sie verlieren, ist Ihr Guthaben für immer verloren.';

  @override
  String get seedPhraseLabel => 'Seed-Phrase';

  @override
  String get privateViewKeyLabel => 'Privater View-Key';

  @override
  String get privateSpendKeyLabel => 'Privater Spend-Key';

  @override
  String get backupConfirmCheck =>
      'Ich habe meine Seed-Phrase sicher gesichert';

  @override
  String get continueToWallet => 'Zur Wallet fortfahren';

  @override
  String get sectionDaemonNode => 'Daemon-Node';

  @override
  String get apply => 'Anwenden';

  @override
  String nodeUpdated(String host, int port) {
    return 'Node aktualisiert auf $host:$port';
  }

  @override
  String get hostRequired => 'Host ist erforderlich';

  @override
  String currentWallet(String name) {
    return 'Aktuelle Wallet — $name';
  }

  @override
  String get saveWallet => 'Wallet speichern';

  @override
  String get walletSaved => 'Wallet gespeichert';

  @override
  String saveFailed(String error) {
    return 'Speichern fehlgeschlagen: $error';
  }

  @override
  String get backupSeed => 'Seed sichern';

  @override
  String get changePassword => 'Passwort ändern';

  @override
  String get resetScanHeight => 'Scan-Höhe zurücksetzen';

  @override
  String get reset => 'Zurücksetzen';

  @override
  String resetScanConfirm(int height) {
    return 'Dies scannt die Blockchain ab Block $height neu. Dies kann eine Weile dauern. Fortfahren?';
  }

  @override
  String scanResetTo(int height) {
    return 'Scan auf Block $height zurückgesetzt';
  }

  @override
  String resetFailed(String error) {
    return 'Zurücksetzen fehlgeschlagen: $error';
  }

  @override
  String get enterPasswordTitle => 'Passwort eingeben';

  @override
  String get confirm => 'Bestätigen';

  @override
  String get seedBackup => 'Seed-Sicherung';

  @override
  String get seedPhraseColon => 'Seed-Phrase:';

  @override
  String get privateViewKeyColon => 'Privater View-Key:';

  @override
  String get iveBackedUp => 'Ich habe gesichert';

  @override
  String get currentPasswordLabel => 'Aktuelles Passwort';

  @override
  String get newPasswordLabel => 'Neues Passwort';

  @override
  String get confirmNewPasswordLabel => 'Neues Passwort bestätigen';

  @override
  String get change => 'Ändern';

  @override
  String get currentPasswordIncorrect => 'Aktuelles Passwort ist falsch';

  @override
  String get newPasswordsDoNotMatch => 'Neue Passwörter stimmen nicht überein';

  @override
  String get passwordChanged => 'Passwort geändert';

  @override
  String get walletManagement => 'Wallet-Verwaltung';

  @override
  String get switchWalletSubtitle => 'Speichern & schließen, andere auswählen';

  @override
  String get manageWallets => 'Wallets verwalten';

  @override
  String get manageWalletsSubtitle => 'Wallets umbenennen oder löschen';

  @override
  String get currentlyOpen => '(aktuell geöffnet)';

  @override
  String get close => 'Schließen';

  @override
  String get renameWallet => 'Wallet umbenennen';

  @override
  String get newName => 'Neuer Name';

  @override
  String get rename => 'Umbenennen';

  @override
  String deleteWalletConfirmShort(String name) {
    return '\"$name\" löschen? Dies kann nicht rückgängig gemacht werden.';
  }

  @override
  String get security => 'Sicherheit';

  @override
  String get biometricUnlock => 'Biometrisches Entsperren';

  @override
  String get biometricSubtitle => 'Fingerabdruck / Gesichtserkennung';

  @override
  String get biometricNotAvailable => 'Biometrie nicht verfügbar';

  @override
  String get autoLock => 'Automatische Sperre';

  @override
  String get appearance => 'Erscheinungsbild';

  @override
  String get theme => 'Design';

  @override
  String get themeAuto => 'Automatisch';

  @override
  String get themeLight => 'Hell';

  @override
  String get themeDark => 'Dunkel';

  @override
  String get preferences => 'Einstellungen';

  @override
  String get transactionNotifications => 'Transaktionsbenachrichtigungen';

  @override
  String get notificationsSubtitle =>
      'Bei eingehenden Transaktionen benachrichtigen';

  @override
  String get autosave => 'Automatisches Speichern';

  @override
  String get autosaveSubtitle =>
      'Nach Synchronisierung speichern, dann alle 5 Minuten';

  @override
  String get scanCoinbaseTx => 'Coinbase-Transaktionen scannen';

  @override
  String get scanCoinbaseSubtitle =>
      'Miner-Belohnungen einschließen (standardmäßig deaktiviert)';

  @override
  String get dangerZone => 'Gefahrenzone';

  @override
  String get deleteCurrentWallet => 'Aktuelle Wallet löschen';

  @override
  String get deleteCurrentWalletSubtitle => 'Wallet-Daten dauerhaft entfernen';

  @override
  String get deleteWalletTypeCaps =>
      'Dies löscht die Wallet-Datei und die Schlüssel dauerhaft. Stellen Sie sicher, dass Sie Ihre Seed-Phrase gesichert haben.\n\nGeben Sie DELETE zur Bestätigung ein:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get language => 'Sprache';

  @override
  String get selectLanguage => 'Sprache auswählen';

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
  String get autoLockImmediately => 'Sofort';

  @override
  String get autoLock1Min => '1 Minute';

  @override
  String get autoLock5Min => '5 Minuten';

  @override
  String get autoLockNever => 'Nie';

  @override
  String get preparedTransactionExpired =>
      'Diese Transaktion ist nicht mehr gültig. Gehen Sie zurück und erstellen Sie sie erneut.';

  @override
  String get deleteConfirmMismatch =>
      'Geben Sie genau DELETE ein, um zu bestätigen.';

  @override
  String get seedNotBackedUpWarning =>
      'Sie haben keine Sicherung der Seed-Phrase dieser Wallet bestätigt. Wenn Sie sie jetzt löschen, sind die Mittel unwiederbringlich.';

  @override
  String get wrkzReceived => 'WRKZ erhalten';

  @override
  String get retry => 'Erneut versuchen';

  @override
  String youReceivedAmount(String amount) {
    return 'Sie haben $amount erhalten';
  }

  @override
  String get ringSize => 'Ringgröße';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Ringgröße auf $actual reduziert (normalerweise $normal). Für die gesendeten Beträge gibt es in der Blockchain nicht genug Ausgänge für einen vollständigen Ring, daher ist diese Transaktion weniger privat als üblich.';
  }

  @override
  String get liteNodeTitle => 'Lite-Node';

  @override
  String liteNodeServesFrom(int height) {
    return 'Dieser Node hält nur Blöcke ab $height. Transaktionen vor diesem Block sind über ihn nicht auffindbar.';
  }

  @override
  String liteNodeMissesHistory(int nodeHeight, int walletHeight) {
    return 'Dieser Node beginnt bei Block $nodeHeight, diese Wallet aber bei Block $walletHeight. Alles, was dazwischen empfangen wurde, ist hier unsichtbar, der angezeigte Kontostand kann also zu niedrig sein. Verbinde einen Node mit der vollständigen Blockchain, um ihn zu sehen.';
  }

  @override
  String liteNodeSyncStalled(int wallet, int node) {
    return 'Synchronisierung bei Block $wallet gestoppt. Dieser Node hält nichts unterhalb von Block $node, die Blöcke dazwischen können also nicht von ihm geladen werden. Der Kontostand bleibt unvollständig, bis du einen Node mit der vollständigen Blockchain verbindest.';
  }

  @override
  String get liteNodeRescanRefusedTitle =>
      'Dieser Node kann nicht so weit zurück scannen';

  @override
  String liteNodeRescanRefused(int height) {
    return 'Der verbundene Node ist ein Lite-Node ohne Blockdaten unterhalb von $height. Ein Rescan von tiefer würde bereits gefundene Transaktionen dieser Wallet verwerfen, ohne sie hier wiederfinden zu können. Es wurde nichts geändert.';
  }

  @override
  String liteNodeRescanFromInstead(int height) {
    return 'Stattdessen ab $height scannen';
  }

  @override
  String liteNodeRescanHint(int height) {
    return 'Der verbundene Node kann nur ab Block $height oder höher scannen.';
  }

  @override
  String get nodeServesFromLabel => 'Liefert Blöcke ab';

  @override
  String get nodeFullChain => 'Vollständige Blockchain';

  @override
  String get localNodeMobileFuture =>
      'Den Node direkt auf dem Telefon zu betreiben ist geplant, aber noch nicht verfügbar — ein Node braucht mehrere GB Speicher und Stunden zum Synchronisieren. Richte diese Wallet bis dahin auf einen selbst betriebenen Node.';
}
