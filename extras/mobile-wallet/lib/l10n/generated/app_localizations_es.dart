// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Spanish Castilian (`es`).
class SEs extends S {
  SEs([String locale = 'es']) : super(locale);

  @override
  String get appTitle => 'PLUTON Mobile';

  @override
  String get tabOverview => 'Resumen';

  @override
  String get tabReceive => 'Recibir';

  @override
  String get tabSend => 'Enviar';

  @override
  String get tabHistory => 'Historial';

  @override
  String get tabSettings => 'Ajustes';

  @override
  String get send => 'Enviar';

  @override
  String get receive => 'Recibir';

  @override
  String get available => 'Disponible';

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
  String get recentTransactions => 'Transacciones recientes';

  @override
  String get viewAll => 'Ver todo';

  @override
  String get noTransactionsYet => 'Sin transacciones aún';

  @override
  String get noMatchingTransactions => 'Sin transacciones coincidentes';

  @override
  String get pending => 'Pendiente...';

  @override
  String get justNow => 'Ahora mismo';

  @override
  String minutesAgo(int count) {
    return 'hace ${count}m';
  }

  @override
  String hoursAgo(int count) {
    return 'hace ${count}h';
  }

  @override
  String daysAgo(int count) {
    return 'hace ${count}d';
  }

  @override
  String get received => 'Recibido';

  @override
  String get sent => 'Enviado';

  @override
  String get networkStatus => 'Estado de la red';

  @override
  String get node => 'Nodo';

  @override
  String get status => 'Estado';

  @override
  String get connected => 'Conectado';

  @override
  String get disconnected => 'Desconectado';

  @override
  String get walletHeight => 'Altura de la cartera';

  @override
  String get networkHeight => 'Altura de la red';

  @override
  String get peers => 'Pares';

  @override
  String get type => 'Tipo';

  @override
  String get viewOnly => 'Solo lectura';

  @override
  String get couldNotFetchStatus =>
      'No se pudo obtener el estado. Verifique su nodo en Ajustes.';

  @override
  String errorPrefix(String message) {
    return 'Error: $message';
  }

  @override
  String get seedBackupWarning =>
      'Haga una copia de seguridad de su frase semilla en Ajustes para proteger sus fondos.';

  @override
  String get noConnectionToDaemon => 'Sin conexión al daemon';

  @override
  String syncingPercent(String percent) {
    return 'Sincronizando $percent%';
  }

  @override
  String get yourAddress => 'Su dirección';

  @override
  String get errorLoadingAddress => 'Error al cargar la dirección';

  @override
  String get integratedAddress => 'Dirección integrada';

  @override
  String get embedPaymentId => 'Incorporar un ID de pago en su dirección';

  @override
  String get randomShort => 'Aleatorio corto (16)';

  @override
  String get randomLong => 'Aleatorio largo (64)';

  @override
  String get enterCustomPaymentId =>
      'O ingrese un ID de pago personalizado (16 o 64 hex)';

  @override
  String get enterPaymentId => 'Ingrese un ID de pago';

  @override
  String get paymentIdInvalid =>
      'El ID de pago debe tener 16 o 64 caracteres hexadecimales';

  @override
  String get shortPid => 'PID corto';

  @override
  String get longPid => 'PID largo';

  @override
  String get share => 'Compartir';

  @override
  String get copy => 'Copiar';

  @override
  String get sweepAllFunds => 'Transferir todos los fondos';

  @override
  String get normalSend => 'Envío normal';

  @override
  String get sweep => 'Transferir';

  @override
  String get recipientAddress => 'Dirección del destinatario';

  @override
  String get scanQr => 'Escanear QR';

  @override
  String get amount => 'Monto';

  @override
  String availableBalance(String amount) {
    return 'Disponible: $amount';
  }

  @override
  String sweepInfo(String amount) {
    return 'Transferir consolida todos los UTXOs y envía su saldo desbloqueado completo ($amount) menos las comisiones.';
  }

  @override
  String get paymentIdOptional => 'ID de pago (opcional)';

  @override
  String get hexCharacters => '16 o 64 caracteres hexadecimales';

