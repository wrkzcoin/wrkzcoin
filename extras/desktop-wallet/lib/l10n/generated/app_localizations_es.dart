// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Spanish Castilian (`es`).
class SEs extends S {
  SEs([String locale = 'es']) : super(locale);

  @override
  String get appTitle => 'PLUTON v2';

  @override
  String get tabOverview => 'Resumen';

  @override
  String get tabReceive => 'Recibir';

  @override
  String get tabTransfer => 'Transferir';

  @override
  String get tabHistory => 'Historial';

  @override
  String get tabAddressBook => 'Libreta de direcciones';

  @override
  String get tabSettings => 'Configuración';

  @override
  String get tabAbout => 'Acerca de';

  @override
  String get lockWallet => 'Bloquear billetera';

  @override
  String get send => 'Enviar';

  @override
  String get receive => 'Recibir';

  @override
  String get transfer => 'Transferir';

  @override
  String get available => 'Disponible';

  @override
  String get locked => 'Bloqueado';

  @override
  String get total => 'Total';

  @override
  String get availableBalance => 'Saldo disponible';

  @override
  String lockedUnconfirmed(String amount, String ticker) {
    return 'Bloqueado (sin confirmar): $amount $ticker';
  }

  @override
  String totalBalance(String amount, String ticker) {
    return 'Total: $amount $ticker';
  }

  @override
  String get balanceIncompleteWhileSyncing =>
      'El saldo puede estar incompleto durante la sincronización';

  @override
  String errorPrefix(String message) {
    return 'Error: $message';
  }

  @override
  String get network => 'Red';

  @override
  String get syncStatus => 'Estado de sincronización';

  @override
  String get synced => 'Sincronizado';

  @override
  String get syncing => 'Sincronizando…';

  @override
  String get walletBlock => 'Bloque de la billetera';

  @override
  String get networkBlock => 'Bloque de la red';

  @override
  String get peers => 'Pares';

  @override
  String get walletType => 'Tipo de billetera';

  @override
  String get viewOnly => 'Solo lectura';

  @override
  String get full => 'Completa';

  @override
  String get nodeConnectionIssue => 'Problema de conexión con el nodo';

  @override
  String get switchNodeInSettings => 'Cambiar nodo en Configuración →';

  @override
  String get recentTransactions => 'Transacciones recientes';

  @override
  String get viewAll => 'Ver todas →';

  @override
  String get noTransactionsYet => 'Aún no hay transacciones';

  @override
  String get received => 'Recibido';

  @override
  String get sent => 'Enviado';

  @override
  String syncingProgress(String pct, int wallet, int network) {
    return 'Sincronizando $pct% (bloque $wallet / $network)';
  }

  @override
  String get shareAddressSubtitle => 'Comparte tu dirección para recibir WRKZ';

  @override
  String get yourAddress => 'Tu dirección';

  @override
  String get generateIntegratedAddress => 'Generar dirección integrada';

  @override
  String get integratedAddressDescription =>
      'Combina tu dirección con un ID de pago. Usa los botones aleatorios para un nuevo ID, o ingresa el tuyo abajo.';

  @override
  String get randomShort16 => 'Aleatorio corto (16)';

  @override
  String get randomLong64 => 'Aleatorio largo (64)';

  @override
  String get customPaymentIdLabel =>
      'ID de pago personalizado (16 o 64 caracteres hexadecimales)';

  @override
  String get generate => 'Generar';

  @override
  String get integratedAddress => 'Dirección integrada';

  @override
  String get paymentIdShort => 'Corto (16)';

  @override
  String get paymentIdLong => 'Largo (64)';

  @override
  String paymentIdLabel(String label) {
    return 'ID de pago · $label';
  }

  @override
  String get enterPaymentIdError =>
      'Ingresa un ID de pago (16 o 64 caracteres hexadecimales)';

  @override
  String get paymentIdInvalidError =>
      'El ID de pago debe tener 16 o 64 caracteres hexadecimales';

  @override
  String get copyAddress => 'Copiar dirección';

  @override
  String get copyPaymentId => 'Copiar ID de pago';

