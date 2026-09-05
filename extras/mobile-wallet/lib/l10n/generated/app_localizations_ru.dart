// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Russian (`ru`).
class SRu extends S {
  SRu([String locale = 'ru']) : super(locale);

  @override
  String get appTitle => 'PLUTON Mobile';

  @override
  String get tabOverview => 'Обзор';

  @override
  String get tabReceive => 'Получить';

  @override
  String get tabSend => 'Отправить';

  @override
  String get tabHistory => 'История';

  @override
  String get tabSettings => 'Настройки';

  @override
  String get send => 'Отправить';

  @override
  String get receive => 'Получить';

  @override
  String get available => 'Доступно';

  @override
  String get locked => 'Заблокировано';

  @override
  String get total => 'Итого';

  @override
  String lockedAmount(String amount) {
    return 'Заблокировано: $amount';
  }

  @override
  String totalAmount(String amount) {
    return 'Итого: $amount';
  }

  @override
  String get recentTransactions => 'Последние транзакции';

  @override
  String get viewAll => 'Смотреть все';

  @override
  String get noTransactionsYet => 'Транзакций пока нет';

  @override
  String get noMatchingTransactions => 'Подходящих транзакций не найдено';

  @override
  String get pending => 'В ожидании...';

  @override
  String get justNow => 'Только что';

  @override
  String minutesAgo(int count) {
    return '$count мин. назад';
  }

  @override
  String hoursAgo(int count) {
    return '$count ч. назад';
  }

  @override
  String daysAgo(int count) {
    return '$count д. назад';
  }

  @override
  String get received => 'Получено';

  @override
  String get sent => 'Отправлено';

  @override
  String get networkStatus => 'Состояние сети';

  @override
  String get node => 'Узел';

  @override
  String get status => 'Статус';

  @override
  String get connected => 'Подключено';

  @override
  String get disconnected => 'Отключено';

  @override
  String get walletHeight => 'Высота кошелька';

  @override
  String get networkHeight => 'Высота сети';

  @override
  String get peers => 'Пиры';

  @override
  String get type => 'Тип';

  @override
  String get viewOnly => 'Только просмотр';

  @override
  String get couldNotFetchStatus =>
      'Не удалось получить статус. Проверьте узел в Настройках.';

  @override
  String errorPrefix(String message) {
    return 'Ошибка: $message';
  }

  @override
  String get seedBackupWarning =>
      'Сделайте резервную копию сид-фразы в Настройках, чтобы защитить свои средства.';

  @override
  String get noConnectionToDaemon => 'Нет подключения к демону';

  @override
  String syncingPercent(String percent) {
    return 'Синхронизация $percent%';
  }

  @override
  String get yourAddress => 'Ваш адрес';

  @override
  String get errorLoadingAddress => 'Ошибка загрузки адреса';

  @override
  String get integratedAddress => 'Интегрированный адрес';

  @override
  String get embedPaymentId => 'Встроить идентификатор платежа в адрес';

  @override
  String get randomShort => 'Случайный короткий (16)';

  @override
  String get randomLong => 'Случайный длинный (64)';

  @override
  String get enterCustomPaymentId =>
      'Или введите свой идентификатор платежа (16 или 64 hex)';

  @override
  String get enterPaymentId => 'Введите идентификатор платежа';

  @override
  String get paymentIdInvalid =>
      'Идентификатор платежа должен содержать 16 или 64 hex-символа';

  @override
  String get shortPid => 'Короткий PID';

  @override
  String get longPid => 'Длинный PID';

  @override
  String get share => 'Поделиться';

  @override
  String get copy => 'Копировать';

  @override
  String get sweepAllFunds => 'Собрать все средства';

  @override
  String get normalSend => 'Обычная отправка';

  @override
  String get sweep => 'Собрать';

  @override
  String get recipientAddress => 'Адрес получателя';

  @override
  String get scanQr => 'Сканировать QR';

  @override
  String get amount => 'Сумма';

  @override
  String availableBalance(String amount) {
    return 'Доступно: $amount';
  }

  @override
  String sweepInfo(String amount) {
    return 'Сбор объединяет все UTXO и отправляет весь разблокированный баланс ($amount) за вычетом комиссии.';
  }

