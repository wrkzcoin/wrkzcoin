// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for French (`fr`).
class SFr extends S {
  SFr([String locale = 'fr']) : super(locale);

  @override
  String get appTitle => 'PLUTON Mobile';

  @override
  String get tabOverview => 'Aperçu';

  @override
  String get tabReceive => 'Recevoir';

  @override
  String get tabSend => 'Envoyer';

  @override
  String get tabHistory => 'Historique';

  @override
  String get tabSettings => 'Paramètres';

  @override
  String get send => 'Envoyer';

  @override
  String get receive => 'Recevoir';

  @override
  String get available => 'Disponible';

  @override
  String get locked => 'Verrouillé';

  @override
  String get total => 'Total';

  @override
  String lockedAmount(String amount) {
    return 'Verrouillé : $amount';
  }

  @override
  String totalAmount(String amount) {
    return 'Total : $amount';
  }

  @override
  String get recentTransactions => 'Transactions récentes';

  @override
  String get viewAll => 'Voir tout';

  @override
  String get noTransactionsYet => 'Aucune transaction pour l\'instant';

  @override
  String get noMatchingTransactions => 'Aucune transaction correspondante';

  @override
  String get pending => 'En attente...';

  @override
  String get justNow => 'À l\'instant';

  @override
  String minutesAgo(int count) {
    return 'Il y a ${count}m';
  }

  @override
  String hoursAgo(int count) {
    return 'Il y a ${count}h';
  }

  @override
  String daysAgo(int count) {
    return 'Il y a ${count}j';
  }

  @override
  String get received => 'Reçu';

  @override
  String get sent => 'Envoyé';

  @override
  String get networkStatus => 'État du réseau';

  @override
  String get node => 'Nœud';

  @override
  String get status => 'État';

  @override
  String get connected => 'Connecté';

  @override
  String get disconnected => 'Déconnecté';

  @override
  String get walletHeight => 'Hauteur du portefeuille';

  @override
  String get networkHeight => 'Hauteur du réseau';

  @override
  String get peers => 'Pairs';

  @override
  String get type => 'Type';

  @override
  String get viewOnly => 'Lecture seule';

  @override
  String get couldNotFetchStatus =>
      'Impossible de récupérer l\'état. Vérifiez votre nœud dans les paramètres.';

  @override
  String errorPrefix(String message) {
    return 'Erreur : $message';
  }

  @override
  String get seedBackupWarning =>
      'Sauvegardez votre phrase de récupération dans les paramètres pour protéger vos fonds.';

  @override
  String get noConnectionToDaemon => 'Pas de connexion au daemon';

  @override
  String syncingPercent(String percent) {
    return 'Synchronisation $percent%';
  }

  @override
  String get yourAddress => 'Votre adresse';

  @override
  String get errorLoadingAddress => 'Erreur lors du chargement de l\'adresse';

  @override
  String get integratedAddress => 'Adresse intégrée';

  @override
  String get embedPaymentId =>
      'Intégrer un identifiant de paiement dans votre adresse';

  @override
  String get randomShort => 'Court aléatoire (16)';

  @override
  String get randomLong => 'Long aléatoire (64)';

  @override
  String get enterCustomPaymentId =>
      'Ou entrez un identifiant de paiement personnalisé (16 ou 64 hex)';

  @override
  String get enterPaymentId => 'Entrez un identifiant de paiement';

  @override
  String get paymentIdInvalid =>
      'L\'identifiant de paiement doit comporter 16 ou 64 caractères hexadécimaux';

  @override
  String get shortPid => 'PID court';

  @override
  String get longPid => 'PID long';

  @override
  String get share => 'Partager';

  @override
  String get copy => 'Copier';

  @override
  String get sweepAllFunds => 'Balayer tous les fonds';

  @override
  String get normalSend => 'Envoi normal';

  @override
  String get sweep => 'Balayer';

  @override
  String get recipientAddress => 'Adresse du destinataire';

  @override
  String get scanQr => 'Scanner QR';

  @override
  String get amount => 'Montant';

  @override
  String availableBalance(String amount) {
    return 'Disponible : $amount';
  }

