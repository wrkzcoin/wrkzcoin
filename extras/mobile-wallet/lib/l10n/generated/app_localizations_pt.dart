// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Portuguese (`pt`).
class SPt extends S {
  SPt([String locale = 'pt']) : super(locale);

  @override
  String get appTitle => 'PLUTON Mobile';

  @override
  String get tabOverview => 'Visão Geral';

  @override
  String get tabReceive => 'Receber';

  @override
  String get tabSend => 'Enviar';

  @override
  String get tabHistory => 'Histórico';

  @override
  String get tabSettings => 'Configurações';

  @override
  String get send => 'Enviar';

  @override
  String get receive => 'Receber';

  @override
  String get available => 'Disponível';

  @override
  String get locked => 'Bloqueado';

  @override
  String get total => 'Total';

  @override
  String lockedAmount(String amount) {
    return 'Bloqueado: $amount';
  }

  @override
  String totalAmount(String amount) {
    return 'Total: $amount';
  }

  @override
  String get recentTransactions => 'Transações Recentes';

  @override
  String get viewAll => 'Ver tudo';

  @override
  String get noTransactionsYet => 'Nenhuma transação ainda';

  @override
  String get noMatchingTransactions => 'Nenhuma transação correspondente';

  @override
  String get pending => 'Pendente...';

  @override
  String get justNow => 'Agora mesmo';

  @override
  String minutesAgo(int count) {
    return 'há ${count}m';
  }

  @override
  String hoursAgo(int count) {
    return 'há ${count}h';
  }

  @override
  String daysAgo(int count) {
    return 'há ${count}d';
  }

  @override
  String get received => 'Recebido';

  @override
  String get sent => 'Enviado';

  @override
  String get networkStatus => 'Status da Rede';

  @override
  String get node => 'Nó';

  @override
  String get status => 'Status';

  @override
  String get connected => 'Conectado';

  @override
  String get disconnected => 'Desconectado';

  @override
  String get walletHeight => 'Altura da Carteira';

  @override
  String get networkHeight => 'Altura da Rede';

  @override
  String get peers => 'Peers';

  @override
  String get type => 'Tipo';

  @override
  String get viewOnly => 'Somente visualização';

  @override
  String get couldNotFetchStatus =>
      'Não foi possível obter o status. Verifique seu nó nas Configurações.';

  @override
  String errorPrefix(String message) {
    return 'Erro: $message';
  }

  @override
  String get seedBackupWarning =>
      'Faça backup da sua frase semente nas Configurações para proteger seus fundos.';

  @override
  String get noConnectionToDaemon => 'Sem conexão com o daemon';

  @override
  String syncingPercent(String percent) {
    return 'Sincronizando $percent%';
  }

  @override
  String get yourAddress => 'Seu Endereço';

  @override
  String get errorLoadingAddress => 'Erro ao carregar endereço';

  @override
  String get integratedAddress => 'Endereço Integrado';

  @override
  String get embedPaymentId => 'Incorporar um ID de pagamento ao seu endereço';

  @override
  String get randomShort => 'Curto Aleatório (16)';

  @override
  String get randomLong => 'Longo Aleatório (64)';

  @override
  String get enterCustomPaymentId =>
      'Ou insira um ID de pagamento personalizado (16 ou 64 hex)';

  @override
  String get enterPaymentId => 'Insira um ID de pagamento';

  @override
  String get paymentIdInvalid =>
      'O ID de pagamento deve ter 16 ou 64 caracteres hexadecimais';

  @override
  String get shortPid => 'PID Curto';

  @override
  String get longPid => 'PID Longo';

  @override
  String get share => 'Compartilhar';

  @override
  String get copy => 'Copiar';

  @override
  String get sweepAllFunds => 'Varrer Todos os Fundos';

  @override
  String get normalSend => 'Envio Normal';

  @override
  String get sweep => 'Varrer';

  @override
  String get recipientAddress => 'Endereço do Destinatário';

  @override
  String get scanQr => 'Escanear QR';

  @override
  String get amount => 'Valor';