  @override
  String get copy => 'Copiar';

  @override
  String get copied => '¡Copiado!';

  @override
  String get sendWrkzToAny => 'Enviar WRKZ a cualquier dirección';

  @override
  String get sweepAllDescription =>
      'Enviar todos los fondos a una dirección (consolida UTXOs)';

  @override
  String get sweepAll => 'Enviar todo';

  @override
  String get sweepWarning =>
      'Enviar todo consolida todas las UTXOs en una sola salida. Úsalo cuando las transacciones fallen por demasiadas entradas.';

  @override
  String sweepAvailableBalance(String amount, String ticker) {
    return 'Disponible: $amount $ticker (se enviará el saldo completo menos la comisión)';
  }

  @override
  String get destinationAddress => 'Dirección de destino';

  @override
  String get addressBook => 'Libreta de direcciones';

  @override
  String get sweepAllFunds => 'Enviar todos los fondos';

  @override
  String get recipientAddress => 'Dirección del destinatario';

  @override
  String get amount => 'Monto';

  @override
  String get paymentIdOptional => 'ID de pago (opcional)';

  @override
  String get hexCharacters => '16 o 64 caracteres hexadecimales';

  @override
  String get reviewTransaction => 'Revisar transacción';

  @override
  String get reviewAndConfirm => 'Revisar y confirmar';

  @override
  String get to => 'Para';

  @override
  String get fee => 'Comisión';

  @override
  String get totalDeducted => 'Total deducido';

  @override
  String get paymentId => 'ID de pago';

  @override
  String get transactionsIrreversible =>
      'Las transacciones son irreversibles. Verifica la dirección antes de confirmar.';

  @override
  String get back => 'Atrás';

  @override
  String get confirmAndSend => 'Confirmar y enviar';

  @override
  String get transactionSent => '¡Transacción enviada!';

  @override
  String get transactionBroadcast =>
      'Tu transacción ha sido transmitida a la red.';

  @override
  String get transactionHash => 'Hash de transacción';

  @override
  String get sendAnother => 'Enviar otra';

  @override
  String get enterDestinationAddress => 'Ingresa una dirección de destino';

  @override
  String get enterValidAmount => 'Ingresa un monto válido';

  @override
  String computingPow(int seconds) {
    return 'Calculando PoW... ${seconds}s';
  }

  @override
  String get stepFillDetails => 'Completar datos';

  @override
  String get stepReview => 'Revisar';

  @override
  String get stepDone => 'Listo';

  @override
  String get sweepFailed => 'El envío total falló';

  @override
  String get addressBookTitle => 'Libreta de direcciones';

  @override
  String get transactionHistory => 'Historial de transacciones';

  @override
  String get searchByHash => 'Buscar por hash, dirección o ID de pago…';

  @override
  String get all => 'Todas';

  @override
  String get filterReceived => 'Recibidas';

  @override
  String get filterSent => 'Enviadas';

  @override
  String get refresh => 'Actualizar';

  @override
  String get noTransactionsFound => 'No se encontraron transacciones';

  @override
  String get confirmed => 'Confirmada';

  @override
  String get pending => 'Pendiente';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Dirección';

  @override
  String get block => 'Bloque';

  @override
  String showingRange(int start, int end, int total) {
    return 'Mostrando $start–$end de $total';
  }

  @override
  String get previous => 'Anterior';

  @override
  String get next => 'Siguiente';

  @override
  String get walletLocked => 'Billetera bloqueada';

  @override
  String get enterPasswordToContinue =>
      'Ingresa la contraseña de tu billetera para continuar';

  @override
  String get password => 'Contraseña';

  @override
  String get incorrectPassword => 'Contraseña incorrecta';

  @override
  String get unlock => 'Desbloquear';

  @override
  String get closeWalletInstead => 'Cerrar billetera en su lugar';

  @override
  String get closeWallet => 'Cerrar billetera';

  @override
  String get closeWalletDescription =>
      'Esto guardará y cerrará la billetera.\n\nSerás devuelto a la pantalla de inicio de sesión.';

  @override
  String get cancel => 'Cancelar';