  @override
  String get paymentIdOptional => 'Идентификатор платежа (необязательно)';

  @override
  String get hexCharacters => '16 или 64 hex-символа';

  @override
  String get mustBeHex => 'Должно быть 16 или 64 hex-символа';

  @override
  String get recipientRequired => 'Адрес получателя обязателен';

  @override
  String get invalidAddress => 'Недействительный адрес WRKZ';

  @override
  String get enterValidAmount => 'Введите корректную сумму';

  @override
  String get reviewTransaction => 'Проверка транзакции';

  @override
  String get to => 'Кому';

  @override
  String get fee => 'Комиссия';

  @override
  String get totalDeducted => 'Итого списано';

  @override
  String get paymentId => 'Идентификатор платежа';

  @override
  String get transactionsIrreversible =>
      'Транзакции необратимы. Пожалуйста, проверьте данные.';

  @override
  String get back => 'Назад';

  @override
  String get confirmAndSend => 'Подтвердить и отправить';

  @override
  String get transactionSent => 'Транзакция отправлена!';

  @override
  String get transactionHash => 'Хэш транзакции';

  @override
  String get sendAnother => 'Отправить ещё';

  @override
  String get scanQrCode => 'Сканировать QR-код';

  @override
  String get scannedAddress => 'Отсканированный адрес';

  @override
  String get cancel => 'Отмена';

  @override
  String get useThisAddress => 'Использовать этот адрес';

  @override
  String get sweepFailed => 'Сбор не удался';

  @override
  String get searchPlaceholder =>
      'Поиск по хэшу, адресу, идентификатору платежа...';

  @override
  String get all => 'Все';

  @override
  String get filterReceived => 'Полученные';

  @override
  String get filterSent => 'Отправленные';

  @override
  String get hash => 'Хэш';

  @override
  String get address => 'Адрес';

  @override
  String get block => 'Блок';

  @override
  String get confirmed => 'Подтверждено';

  @override
  String get password => 'Пароль';

  @override
  String get unlock => 'Разблокировать';

  @override
  String get switchWallet => 'Сменить кошелёк';

  @override
  String get enterPasswordToUnlock => 'Введите пароль для разблокировки';

  @override
  String get incorrectPassword => 'Неверный пароль';

  @override
  String get enterYourPassword => 'Введите ваш пароль';

  @override
  String get plutonMobile => 'PLUTON Mobile';

  @override
  String get createFirstWalletSubtitle =>
      'Создайте первый кошелёк для начала работы';

  @override
  String get selectWalletSubtitle => 'Выберите кошелёк для открытия';

  @override
  String get yourWallets => 'Ваши кошельки';

  @override
  String get noWalletsYet => 'Кошельков пока нет';

  @override
  String get lastOpened => 'Последнее открытие';

  @override
  String createdDate(String date) {
    return 'Создан $date';
  }

  @override
  String get createFirstWallet => 'Создать первый кошелёк';

  @override
  String get addWallet => 'Добавить кошелёк';

  @override
  String get deleteWallet => 'Удалить кошелёк';

  @override
  String deleteWalletConfirm(String name) {
    return 'Удалить \"$name\"?\n\nЭто навсегда удалит файл кошелька и ключи. Убедитесь, что вы сохранили резервную копию сид-фразы.';
  }

  @override
  String get delete => 'Удалить';

  @override
  String get createNewWallet => 'Создать новый кошелёк';

  @override
  String get createNewWalletSubtitle =>
      'Сгенерировать новый кошелёк с новой сид-фразой';

  @override
  String get importFromSeed => 'Импорт из сид-фразы';

  @override
  String get importFromSeedSubtitle =>
      'Восстановить кошелёк с помощью мнемонической сид-фразы из 25 слов';

  @override
  String get importFromKeys => 'Импорт из приватных ключей';

  @override
  String get importFromKeysSubtitle =>
      'Восстановить с помощью ключа расходования и ключа просмотра';

  @override
  String get viewOnlyWallet => 'Кошелёк только для просмотра';