  @override
  String availableBalance(String amount) {
    return 'Disponível: $amount';
  }

  @override
  String sweepInfo(String amount) {
    return 'A varredura consolida todos os UTXOs e envia todo o seu saldo desbloqueado ($amount) menos as taxas.';
  }

  @override
  String get paymentIdOptional => 'ID de pagamento (opcional)';

  @override
  String get hexCharacters => '16 ou 64 caracteres hexadecimais';

  @override
  String get mustBeHex => 'Deve ter 16 ou 64 caracteres hexadecimais';

  @override
  String get recipientRequired => 'O endereço do destinatário é obrigatório';

  @override
  String get invalidAddress => 'Endereço WRKZ inválido';

  @override
  String get enterValidAmount => 'Insira um valor válido';

  @override
  String get reviewTransaction => 'Revisar Transação';

  @override
  String get to => 'Para';

  @override
  String get fee => 'Taxa';

  @override
  String get totalDeducted => 'Total Deduzido';

  @override
  String get paymentId => 'ID de Pagamento';

  @override
  String get transactionsIrreversible =>
      'As transações são irreversíveis. Por favor, verifique os detalhes.';

  @override
  String get back => 'Voltar';

  @override
  String get confirmAndSend => 'Confirmar e Enviar';

  @override
  String get transactionSent => 'Transação Enviada!';

  @override
  String get transactionHash => 'Hash da Transação';

  @override
  String get sendAnother => 'Enviar Outra';

  @override
  String get scanQrCode => 'Escanear Código QR';

  @override
  String get scannedAddress => 'Endereço Escaneado';

  @override
  String get cancel => 'Cancelar';

  @override
  String get useThisAddress => 'Usar este endereço';

  @override
  String get sweepFailed => 'Varredura falhou';

  @override
  String get searchPlaceholder =>
      'Buscar por hash, endereço, ID de pagamento...';

  @override
  String get all => 'Todos';

  @override
  String get filterReceived => 'Recebido';

  @override
  String get filterSent => 'Enviado';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Endereço';

  @override
  String get block => 'Bloco';

  @override
  String get confirmed => 'Confirmado';

  @override
  String get password => 'Senha';

  @override
  String get unlock => 'Desbloquear';

  @override
  String get switchWallet => 'Trocar Carteira';

  @override
  String get enterPasswordToUnlock => 'Digite sua senha para desbloquear';

  @override
  String get incorrectPassword => 'Senha incorreta';

  @override
  String get enterYourPassword => 'Digite sua senha';

  @override
  String get plutonMobile => 'PLUTON Mobile';

  @override
  String get createFirstWalletSubtitle =>
      'Crie sua primeira carteira para começar';

  @override
  String get selectWalletSubtitle => 'Selecione uma carteira para abrir';

  @override
  String get yourWallets => 'Suas Carteiras';

  @override
  String get noWalletsYet => 'Nenhuma carteira ainda';

  @override
  String get lastOpened => 'Aberta pela última vez';

  @override
  String createdDate(String date) {
    return 'Criada em $date';
  }

  @override
  String get createFirstWallet => 'Criar Primeira Carteira';

  @override
  String get addWallet => 'Adicionar Carteira';

  @override
  String get deleteWallet => 'Excluir Carteira';

  @override
  String deleteWalletConfirm(String name) {
    return 'Excluir \"$name\"?\n\nIsso removerá permanentemente o arquivo e as chaves da carteira. Certifique-se de ter feito backup da sua frase semente.';
  }

  @override
  String get delete => 'Excluir';

  @override
  String get createNewWallet => 'Criar Nova Carteira';

  @override
  String get createNewWalletSubtitle =>
      'Gerar uma nova carteira com uma frase semente nova';

  @override
  String get importFromSeed => 'Importar da Frase Semente';

  @override
  String get importFromSeedSubtitle =>
      'Restaurar carteira usando sua semente mnemônica de 25 palavras';

  @override
  String get importFromKeys => 'Importar de Chaves Privadas';