  @override
  String get welcomeToPluton => 'Bienvenido a PLUTON v2';

  @override
  String get selectOptionToStart => 'Selecciona una opción para comenzar';

  @override
  String get createNewWallet => 'Crear nueva billetera';

  @override
  String get openExistingWallet => 'Abrir billetera existente';

  @override
  String get importFromSeed => 'Importar desde frase semilla';

  @override
  String get importFromKeys => 'Importar desde claves privadas';

  @override
  String get openWallet => 'Abrir billetera';

  @override
  String get importFromSeedTitle => 'Importar desde semilla';

  @override
  String get importFromKeysTitle => 'Importar desde claves';

  @override
  String get saveWalletTo => 'Guardar billetera en';

  @override
  String get walletFile => 'Archivo de billetera';

  @override
  String get walletPassword => 'Contraseña de billetera';

  @override
  String get mnemonicSeedPhrase => 'Frase semilla mnemónica';

  @override
  String get scanFromHeight => 'Escanear desde altura (0 = escaneo completo)';

  @override
  String get daemonHost => 'Host del daemon';

  @override
  String get port => 'Puerto';

  @override
  String get continueButton => 'Continuar';

  @override
  String get browse => 'Examinar…';

  @override
  String get backupWarning =>
      'Haz una copia de seguridad de tu billetera antes de continuar.\nEstas claves no se pueden recuperar si se pierden.';

  @override
  String get yourWalletAddress => 'Tu dirección de billetera';

  @override
  String get seedPhrase25Words => 'Frase semilla (25 palabras)';

  @override
  String get privateViewKey => 'Clave privada de visualización';

  @override
  String get privateSpendKey => 'Clave privada de gasto';

  @override
  String get seedBackupConfirm =>
      'He anotado mi frase semilla y claves privadas en un lugar seguro.';

  @override
  String get backedUpContinue => 'He respaldado mi billetera — Continuar';

  @override
  String get settings => 'Configuración';

  @override
  String get sectionDaemonNode => 'Nodo del daemon';

  @override
  String get nodeDescription =>
      'Conéctate a un nodo daemon local o remoto. Los cambios surten efecto inmediatamente.';

  @override
  String get hostIpAddress => 'Host / Dirección IP';

  @override
  String get ssl => 'SSL';

  @override
  String get apply => 'Aplicar';

  @override
  String get nodeUpdatedSuccess => 'Nodo actualizado correctamente';

  @override
  String get nodeUnreachable =>
      'No se puede alcanzar el nodo actual. Ingresa una nueva dirección de nodo abajo y pulsa Aplicar.';

  @override
  String get sectionWallet => 'Billetera';

  @override
  String get saveWallet => 'Guardar billetera';

  @override
  String get saveWalletSubtitle => 'Guardar estado actual en disco';

  @override
  String get walletSaved => 'Billetera guardada';

  @override
  String get exportToJson => 'Exportar a JSON';

  @override
  String get exportToJsonSubtitle =>
      'Guardar datos de la billetera como archivo JSON';

  @override
  String get exportJsonTitle => 'Exportar JSON de billetera';

  @override
  String exportedTo(String path) {
    return 'Exportado a $path';
  }

  @override
  String exportFailed(String error) {
    return 'Error al exportar: $error';
  }

  @override
  String get resetScanHeight => 'Restablecer altura de escaneo';

  @override
  String get resetScanHeightSubtitle =>
      'Reescanear blockchain desde una altura específica';

  @override
  String get resetScanHeightDescription =>
      'Ingresa una altura de bloque desde la cual reescanear. Usa 0 para un escaneo completo.';

  @override
  String get scanHeight => 'Altura de escaneo';

  @override
  String get reset => 'Restablecer';

  @override
  String get autosave => 'Autoguardado';

  @override
  String get autosaveSubtitle =>
      'Guardar billetera en disco después de sincronizar y cada 5 minutos';

  @override
  String get scanCoinbaseTx => 'Escanear transacciones Coinbase';

  @override
  String get scanCoinbaseSubtitle =>
      'Incluir recompensas de minería al sincronizar (desactivado por defecto)';