  @override
  String get viewOnlyWalletSubtitle =>
      'Кошелёк-наблюдатель с использованием ключа просмотра и адреса';

  @override
  String get createWallet => 'Создать кошелёк';

  @override
  String get importWallet => 'Импортировать кошелёк';

  @override
  String get walletName => 'Название кошелька';

  @override
  String get walletNameHint => 'напр. Основной кошелёк';

  @override
  String get passwordLabel => 'Пароль';

  @override
  String get enterPassword => 'Введите пароль';

  @override
  String get confirmPassword => 'Подтвердите пароль';

  @override
  String get seedPhrase => 'Сид-фраза (25 слов)';

  @override
  String get enterSeedPhrase => 'Введите вашу сид-фразу...';

  @override
  String get scanHeight => 'Высота сканирования (необязательно)';

  @override
  String get scanHeightHint => '0 = сканировать с начала';

  @override
  String get privateSpendKey => 'Приватный ключ расходования';

  @override
  String get privateViewKey => 'Приватный ключ просмотра';

  @override
  String get walletAddress => 'Адрес кошелька';

  @override
  String get walletAddressHint => 'Wrkz... адрес';

  @override
  String get hexKey => '64-символьный hex';

  @override
  String get daemonNode => 'Узел демона';

  @override
  String get custom => 'Свой';

  @override
  String get host => 'Хост';

  @override
  String get hostHint => 'Хост / IP';

  @override
  String get port => 'Порт';

  @override
  String get ssl => 'SSL';

  @override
  String get walletNameRequired => 'Название кошелька обязательно';

  @override
  String get passwordRequired => 'Пароль обязателен';

  @override
  String passwordTooShort(int count) {
    return 'Пароль должен содержать не менее $count символов';
  }

  @override
  String get passwordsDoNotMatch => 'Пароли не совпадают';

  @override
  String get seedRequired => 'Сид-фраза обязательна';

  @override
  String get spendKeyRequired => 'Ключ расходования обязателен';

  @override
  String get viewKeyRequired => 'Ключ просмотра обязателен';

  @override
  String get addressRequired => 'Адрес обязателен';

  @override
  String get daemonHostRequired => 'Хост демона обязателен';

  @override
  String get backupSeedTitle => 'Резервная копия сид-фразы';

  @override
  String get backupWarning =>
      'Запишите сид-фразу и храните её в надёжном месте. Если вы её потеряете, ваши средства будут утеряны навсегда.';

  @override
  String get seedPhraseLabel => 'Сид-фраза';

  @override
  String get privateViewKeyLabel => 'Приватный ключ просмотра';

  @override
  String get privateSpendKeyLabel => 'Приватный ключ расходования';

  @override
  String get backupConfirmCheck =>
      'Я надёжно сохранил(а) резервную копию сид-фразы';

  @override
  String get continueToWallet => 'Перейти к кошельку';

  @override
  String get sectionDaemonNode => 'Узел демона';

  @override
  String get apply => 'Применить';

  @override
  String nodeUpdated(String host, int port) {
    return 'Узел обновлён: $host:$port';
  }

  @override
  String get hostRequired => 'Хост обязателен';

  @override
  String currentWallet(String name) {
    return 'Текущий кошелёк — $name';
  }

  @override
  String get saveWallet => 'Сохранить кошелёк';

  @override
  String get walletSaved => 'Кошелёк сохранён';

  @override
  String saveFailed(String error) {
    return 'Ошибка сохранения: $error';
  }

  @override
  String get backupSeed => 'Резервная копия сид-фразы';

  @override
  String get changePassword => 'Изменить пароль';

  @override
  String get resetScanHeight => 'Сбросить высоту сканирования';

  @override
  String get reset => 'Сбросить';

  @override
  String resetScanConfirm(int height) {
    return 'Это приведёт к повторному сканированию блокчейна начиная с блока $height. Это может занять некоторое время. Продолжить?';
  }

  @override
  String scanResetTo(int height) {
    return 'Сканирование сброшено до блока $height';
  }

  @override
  String resetFailed(String error) {
    return 'Сброс не удался: $error';
  }

  @override
  String get enterPasswordTitle => 'Введите пароль';