  @override
  String get importFromKeysSubtitle =>
      'Restaurar usando chave de gasto e chave de visualização';

  @override
  String get viewOnlyWallet => 'Carteira Somente Leitura';

  @override
  String get viewOnlyWalletSubtitle =>
      'Carteira de monitoramento usando chave de visualização e endereço';

  @override
  String get createWallet => 'Criar Carteira';

  @override
  String get importWallet => 'Importar Carteira';

  @override
  String get walletName => 'Nome da Carteira';

  @override
  String get walletNameHint => 'ex.: Carteira Principal';

  @override
  String get passwordLabel => 'Senha';

  @override
  String get enterPassword => 'Digite a senha';

  @override
  String get confirmPassword => 'Confirmar senha';

  @override
  String get seedPhrase => 'Frase Semente (25 palavras)';

  @override
  String get enterSeedPhrase => 'Digite sua frase semente...';

  @override
  String get scanHeight => 'Altura de Escaneamento (opcional)';

  @override
  String get scanHeightHint => '0 = escanear do início';

  @override
  String get privateSpendKey => 'Chave de Gasto Privada';

  @override
  String get privateViewKey => 'Chave de Visualização Privada';

  @override
  String get walletAddress => 'Endereço da Carteira';

  @override
  String get walletAddressHint => 'Endereço Wrkz...';

  @override
  String get hexKey => '64 caracteres hex';

  @override
  String get daemonNode => 'Nó do Daemon';

  @override
  String get custom => 'Personalizado';

  @override
  String get host => 'Host';

  @override
  String get hostHint => 'Host / IP';

  @override
  String get port => 'Porta';

  @override
  String get ssl => 'SSL';

  @override
  String get walletNameRequired => 'O nome da carteira é obrigatório';

  @override
  String get passwordRequired => 'A senha é obrigatória';

  @override
  String passwordTooShort(int count) {
    return 'A senha deve ter pelo menos $count caracteres';
  }

  @override
  String get passwordsDoNotMatch => 'As senhas não coincidem';

  @override
  String get seedRequired => 'A frase semente é obrigatória';

  @override
  String get spendKeyRequired => 'A chave de gasto é obrigatória';

  @override
  String get viewKeyRequired => 'A chave de visualização é obrigatória';

  @override
  String get addressRequired => 'O endereço é obrigatório';

  @override
  String get daemonHostRequired => 'O host do daemon é obrigatório';

  @override
  String get backupSeedTitle => 'Backup da Sua Semente';

  @override
  String get backupWarning =>
      'Anote sua frase semente e guarde-a em segurança. Se você a perder, seus fundos serão perdidos para sempre.';

  @override
  String get seedPhraseLabel => 'Frase Semente';

  @override
  String get privateViewKeyLabel => 'Chave de Visualização Privada';

  @override
  String get privateSpendKeyLabel => 'Chave de Gasto Privada';

  @override
  String get backupConfirmCheck =>
      'Fiz backup da minha frase semente com segurança';

  @override
  String get continueToWallet => 'Continuar para a Carteira';

  @override
  String get sectionDaemonNode => 'Nó do Daemon';

  @override
  String get apply => 'Aplicar';

  @override
  String nodeUpdated(String host, int port) {
    return 'Nó atualizado para $host:$port';
  }

  @override
  String get hostRequired => 'O host é obrigatório';

  @override
  String currentWallet(String name) {
    return 'Carteira Atual — $name';
  }

  @override
  String get saveWallet => 'Salvar Carteira';

  @override
  String get walletSaved => 'Carteira salva';

  @override
  String saveFailed(String error) {
    return 'Falha ao salvar: $error';
  }

  @override
  String get backupSeed => 'Backup da Semente';

  @override
  String get changePassword => 'Alterar Senha';

  @override
  String get resetScanHeight => 'Redefinir Altura de Escaneamento';

  @override
  String get reset => 'Redefinir';

  @override
  String resetScanConfirm(int height) {
    return 'Isso irá reescanear a blockchain a partir do bloco $height. Isso pode demorar um pouco. Continuar?';
  }

