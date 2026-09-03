// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Portuguese (`pt`).
class SPt extends S {
  SPt([String locale = 'pt']) : super(locale);

  @override
  String get appTitle => 'PLUTON v2';

  @override
  String get tabOverview => 'Visão Geral';

  @override
  String get tabReceive => 'Receber';

  @override
  String get tabTransfer => 'Transferir';

  @override
  String get tabHistory => 'Histórico';

  @override
  String get tabAddressBook => 'Agenda';

  @override
  String get tabSettings => 'Configurações';

  @override
  String get tabAbout => 'Sobre';

  @override
  String get lockWallet => 'Bloquear Carteira';

  @override
  String get send => 'Enviar';

  @override
  String get receive => 'Receber';

  @override
  String get transfer => 'Transferir';

  @override
  String get available => 'Disponível';

  @override
  String get locked => 'Bloqueado';

  @override
  String get total => 'Total';

  @override
  String get availableBalance => 'Saldo Disponível';

  @override
  String lockedUnconfirmed(String amount, String ticker) {
    return 'Bloqueado (não confirmado): $amount $ticker';
  }

  @override
  String totalBalance(String amount, String ticker) {
    return 'Total: $amount $ticker';
  }

  @override
  String get balanceIncompleteWhileSyncing =>
      'O saldo pode estar incompleto durante a sincronização';

  @override
  String errorPrefix(String message) {
    return 'Erro: $message';
  }

  @override
  String get network => 'Rede';

  @override
  String get syncStatus => 'Status de sincronização';

  @override
  String get synced => 'Sincronizado';

  @override
  String get syncing => 'Sincronizando…';

  @override
  String get walletBlock => 'Bloco da carteira';

  @override
  String get networkBlock => 'Bloco da rede';

  @override
  String get peers => 'Pares';

  @override
  String get walletType => 'Tipo de carteira';

  @override
  String get viewOnly => 'Somente leitura';

  @override
  String get full => 'Completa';

  @override
  String get nodeConnectionIssue => 'Problema de conexão com o nó';

  @override
  String get switchNodeInSettings => 'Altere o nó em Configurações →';

  @override
  String get recentTransactions => 'Transações Recentes';

  @override
  String get viewAll => 'Ver tudo →';

  @override
  String get noTransactionsYet => 'Nenhuma transação ainda';

  @override
  String get received => 'Recebido';

  @override
  String get sent => 'Enviado';

  @override
  String syncingProgress(String pct, int wallet, int network) {
    return 'Sincronizando $pct% (bloco $wallet / $network)';
  }

  @override
  String get shareAddressSubtitle =>
      'Compartilhe seu endereço para receber WRKZ';

  @override
  String get yourAddress => 'Seu Endereço';

  @override
  String get generateIntegratedAddress => 'Gerar Endereço Integrado';

  @override
  String get integratedAddressDescription =>
      'Combine seu endereço com um ID de pagamento. Use os botões aleatórios para um novo ID ou insira o seu abaixo.';

  @override
  String get randomShort16 => 'Aleatório Curto (16)';

  @override
  String get randomLong64 => 'Aleatório Longo (64)';

  @override
  String get customPaymentIdLabel =>
      'ID de pagamento personalizado (16 ou 64 caracteres hex)';

  @override
  String get generate => 'Gerar';

  @override
  String get integratedAddress => 'Endereço Integrado';

  @override
  String get paymentIdShort => 'Curto (16)';

  @override
  String get paymentIdLong => 'Longo (64)';

  @override
  String paymentIdLabel(String label) {
    return 'ID de Pagamento · $label';
  }

  @override
  String get enterPaymentIdError =>
      'Insira um ID de pagamento (16 ou 64 caracteres hex)';

  @override
  String get paymentIdInvalidError =>
      'O ID de pagamento deve ter 16 ou 64 caracteres hexadecimais';

  @override
  String get copyAddress => 'Copiar endereço';

  @override
  String get copyPaymentId => 'Copiar ID de pagamento';

  @override
  String get copy => 'Copiar';

  @override
  String get copied => 'Copiado!';

  @override
  String get sendWrkzToAny => 'Enviar WRKZ para qualquer endereço';

  @override
  String get sweepAllDescription =>
      'Enviar todos os fundos para um endereço (consolida UTXOs)';

  @override
  String get sweepAll => 'Varrer tudo';

