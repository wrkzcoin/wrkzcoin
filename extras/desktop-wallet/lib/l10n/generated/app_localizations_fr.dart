// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for French (`fr`).
class SFr extends S {
  SFr([String locale = 'fr']) : super(locale);

  @override
  String get appTitle => 'PLUTON v2';

  @override
  String get tabOverview => 'Aperçu';

  @override
  String get tabReceive => 'Recevoir';

  @override
  String get tabTransfer => 'Transférer';

  @override
  String get tabHistory => 'Historique';

  @override
  String get tabAddressBook => 'Carnet d\'adresses';

  @override
  String get tabSettings => 'Paramètres';

  @override
  String get tabAbout => 'À propos';

  @override
  String get lockWallet => 'Verrouiller le portefeuille';

  @override
  String get send => 'Envoyer';

  @override
  String get receive => 'Recevoir';

  @override
  String get transfer => 'Transférer';

  @override
  String get available => 'Disponible';

  @override
  String get locked => 'Verrouillé';

  @override
  String get total => 'Total';

  @override
  String get availableBalance => 'Solde disponible';

  @override
  String lockedUnconfirmed(String amount, String ticker) {
    return 'Verrouillé (non confirmé) : $amount $ticker';
  }

  @override
  String totalBalance(String amount, String ticker) {
    return 'Total : $amount $ticker';
  }

  @override
  String get balanceIncompleteWhileSyncing =>
      'Le solde peut être incomplet pendant la synchronisation';

  @override
  String errorPrefix(String message) {
    return 'Erreur : $message';
  }

  @override
  String get network => 'Réseau';

  @override
  String get syncStatus => 'État de la synchronisation';

  @override
  String get synced => 'Synchronisé';

  @override
  String get syncing => 'Synchronisation…';

  @override
  String get walletBlock => 'Bloc du portefeuille';

  @override
  String get networkBlock => 'Bloc du réseau';

  @override
  String get peers => 'Pairs';

  @override
  String get walletType => 'Type de portefeuille';

  @override
  String get viewOnly => 'Lecture seule';

  @override
  String get full => 'Complet';

  @override
  String get nodeConnectionIssue => 'Problème de connexion au nœud';

  @override
  String get switchNodeInSettings => 'Changer de nœud dans les Paramètres →';

  @override
  String get recentTransactions => 'Transactions récentes';

  @override
  String get viewAll => 'Voir tout →';

  @override
  String get noTransactionsYet => 'Aucune transaction pour le moment';

  @override
  String get received => 'Reçu';

  @override
  String get sent => 'Envoyé';

  @override
  String syncingProgress(String pct, int wallet, int network) {
    return 'Synchronisation $pct% (bloc $wallet / $network)';
  }

  @override
  String get shareAddressSubtitle =>
      'Partagez votre adresse pour recevoir des WRKZ';

  @override
  String get yourAddress => 'Votre adresse';

  @override
  String get generateIntegratedAddress => 'Générer une adresse intégrée';

  @override
  String get integratedAddressDescription =>
      'Combinez votre adresse avec un identifiant de paiement. Utilisez les boutons aléatoires pour un nouvel identifiant, ou entrez le vôtre ci-dessous.';

  @override
  String get randomShort16 => 'Aléatoire court (16)';

  @override
  String get randomLong64 => 'Aléatoire long (64)';

  @override
  String get customPaymentIdLabel =>
      'Identifiant de paiement personnalisé (16 ou 64 caractères hex)';

  @override
  String get generate => 'Générer';

  @override
  String get integratedAddress => 'Adresse intégrée';

  @override
  String get paymentIdShort => 'Court (16)';

  @override
  String get paymentIdLong => 'Long (64)';

  @override
  String paymentIdLabel(String label) {
    return 'Identifiant de paiement · $label';
  }

  @override
  String get enterPaymentIdError =>
      'Entrez un identifiant de paiement (16 ou 64 caractères hex)';