  @override
  String get sectionAppearance => 'Apariencia';

  @override
  String get theme => 'Tema';

  @override
  String get themeSubtitle => 'Elegir esquema de colores de la aplicación';

  @override
  String get themeSystem => 'Sistema';

  @override
  String get themeLight => 'Claro';

  @override
  String get themeDark => 'Oscuro';

  @override
  String get sectionNotifications => 'Notificaciones';

  @override
  String get incomingTxAlerts => 'Alertas de transacciones entrantes';

  @override
  String get incomingTxAlertsSubtitle =>
      'Mostrar una notificación de escritorio al recibir WRKZ';

  @override
  String get sectionDebugLogs => 'Depuración y registros';

  @override
  String get logLevel => 'Nivel de registro';

  @override
  String get logLevelSubtitle =>
      'Controla la verbosidad de la biblioteca de billetera';

  @override
  String get viewLogs => 'Ver registros';

  @override
  String get viewLogsSubtitle =>
      'Salida en vivo del registro de la biblioteca de billetera';

  @override
  String get walletLogs => 'Registros de billetera';

  @override
  String logEntries(int count) {
    return '$count entradas';
  }

  @override
  String get autoScroll => 'Desplazamiento automático';

  @override
  String get copyAll => 'Copiar todo';

  @override
  String get clear => 'Limpiar';

  @override
  String get close => 'Cerrar';

  @override
  String get noLogsYet =>
      'Aún no hay registros. Establece un nivel de registro superior a Desactivado para ver la salida.';

  @override
  String get logsCopied => 'Registros copiados al portapapeles';

  @override
  String get sectionDangerZone => 'Zona de peligro';

  @override
  String get deleteWalletData => 'Eliminar datos de billetera';

  @override
  String get deleteWalletDataSubtitle =>
      'Eliminar permanentemente el archivo de billetera del disco';

  @override
  String get deleteWalletWarning =>
      'Esto eliminará permanentemente tu archivo de billetera del disco.\n\nAsegúrate de haber respaldado tu frase semilla y claves privadas antes de continuar. Esta acción no se puede deshacer.';

  @override
  String get iUnderstandContinue => 'Entiendo, continuar';

  @override
  String get finalConfirmation => 'Confirmación final';

  @override
  String get typeDeleteToConfirm => 'Escribe ELIMINAR para confirmar:';

  @override
  String get deleteHint => 'ELIMINAR';

  @override
  String get deletePermanently => 'Eliminar permanentemente';

  @override
  String get aboutTitle => 'Acerca de';