  @override
  String get mustBeHex => 'Debe tener 16 o 64 caracteres hexadecimales';

  @override
  String get recipientRequired =>
      'La dirección del destinatario es obligatoria';

  @override
  String get invalidAddress => 'Dirección WRKZ inválida';

  @override
  String get enterValidAmount => 'Ingrese un monto válido';

  @override
  String get reviewTransaction => 'Revisar transacción';

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
      'Las transacciones son irreversibles. Por favor verifique los detalles.';

  @override
  String get back => 'Atrás';

  @override
  String get confirmAndSend => 'Confirmar y enviar';

  @override
  String get transactionSent => '¡Transacción enviada!';

  @override
  String get transactionHash => 'Hash de transacción';

  @override
  String get sendAnother => 'Enviar otra';

  @override
  String get scanQrCode => 'Escanear código QR';

  @override
  String get scannedAddress => 'Dirección escaneada';

  @override
  String get cancel => 'Cancelar';

  @override
  String get useThisAddress => 'Usar esta dirección';

  @override
  String get sweepFailed => 'Transferencia fallida';

  @override
  String get searchPlaceholder => 'Buscar por hash, dirección, ID de pago...';

  @override
  String get all => 'Todo';

  @override
  String get filterReceived => 'Recibido';

  @override
  String get filterSent => 'Enviado';

  @override
  String get hash => 'Hash';

  @override
  String get address => 'Dirección';

  @override
  String get block => 'Bloque';

  @override
  String get confirmed => 'Confirmado';

  @override
  String get password => 'Contraseña';

  @override
  String get unlock => 'Desbloquear';

  @override
  String get switchWallet => 'Cambiar cartera';

  @override
  String get enterPasswordToUnlock => 'Ingrese su contraseña para desbloquear';

  @override
  String get incorrectPassword => 'Contraseña incorrecta';

  @override
  String get enterYourPassword => 'Ingrese su contraseña';

  @override
  String get plutonMobile => 'PLUTON Mobile';

  @override
  String get createFirstWalletSubtitle =>
      'Cree su primera cartera para comenzar';

  @override
  String get selectWalletSubtitle => 'Seleccione una cartera para abrir';

  @override
  String get yourWallets => 'Sus carteras';

  @override
  String get noWalletsYet => 'Sin carteras aún';

  @override
  String get lastOpened => 'Último acceso';

  @override
  String createdDate(String date) {
    return 'Creada el $date';
  }

  @override
  String get createFirstWallet => 'Crear primera cartera';

  @override
  String get addWallet => 'Agregar cartera';

  @override
  String get deleteWallet => 'Eliminar cartera';

  @override
  String deleteWalletConfirm(String name) {
    return '¿Eliminar \"$name\"?\n\nEsto eliminará permanentemente el archivo de cartera y las claves. Asegúrese de haber respaldado su frase semilla.';
  }

  @override
  String get delete => 'Eliminar';

  @override
  String get createNewWallet => 'Crear nueva cartera';

  @override
  String get createNewWalletSubtitle =>
      'Genere una nueva cartera con una frase semilla nueva';

  @override
  String get importFromSeed => 'Importar desde frase semilla';

  @override
  String get importFromSeedSubtitle =>
      'Restaurar cartera usando su semilla mnemónica de 25 palabras';

  @override
  String get importFromKeys => 'Importar desde claves privadas';

  @override
  String get importFromKeysSubtitle =>
      'Restaurar usando clave de gasto y clave de vista';

  @override
  String get viewOnlyWallet => 'Cartera de solo lectura';

  @override
  String get viewOnlyWalletSubtitle =>
      'Cartera de observación usando clave de vista y dirección';

  @override
  String get createWallet => 'Crear cartera';

  @override
  String get importWallet => 'Importar cartera';

  @override
  String get walletName => 'Nombre de cartera';

  @override
  String get walletNameHint => 'p.ej. Cartera principal';

  @override
  String get passwordLabel => 'Contraseña';

  @override
  String get enterPassword => 'Ingrese la contraseña';

