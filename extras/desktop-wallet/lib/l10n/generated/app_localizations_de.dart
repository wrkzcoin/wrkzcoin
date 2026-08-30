// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for German (`de`).
class SDe extends S {
  SDe([String locale = 'de']) : super(locale);

  @override
  String get appTitle => 'PLUTON v2';

  @override
  String get tabOverview => 'Übersicht';

  @override
  String get tabReceive => 'Empfangen';

  @override
  String get tabTransfer => 'Überweisen';

  @override
  String get tabHistory => 'Verlauf';

  @override
  String get tabAddressBook => 'Adressbuch';

  @override
  String get tabSettings => 'Einstellungen';

  @override
  String get tabAbout => 'Info';

  @override
  String get lockWallet => 'Wallet sperren';

  @override
  String get send => 'Senden';

  @override
  String get receive => 'Empfangen';

  @override
  String get transfer => 'Überweisen';

  @override
  String get available => 'Verfügbar';

  @override
  String get locked => 'Gesperrt';

  @override
  String get total => 'Gesamt';

  @override
  String get availableBalance => 'Verfügbares Guthaben';

  @override
  String lockedUnconfirmed(String amount, String ticker) {
    return 'Gesperrt (unbestätigt): $amount $ticker';
  }

  @override
  String totalBalance(String amount, String ticker) {
    return 'Gesamt: $amount $ticker';
  }

  @override
  String get balanceIncompleteWhileSyncing =>
      'Guthaben kann während der Synchronisierung unvollständig sein';

  @override
  String errorPrefix(String message) {
    return 'Fehler: $message';
  }

  @override
  String get network => 'Netzwerk';

  @override
  String get syncStatus => 'Sync-Status';

  @override
  String get synced => 'Synchronisiert';

  @override
  String get syncing => 'Synchronisiere…';

  @override
  String get walletBlock => 'Wallet-Block';

  @override
  String get networkBlock => 'Netzwerk-Block';

  @override
  String get peers => 'Peers';

  @override
  String get walletType => 'Wallet-Typ';

  @override
  String get viewOnly => 'Nur-Lesen';

  @override
  String get full => 'Voll';

  @override
  String get nodeConnectionIssue => 'Problem mit der Knotenverbindung';

  @override
  String get switchNodeInSettings => 'Knoten in den Einstellungen wechseln →';

  @override
  String get recentTransactions => 'Letzte Transaktionen';

  @override
  String get viewAll => 'Alle anzeigen →';

  @override
  String get noTransactionsYet => 'Noch keine Transaktionen';

  @override
  String get received => 'Empfangen';

  @override
  String get sent => 'Gesendet';

  @override
  String syncingProgress(String pct, int wallet, int network) {
    return 'Synchronisiere $pct% (Block $wallet / $network)';
  }

  @override
  String get shareAddressSubtitle =>
      'Teile deine Adresse, um WRKZ zu empfangen';

  @override
  String get yourAddress => 'Deine Adresse';

  @override
  String get generateIntegratedAddress => 'Integrierte Adresse generieren';

  @override
  String get integratedAddressDescription =>
      'Kombiniere deine Adresse mit einer Zahlungs-ID. Verwende die Zufallstasten für eine neue ID oder gib unten deine eigene ein.';

  @override
  String get randomShort16 => 'Zufällig kurz (16)';

  @override
  String get randomLong64 => 'Zufällig lang (64)';

  @override
  String get customPaymentIdLabel =>
      'Benutzerdefinierte Zahlungs-ID (16 oder 64 Hex-Zeichen)';

  @override
  String get generate => 'Generieren';

  @override
  String get integratedAddress => 'Integrierte Adresse';

  @override
  String get paymentIdShort => 'Kurz (16)';

  @override
  String get paymentIdLong => 'Lang (64)';

  @override
  String paymentIdLabel(String label) {
    return 'Zahlungs-ID · $label';
  }

  @override
  String get enterPaymentIdError =>
      'Zahlungs-ID eingeben (16 oder 64 Hex-Zeichen)';