  @override
  String versionInfo(String version) {
    return 'Versión $version — Billetera de escritorio WRKZ';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 es la billetera de escritorio oficial para WrkzCoin (WRKZ), una criptomoneda rápida y ligera basada en CryptoNote.\n\nDesarrollada con Flutter, impulsada por wallet-api.';

  @override
  String get github => 'GitHub';

  @override
  String get githubSubtitle => 'Ver código fuente y versiones';

  @override
  String get discord => 'Discord';

  @override
  String get discordSubtitle => 'Únete a la comunidad';

  @override
  String get twitterX => 'Twitter / X';

  @override
  String get twitterXSubtitle => 'Seguir a @wrkzcoin';

  @override
  String get website => 'Sitio web';

  @override
  String get websiteSubtitle => 'wrkz.work';

  @override
  String get license => 'Licencia';

  @override
  String get licenseText =>
      'Publicado bajo la licencia MIT.\nÚsalo bajo tu propio riesgo. Siempre respalda tu frase semilla.';

  @override
  String get addButton => 'Añadir';

  @override
  String get noSavedAddresses => 'No hay direcciones guardadas';

  @override
  String get tapAddToSave =>
      'Pulsa Añadir para guardar una dirección de uso frecuente.';

  @override
  String get addAddress => 'Añadir dirección';

  @override
  String get nameLabel => 'Nombre / etiqueta';

  @override
  String get addressLabel => 'Dirección';

  @override
  String get noteOptional => 'Nota (opcional)';

  @override
  String get nameAndAddressRequired =>
      'El nombre y la dirección son obligatorios';

  @override
  String get invalidWrkzAddress =>
      'Dirección WRKZ inválida. Debe tener 98 (estándar), 120 (integrada corta) o 186 (integrada larga) caracteres y comenzar con \"Wrkz\".';

  @override
  String get save => 'Guardar';

  @override
  String get editEntry => 'Editar entrada';

  @override
  String get deleteEntry => 'Eliminar entrada';

  @override
  String removeFromAddressBook(String name) {
    return '¿Eliminar \"$name\" de tu libreta de direcciones?';
  }

  @override
  String get delete => 'Eliminar';

  @override
  String get edit => 'Editar';

  @override
  String get wrkzReceived => 'WRKZ recibido';

  @override
  String youReceivedAmount(String amount) {
    return 'Recibiste $amount';
  }

  @override
  String get show => 'Mostrar';

  @override
  String get exit => 'Salir';

  @override
  String get plutonWallet => 'Billetera PLUTON';

  @override
  String get language => 'Idioma';

  @override
  String get selectLanguage => 'Seleccionar idioma';

  @override
  String get chooseLanguage => 'Elige tu idioma preferido';

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
      'Esta transacción ya no es válida. Vuelva atrás y créela de nuevo.';

  @override
  String get deleteConfirmMismatch =>
      'Escriba exactamente DELETE para confirmar.';

  @override
  String get unlockNeedsReopen =>
      'No se puede verificar la contraseña en este dispositivo. Use «Cerrar cartera» y vuelva a abrirla.';

  @override
  String get exportJsonWarningTitle => '¿Exportar la cartera sin cifrar?';

  @override
  String get exportJsonWarningBody =>
      'El archivo exportado contiene su clave de visualización privada y sus claves de gasto privadas en texto plano. Cualquiera que lo lea puede gastar sus fondos.\n\nGuárdelo solo en un almacenamiento que usted controle y elimínelo en cuanto termine.';

  @override
  String passwordTooShort(int count) {
    return 'La contraseña debe tener al menos $count caracteres';
  }

  @override
  String get ringSize => 'Tamaño del anillo';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Tamaño del anillo reducido a $actual (normalmente $normal). Los importes que se envían no tienen suficientes salidas en la cadena para formar un anillo completo, por lo que esta transacción es menos privada de lo habitual.';
  }

  @override
  String get liteNodeTitle => 'Nodo ligero';

  @override
  String liteNodeServesFrom(int height) {
    return 'Este nodo solo guarda bloques a partir de $height. Las transacciones anteriores a ese bloque no se pueden encontrar a través de él.';
  }

  @override
  String liteNodeMissesHistory(int nodeHeight, int walletHeight) {
    return 'Este nodo empieza en el bloque $nodeHeight, pero esta cartera empieza en el bloque $walletHeight. Todo lo recibido entre medias es invisible aquí, así que el saldo mostrado puede ser demasiado bajo. Conecta un nodo con la cadena completa para verlo.';
  }

  @override
  String liteNodeSyncStalled(int wallet, int node) {
    return 'La sincronización se detuvo en el bloque $wallet. Este nodo no guarda nada por debajo del bloque $node, así que los bloques intermedios no se pueden descargar de él. El saldo estará incompleto hasta que conectes un nodo con la cadena completa.';
  }

  @override
  String get liteNodeRescanRefusedTitle =>
      'Este nodo no puede reescanear tan atrás';

  @override
  String liteNodeRescanRefused(int height) {
    return 'El nodo conectado es un nodo ligero sin datos de bloque por debajo de $height. Reescanear desde más abajo descartaría transacciones que esta cartera ya encontró, sin forma de recuperarlas aquí. No se ha cambiado nada.';
  }

  @override
  String liteNodeRescanFromInstead(int height) {
    return 'Reescanear desde $height en su lugar';
  }

  @override
  String liteNodeRescanHint(int height) {
    return 'El nodo conectado solo puede reescanear desde el bloque $height o superior.';
  }

  @override
  String get nodeServesFromLabel => 'Sirve bloques desde';

  @override
  String get nodeFullChain => 'Cadena completa';