  @override
  String scanResetTo(int height) {
    return 'Escaneamento redefinido para o bloco $height';
  }

  @override
  String resetFailed(String error) {
    return 'Falha ao redefinir: $error';
  }

  @override
  String get enterPasswordTitle => 'Digite a Senha';

  @override
  String get confirm => 'Confirmar';

  @override
  String get seedBackup => 'Backup da Semente';

  @override
  String get seedPhraseColon => 'Frase Semente:';

  @override
  String get privateViewKeyColon => 'Chave de Visualização Privada:';

  @override
  String get iveBackedUp => 'Fiz o backup';

  @override
  String get currentPasswordLabel => 'Senha atual';

  @override
  String get newPasswordLabel => 'Nova senha';

  @override
  String get confirmNewPasswordLabel => 'Confirmar nova senha';

  @override
  String get change => 'Alterar';

  @override
  String get currentPasswordIncorrect => 'A senha atual está incorreta';

  @override
  String get newPasswordsDoNotMatch => 'As novas senhas não coincidem';

  @override
  String get passwordChanged => 'Senha alterada';

  @override
  String get walletManagement => 'Gerenciamento de Carteiras';

  @override
  String get switchWalletSubtitle => 'Salvar e fechar, escolher outra';

  @override
  String get manageWallets => 'Gerenciar Carteiras';

  @override
  String get manageWalletsSubtitle => 'Renomear ou excluir carteiras';

  @override
  String get currentlyOpen => '(aberta no momento)';

  @override
  String get close => 'Fechar';

  @override
  String get renameWallet => 'Renomear Carteira';

  @override
  String get newName => 'Novo nome';

  @override
  String get rename => 'Renomear';

  @override
  String deleteWalletConfirmShort(String name) {
    return 'Excluir \"$name\"? Esta ação não pode ser desfeita.';
  }

  @override
  String get security => 'Segurança';

  @override
  String get biometricUnlock => 'Desbloqueio Biométrico';

  @override
  String get biometricSubtitle => 'Impressão digital / Face ID';

  @override
  String get biometricNotAvailable => 'Biometria não disponível';

  @override
  String get autoLock => 'Bloqueio Automático';

  @override
  String get appearance => 'Aparência';

  @override
  String get theme => 'Tema';

  @override
  String get themeAuto => 'Automático';

  @override
  String get themeLight => 'Claro';

  @override
  String get themeDark => 'Escuro';

  @override
  String get preferences => 'Preferências';

  @override
  String get transactionNotifications => 'Notificações de Transações';

  @override
  String get notificationsSubtitle => 'Alertar em transações recebidas';

  @override
  String get autosave => 'Salvamento Automático';

  @override
  String get autosaveSubtitle =>
      'Salvar após sincronização, depois a cada 5 minutos';

  @override
  String get scanCoinbaseTx => 'Escanear Transações Coinbase';

  @override
  String get scanCoinbaseSubtitle =>
      'Incluir recompensas de mineração (desativado por padrão)';

  @override
  String get dangerZone => 'Zona de Perigo';

  @override
  String get deleteCurrentWallet => 'Excluir Carteira Atual';

  @override
  String get deleteCurrentWalletSubtitle =>
      'Remover permanentemente os dados da carteira';

  @override
  String get deleteWalletTypeCaps =>
      'Isso excluirá permanentemente o arquivo e as chaves da carteira. Certifique-se de ter feito backup da sua frase semente.\n\nDigite DELETE para confirmar:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get language => 'Idioma';

  @override
  String get selectLanguage => 'Selecionar Idioma';

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
  String get autoLockImmediately => 'Imediatamente';

  @override
  String get autoLock1Min => '1 minuto';

  @override
  String get autoLock5Min => '5 minutos';

  @override
  String get autoLockNever => 'Nunca';

  @override
  String get preparedTransactionExpired =>
      'Esta transação já não é válida. Volte atrás e crie-a novamente.';

  @override
  String get deleteConfirmMismatch =>
      'Digite exatamente DELETE para confirmar.';