  @override
  String get confirm => 'Подтвердить';

  @override
  String get seedBackup => 'Резервная копия сид-фразы';

  @override
  String get seedPhraseColon => 'Сид-фраза:';

  @override
  String get privateViewKeyColon => 'Приватный ключ просмотра:';

  @override
  String get iveBackedUp => 'Я сохранил(а) резервную копию';

  @override
  String get currentPasswordLabel => 'Текущий пароль';

  @override
  String get newPasswordLabel => 'Новый пароль';

  @override
  String get confirmNewPasswordLabel => 'Подтвердите новый пароль';

  @override
  String get change => 'Изменить';

  @override
  String get currentPasswordIncorrect => 'Текущий пароль неверен';

  @override
  String get newPasswordsDoNotMatch => 'Новые пароли не совпадают';

  @override
  String get passwordChanged => 'Пароль изменён';

  @override
  String get walletManagement => 'Управление кошельками';

  @override
  String get switchWalletSubtitle => 'Сохранить и закрыть, выбрать другой';

  @override
  String get manageWallets => 'Управление кошельками';

  @override
  String get manageWalletsSubtitle => 'Переименовать или удалить кошельки';

  @override
  String get currentlyOpen => '(открыт сейчас)';

  @override
  String get close => 'Закрыть';

  @override
  String get renameWallet => 'Переименовать кошелёк';

  @override
  String get newName => 'Новое название';

  @override
  String get rename => 'Переименовать';

  @override
  String deleteWalletConfirmShort(String name) {
    return 'Удалить \"$name\"? Это действие нельзя отменить.';
  }

  @override
  String get security => 'Безопасность';

  @override
  String get biometricUnlock => 'Биометрическая разблокировка';

  @override
  String get biometricSubtitle => 'Отпечаток пальца / Face ID';

  @override
  String get biometricNotAvailable => 'Биометрия недоступна';

  @override
  String get autoLock => 'Автоблокировка';

  @override
  String get appearance => 'Внешний вид';

  @override
  String get theme => 'Тема';

  @override
  String get themeAuto => 'Авто';

  @override
  String get themeLight => 'Светлая';

  @override
  String get themeDark => 'Тёмная';

  @override
  String get preferences => 'Предпочтения';

  @override
  String get transactionNotifications => 'Уведомления о транзакциях';

  @override
  String get notificationsSubtitle => 'Оповещение о входящих транзакциях';

  @override
  String get autosave => 'Автосохранение';

  @override
  String get autosaveSubtitle =>
      'Сохранять после синхронизации, затем каждые 5 минут';

  @override
  String get scanCoinbaseTx => 'Сканировать транзакции Coinbase';

  @override
  String get scanCoinbaseSubtitle =>
      'Включить вознаграждения майнеров (по умолчанию отключено)';

  @override
  String get dangerZone => 'Опасная зона';

  @override
  String get deleteCurrentWallet => 'Удалить текущий кошелёк';

  @override
  String get deleteCurrentWalletSubtitle => 'Навсегда удалить данные кошелька';

  @override
  String get deleteWalletTypeCaps =>
      'Это навсегда удалит файл кошелька и ключи. Убедитесь, что вы сохранили резервную копию сид-фразы.\n\nВведите DELETE для подтверждения:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get language => 'Язык';

  @override
  String get selectLanguage => 'Выбрать язык';

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
  String get autoLockImmediately => 'Немедленно';

  @override
  String get autoLock1Min => '1 минута';

  @override
  String get autoLock5Min => '5 минут';

  @override
  String get autoLockNever => 'Никогда';

  @override
  String get preparedTransactionExpired =>
      'Эта транзакция больше не действительна. Вернитесь и создайте её заново.';

  @override
  String get deleteConfirmMismatch => 'Введите точно DELETE для подтверждения.';

  @override
  String get seedNotBackedUpWarning =>
      'Вы не подтвердили резервную копию сид-фразы этого кошелька. Удаление сейчас означает, что средства невозможно будет восстановить.';

  @override
  String get wrkzReceived => 'WRKZ получены';

  @override
  String get retry => 'Повторить';