  @override
  String get sectionLocalNode => 'Nodo ligero local';

  @override
  String get localNodeDescription =>
      'Ejecuta un nodo en este ordenador y sincroniza con él en vez de con un servidor remoto. Un nodo ligero solo guarda lo que necesita una cartera, pero aun así descarga la cadena entera una vez.';

  @override
  String get localNodeSetUp => 'Configurar nodo local';

  @override
  String get localNodeStart => 'Iniciar';

  @override
  String get localNodeStop => 'Detener';

  @override
  String get localNodeUse => 'Usar este nodo';

  @override
  String get localNodeDelete => 'Eliminar datos del nodo';

  @override
  String get localNodeStateStopped => 'Detenido';

  @override
  String get localNodeStateStarting => 'Iniciando';

  @override
  String get localNodeStateSyncing => 'Sincronizando';

  @override
  String get localNodeStateReady => 'Sincronizado';

  @override
  String get localNodeStateFailed => 'Falló';

  @override
  String localNodeProgress(int height, int network) {
    return 'Bloque $height de $network';
  }

  @override
  String localNodePeers(int count) {
    return '$count pares';
  }

  @override
  String get localNodeNotReadyYet =>
      'El nodo local todavía se está poniendo al día y aún no puede servir a la cartera. Sigue sincronizando en segundo plano: quédate en un nodo remoto hasta que esté listo y luego cambia.';

  @override
  String get localNodeInUse => 'La cartera está conectada a este nodo.';

  @override
  String localNodeBinaryMissing(String name) {
    return 'No se encontró $name. Coloca el binario del demonio junto al ejecutable de la cartera, o en una carpeta «sidecar» al lado, e inténtalo de nuevo.';
  }

  @override
  String get localNodeSetupTitle => 'Configurar un nodo ligero local';

  @override
  String get localNodeSetupCost =>
      'Antes de empezar:\n• Unos 6 GB de espacio en disco, y la cadena entera se descarga una vez.\n• La primera sincronización tarda horas. Continúa en segundo plano y mientras tanto puedes seguir usando un nodo remoto.\n• La altura inicial de abajo es permanente. Cambiarla después implica eliminar el nodo y sincronizar otra vez desde cero.';

  @override
  String get localNodeStartHeightLabel => 'Altura inicial';

  @override
  String localNodeStartHeightHelp(int height) {
    return 'Los bloques por debajo de esta altura se descargan y comprueban, y solo se conserva el índice que necesitan los bloques posteriores. Mantenla igual o por debajo de la altura inicial de esta cartera ($height).';
  }

  @override
  String localNodeStartHeightTooHigh(int height) {
    return 'Superior a la altura inicial de esta cartera ($height). El nodo nunca podría mostrar las transacciones antiguas de esta cartera.';
  }

  @override
  String get localNodeCreate => 'Crear nodo';

  @override
  String get localNodeDeleteTitle => '¿Eliminar los datos del nodo local?';

  @override
  String get localNodeDeleteWarning =>
      'Esto detiene el nodo y elimina permanentemente su base de datos de la cadena del disco. Tu cartera, su semilla y sus fondos no se tocan, pero un nodo local nuevo volverá a sincronizar desde cero, lo que lleva horas.';

  @override
  String get localNodeDeleted => 'Nodo local eliminado.';

  @override
  String localNodeDiskUsage(String size) {
    return '$size en disco';
  }

  @override
  String get nodePresetRemote => 'Nodo remoto';

  @override
  String get nodePresetLocal => 'Nodo ligero local';

  @override
  String get savingWallet => 'Guardando tu monedero…';

  @override
  String get savingWalletBody =>
      'PLUTON está escribiendo tu monedero en el disco. Puede tardar un momento en un monedero grande.';

  @override
  String get shutdownTakingLong =>
      'Está tardando más de lo esperado. Salir ahora pierde este guardado: el archivo del monedero en el disco queda como estaba, así que no se daña nada, pero se pierde todo lo posterior al último guardado.';

  @override
  String get quitAnyway => 'Salir de todos modos';