  @override
  String get paymentIdInvalidError =>
      'L\'identifiant de paiement doit contenir 16 ou 64 caractères hexadécimaux';

  @override
  String get copyAddress => 'Copier l\'adresse';

  @override
  String get copyPaymentId => 'Copier l\'identifiant de paiement';

  @override
  String get copy => 'Copier';

  @override
  String get copied => 'Copié !';

  @override
  String get sendWrkzToAny => 'Envoyer des WRKZ à n\'importe quelle adresse';

  @override
  String get sweepAllDescription =>
      'Envoyer tous les fonds à une adresse (consolide les UTXOs)';

  @override
  String get sweepAll => 'Tout balayer';

  @override
  String get sweepWarning =>
      'Le balayage consolide tous les UTXOs en une seule sortie. Utilisez ceci lorsque les transactions échouent en raison de trop d\'entrées.';

  @override
  String sweepAvailableBalance(String amount, String ticker) {
    return 'Disponible : $amount $ticker (le solde entier sera envoyé moins les frais)';
  }

  @override
  String get destinationAddress => 'Adresse de destination';

  @override
  String get addressBook => 'Carnet d\'adresses';

  @override
  String get sweepAllFunds => 'Balayer tous les fonds';

  @override
  String get recipientAddress => 'Adresse du destinataire';

  @override
  String get amount => 'Montant';

  @override
  String get paymentIdOptional => 'Identifiant de paiement (optionnel)';

  @override
  String get hexCharacters => '16 ou 64 caractères hexadécimaux';

  @override
  String get reviewTransaction => 'Vérifier la transaction';

  @override
  String get reviewAndConfirm => 'Vérifier et confirmer';

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
      'Les transactions sont irréversibles. Vérifiez l\'adresse avant de confirmer.';

  @override
  String get back => 'Retour';

  @override
  String get confirmAndSend => 'Confirmer et envoyer';

  @override
  String get transactionSent => 'Transaction envoyée !';

  @override
  String get transactionBroadcast =>
      'Votre transaction a été diffusée sur le réseau.';

  @override
  String get transactionHash => 'Hash de la transaction';

  @override
  String get sendAnother => 'Envoyer une autre';

  @override
  String get enterDestinationAddress => 'Entrez une adresse de destination';

  @override
  String get enterValidAmount => 'Entrez un montant valide';

  @override
  String computingPow(int seconds) {
    return 'Calcul du PoW... ${seconds}s';
  }

  @override
  String get stepFillDetails => 'Remplir les détails';

  @override
  String get stepReview => 'Vérifier';

  @override
  String get stepDone => 'Terminé';

  @override
  String get sweepFailed => 'Échec du balayage';

  @override
  String get addressBookTitle => 'Carnet d\'adresses';

  @override
  String get transactionHistory => 'Historique des transactions';

  @override
  String get searchByHash =>
      'Rechercher par hash, adresse ou identifiant de paiement…';

  @override
  String get all => 'Tout';

  @override
  String get filterReceived => 'Reçu';

  @override
  String get filterSent => 'Envoyé';

  @override
  String get refresh => 'Actualiser';

  @override
  String get noTransactionsFound => 'Aucune transaction trouvée';

  @override
  String get confirmed => 'Confirmé';

  @override
  String get pending => 'En attente';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Adresse';

  @override
  String get block => 'Bloc';

  @override
  String showingRange(int start, int end, int total) {
    return 'Affichage de $start–$end sur $total';
  }

  @override
  String get previous => 'Précédent';

  @override
  String get next => 'Suivant';

  @override
  String get walletLocked => 'Portefeuille verrouillé';

  @override
  String get enterPasswordToContinue =>
      'Entrez le mot de passe de votre portefeuille pour continuer';

  @override
  String get password => 'Mot de passe';

  @override
  String get incorrectPassword => 'Mot de passe incorrect';