  @override
  String get confirmPassword => 'Confirmar contraseña';

  @override
  String get seedPhrase => 'Frase semilla (25 palabras)';

  @override
  String get enterSeedPhrase => 'Ingrese su frase semilla...';

  @override
  String get scanHeight => 'Altura de escaneo (opcional)';

  @override
  String get scanHeightHint => '0 = escanear desde el inicio';

  @override
  String get privateSpendKey => 'Clave de gasto privada';

  @override
  String get privateViewKey => 'Clave de vista privada';

  @override
  String get walletAddress => 'Dirección de cartera';

  @override
  String get walletAddressHint => 'Dirección Wrkz...';

  @override
  String get hexKey => '64 caracteres hex';

  @override
  String get daemonNode => 'Nodo daemon';

  @override
  String get custom => 'Personalizado';

  @override
  String get host => 'Host';

  @override
  String get hostHint => 'Host / IP';

  @override
  String get port => 'Puerto';

  @override
  String get ssl => 'SSL';

  @override
  String get walletNameRequired => 'El nombre de cartera es obligatorio';

  @override
  String get passwordRequired => 'La contraseña es obligatoria';

  @override
  String passwordTooShort(int count) {
    return 'La contraseña debe tener al menos $count caracteres';
  }

  @override
  String get passwordsDoNotMatch => 'Las contraseñas no coinciden';

  @override
  String get seedRequired => 'La frase semilla es obligatoria';

  @override
  String get spendKeyRequired => 'La clave de gasto es obligatoria';

  @override
  String get viewKeyRequired => 'La clave de vista es obligatoria';

  @override
  String get addressRequired => 'La dirección es obligatoria';

  @override
  String get daemonHostRequired => 'El host del daemon es obligatorio';

  @override
  String get backupSeedTitle => 'Respaldar su semilla';

  @override
  String get backupWarning =>
      'Anote su frase semilla y guárdela en un lugar seguro. Si la pierde, sus fondos se perderán para siempre.';

  @override
  String get seedPhraseLabel => 'Frase semilla';

  @override
  String get privateViewKeyLabel => 'Clave de vista privada';

  @override
  String get privateSpendKeyLabel => 'Clave de gasto privada';

  @override
  String get backupConfirmCheck =>
      'He respaldado de forma segura mi frase semilla';

  @override
  String get continueToWallet => 'Continuar a la cartera';

  @override
  String get sectionDaemonNode => 'Nodo daemon';

  @override
  String get apply => 'Aplicar';

  @override
  String nodeUpdated(String host, int port) {
    return 'Nodo actualizado a $host:$port';
  }

  @override
  String get hostRequired => 'El host es obligatorio';

  @override
  String currentWallet(String name) {
    return 'Cartera actual — $name';
  }

  @override
  String get saveWallet => 'Guardar cartera';

  @override
  String get walletSaved => 'Cartera guardada';

  @override
  String saveFailed(String error) {
    return 'Error al guardar: $error';
  }

  @override
  String get backupSeed => 'Respaldar semilla';

  @override
  String get changePassword => 'Cambiar contraseña';

  @override
  String get resetScanHeight => 'Restablecer altura de escaneo';

  @override
  String get reset => 'Restablecer';

  @override
  String resetScanConfirm(int height) {
    return 'Esto volverá a escanear la cadena de bloques desde el bloque $height. Esto puede tardar un tiempo. ¿Continuar?';
  }

  @override
  String scanResetTo(int height) {
    return 'Escaneo restablecido al bloque $height';
  }

  @override
  String resetFailed(String error) {
    return 'Error al restablecer: $error';
  }

  @override
  String get enterPasswordTitle => 'Ingresar contraseña';

  @override
  String get confirm => 'Confirmar';

  @override
  String get seedBackup => 'Copia de seguridad de semilla';

  @override
  String get seedPhraseColon => 'Frase semilla:';

  @override
  String get privateViewKeyColon => 'Clave de vista privada:';

  @override
  String get iveBackedUp => 'He realizado la copia de seguridad';

  @override
  String get currentPasswordLabel => 'Contraseña actual';