  @override
  String get paymentIdInvalidError =>
      'Zahlungs-ID muss 16 oder 64 Hex-Zeichen lang sein';

  @override
  String get copyAddress => 'Adresse kopieren';

  @override
  String get copyPaymentId => 'Zahlungs-ID kopieren';

  @override
  String get copy => 'Kopieren';

  @override
  String get copied => 'Kopiert!';

  @override
  String get sendWrkzToAny => 'WRKZ an eine beliebige Adresse senden';

  @override
  String get sweepAllDescription =>
      'Alle Mittel an eine Adresse senden (konsolidiert UTXOs)';

  @override
  String get sweepAll => 'Alles zusammenfassen';

  @override
  String get sweepWarning =>
      'Zusammenfassen konsolidiert alle UTXOs in eine Ausgabe. Verwende dies, wenn Transaktionen wegen zu vieler Eingaben fehlschlagen.';

  @override
  String sweepAvailableBalance(String amount, String ticker) {
    return 'Verfügbar: $amount $ticker (gesamtes Guthaben wird abzüglich Gebühr gesendet)';
  }

  @override
  String get destinationAddress => 'Zieladresse';

  @override
  String get addressBook => 'Adressbuch';

  @override
  String get sweepAllFunds => 'Alle Mittel zusammenfassen';

  @override
  String get recipientAddress => 'Empfängeradresse';

  @override
  String get amount => 'Betrag';

  @override
  String get paymentIdOptional => 'Zahlungs-ID (optional)';

  @override
  String get hexCharacters => '16 oder 64 Hex-Zeichen';

  @override
  String get reviewTransaction => 'Transaktion überprüfen';

  @override
  String get reviewAndConfirm => 'Überprüfen & Bestätigen';

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
      'Transaktionen sind unwiderruflich. Überprüfe die Adresse vor der Bestätigung.';

  @override
  String get back => 'Zurück';

  @override
  String get confirmAndSend => 'Bestätigen & Senden';

  @override
  String get transactionSent => 'Transaktion gesendet!';

  @override
  String get transactionBroadcast =>
      'Deine Transaktion wurde an das Netzwerk gesendet.';

  @override
  String get transactionHash => 'Transaktions-Hash';

  @override
  String get sendAnother => 'Weitere senden';

  @override
  String get enterDestinationAddress => 'Zieladresse eingeben';

  @override
  String get enterValidAmount => 'Gültigen Betrag eingeben';

  @override
  String computingPow(int seconds) {
    return 'Berechne PoW... ${seconds}s';
  }

  @override
  String get stepFillDetails => 'Details ausfüllen';

  @override
  String get stepReview => 'Überprüfen';

  @override
  String get stepDone => 'Fertig';

  @override
  String get sweepFailed => 'Zusammenfassung fehlgeschlagen';

  @override
  String get addressBookTitle => 'Adressbuch';

  @override
  String get transactionHistory => 'Transaktionsverlauf';

  @override
  String get searchByHash => 'Nach Hash, Adresse oder Zahlungs-ID suchen…';

  @override
  String get all => 'Alle';

  @override
  String get filterReceived => 'Empfangen';

  @override
  String get filterSent => 'Gesendet';

  @override
  String get refresh => 'Aktualisieren';

  @override
  String get noTransactionsFound => 'Keine Transaktionen gefunden';

  @override
  String get confirmed => 'Bestätigt';

  @override
  String get pending => 'Ausstehend';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Adresse';

  @override
  String get block => 'Block';

  @override
  String showingRange(int start, int end, int total) {
    return 'Zeige $start–$end von $total';
  }

  @override
  String get previous => 'Zurück';

  @override
  String get next => 'Weiter';

  @override
  String get walletLocked => 'Wallet gesperrt';

  @override
  String get enterPasswordToContinue =>
      'Gib dein Wallet-Passwort ein, um fortzufahren';

  @override
  String get password => 'Passwort';

  @override
  String get incorrectPassword => 'Falsches Passwort';