  @override
  String sweepInfo(String amount) {
    return 'Le balayage consolide tous les UTXOs et envoie l\'intégralité de votre solde déverrouillé ($amount) moins les frais.';
  }

  @override
  String get paymentIdOptional => 'Identifiant de paiement (facultatif)';

  @override
  String get hexCharacters => '16 ou 64 caractères hexadécimaux';

  @override
  String get mustBeHex => 'Doit comporter 16 ou 64 caractères hexadécimaux';

  @override
  String get recipientRequired => 'L\'adresse du destinataire est requise';

  @override
  String get invalidAddress => 'Adresse WRKZ invalide';

  @override
  String get enterValidAmount => 'Entrez un montant valide';

  @override
  String get reviewTransaction => 'Vérifier la transaction';

  @override
  String get to => 'À';

  @override
  String get fee => 'Frais';

  @override
  String get totalDeducted => 'Total déduit';

  @override
  String get paymentId => 'Identifiant de paiement';

  @override
  String get transactionsIrreversible =>
      'Les transactions sont irréversibles. Veuillez vérifier les détails.';

  @override
  String get back => 'Retour';

  @override
  String get confirmAndSend => 'Confirmer et envoyer';

  @override
  String get transactionSent => 'Transaction envoyée !';

  @override
  String get transactionHash => 'Hash de la transaction';

  @override
  String get sendAnother => 'Envoyer une autre';

  @override
  String get scanQrCode => 'Scanner le code QR';

  @override
  String get scannedAddress => 'Adresse scannée';

  @override
  String get cancel => 'Annuler';

  @override
  String get useThisAddress => 'Utiliser cette adresse';

  @override
  String get sweepFailed => 'Échec du balayage';

  @override
  String get searchPlaceholder =>
      'Rechercher par hash, adresse, identifiant de paiement...';

  @override
  String get all => 'Tous';

  @override
  String get filterReceived => 'Reçus';

  @override
  String get filterSent => 'Envoyés';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Adresse';

  @override
  String get block => 'Bloc';

  @override
  String get confirmed => 'Confirmé';

  @override
  String get password => 'Mot de passe';

  @override
  String get unlock => 'Déverrouiller';

  @override
  String get switchWallet => 'Changer de portefeuille';

  @override
  String get enterPasswordToUnlock =>
      'Entrez votre mot de passe pour déverrouiller';

  @override
  String get incorrectPassword => 'Mot de passe incorrect';

  @override
  String get enterYourPassword => 'Entrez votre mot de passe';

  @override
  String get plutonMobile => 'PLUTON Mobile';

  @override
  String get createFirstWalletSubtitle =>
      'Créez votre premier portefeuille pour commencer';

  @override
  String get selectWalletSubtitle => 'Sélectionnez un portefeuille à ouvrir';

  @override
  String get yourWallets => 'Vos portefeuilles';

  @override
  String get noWalletsYet => 'Aucun portefeuille pour l\'instant';

  @override
  String get lastOpened => 'Dernière ouverture';

  @override
  String createdDate(String date) {
    return 'Créé le $date';
  }

  @override
  String get createFirstWallet => 'Créer le premier portefeuille';

  @override
  String get addWallet => 'Ajouter un portefeuille';

  @override
  String get deleteWallet => 'Supprimer le portefeuille';

  @override
  String deleteWalletConfirm(String name) {
    return 'Supprimer « $name » ?\n\nCela supprimera définitivement le fichier portefeuille et les clés. Assurez-vous d\'avoir sauvegardé votre phrase de récupération.';
  }

  @override
  String get delete => 'Supprimer';

  @override
  String get createNewWallet => 'Créer un nouveau portefeuille';

  @override
  String get createNewWalletSubtitle =>
      'Générer un nouveau portefeuille avec une nouvelle phrase de récupération';

  @override
  String get importFromSeed => 'Importer depuis une phrase de récupération';

  @override
  String get importFromSeedSubtitle =>
      'Restaurer le portefeuille à l\'aide de votre mnémonique de 25 mots';

  @override
  String get importFromKeys => 'Importer depuis des clés privées';

  @override
  String get importFromKeysSubtitle =>
      'Restaurer à l\'aide de la clé de dépense et de la clé de visualisation';