  @override
  String get unlock => 'Déverrouiller';

  @override
  String get closeWalletInstead => 'Fermer le portefeuille à la place';

  @override
  String get closeWallet => 'Fermer le portefeuille';

  @override
  String get closeWalletDescription =>
      'Cela sauvegardera et fermera le portefeuille.\n\nVous serez redirigé vers l\'écran de connexion.';

  @override
  String get cancel => 'Annuler';

  @override
  String get welcomeToPluton => 'Bienvenue sur PLUTON v2';

  @override
  String get selectOptionToStart => 'Sélectionnez une option pour commencer';

  @override
  String get createNewWallet => 'Créer un nouveau portefeuille';

  @override
  String get openExistingWallet => 'Ouvrir un portefeuille existant';

  @override
  String get importFromSeed => 'Importer depuis une phrase de récupération';

  @override
  String get importFromKeys => 'Importer depuis des clés privées';

  @override
  String get openWallet => 'Ouvrir le portefeuille';

  @override
  String get importFromSeedTitle =>
      'Importer depuis une phrase de récupération';

  @override
  String get importFromKeysTitle => 'Importer depuis des clés';

  @override
  String get saveWalletTo => 'Enregistrer le portefeuille dans';

  @override
  String get walletFile => 'Fichier du portefeuille';

  @override
  String get walletPassword => 'Mot de passe du portefeuille';

  @override
  String get mnemonicSeedPhrase => 'Phrase mnémonique de récupération';

  @override
  String get scanFromHeight => 'Scanner depuis la hauteur (0 = scan complet)';

  @override
  String get daemonHost => 'Hôte du daemon';

  @override
  String get port => 'Port';

  @override
  String get continueButton => 'Continuer';

  @override
  String get browse => 'Parcourir';

  @override
  String get backupWarning =>
      'Sauvegardez votre portefeuille avant de continuer.\nCes clés ne peuvent pas être récupérées en cas de perte.';

  @override
  String get yourWalletAddress => 'Votre adresse de portefeuille';

  @override
  String get seedPhrase25Words => 'Phrase de récupération (25 mots)';

  @override
  String get privateViewKey => 'Clé privée de visualisation';

  @override
  String get privateSpendKey => 'Clé privée de dépense';

  @override
  String get seedBackupConfirm =>
      'J\'ai noté ma phrase de récupération et mes clés privées dans un endroit sûr.';

  @override
  String get backedUpContinue =>
      'J\'ai sauvegardé mon portefeuille — Continuer';

  @override
  String get settings => 'Paramètres';

  @override
  String get sectionDaemonNode => 'Nœud du daemon';

  @override
  String get nodeDescription =>
      'Connectez-vous à un nœud daemon local ou distant. Les modifications prennent effet immédiatement.';

  @override
  String get hostIpAddress => 'Hôte / adresse IP';

  @override
  String get ssl => 'SSL';

  @override
  String get apply => 'Appliquer';

  @override
  String get nodeUpdatedSuccess => 'Nœud mis à jour avec succès';

  @override
  String get nodeUnreachable =>
      'Impossible de joindre le nœud actuel. Entrez une nouvelle adresse de nœud ci-dessous et appuyez sur Appliquer.';

  @override
  String get sectionWallet => 'Portefeuille';

  @override
  String get saveWallet => 'Sauvegarder le portefeuille';

  @override
  String get saveWalletSubtitle => 'Écrire l\'état actuel sur le disque';

  @override
  String get walletSaved => 'Portefeuille sauvegardé';

  @override
  String get exportToJson => 'Exporter en JSON';

  @override
  String get exportToJsonSubtitle =>
      'Sauvegarder les données du portefeuille en fichier JSON';

  @override
  String get exportJsonTitle => 'Exporter le JSON du portefeuille';

  @override
  String exportedTo(String path) {
    return 'Exporté vers $path';
  }

  @override
  String exportFailed(String error) {
    return 'Échec de l\'exportation : $error';
  }

