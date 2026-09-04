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
  String get browse => 'Examinar';

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
    return 'Versión $version — Billetera web WRKZ';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 es la billetera web oficial para WrkzCoin (WRKZ), una criptomoneda rápida y ligera basada en CryptoNote.\n\nDesarrollada con Flutter, impulsada por wallet-api.';

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
  String get ringSize => 'Tamaño del anillo';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Tamaño del anillo reducido a $actual (normalmente $normal). Los importes que se envían no tienen suficientes salidas en la cadena para formar un anillo completo, por lo que esta transacción es menos privada de lo habitual.';
  }

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

  @override
  String get txPowServerTest => 'Probar';

  @override
  String txPowServerTestOk(int ms, int threads, int queue, int capacity) {
    return 'Servidor accesible en $ms ms: $threads hilos, $queue de $capacity puestos de cola en uso';
  }

  @override
  String txPowServerTestFailed(String error) {
    return 'Servidor no accesible: $error';
  }

  @override
  String get nodeTest => 'Probar';

  @override
  String get nodeInvalid => 'Introduce un host y un puerto válidos';

  @override
  String nodeTestOk(int ms, int height, int peers) {
    return 'Accesible en $ms ms: altura $height, $peers pares';
  }

  @override
  String nodeTestSyncing(int ms, int height, int networkHeight) {
    return 'Accesible en $ms ms, pero el nodo aún se está sincronizando: altura $height de $networkHeight';
  }

  @override
  String nodeTestFailed(String error) {
    return 'Nodo no accesible: $error';
  }
}