  @override
  String get sweepWarning =>
      'Varrer consolida todos os UTXOs em uma única saída. Use quando as transações falharem devido a muitas entradas.';

  @override
  String sweepAvailableBalance(String amount, String ticker) {
    return 'Disponível: $amount $ticker (todo o saldo será enviado menos a taxa)';
  }

  @override
  String get destinationAddress => 'Endereço de destino';

  @override
  String get addressBook => 'Agenda de endereços';

  @override
  String get sweepAllFunds => 'Varrer Todos os Fundos';

  @override
  String get recipientAddress => 'Endereço do destinatário';

  @override
  String get amount => 'Quantia';

  @override
  String get paymentIdOptional => 'ID de pagamento (opcional)';

  @override
  String get hexCharacters => '16 ou 64 caracteres hexadecimais';

  @override
  String get reviewTransaction => 'Revisar Transação';

  @override
  String get reviewAndConfirm => 'Revisar e Confirmar';

  @override
  String get to => 'Para';

  @override
  String get fee => 'Taxa';

  @override
  String get totalDeducted => 'Total deduzido';

  @override
  String get paymentId => 'ID de Pagamento';

  @override
  String get transactionsIrreversible =>
      'As transações são irreversíveis. Verifique o endereço antes de confirmar.';

  @override
  String get back => 'Voltar';

  @override
  String get confirmAndSend => 'Confirmar e Enviar';

  @override
  String get transactionSent => 'Transação Enviada!';

  @override
  String get transactionBroadcast =>
      'Sua transação foi transmitida para a rede.';

  @override
  String get transactionHash => 'Hash da Transação';

  @override
  String get sendAnother => 'Enviar Outra';

  @override
  String get enterDestinationAddress => 'Insira um endereço de destino';

  @override
  String get enterValidAmount => 'Insira uma quantia válida';

  @override
  String computingPow(int seconds) {
    return 'Calculando PoW... ${seconds}s';
  }

  @override
  String get stepFillDetails => 'Preencher Detalhes';

  @override
  String get stepReview => 'Revisar';

  @override
  String get stepDone => 'Concluído';

  @override
  String get sweepFailed => 'Falha na varredura';

  @override
  String get addressBookTitle => 'Agenda de Endereços';

  @override
  String get transactionHistory => 'Histórico de Transações';

  @override
  String get searchByHash => 'Pesquisar por hash, endereço ou ID de pagamento…';

  @override
  String get all => 'Todos';

  @override
  String get filterReceived => 'Recebidos';

  @override
  String get filterSent => 'Enviados';

  @override
  String get refresh => 'Atualizar';

  @override
  String get noTransactionsFound => 'Nenhuma transação encontrada';

  @override
  String get confirmed => 'Confirmada';

  @override
  String get pending => 'Pendente';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Endereço';

  @override
  String get block => 'Bloco';

  @override
  String showingRange(int start, int end, int total) {
    return 'Mostrando $start–$end de $total';
  }

  @override
  String get previous => 'Anterior';

  @override
  String get next => 'Próximo';

  @override
  String get walletLocked => 'Carteira Bloqueada';

  @override
  String get enterPasswordToContinue =>
      'Digite sua senha da carteira para continuar';

  @override
  String get password => 'Senha';

  @override
  String get incorrectPassword => 'Senha incorreta';

  @override
  String get unlock => 'Desbloquear';

  @override
  String get closeWalletInstead => 'Fechar a carteira em vez disso';

  @override
  String get closeWallet => 'Fechar Carteira';

  @override
  String get closeWalletDescription =>
      'Isso irá salvar e fechar a carteira.\n\nVocê será retornado à tela de login.';

  @override
  String get cancel => 'Cancelar';

  @override
  String get welcomeToPluton => 'Bem-vindo ao PLUTON v2';

  @override
  String get selectOptionToStart => 'Selecione uma opção para começar';

  @override
  String get createNewWallet => 'Criar Nova Carteira';

  @override
  String get openExistingWallet => 'Abrir Carteira Existente';

  @override
  String get importFromSeed => 'Importar da Frase Semente';

  @override
  String get importFromKeys => 'Importar das Chaves Privadas';

  @override
  String get openWallet => 'Abrir Carteira';

  @override
  String get importFromSeedTitle => 'Importar da Semente';

  @override
  String get importFromKeysTitle => 'Importar das Chaves';

  @override
  String get saveWalletTo => 'Salvar carteira em';