  @override
  String youReceivedAmount(String amount) {
    return 'Вы получили $amount';
  }

  @override
  String get ringSize => 'Размер кольца';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Размер кольца уменьшен до $actual (обычно $normal). Для отправляемых сумм в цепочке недостаточно выходов, чтобы сформировать полное кольцо, поэтому эта транзакция менее приватна, чем обычно.';
  }

  @override
  String get liteNodeTitle => 'Облегчённый узел';

  @override
  String liteNodeServesFrom(int height) {
    return 'Этот узел хранит блоки только начиная с $height. Транзакции до этого блока через него найти невозможно.';
  }

  @override
  String liteNodeMissesHistory(int nodeHeight, int walletHeight) {
    return 'Этот узел начинается с блока $nodeHeight, а кошелёк — с блока $walletHeight. Всё полученное между ними здесь не видно, поэтому показанный баланс может быть занижен. Подключитесь к узлу с полной цепочкой, чтобы увидеть его.';
  }

  @override
  String liteNodeSyncStalled(int wallet, int node) {
    return 'Синхронизация остановлена на блоке $wallet. Этот узел не хранит ничего ниже блока $node, поэтому промежуточные блоки с него не скачать. Баланс останется неполным, пока вы не подключите узел с полной цепочкой.';
  }

  @override
  String get liteNodeRescanRefusedTitle =>
      'Этот узел не может пересканировать так далеко назад';

  @override
  String liteNodeRescanRefused(int height) {
    return 'Подключённый узел — облегчённый и не хранит данные блоков ниже $height. Пересканирование с меньшей высоты отбросит уже найденные кошельком транзакции, и найти их здесь снова будет невозможно. Ничего не изменено.';
  }

  @override
  String liteNodeRescanFromInstead(int height) {
    return 'Пересканировать с $height';
  }

  @override
  String liteNodeRescanHint(int height) {
    return 'Подключённый узел может пересканировать только с блока $height и выше.';
  }

  @override
  String get nodeServesFromLabel => 'Отдаёт блоки с';

  @override
  String get nodeFullChain => 'Полная цепочка';

  @override
  String get localNodeMobileFuture =>
      'Запуск узла прямо на телефоне запланирован, но пока недоступен: узлу нужно несколько ГБ памяти и часы синхронизации. А пока укажите кошельку адрес своего собственного узла.';

  @override
  String get syncStoppedTitle => 'Синхронизация остановлена';

  @override
  String syncGapStalled(int covered, int servesFrom) {
    return 'Синхронизация остановлена на блоке $covered. Узел, с которым шёл обмен, отвечает только начиная с блока $servesFrom, поэтому блоки между ними с него не скачать. Баланс неполный, пока вы не подключите узел с полной цепочкой.';
  }

  @override
  String get txPowServerSection => 'PoW-сервер транзакций';

  @override
  String get txPowServerUse => 'Использовать внешний PoW-сервер';

  @override
  String get txPowServerSubtitle =>
      'Отправлять proof of work транзакции на сервер вместо вычисления на этом устройстве. Если сервер не отвечает, используется процессор этого устройства.';

  @override
  String get txPowServerSaved => 'Настройки PoW-сервера сохранены';

  @override
  String get txPowServerInvalid => 'Введите корректный хост и порт';

  @override
  String get txPowServerTest => 'Проверить';

  @override
  String txPowServerTestOk(int ms, int threads, int queue, int capacity) {
    return 'Сервер доступен за $ms мс: потоков $threads, занято $queue из $capacity мест в очереди';
  }

  @override
  String txPowServerTestFailed(String error) {
    return 'Сервер недоступен: $error';
  }

  @override
  String get nodeTest => 'Проверить';

  @override
  String nodeTestOk(int ms, int height, int peers) {
    return 'Доступна за $ms мс: высота $height, пиров $peers';
  }

  @override
  String nodeTestSyncing(int ms, int height, int networkHeight) {
    return 'Доступна за $ms мс, но нода ещё синхронизируется: высота $height из $networkHeight';
  }

  @override
  String nodeTestFailed(String error) {
    return 'Нода недоступна: $error';
  }
}