  @override
  String get unlock => 'Entsperren';

  @override
  String get closeWalletInstead => 'Wallet stattdessen schließen';

  @override
  String get closeWallet => 'Wallet schließen';

  @override
  String get closeWalletDescription =>
      'Dies wird das Wallet speichern und schließen.\n\nDu wirst zum Anmeldebildschirm zurückkehren.';

  @override
  String get cancel => 'Abbrechen';

  @override
  String get welcomeToPluton => 'Willkommen bei PLUTON v2';

  @override
  String get selectOptionToStart => 'Wähle eine Option, um loszulegen';

  @override
  String get createNewWallet => 'Neues Wallet erstellen';

  @override
  String get openExistingWallet => 'Vorhandenes Wallet öffnen';

  @override
  String get importFromSeed => 'Aus Seed-Phrase importieren';

  @override
  String get importFromKeys => 'Aus privaten Schlüsseln importieren';

  @override
  String get openWallet => 'Wallet öffnen';

  @override
  String get importFromSeedTitle => 'Aus Seed importieren';

  @override
  String get importFromKeysTitle => 'Aus Schlüsseln importieren';

  @override
  String get saveWalletTo => 'Wallet speichern unter';

  @override
  String get walletFile => 'Wallet-Datei';

  @override
  String get walletPassword => 'Wallet-Passwort';

  @override
  String get mnemonicSeedPhrase => 'Mnemonische Seed-Phrase';

  @override
  String get scanFromHeight => 'Ab Höhe scannen (0 = vollständiger Scan)';

  @override
  String get daemonHost => 'Daemon-Host';

  @override
  String get port => 'Port';

  @override
  String get continueButton => 'Weiter';

  @override
  String get browse => 'Durchsuchen';

  @override
  String get backupWarning =>
      'Sichere dein Wallet, bevor du fortfährst.\nDiese Schlüssel können nicht wiederhergestellt werden, wenn sie verloren gehen.';

  @override
  String get yourWalletAddress => 'Deine Wallet-Adresse';

  @override
  String get seedPhrase25Words => 'Seed-Phrase (25 Wörter)';

  @override
  String get privateViewKey => 'Privater View-Schlüssel';

  @override
  String get privateSpendKey => 'Privater Spend-Schlüssel';

  @override
  String get seedBackupConfirm =>
      'Ich habe meine Seed-Phrase und privaten Schlüssel an einem sicheren Ort aufgeschrieben.';

  @override
  String get backedUpContinue => 'Ich habe mein Wallet gesichert — Weiter';

  @override
  String get settings => 'Einstellungen';

  @override
  String get sectionDaemonNode => 'Daemon-Knoten';

  @override
  String get nodeDescription =>
      'Verbinde dich mit einem lokalen oder entfernten Daemon-Knoten. Änderungen werden sofort wirksam.';

  @override
  String get hostIpAddress => 'Host / IP-Adresse';

  @override
  String get ssl => 'SSL';

  @override
  String get apply => 'Anwenden';

  @override
  String get nodeUpdatedSuccess => 'Knoten erfolgreich aktualisiert';

  @override
  String get nodeUnreachable =>
      'Der aktuelle Knoten ist nicht erreichbar. Gib unten eine neue Knotenadresse ein und tippe auf Anwenden.';

  @override
  String get sectionWallet => 'Wallet';

  @override
  String get saveWallet => 'Wallet speichern';

  @override
  String get saveWalletSubtitle =>
      'Aktuellen Status auf die Festplatte schreiben';

  @override
  String get walletSaved => 'Wallet gespeichert';

  @override
  String get exportToJson => 'Als JSON exportieren';

  @override
  String get exportToJsonSubtitle => 'Wallet-Daten als JSON-Datei speichern';

  @override
  String get exportJsonTitle => 'Wallet-JSON exportieren';

  @override
  String exportedTo(String path) {
    return 'Exportiert nach $path';
  }