  @override
  String get viewOnlyWallet => 'Portefeuille en lecture seule';

  @override
  String get viewOnlyWalletSubtitle =>
      'Portefeuille de surveillance utilisant la clé de visualisation et l\'adresse';

  @override
  String get createWallet => 'Créer le portefeuille';

  @override
  String get importWallet => 'Importer le portefeuille';

  @override
  String get walletName => 'Nom du portefeuille';

  @override
  String get walletNameHint => 'ex. Portefeuille principal';

  @override
  String get passwordLabel => 'Mot de passe';

  @override
  String get enterPassword => 'Entrez le mot de passe';

  @override
  String get confirmPassword => 'Confirmer le mot de passe';

  @override
  String get seedPhrase => 'Phrase de récupération (25 mots)';

  @override
  String get enterSeedPhrase => 'Entrez votre phrase de récupération...';

  @override
  String get scanHeight => 'Hauteur de scan (facultatif)';

  @override
  String get scanHeightHint => '0 = scanner depuis le début';

  @override
  String get privateSpendKey => 'Clé de dépense privée';

  @override
  String get privateViewKey => 'Clé de visualisation privée';

  @override
  String get walletAddress => 'Adresse du portefeuille';

  @override
  String get walletAddressHint => 'Adresse Wrkz...';

  @override
  String get hexKey => 'Hex de 64 caractères';

  @override
  String get daemonNode => 'Nœud daemon';

  @override
  String get custom => 'Personnalisé';

  @override
  String get host => 'Hôte';

  @override
  String get hostHint => 'Hôte / IP';

  @override
  String get port => 'Port';

  @override
  String get ssl => 'SSL';

  @override
  String get walletNameRequired => 'Le nom du portefeuille est requis';

  @override
  String get passwordRequired => 'Le mot de passe est requis';

  @override
  String passwordTooShort(int count) {
    return 'Le mot de passe doit comporter au moins $count caractères';
  }

  @override
  String get passwordsDoNotMatch => 'Les mots de passe ne correspondent pas';

  @override
  String get seedRequired => 'La phrase de récupération est requise';

  @override
  String get spendKeyRequired => 'La clé de dépense est requise';

  @override
  String get viewKeyRequired => 'La clé de visualisation est requise';

  @override
  String get addressRequired => 'L\'adresse est requise';

  @override
  String get daemonHostRequired => 'L\'hôte daemon est requis';

  @override
  String get backupSeedTitle => 'Sauvegarder votre phrase de récupération';

  @override
  String get backupWarning =>
      'Notez votre phrase de récupération et conservez-la en lieu sûr. Si vous la perdez, vos fonds sont perdus pour toujours.';

  @override
  String get seedPhraseLabel => 'Phrase de récupération';

  @override
  String get privateViewKeyLabel => 'Clé de visualisation privée';

  @override
  String get privateSpendKeyLabel => 'Clé de dépense privée';

  @override
  String get backupConfirmCheck =>
      'J\'ai sauvegardé ma phrase de récupération en lieu sûr';

  @override
  String get continueToWallet => 'Continuer vers le portefeuille';

  @override
  String get sectionDaemonNode => 'Nœud daemon';

  @override
  String get apply => 'Appliquer';

  @override
  String nodeUpdated(String host, int port) {
    return 'Nœud mis à jour vers $host:$port';
  }

  @override
  String get hostRequired => 'L\'hôte est requis';

  @override
  String currentWallet(String name) {
    return 'Portefeuille actuel — $name';
  }

  @override
  String get saveWallet => 'Enregistrer le portefeuille';

  @override
  String get walletSaved => 'Portefeuille enregistré';

  @override
  String saveFailed(String error) {
    return 'Échec de l\'enregistrement : $error';
  }

  @override
  String get backupSeed => 'Sauvegarder la phrase de récupération';

  @override
  String get changePassword => 'Modifier le mot de passe';

  @override
  String get resetScanHeight => 'Réinitialiser la hauteur de scan';

  @override
  String get reset => 'Réinitialiser';

  @override
  String resetScanConfirm(int height) {
    return 'Cela va rescanner la blockchain depuis le bloc $height. Cela peut prendre un certain temps. Continuer ?';
  }