  @override
  String get newPasswordLabel => 'Nueva contraseña';

  @override
  String get confirmNewPasswordLabel => 'Confirmar nueva contraseña';

  @override
  String get change => 'Cambiar';

  @override
  String get currentPasswordIncorrect => 'La contraseña actual es incorrecta';

  @override
  String get newPasswordsDoNotMatch => 'Las nuevas contraseñas no coinciden';

  @override
  String get passwordChanged => 'Contraseña cambiada';

  @override
  String get walletManagement => 'Gestión de carteras';

  @override
  String get switchWalletSubtitle => 'Guardar y cerrar, elegir otra';

  @override
  String get manageWallets => 'Administrar carteras';

  @override
  String get manageWalletsSubtitle => 'Renombrar o eliminar carteras';

  @override
  String get currentlyOpen => '(actualmente abierta)';

  @override
  String get close => 'Cerrar';

  @override
  String get renameWallet => 'Renombrar cartera';

  @override
  String get newName => 'Nuevo nombre';

  @override
  String get rename => 'Renombrar';

  @override
  String deleteWalletConfirmShort(String name) {
    return '¿Eliminar \"$name\"? Esto no se puede deshacer.';
  }

  @override
  String get security => 'Seguridad';

  @override
  String get biometricUnlock => 'Desbloqueo biométrico';

  @override
  String get biometricSubtitle => 'Huella dactilar / Face ID';

  @override
  String get biometricNotAvailable => 'Biometría no disponible';

  @override
  String get autoLock => 'Bloqueo automático';

  @override
  String get appearance => 'Apariencia';

  @override
  String get theme => 'Tema';

  @override
  String get themeAuto => 'Automático';

  @override
  String get themeLight => 'Claro';

  @override
  String get themeDark => 'Oscuro';

  @override
  String get preferences => 'Preferencias';

  @override
  String get transactionNotifications => 'Notificaciones de transacciones';

  @override
  String get notificationsSubtitle => 'Alertar en transacciones entrantes';

  @override
  String get autosave => 'Guardado automático';

  @override
  String get autosaveSubtitle =>
      'Guardar tras sincronización, luego cada 5 minutos';

  @override
  String get scanCoinbaseTx => 'Escanear transacciones Coinbase';

  @override
  String get scanCoinbaseSubtitle =>
      'Incluir recompensas de minero (desactivado por defecto)';

  @override
  String get dangerZone => 'Zona de peligro';

  @override
  String get deleteCurrentWallet => 'Eliminar cartera actual';

  @override
  String get deleteCurrentWalletSubtitle =>
      'Eliminar permanentemente los datos de la cartera';

  @override
  String get deleteWalletTypeCaps =>
      'Esto eliminará permanentemente el archivo de cartera y las claves. Asegúrese de haber respaldado su frase semilla.\n\nEscriba DELETE para confirmar:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get language => 'Idioma';

  @override
  String get selectLanguage => 'Seleccionar idioma';

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
  String get autoLockImmediately => 'Inmediatamente';

  @override
  String get autoLock1Min => '1 minuto';

  @override
  String get autoLock5Min => '5 minutos';

  @override
  String get autoLockNever => 'Nunca';

  @override
  String get preparedTransactionExpired =>
      'Esta transacción ya no es válida. Vuelva atrás y créela de nuevo.';

  @override
  String get deleteConfirmMismatch =>
      'Escriba exactamente DELETE para confirmar.';

  @override
  String get seedNotBackedUpWarning =>
      'No ha confirmado una copia de seguridad de la frase semilla de esta cartera. Eliminarla ahora significa que los fondos no se podrán recuperar.';

  @override
  String get wrkzReceived => 'WRKZ recibidos';

  @override
  String get retry => 'Reintentar';

  @override
  String youReceivedAmount(String amount) {
    return 'Ha recibido $amount';
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
  String get localNodeMobileFuture =>
      'Ejecutar el nodo en el propio teléfono está planeado, pero aún no disponible: un nodo necesita varios GB de almacenamiento y horas de sincronización. Mientras tanto, apunta esta cartera a un nodo tuyo.';
}