  @override
  String get walletFile => 'Arquivo da carteira';

  @override
  String get walletPassword => 'Senha da carteira';

  @override
  String get mnemonicSeedPhrase => 'Frase Semente Mnemônica';

  @override
  String get scanFromHeight =>
      'Escanear a partir da altura (0 = escaneamento completo)';

  @override
  String get daemonHost => 'Host do daemon';

  @override
  String get port => 'Porta';

  @override
  String get continueButton => 'Continuar';

  @override
  String get browse => 'Procurar';

  @override
  String get backupWarning =>
      'Faça backup da sua carteira antes de continuar.\nEssas chaves não podem ser recuperadas se perdidas.';

  @override
  String get yourWalletAddress => 'Seu Endereço da Carteira';

  @override
  String get seedPhrase25Words => 'Frase Semente (25 palavras)';

  @override
  String get privateViewKey => 'Chave Privada de Visualização';

  @override
  String get privateSpendKey => 'Chave Privada de Gasto';

  @override
  String get seedBackupConfirm =>
      'Eu anotei minha frase semente e chaves privadas em um local seguro.';

  @override
  String get backedUpContinue => 'Fiz backup da minha carteira — Continuar';

  @override
  String get settings => 'Configurações';

  @override
  String get sectionDaemonNode => 'Nó do Daemon';

  @override
  String get nodeDescription =>
      'Conecte-se a um nó daemon local ou remoto. As alterações entram em vigor imediatamente.';

  @override
  String get hostIpAddress => 'Host / Endereço IP';

  @override
  String get ssl => 'SSL';

  @override
  String get apply => 'Aplicar';

  @override
  String get nodeUpdatedSuccess => 'Nó atualizado com sucesso';

  @override
  String get nodeUnreachable =>
      'Não é possível alcançar o nó atual. Insira um novo endereço de nó abaixo e toque em Aplicar.';

  @override
  String get sectionWallet => 'Carteira';

  @override
  String get saveWallet => 'Salvar Carteira';

  @override
  String get saveWalletSubtitle => 'Gravar estado atual no disco';

  @override
  String get walletSaved => 'Carteira salva';

  @override
  String get exportToJson => 'Exportar para JSON';

  @override
  String get exportToJsonSubtitle =>
      'Salvar dados da carteira como arquivo JSON';

  @override
  String get exportJsonTitle => 'Exportar JSON da carteira';

  @override
  String exportedTo(String path) {
    return 'Exportado para $path';
  }

  @override
  String exportFailed(String error) {
    return 'Falha na exportação: $error';
  }

  @override
  String get resetScanHeight => 'Redefinir Altura de Escaneamento';

  @override
  String get resetScanHeightSubtitle =>
      'Reescanear blockchain a partir de uma altura específica';

  @override
  String get resetScanHeightDescription =>
      'Insira uma altura de bloco para reescanear. Use 0 para um escaneamento completo.';

  @override
  String get scanHeight => 'Altura de escaneamento';

  @override
  String get reset => 'Redefinir';

  @override
  String get autosave => 'Salvamento automático';

  @override
  String get autosaveSubtitle =>
      'Salvar carteira no disco após sincronização e a cada 5 minutos';

  @override
  String get scanCoinbaseTx => 'Escanear Transações Coinbase';

  @override
  String get scanCoinbaseSubtitle =>
      'Incluir recompensas de mineração ao sincronizar (desativado por padrão)';

  @override
  String get sectionAppearance => 'Aparência';

  @override
  String get theme => 'Tema';

  @override
  String get themeSubtitle => 'Escolha o esquema de cores do aplicativo';

  @override
  String get themeSystem => 'Sistema';

  @override
  String get themeLight => 'Claro';

  @override
  String get themeDark => 'Escuro';

  @override
  String get sectionNotifications => 'Notificações';

  @override
  String get incomingTxAlerts => 'Alertas de Transações Recebidas';

  @override
  String get incomingTxAlertsSubtitle =>
      'Mostrar uma notificação na área de trabalho quando WRKZ for recebido';

  @override
  String get sectionDebugLogs => 'Depuração e Logs';

  @override
  String get logLevel => 'Nível de Log';

  @override
  String get logLevelSubtitle =>
      'Controla a verbosidade da biblioteca da carteira';

  @override
  String get viewLogs => 'Ver Logs';

  @override
  String get viewLogsSubtitle =>
      'Saída de log ao vivo da biblioteca da carteira';