  @override
  String scanResetTo(int height) {
    return 'Scan réinitialisé au bloc $height';
  }

  @override
  String resetFailed(String error) {
    return 'Échec de la réinitialisation : $error';
  }

  @override
  String get enterPasswordTitle => 'Entrez le mot de passe';

  @override
  String get confirm => 'Confirmer';

  @override
  String get seedBackup => 'Sauvegarde de la phrase de récupération';

  @override
  String get seedPhraseColon => 'Phrase de récupération :';

  @override
  String get privateViewKeyColon => 'Clé de visualisation privée :';

  @override
  String get iveBackedUp => 'J\'ai sauvegardé';

  @override
  String get currentPasswordLabel => 'Mot de passe actuel';

  @override
  String get newPasswordLabel => 'Nouveau mot de passe';

  @override
  String get confirmNewPasswordLabel => 'Confirmer le nouveau mot de passe';

  @override
  String get change => 'Modifier';

  @override
  String get currentPasswordIncorrect => 'Le mot de passe actuel est incorrect';

  @override
  String get newPasswordsDoNotMatch =>
      'Les nouveaux mots de passe ne correspondent pas';

  @override
  String get passwordChanged => 'Mot de passe modifié';

  @override
  String get walletManagement => 'Gestion des portefeuilles';

  @override
  String get switchWalletSubtitle =>
      'Enregistrer et fermer, en choisir un autre';

  @override
  String get manageWallets => 'Gérer les portefeuilles';

  @override
  String get manageWalletsSubtitle => 'Renommer ou supprimer des portefeuilles';

  @override
  String get currentlyOpen => '(actuellement ouvert)';

  @override
  String get close => 'Fermer';

  @override
  String get renameWallet => 'Renommer le portefeuille';

  @override
  String get newName => 'Nouveau nom';

  @override
  String get rename => 'Renommer';

  @override
  String deleteWalletConfirmShort(String name) {
    return 'Supprimer « $name » ? Cette action est irréversible.';
  }

  @override
  String get security => 'Sécurité';

  @override
  String get biometricUnlock => 'Déverrouillage biométrique';

  @override
  String get biometricSubtitle => 'Empreinte digitale / Face ID';

  @override
  String get biometricNotAvailable => 'Biométrie non disponible';

  @override
  String get autoLock => 'Verrouillage automatique';

  @override
  String get appearance => 'Apparence';

  @override
  String get theme => 'Thème';

  @override
  String get themeAuto => 'Automatique';

  @override
  String get themeLight => 'Clair';

  @override
  String get themeDark => 'Sombre';

  @override
  String get preferences => 'Préférences';

  @override
  String get transactionNotifications => 'Notifications de transaction';

  @override
  String get notificationsSubtitle => 'Alerte lors de transactions entrantes';

  @override
  String get autosave => 'Sauvegarde automatique';

  @override
  String get autosaveSubtitle =>
      'Sauvegarder après la synchronisation, puis toutes les 5 minutes';

  @override
  String get scanCoinbaseTx => 'Scanner les transactions Coinbase';

  @override
  String get scanCoinbaseSubtitle =>
      'Inclure les récompenses de minage (désactivé par défaut)';

  @override
  String get dangerZone => 'Zone dangereuse';

  @override
  String get deleteCurrentWallet => 'Supprimer le portefeuille actuel';

  @override
  String get deleteCurrentWalletSubtitle =>
      'Supprimer définitivement les données du portefeuille';

  @override
  String get deleteWalletTypeCaps =>
      'Cela supprimera définitivement le fichier portefeuille et les clés. Assurez-vous d\'avoir sauvegardé votre phrase de récupération.\n\nTapez DELETE pour confirmer :';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get language => 'Langue';

  @override
  String get selectLanguage => 'Sélectionner la langue';

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
  String get autoLockImmediately => 'Immédiatement';

  @override
  String get autoLock1Min => '1 minute';

  @override
  String get autoLock5Min => '5 minutes';

  @override
  String get autoLockNever => 'Jamais';