  @override
  String get seedNotBackedUpWarning =>
      'Não confirmou uma cópia de segurança da frase semente desta carteira. Eliminá-la agora significa que os fundos não poderão ser recuperados.';

  @override
  String get wrkzReceived => 'WRKZ recebidos';

  @override
  String get retry => 'Tentar novamente';

  @override
  String youReceivedAmount(String amount) {
    return 'Recebeu $amount';
  }

  @override
  String get ringSize => 'Tamanho do anel';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Tamanho do anel reduzido para $actual (normalmente $normal). Os valores enviados não têm saídas suficientes na cadeia para formar um anel completo, por isso esta transação é menos privada do que o habitual.';
  }

  @override
  String get liteNodeTitle => 'Nó leve';

  @override
  String liteNodeServesFrom(int height) {
    return 'Este nó só guarda blocos a partir de $height. Transações anteriores a esse bloco não podem ser encontradas através dele.';
  }

  @override
  String liteNodeMissesHistory(int nodeHeight, int walletHeight) {
    return 'Este nó começa no bloco $nodeHeight, mas esta carteira começa no bloco $walletHeight. Tudo o que foi recebido entre os dois é invisível aqui, por isso o saldo apresentado pode estar baixo demais. Ligue-se a um nó com a cadeia completa para o ver.';
  }

  @override
  String liteNodeSyncStalled(int wallet, int node) {
    return 'Sincronização parada no bloco $wallet. Este nó não guarda nada abaixo do bloco $node, por isso os blocos intermédios não podem ser transferidos dele. O saldo fica incompleto até ligar um nó com a cadeia completa.';
  }

  @override
  String get liteNodeRescanRefusedTitle =>
      'Este nó não consegue reanalisar tão atrás';

  @override
  String liteNodeRescanRefused(int height) {
    return 'O nó ligado é um nó leve sem dados de bloco abaixo de $height. Reanalisar a partir de um ponto mais baixo descartaria transações que esta carteira já encontrou, sem forma de as recuperar aqui. Nada foi alterado.';
  }

  @override
  String liteNodeRescanFromInstead(int height) {
    return 'Reanalisar a partir de $height';
  }

  @override
  String liteNodeRescanHint(int height) {
    return 'O nó ligado só pode reanalisar a partir do bloco $height ou acima.';
  }

  @override
  String get nodeServesFromLabel => 'Fornece blocos a partir de';

  @override
  String get nodeFullChain => 'Cadeia completa';

  @override
  String get localNodeMobileFuture =>
      'Executar o nó no próprio telemóvel está planeado, mas ainda não está disponível — um nó precisa de vários GB de armazenamento e horas de sincronização. Até lá, aponte esta carteira para um nó seu.';

  @override
  String get syncStoppedTitle => 'Sincronização parada';

  @override
  String syncGapStalled(int covered, int servesFrom) {
    return 'Sincronização parada no bloco $covered. O nó com que falava só responde a partir do bloco $servesFrom, então os blocos intermediários não podem ser baixados dele. O saldo fica incompleto até você conectar um nó com a cadeia inteira.';
  }

  @override
  String get txPowServerSection => 'Servidor PoW de transações';

  @override
  String get txPowServerUse => 'Usar um servidor PoW externo';

  @override
  String get txPowServerSubtitle =>
      'Enviar a prova de trabalho da transação para um servidor em vez de calculá-la neste dispositivo. Se o servidor não responder, a CPU deste dispositivo é usada.';

  @override
  String get txPowServerSaved => 'Configurações do servidor PoW salvas';

  @override
  String get txPowServerInvalid => 'Informe um host e uma porta válidos';

  @override
  String get txPowServerTest => 'Testar';

  @override
  String txPowServerTestOk(int ms, int threads, int queue, int capacity) {
    return 'Servidor acessível em $ms ms: $threads threads, $queue de $capacity vagas da fila em uso';
  }

  @override
  String txPowServerTestFailed(String error) {
    return 'Servidor inacessível: $error';
  }
}