  @override
  String get walletLogs => 'Logs da Carteira';

  @override
  String logEntries(int count) {
    return '$count entradas';
  }

  @override
  String get autoScroll => 'Rolagem automática';

  @override
  String get copyAll => 'Copiar tudo';

  @override
  String get clear => 'Limpar';

  @override
  String get close => 'Fechar';

  @override
  String get noLogsYet =>
      'Nenhum log ainda. Defina um nível de log acima de Desativado para ver a saída.';

  @override
  String get logsCopied => 'Logs copiados para a área de transferência';

  @override
  String get sectionDangerZone => 'Zona de Perigo';

  @override
  String get deleteWalletData => 'Excluir Dados da Carteira';

  @override
  String get deleteWalletDataSubtitle =>
      'Remover permanentemente o arquivo da carteira do disco';

  @override
  String get deleteWalletWarning =>
      'Isso excluirá permanentemente o arquivo da sua carteira do disco.\n\nCertifique-se de ter feito backup da sua frase semente e chaves privadas antes de prosseguir. Esta ação não pode ser desfeita.';

  @override
  String get iUnderstandContinue => 'Eu entendo, continuar';

  @override
  String get finalConfirmation => 'Confirmação Final';

  @override
  String get typeDeleteToConfirm => 'Digite DELETE para confirmar:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get deletePermanently => 'Excluir permanentemente';

  @override
  String get aboutTitle => 'Sobre';

  @override
  String versionInfo(String version) {
    return 'Versão $version — Carteira Web WRKZ';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 é a carteira web oficial do WrkzCoin (WRKZ), uma criptomoeda rápida e leve baseada em CryptoNote.\n\nConstruída com Flutter, alimentada por wallet-api.';

  @override
  String get github => 'GitHub';

  @override
  String get githubSubtitle => 'Ver código-fonte e lançamentos';

  @override
  String get discord => 'Discord';

  @override
  String get discordSubtitle => 'Junte-se à comunidade';

  @override
  String get twitterX => 'Twitter / X';

  @override
  String get twitterXSubtitle => 'Siga @wrkzcoin';

  @override
  String get website => 'Site';

  @override
  String get websiteSubtitle => 'wrkz.work';

  @override
  String get license => 'Licença';

  @override
  String get licenseText =>
      'Lançado sob a Licença MIT.\nUse por sua própria conta e risco. Sempre faça backup da sua frase semente.';

  @override
  String get addButton => 'Adicionar';

  @override
  String get noSavedAddresses => 'Nenhum endereço salvo';

  @override
  String get tapAddToSave =>
      'Toque em Adicionar para salvar um endereço usado frequentemente.';

  @override
  String get addAddress => 'Adicionar Endereço';

  @override
  String get nameLabel => 'Nome / rótulo';

  @override
  String get addressLabel => 'Endereço';

  @override
  String get noteOptional => 'Nota (opcional)';

  @override
  String get nameAndAddressRequired => 'Nome e endereço são obrigatórios';

  @override
  String get invalidWrkzAddress =>
      'Endereço WRKZ inválido. Deve ter 98 (padrão), 120 (integrado curto) ou 186 (integrado longo) caracteres começando com \"Wrkz\".';

  @override
  String get save => 'Salvar';

  @override
  String get editEntry => 'Editar Entrada';

  @override
  String get deleteEntry => 'Excluir Entrada';

  @override
  String removeFromAddressBook(String name) {
    return 'Remover \"$name\" da sua agenda de endereços?';
  }

  @override
  String get delete => 'Excluir';

  @override
  String get edit => 'Editar';

  @override
  String get wrkzReceived => 'WRKZ Recebido';

  @override
  String youReceivedAmount(String amount) {
    return 'Você recebeu $amount';
  }

  @override
  String get show => 'Mostrar';

  @override
  String get exit => 'Sair';

  @override
  String get plutonWallet => 'Carteira PLUTON';

  @override
  String get language => 'Idioma';

  @override
  String get selectLanguage => 'Selecionar Idioma';

  @override
  String get chooseLanguage => 'Escolha seu idioma preferido';

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
  String get ringSize => 'Tamanho do anel';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Tamanho do anel reduzido para $actual (normalmente $normal). Os valores enviados não têm saídas suficientes na cadeia para formar um anel completo, por isso esta transação é menos privada do que o habitual.';
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
}