  @override
  String get preparedTransactionExpired =>
      'Cette transaction n\'est plus valide. Revenez en arrière et recréez-la.';

  @override
  String get deleteConfirmMismatch => 'Tapez exactement DELETE pour confirmer.';

  @override
  String get seedNotBackedUpWarning =>
      'Vous n\'avez pas confirmé la sauvegarde de la phrase de récupération de ce portefeuille. Le supprimer maintenant rendra les fonds irrécupérables.';

  @override
  String get wrkzReceived => 'WRKZ reçus';

  @override
  String get retry => 'Réessayer';

  @override
  String youReceivedAmount(String amount) {
    return 'Vous avez reçu $amount';
  }

  @override
  String get ringSize => 'Taille de l\'anneau';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Taille de l\'anneau réduite à $actual (normalement $normal). Les montants envoyés n\'ont pas assez de sorties sur la chaîne pour former un anneau complet, cette transaction est donc moins privée que d\'habitude.';
  }

  @override
  String get liteNodeTitle => 'Nœud allégé';

  @override
  String liteNodeServesFrom(int height) {
    return 'Ce nœud ne conserve que les blocs à partir de $height. Les transactions antérieures à ce bloc sont introuvables via ce nœud.';
  }

  @override
  String liteNodeMissesHistory(int nodeHeight, int walletHeight) {
    return 'Ce nœud démarre au bloc $nodeHeight, mais ce portefeuille démarre au bloc $walletHeight. Tout ce qui a été reçu entre les deux est invisible ici, le solde affiché peut donc être trop bas. Connectez un nœud disposant de la chaîne complète pour le voir.';
  }

  @override
  String liteNodeSyncStalled(int wallet, int node) {
    return 'Synchronisation arrêtée au bloc $wallet. Ce nœud ne conserve rien en dessous du bloc $node, les blocs intermédiaires ne peuvent donc pas en être téléchargés. Le solde restera incomplet tant qu\'un nœud disposant de la chaîne complète n\'est pas connecté.';
  }

  @override
  String get liteNodeRescanRefusedTitle =>
      'Ce nœud ne peut pas réanalyser aussi loin';

  @override
  String liteNodeRescanRefused(int height) {
    return 'Le nœud connecté est un nœud allégé sans données de bloc en dessous de $height. Réanalyser plus bas supprimerait des transactions déjà trouvées par ce portefeuille, sans moyen de les retrouver ici. Rien n\'a été modifié.';
  }

  @override
  String liteNodeRescanFromInstead(int height) {
    return 'Réanalyser depuis $height à la place';
  }

  @override
  String liteNodeRescanHint(int height) {
    return 'Le nœud connecté ne peut réanalyser qu\'à partir du bloc $height ou au-delà.';
  }

  @override
  String get nodeServesFromLabel => 'Fournit les blocs à partir de';

  @override
  String get nodeFullChain => 'Chaîne complète';

  @override
  String get localNodeMobileFuture =>
      'Faire tourner le nœud sur le téléphone est prévu, mais pas encore disponible : un nœud demande plusieurs Go de stockage et des heures de synchronisation. En attendant, pointez ce portefeuille vers un nœud que vous hébergez.';

  @override
  String get syncStoppedTitle => 'Synchronisation arrêtée';

  @override
  String syncGapStalled(int covered, int servesFrom) {
    return 'Synchronisation arrêtée au bloc $covered. Le nœud interrogé ne répond qu\'à partir du bloc $servesFrom, donc les blocs intermédiaires ne peuvent pas en être téléchargés. Le solde est incomplet tant que vous ne vous connectez pas à un nœud détenant toute la chaîne.';
  }

  @override
  String get txPowServerSection => 'Serveur PoW des transactions';

  @override
  String get txPowServerUse => 'Utiliser un serveur PoW externe';

  @override
  String get txPowServerSubtitle =>
      'Envoyer la preuve de travail de la transaction à un serveur au lieu de la calculer sur cet appareil. Si le serveur ne répond pas, le processeur de cet appareil est utilisé.';

  @override
  String get txPowServerSaved => 'Paramètres du serveur PoW enregistrés';

  @override
  String get txPowServerInvalid => 'Saisissez un hôte et un port valides';
}