  @override
  String get resetScanHeight => 'Réinitialiser la hauteur de scan';

  @override
  String get resetScanHeightSubtitle =>
      'Rescanner la blockchain depuis une hauteur spécifique';

  @override
  String get resetScanHeightDescription =>
      'Entrez une hauteur de bloc à partir de laquelle rescanner. Utilisez 0 pour un rescan complet.';

  @override
  String get scanHeight => 'Hauteur de scan';

  @override
  String get reset => 'Réinitialiser';

  @override
  String get autosave => 'Sauvegarde automatique';

  @override
  String get autosaveSubtitle =>
      'Sauvegarder le portefeuille sur le disque après la synchronisation et toutes les 5 minutes';

  @override
  String get scanCoinbaseTx => 'Scanner les transactions Coinbase';

  @override
  String get scanCoinbaseSubtitle =>
      'Inclure les récompenses de minage lors de la synchronisation (désactivé par défaut)';

  @override
  String get sectionAppearance => 'Apparence';

  @override
  String get theme => 'Thème';

  @override
  String get themeSubtitle => 'Choisir le schéma de couleurs de l\'application';

  @override
  String get themeSystem => 'Système';

  @override
  String get themeLight => 'Clair';

  @override
  String get themeDark => 'Sombre';

  @override
  String get sectionNotifications => 'Notifications';

  @override
  String get incomingTxAlerts => 'Alertes de transactions entrantes';

  @override
  String get incomingTxAlertsSubtitle =>
      'Afficher une notification de bureau lorsque des WRKZ sont reçus';

  @override
  String get sectionDebugLogs => 'Débogage et journaux';

  @override
  String get logLevel => 'Niveau de journalisation';

  @override
  String get logLevelSubtitle =>
      'Contrôle la verbosité de la bibliothèque du portefeuille';

  @override
  String get viewLogs => 'Voir les journaux';

  @override
  String get viewLogsSubtitle =>
      'Sortie en direct du journal de la bibliothèque du portefeuille';

  @override
  String get walletLogs => 'Journaux du portefeuille';

  @override
  String logEntries(int count) {
    return '$count entrées';
  }

  @override
  String get autoScroll => 'Défilement automatique';

  @override
  String get copyAll => 'Tout copier';

  @override
  String get clear => 'Effacer';

  @override
  String get close => 'Fermer';

  @override
  String get noLogsYet =>
      'Aucun journal pour le moment. Définissez un niveau de journalisation supérieur à Désactivé pour voir la sortie.';

  @override
  String get logsCopied => 'Journaux copiés dans le presse-papiers';

  @override
  String get sectionDangerZone => 'Zone de danger';

  @override
  String get deleteWalletData => 'Supprimer les données du portefeuille';

  @override
  String get deleteWalletDataSubtitle =>
      'Supprimer définitivement le fichier du portefeuille du disque';

  @override
  String get deleteWalletWarning =>
      'Cela supprimera définitivement votre fichier de portefeuille du disque.\n\nAssurez-vous d\'avoir sauvegardé votre phrase de récupération et vos clés privées avant de continuer. Cette action est irréversible.';

  @override
  String get iUnderstandContinue => 'Je comprends, continuer';

  @override
  String get finalConfirmation => 'Confirmation finale';

  @override
  String get typeDeleteToConfirm => 'Tapez SUPPRIMER pour confirmer :';

  @override
  String get deleteHint => 'SUPPRIMER';

  @override
  String get deletePermanently => 'Supprimer définitivement';

  @override
  String get aboutTitle => 'À propos';