  @override
  String get stillRunningInTray =>
      'PLUTON sigue ejecutándose en la bandeja del sistema. Haz clic en el icono para recuperarlo, o haz clic derecho y elige Salir.';

  @override
  String get localNodeDataFolder => 'Carpeta de datos';

  @override
  String get localNodeDataFolderHelp =>
      'Aquí se escriben unos 6 GB. Elige una unidad con espacio suficiente.';

  @override
  String get localNodeDataFolderInUse =>
      'Esa carpeta ya contiene otros archivos. Elige una carpeta vacía o una nueva.';

  @override
  String get localNodeStartHeightRequired =>
      'Introduce la altura a partir de la cual conservar bloques completos. Debe ser mayor que cero: un nodo lite no puede empezar en el bloque génesis.';

  @override
  String get nodeExitTitle => 'El nodo local sigue en ejecución';

  @override
  String get nodeExitBodySyncing =>
      'Su primera sincronización no ha terminado. Tarda horas y solo avanza mientras el nodo está en marcha, pero un nodo en marcha sigue usando el disco y la red después de cerrar PLUTON.';

  @override
  String get nodeExitBodySynced =>
      'Está al día con la red. Dejarlo en marcha lo mantiene así y sigue usando CPU, ancho de banda y disco mientras PLUTON está cerrado; detenerlo cuesta una puesta al día breve la próxima vez.';

  @override
  String get nodeExitKeep => 'Dejarlo en marcha';

  @override
  String get nodeExitStop => 'Detenerlo';

  @override
  String get nodeExitChangeLater =>
      'Se puede cambiar después en Ajustes, en Nodo lite local.';

  @override
  String get rememberMyChoice => 'Recordar mi elección';

  @override
  String get shutdownStoppingNode => 'Deteniendo el nodo local…';

  @override
  String get shutdownStoppingNodeBody =>
      'Se le deja vaciar su base de datos para que el próximo inicio no tenga que repetir el registro de escritura.';

  @override
  String get nodeExitPolicyLabel => 'Al cerrar el monedero';

  @override
  String get nodeExitPolicyAsk => 'Preguntarme';

  @override
  String get nodeExitPolicyKeep => 'Dejar el nodo en marcha';

  @override
  String get nodeExitPolicyStop => 'Detener el nodo';

  @override
  String get localNodeStillRunningBody =>
      'El nodo lite local sigue ejecutándose en segundo plano. Abre PLUTON para detenerlo.';

  @override
  String get syncStoppedTitle => 'Sincronización detenida';

  @override
  String syncGapStalled(int covered, int servesFrom) {
    return 'Sincronización detenida en el bloque $covered. El nodo con el que hablaba solo responde desde el bloque $servesFrom en adelante, así que los bloques intermedios no pueden descargarse de él. El saldo está incompleto hasta que conectes un nodo con la cadena completa.';
  }

  @override
  String get localNodeNotReadyTitle => 'Este nodo no está listo';

  @override
  String localNodeNotReadyBody(int behind) {
    return 'Al nodo local aún le faltan $behind bloques para alcanzar la red. Si apuntas el monedero ahí ahora, la sincronización se detendrá hasta que se ponga al día, mostrando un saldo al que le falta todo lo que no ha alcanzado, y sin nada en pantalla que lo explique. Quedarte en el nodo remoto no cuesta nada; el nodo sigue sincronizando igual.';
  }

  @override
  String get switchAnyway => 'Cambiar de todos modos';

  @override
  String get switchToRemoteNode => 'Cambiar al nodo remoto';

  @override
  String get nodeWillServeFromLabel => 'Servirá bloques desde';

  @override
  String get txPowServerSection => 'Servidor PoW de transacciones';

  @override
  String get txPowServerUse => 'Usar un servidor PoW externo';

  @override
  String get txPowServerSubtitle =>
      'Enviar la prueba de trabajo de la transacción a un servidor en lugar de calcularla en este dispositivo. Si el servidor no responde, se usa la CPU de este dispositivo.';

  @override
  String get txPowServerSaved => 'Ajustes del servidor PoW guardados';

  @override
  String get txPowServerInvalid => 'Introduce un host y un puerto válidos';
}