  @override
  String exportFailed(String error) {
    return 'Export fehlgeschlagen: $error';
  }

  @override
  String get resetScanHeight => 'Scan-Höhe zurücksetzen';

  @override
  String get resetScanHeightSubtitle =>
      'Blockchain ab einer bestimmten Höhe erneut scannen';

  @override
  String get resetScanHeightDescription =>
      'Gib eine Blockhöhe ein, ab der erneut gescannt werden soll. Verwende 0 für einen vollständigen Scan.';

  @override
  String get scanHeight => 'Scan-Höhe';

  @override
  String get reset => 'Zurücksetzen';

  @override
  String get autosave => 'Automatisches Speichern';

  @override
  String get autosaveSubtitle =>
      'Wallet nach Synchronisierung und alle 5 Minuten auf die Festplatte speichern';

  @override
  String get scanCoinbaseTx => 'Coinbase-Transaktionen scannen';

  @override
  String get scanCoinbaseSubtitle =>
      'Miner-Belohnungen beim Synchronisieren einbeziehen (standardmäßig aus)';

  @override
  String get sectionAppearance => 'Erscheinungsbild';

  @override
  String get theme => 'Design';

  @override
  String get themeSubtitle => 'Farbschema der App wählen';

  @override
  String get themeSystem => 'System';

  @override
  String get themeLight => 'Hell';

  @override
  String get themeDark => 'Dunkel';

  @override
  String get sectionNotifications => 'Benachrichtigungen';

  @override
  String get incomingTxAlerts => 'Eingehende Transaktionsbenachrichtigungen';

  @override
  String get incomingTxAlertsSubtitle =>
      'Desktop-Benachrichtigung anzeigen, wenn WRKZ empfangen wird';

  @override
  String get sectionDebugLogs => 'Debug & Protokolle';

  @override
  String get logLevel => 'Protokollstufe';

  @override
  String get logLevelSubtitle =>
      'Steuert die Ausführlichkeit der Wallet-Bibliothek';

  @override
  String get viewLogs => 'Protokolle anzeigen';

  @override
  String get viewLogsSubtitle => 'Live-Protokollausgabe der Wallet-Bibliothek';

  @override
  String get walletLogs => 'Wallet-Protokolle';

  @override
  String logEntries(int count) {
    return '$count Einträge';
  }

  @override
  String get autoScroll => 'Automatisch scrollen';

  @override
  String get copyAll => 'Alles kopieren';

  @override
  String get clear => 'Löschen';

  @override
  String get close => 'Schließen';

  @override
  String get noLogsYet =>
      'Noch keine Protokolle. Setze eine Protokollstufe über Deaktiviert, um die Ausgabe zu sehen.';

  @override
  String get logsCopied => 'Protokolle in die Zwischenablage kopiert';

  @override
  String get sectionDangerZone => 'Gefahrenzone';

  @override
  String get deleteWalletData => 'Wallet-Daten löschen';

  @override
  String get deleteWalletDataSubtitle =>
      'Wallet-Datei dauerhaft von der Festplatte entfernen';

  @override
  String get deleteWalletWarning =>
      'Dies wird deine Wallet-Datei dauerhaft von der Festplatte löschen.\n\nStelle sicher, dass du deine Seed-Phrase und privaten Schlüssel gesichert hast, bevor du fortfährst. Diese Aktion kann nicht rückgängig gemacht werden.';

  @override
  String get iUnderstandContinue => 'Ich verstehe, fortfahren';

  @override
  String get finalConfirmation => 'Letzte Bestätigung';

  @override
  String get typeDeleteToConfirm => 'Gib DELETE ein, um zu bestätigen:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get deletePermanently => 'Dauerhaft löschen';

  @override
  String get aboutTitle => 'Info';