  @override
  String versionInfo(String version) {
    return 'Version $version — Portefeuille de bureau WRKZ';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 est le portefeuille de bureau officiel pour WrkzCoin (WRKZ), une cryptomonnaie rapide et légère basée sur CryptoNote.\n\nCréé avec Flutter, propulsé par wallet-api.';

  @override
  String get github => 'GitHub';

  @override
  String get githubSubtitle => 'Voir le code source et les versions';

  @override
  String get discord => 'Discord';

  @override
  String get discordSubtitle => 'Rejoindre la communauté';

  @override
  String get twitterX => 'Twitter / X';

  @override
  String get twitterXSubtitle => 'Suivre @wrkzcoin';

  @override
  String get website => 'Site web';

  @override
  String get websiteSubtitle => 'wrkz.work';

  @override
  String get license => 'Licence';

  @override
  String get licenseText =>
      'Publié sous la licence MIT.\nUtilisation à vos propres risques. Sauvegardez toujours votre phrase de récupération.';

  @override
  String get addButton => 'Ajouter';

  @override
  String get noSavedAddresses => 'Aucune adresse enregistrée';

  @override
  String get tapAddToSave =>
      'Appuyez sur Ajouter pour enregistrer une adresse fréquemment utilisée.';

  @override
  String get addAddress => 'Ajouter une adresse';

  @override
  String get nameLabel => 'Nom / libellé';

  @override
  String get addressLabel => 'Adresse';

  @override
  String get noteOptional => 'Note (optionnel)';

  @override
  String get nameAndAddressRequired => 'Le nom et l\'adresse sont requis';

  @override
  String get invalidWrkzAddress =>
      'Adresse WRKZ invalide. Doit contenir 98 (standard), 120 (intégrée courte) ou 186 (intégrée longue) caractères commençant par « Wrkz ».';

  @override
  String get save => 'Enregistrer';

  @override
  String get editEntry => 'Modifier l\'entrée';

  @override
  String get deleteEntry => 'Supprimer l\'entrée';

  @override
  String removeFromAddressBook(String name) {
    return 'Supprimer « $name » de votre carnet d\'adresses ?';
  }

  @override
  String get delete => 'Supprimer';

  @override
  String get edit => 'Modifier';

  @override
  String get wrkzReceived => 'WRKZ reçus';

  @override
  String youReceivedAmount(String amount) {
    return 'Vous avez reçu $amount';
  }

  @override
  String get show => 'Afficher';

  @override
  String get exit => 'Quitter';

  @override
  String get plutonWallet => 'Portefeuille PLUTON';

  @override
  String get language => 'Langue';

  @override
  String get selectLanguage => 'Sélectionner la langue';

  @override
  String get chooseLanguage => 'Choisissez votre langue préférée';

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
      'Cette transaction n\'est plus valide. Revenez en arrière et recréez-la.';

  @override
  String get deleteConfirmMismatch => 'Tapez exactement DELETE pour confirmer.';

  @override
  String get unlockNeedsReopen =>
      'Impossible de vérifier le mot de passe sur cet appareil. Utilisez « Fermer le portefeuille » puis rouvrez-le.';

  @override
  String get exportJsonWarningTitle => 'Exporter le portefeuille non chiffré ?';

  @override
  String get exportJsonWarningBody =>
      'Le fichier exporté contient votre clé de vue privée et vos clés de dépense privées en clair. Quiconque le lit peut dépenser vos fonds.\n\nEnregistrez-le uniquement sur un support que vous contrôlez et supprimez-le dès que vous avez terminé.';

  @override
  String passwordTooShort(int count) {
    return 'Le mot de passe doit comporter au moins $count caractères';
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
  String get sectionLocalNode => 'Nœud allégé local';

  @override
  String get localNodeDescription =>
      'Faites tourner un nœud sur cet ordinateur et synchronisez dessus plutôt que sur un serveur distant. Un nœud allégé ne stocke que ce dont un portefeuille a besoin, mais télécharge quand même toute la chaîne une fois.';

  @override
  String get localNodeSetUp => 'Configurer le nœud local';

  @override
  String get localNodeStart => 'Démarrer';

  @override
  String get localNodeStop => 'Arrêter';

  @override
  String get localNodeUse => 'Utiliser ce nœud';

  @override
  String get localNodeDelete => 'Supprimer les données du nœud';

  @override
  String get localNodeStateStopped => 'Arrêté';

  @override
  String get localNodeStateStarting => 'Démarrage';

  @override
  String get localNodeStateSyncing => 'Synchronisation';

  @override
  String get localNodeStateReady => 'Synchronisé';

  @override
  String get localNodeStateFailed => 'Échec';

  @override
  String localNodeProgress(int height, int network) {
    return 'Bloc $height sur $network';
  }

  @override
  String localNodePeers(int count) {
    return '$count pairs';
  }

  @override
  String get localNodeNotReadyYet =>
      'Le nœud local est encore en train de rattraper son retard et ne peut pas encore servir le portefeuille. Il continue de se synchroniser en arrière-plan — restez sur un nœud distant jusqu\'à ce qu\'il soit prêt, puis basculez.';

  @override
  String get localNodeInUse => 'Le portefeuille est connecté à ce nœud.';

  @override
  String localNodeBinaryMissing(String name) {
    return '$name est introuvable. Placez le binaire du démon à côté de l\'exécutable du portefeuille, ou dans un dossier « sidecar » adjacent, puis réessayez.';
  }

  @override
  String get localNodeSetupTitle => 'Configurer un nœud allégé local';

  @override
  String get localNodeSetupCost =>
      'Avant de commencer :\n• Environ 6 Go d\'espace disque, et toute la chaîne est téléchargée une fois.\n• La première synchronisation dure des heures. Elle continue en arrière-plan et vous pouvez utiliser un nœud distant entre-temps.\n• La hauteur de départ ci-dessous est définitive. La changer plus tard implique de supprimer le nœud et de resynchroniser depuis zéro.';

  @override
  String get localNodeStartHeightLabel => 'Hauteur de départ';

  @override
  String localNodeStartHeightHelp(int height) {
    return 'Les blocs sous cette hauteur sont téléchargés et vérifiés, puis seul l\'index dont les blocs suivants ont besoin est conservé. Gardez-la égale ou inférieure à la hauteur de départ de ce portefeuille ($height).';
  }

  @override
  String localNodeStartHeightTooHigh(int height) {
    return 'Supérieure à la hauteur de départ de ce portefeuille ($height). Le nœud ne pourrait jamais afficher ses transactions plus anciennes.';
  }

  @override
  String get localNodeCreate => 'Créer le nœud';

  @override
  String get localNodeDeleteTitle => 'Supprimer les données du nœud local ?';

  @override
  String get localNodeDeleteWarning =>
      'Cela arrête le nœud et supprime définitivement sa base de données de la chaîne du disque. Votre portefeuille, sa phrase de récupération et ses fonds ne sont pas touchés — mais un nouveau nœud local resynchronisera depuis zéro, ce qui prend des heures.';

  @override
  String get localNodeDeleted => 'Nœud local supprimé.';

  @override
  String localNodeDiskUsage(String size) {
    return '$size sur le disque';
  }

  @override
  String get nodePresetRemote => 'Nœud distant';

  @override
  String get nodePresetLocal => 'Nœud allégé local';

  @override
  String get savingWallet => 'Enregistrement de votre portefeuille…';

  @override
  String get savingWalletBody =>
      'PLUTON écrit votre portefeuille sur le disque. Cela peut prendre un moment sur un gros portefeuille.';

  @override
  String get shutdownTakingLong =>
      'Cela prend plus de temps que prévu. Quitter maintenant peut endommager le fichier du portefeuille — ne le faites que si PLUTON est bloqué.';

  @override
  String get quitAnyway => 'Quitter quand même';

  @override
  String get stillRunningInTray =>
      'PLUTON fonctionne toujours dans la zone de notification. Cliquez sur l\'icône pour le rouvrir, ou faites un clic droit et choisissez Quitter.';
}