  @override
  String versionInfo(String version) {
    return 'Version $version — WRKZ Desktop-Wallet';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 ist das offizielle Desktop-Wallet für WrkzCoin (WRKZ), eine schnelle und leichtgewichtige CryptoNote-basierte Kryptowährung.\n\nErstellt mit Flutter, betrieben von wallet-api.';

  @override
  String get github => 'GitHub';

  @override
  String get githubSubtitle => 'Quellcode und Releases ansehen';

  @override
  String get discord => 'Discord';

  @override
  String get discordSubtitle => 'Der Community beitreten';

  @override
  String get twitterX => 'Twitter / X';

  @override
  String get twitterXSubtitle => '@wrkzcoin folgen';

  @override
  String get website => 'Webseite';

  @override
  String get websiteSubtitle => 'wrkz.work';

  @override
  String get license => 'Lizenz';

  @override
  String get licenseText =>
      'Veröffentlicht unter der MIT-Lizenz.\nVerwendung auf eigene Gefahr. Sichere immer deine Seed-Phrase.';

  @override
  String get addButton => 'Hinzufügen';

  @override
  String get noSavedAddresses => 'Keine gespeicherten Adressen';

  @override
  String get tapAddToSave =>
      'Tippe auf Hinzufügen, um eine häufig verwendete Adresse zu speichern.';

  @override
  String get addAddress => 'Adresse hinzufügen';

  @override
  String get nameLabel => 'Name / Bezeichnung';

  @override
  String get addressLabel => 'Adresse';

  @override
  String get noteOptional => 'Notiz (optional)';

  @override
  String get nameAndAddressRequired => 'Name und Adresse sind erforderlich';

  @override
  String get invalidWrkzAddress =>
      'Ungültige WRKZ-Adresse. Muss 98 (Standard), 120 (kurz integriert) oder 186 (lang integriert) Zeichen lang sein und mit \"Wrkz\" beginnen.';

  @override
  String get save => 'Speichern';

  @override
  String get editEntry => 'Eintrag bearbeiten';

  @override
  String get deleteEntry => 'Eintrag löschen';

  @override
  String removeFromAddressBook(String name) {
    return '\"$name\" aus dem Adressbuch entfernen?';
  }

  @override
  String get delete => 'Löschen';

  @override
  String get edit => 'Bearbeiten';

  @override
  String get wrkzReceived => 'WRKZ empfangen';

  @override
  String youReceivedAmount(String amount) {
    return 'Du hast $amount empfangen';
  }

  @override
  String get show => 'Anzeigen';

  @override
  String get exit => 'Beenden';

  @override
  String get plutonWallet => 'PLUTON Wallet';

  @override
  String get language => 'Sprache';

  @override
  String get selectLanguage => 'Sprache auswählen';

  @override
  String get chooseLanguage => 'Wähle deine bevorzugte Sprache';

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
  String get preparedTransactionExpired =>
      'Diese Transaktion ist nicht mehr gültig. Gehen Sie zurück und erstellen Sie sie erneut.';

  @override
  String get deleteConfirmMismatch =>
      'Geben Sie genau DELETE ein, um zu bestätigen.';

  @override
  String get unlockNeedsReopen =>
      'Das Passwort kann auf diesem Gerät nicht überprüft werden. Verwenden Sie „Wallet stattdessen schließen“ und öffnen Sie die Wallet erneut.';

  @override
  String get exportJsonWarningTitle => 'Unverschlüsselte Wallet exportieren?';

  @override
  String get exportJsonWarningBody =>
      'Die exportierte Datei enthält Ihren privaten View-Key und Ihre privaten Spend-Keys im Klartext. Wer sie liest, kann Ihr Guthaben ausgeben.\n\nSpeichern Sie sie nur auf einem Datenträger, den Sie kontrollieren, und löschen Sie sie danach sofort.';

  @override
  String passwordTooShort(int count) {
    return 'Das Passwort muss mindestens $count Zeichen lang sein';
  }

  @override
  String get ringSize => 'Ringgröße';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Ringgröße auf $actual reduziert (normalerweise $normal). Für die gesendeten Beträge gibt es in der Blockchain nicht genug Ausgänge für einen vollständigen Ring, daher ist diese Transaktion weniger privat als üblich.';
  }
}
