// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Russian (`ru`).
class SRu extends S {
  SRu([String locale = 'ru']) : super(locale);

  @override
  String get appTitle => 'PLUTON v2';

  @override
  String get tabOverview => 'Обзор';

  @override
  String get tabReceive => 'Получить';

  @override
  String get tabTransfer => 'Перевод';

  @override
  String get tabHistory => 'История';

  @override
  String get tabAddressBook => 'Адресная книга';

  @override
  String get tabSettings => 'Настройки';

  @override
  String get tabAbout => 'О приложении';

  @override
  String get lockWallet => 'Заблокировать кошелёк';

  @override
  String get send => 'Отправить';

  @override
  String get receive => 'Получить';

  @override
  String get transfer => 'Перевод';

  @override
  String get available => 'Доступно';

  @override
  String get locked => 'Заблокировано';

  @override
  String get total => 'Итого';

  @override
  String get availableBalance => 'Доступный баланс';

  @override
  String lockedUnconfirmed(String amount, String ticker) {
    return 'Заблокировано (неподтверждённые): $amount $ticker';
  }

  @override
  String totalBalance(String amount, String ticker) {
    return 'Итого: $amount $ticker';
  }

  @override
  String get balanceIncompleteWhileSyncing =>
      'Баланс может быть неполным во время синхронизации';

  @override
  String errorPrefix(String message) {
    return 'Ошибка: $message';
  }

  @override
  String get network => 'Сеть';

  @override
  String get syncStatus => 'Статус синхронизации';

  @override
  String get synced => 'Синхронизировано';

  @override
  String get syncing => 'Синхронизация…';

  @override
  String get walletBlock => 'Блок кошелька';

  @override
  String get networkBlock => 'Блок сети';

  @override
  String get peers => 'Пиры';

  @override
  String get walletType => 'Тип кошелька';

  @override
  String get viewOnly => 'Только просмотр';

  @override
  String get full => 'Полный';

  @override
  String get nodeConnectionIssue => 'Проблема подключения к ноде';

  @override
  String get switchNodeInSettings => 'Сменить ноду в Настройках →';

  @override
  String get recentTransactions => 'Последние транзакции';

  @override
  String get viewAll => 'Показать все →';

  @override
  String get noTransactionsYet => 'Транзакций пока нет';

  @override
  String get received => 'Получено';

  @override
  String get sent => 'Отправлено';

  @override
  String syncingProgress(String pct, int wallet, int network) {
    return 'Синхронизация $pct% (блок $wallet / $network)';
  }

  @override
  String get shareAddressSubtitle =>
      'Поделитесь своим адресом для получения WRKZ';

  @override
  String get yourAddress => 'Ваш адрес';

  @override
  String get generateIntegratedAddress => 'Сгенерировать интегрированный адрес';

  @override
  String get integratedAddressDescription =>
      'Объедините ваш адрес с идентификатором платежа. Используйте кнопки случайной генерации для нового ID или введите свой ниже.';

  @override
  String get randomShort16 => 'Случайный короткий (16)';

  @override
  String get randomLong64 => 'Случайный длинный (64)';

  @override
  String get customPaymentIdLabel =>
      'Произвольный идентификатор платежа (16 или 64 hex-символа)';

  @override
  String get generate => 'Сгенерировать';

  @override
  String get integratedAddress => 'Интегрированный адрес';

  @override
  String get paymentIdShort => 'Короткий (16)';

  @override
  String get paymentIdLong => 'Длинный (64)';

  @override
  String paymentIdLabel(String label) {
    return 'Идентификатор платежа · $label';
  }

  @override
  String get enterPaymentIdError =>
      'Введите идентификатор платежа (16 или 64 hex-символа)';

  @override
  String get paymentIdInvalidError =>
      'Идентификатор платежа должен содержать 16 или 64 шестнадцатеричных символа';

  @override
  String get copyAddress => 'Копировать адрес';

  @override
  String get copyPaymentId => 'Копировать идентификатор платежа';

  @override
  String get copy => 'Копировать';

  @override
  String get copied => 'Скопировано!';

  @override
  String get sendWrkzToAny => 'Отправить WRKZ на любой адрес';

  @override
  String get sweepAllDescription =>
      'Отправить все средства на адрес (консолидация UTXO)';

  @override
  String get sweepAll => 'Отправить всё';

  @override
  String get sweepWarning =>
      'Отправка всего объединяет все UTXO в один выход. Используйте эту функцию, когда транзакции не проходят из-за слишком большого количества входов.';

  @override
  String sweepAvailableBalance(String amount, String ticker) {
    return 'Доступно: $amount $ticker (весь баланс будет отправлен за вычетом комиссии)';
  }

  @override
  String get destinationAddress => 'Адрес назначения';

  @override
  String get addressBook => 'Адресная книга';

  @override
  String get sweepAllFunds => 'Отправить все средства';

  @override
  String get recipientAddress => 'Адрес получателя';

  @override
  String get amount => 'Сумма';

  @override
  String get paymentIdOptional => 'Идентификатор платежа (необязательно)';

  @override
  String get hexCharacters => '16 или 64 шестнадцатеричных символа';

  @override
  String get reviewTransaction => 'Проверить транзакцию';

  @override
  String get reviewAndConfirm => 'Проверить и подтвердить';

  @override
  String get to => 'Кому';

  @override
  String get fee => 'Комиссия';

  @override
  String get totalDeducted => 'Итого к списанию';

  @override
  String get paymentId => 'Идентификатор платежа';

  @override
  String get transactionsIrreversible =>
      'Транзакции необратимы. Проверьте адрес перед подтверждением.';

  @override
  String get back => 'Назад';

  @override
  String get confirmAndSend => 'Подтвердить и отправить';

  @override
  String get transactionSent => 'Транзакция отправлена!';

  @override
  String get transactionBroadcast => 'Ваша транзакция была отправлена в сеть.';

  @override
  String get transactionHash => 'Хеш транзакции';

  @override
  String get sendAnother => 'Отправить ещё';

  @override
  String get enterDestinationAddress => 'Введите адрес назначения';

  @override
  String get enterValidAmount => 'Введите корректную сумму';

  @override
  String computingPow(int seconds) {
    return 'Вычисление PoW... $secondsс';
  }

  @override
  String get stepFillDetails => 'Заполните данные';

  @override
  String get stepReview => 'Проверка';

  @override
  String get stepDone => 'Готово';

  @override
  String get sweepFailed => 'Ошибка отправки всех средств';

  @override
  String get addressBookTitle => 'Адресная книга';

  @override
  String get transactionHistory => 'История транзакций';

  @override
  String get searchByHash =>
      'Поиск по хешу, адресу или идентификатору платежа…';

  @override
  String get all => 'Все';

  @override
  String get filterReceived => 'Полученные';

  @override
  String get filterSent => 'Отправленные';

  @override
  String get refresh => 'Обновить';

  @override
  String get noTransactionsFound => 'Транзакции не найдены';

  @override
  String get confirmed => 'Подтверждена';

  @override
  String get pending => 'Ожидание';

  @override
  String get hash => 'Хеш';

  @override
  String get address => 'Адрес';

  @override
  String get block => 'Блок';

  @override
  String showingRange(int start, int end, int total) {
    return 'Показано $start–$end из $total';
  }

  @override
  String get previous => 'Назад';

  @override
  String get next => 'Далее';

  @override
  String get walletLocked => 'Кошелёк заблокирован';

  @override
  String get enterPasswordToContinue =>
      'Введите пароль кошелька для продолжения';

  @override
  String get password => 'Пароль';

  @override
  String get incorrectPassword => 'Неверный пароль';

  @override
  String get unlock => 'Разблокировать';

  @override
  String get closeWalletInstead => 'Закрыть кошелёк';

  @override
  String get closeWallet => 'Закрыть кошелёк';

  @override
  String get closeWalletDescription =>
      'Кошелёк будет сохранён и закрыт.\n\nВы вернётесь на экран входа.';

  @override
  String get cancel => 'Отмена';

  @override
  String get welcomeToPluton => 'Добро пожаловать в PLUTON v2';

  @override
  String get selectOptionToStart => 'Выберите действие для начала работы';

  @override
  String get createNewWallet => 'Создать новый кошелёк';

  @override
  String get openExistingWallet => 'Открыть существующий кошелёк';

  @override
  String get importFromSeed => 'Импорт из мнемонической фразы';

  @override
  String get importFromKeys => 'Импорт из приватных ключей';

  @override
  String get openWallet => 'Открыть кошелёк';

  @override
  String get importFromSeedTitle => 'Импорт из фразы';

  @override
  String get importFromKeysTitle => 'Импорт из ключей';

  @override
  String get saveWalletTo => 'Сохранить кошелёк в';

  @override
  String get walletFile => 'Файл кошелька';

  @override
  String get walletPassword => 'Пароль кошелька';

  @override
  String get mnemonicSeedPhrase => 'Мнемоническая фраза';

  @override
  String get scanFromHeight => 'Сканировать с высоты (0 = полное сканирование)';

  @override
  String get daemonHost => 'Хост демона';

  @override
  String get port => 'Порт';

  @override
  String get continueButton => 'Продолжить';

  @override
  String get browse => 'Обзор…';

  @override
  String get backupWarning =>
      'Сделайте резервную копию кошелька перед продолжением.\nЭти ключи невозможно восстановить в случае утери.';

  @override
  String get yourWalletAddress => 'Адрес вашего кошелька';

  @override
  String get seedPhrase25Words => 'Мнемоническая фраза (25 слов)';

  @override
  String get privateViewKey => 'Приватный ключ просмотра';

  @override
  String get privateSpendKey => 'Приватный ключ траты';

  @override
  String get seedBackupConfirm =>
      'Я записал(а) мнемоническую фразу и приватные ключи в безопасном месте.';

  @override
  String get backedUpContinue => 'Резервная копия сделана — Продолжить';

  @override
  String get settings => 'Настройки';

  @override
  String get sectionDaemonNode => 'Нода демона';

  @override
  String get nodeDescription =>
      'Подключение к локальной или удалённой ноде. Изменения вступают в силу немедленно.';

  @override
  String get hostIpAddress => 'Хост / IP-адрес';

  @override
  String get ssl => 'SSL';

  @override
  String get apply => 'Применить';

  @override
  String get nodeUpdatedSuccess => 'Нода успешно обновлена';

  @override
  String get nodeUnreachable =>
      'Не удаётся подключиться к текущей ноде. Введите новый адрес ноды ниже и нажмите Применить.';

  @override
  String get sectionWallet => 'Кошелёк';

  @override
  String get saveWallet => 'Сохранить кошелёк';

  @override
  String get saveWalletSubtitle => 'Сохранить текущее состояние на диск';

  @override
  String get walletSaved => 'Кошелёк сохранён';

  @override
  String get exportToJson => 'Экспорт в JSON';

  @override
  String get exportToJsonSubtitle => 'Сохранить данные кошелька в формате JSON';

  @override
  String get exportJsonTitle => 'Экспорт JSON кошелька';

  @override
  String exportedTo(String path) {
    return 'Экспортировано в $path';
  }

  @override
  String exportFailed(String error) {
    return 'Ошибка экспорта: $error';
  }

  @override
  String get resetScanHeight => 'Сбросить высоту сканирования';

  @override
  String get resetScanHeightSubtitle =>
      'Пересканировать блокчейн с указанной высоты';

  @override
  String get resetScanHeightDescription =>
      'Введите высоту блока для пересканирования. Используйте 0 для полного пересканирования.';

  @override
  String get scanHeight => 'Высота сканирования';

  @override
  String get reset => 'Сбросить';

  @override
  String get autosave => 'Автосохранение';

  @override
  String get autosaveSubtitle =>
      'Сохранять кошелёк на диск после синхронизации и каждые 5 минут';

  @override
  String get scanCoinbaseTx => 'Сканировать Coinbase-транзакции';

  @override
  String get scanCoinbaseSubtitle =>
      'Включить награды майнера при синхронизации (по умолчанию выключено)';

  @override
  String get sectionAppearance => 'Внешний вид';

  @override
  String get theme => 'Тема';

  @override
  String get themeSubtitle => 'Выберите цветовую схему приложения';

  @override
  String get themeSystem => 'Системная';

  @override
  String get themeLight => 'Светлая';

  @override
  String get themeDark => 'Тёмная';

  @override
  String get sectionNotifications => 'Уведомления';

  @override
  String get incomingTxAlerts => 'Уведомления о входящих транзакциях';

  @override
  String get incomingTxAlertsSubtitle =>
      'Показывать уведомление на рабочем столе при получении WRKZ';

  @override
  String get sectionDebugLogs => 'Отладка и логи';

  @override
  String get logLevel => 'Уровень логирования';

  @override
  String get logLevelSubtitle =>
      'Управляет детализацией логов библиотеки кошелька';

  @override
  String get viewLogs => 'Просмотр логов';

  @override
  String get viewLogsSubtitle =>
      'Вывод логов библиотеки кошелька в реальном времени';

  @override
  String get walletLogs => 'Логи кошелька';

  @override
  String logEntries(int count) {
    return '$count записей';
  }

  @override
  String get autoScroll => 'Автопрокрутка';

  @override
  String get copyAll => 'Копировать всё';

  @override
  String get clear => 'Очистить';

  @override
  String get close => 'Закрыть';

  @override
  String get noLogsYet =>
      'Логов пока нет. Установите уровень логирования выше «Отключено» для просмотра.';

  @override
  String get logsCopied => 'Логи скопированы в буфер обмена';

  @override
  String get sectionDangerZone => 'Опасная зона';

  @override
  String get deleteWalletData => 'Удалить данные кошелька';

  @override
  String get deleteWalletDataSubtitle =>
      'Безвозвратно удалить файл кошелька с диска';

  @override
  String get deleteWalletWarning =>
      'Файл кошелька будет безвозвратно удалён с диска.\n\nУбедитесь, что вы сохранили мнемоническую фразу и приватные ключи. Это действие нельзя отменить.';

  @override
  String get iUnderstandContinue => 'Я понимаю, продолжить';

  @override
  String get finalConfirmation => 'Финальное подтверждение';

  @override
  String get typeDeleteToConfirm => 'Введите DELETE для подтверждения:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get deletePermanently => 'Удалить безвозвратно';

  @override
  String get aboutTitle => 'О приложении';

  @override
  String versionInfo(String version) {
    return 'Версия $version — Десктопный кошелёк WRKZ';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 — официальный десктопный кошелёк для WrkzCoin (WRKZ), быстрой и лёгкой криптовалюты на основе CryptoNote.\n\nСоздан на Flutter, работает на wallet-api.';

  @override
  String get github => 'GitHub';

  @override
  String get githubSubtitle => 'Исходный код и релизы';

  @override
  String get discord => 'Discord';

  @override
  String get discordSubtitle => 'Присоединяйтесь к сообществу';

  @override
  String get twitterX => 'Twitter / X';

  @override
  String get twitterXSubtitle => 'Подписывайтесь на @wrkzcoin';

  @override
  String get website => 'Веб-сайт';

  @override
  String get websiteSubtitle => 'wrkz.work';

  @override
  String get license => 'Лицензия';

  @override
  String get licenseText =>
      'Выпущено под лицензией MIT.\nИспользуйте на свой риск. Всегда делайте резервную копию мнемонической фразы.';

  @override
  String get addButton => 'Добавить';

  @override
  String get noSavedAddresses => 'Нет сохранённых адресов';

  @override
  String get tapAddToSave =>
      'Нажмите «Добавить», чтобы сохранить часто используемый адрес.';

  @override
  String get addAddress => 'Добавить адрес';

  @override
  String get nameLabel => 'Имя / метка';

  @override
  String get addressLabel => 'Адрес';

  @override
  String get noteOptional => 'Заметка (необязательно)';

  @override
  String get nameAndAddressRequired => 'Имя и адрес обязательны';

  @override
  String get invalidWrkzAddress =>
      'Недопустимый адрес WRKZ. Должен содержать 98 (стандартный), 120 (короткий интегрированный) или 186 (длинный интегрированный) символов и начинаться с «Wrkz».';

  @override
  String get save => 'Сохранить';

  @override
  String get editEntry => 'Редактировать запись';

  @override
  String get deleteEntry => 'Удалить запись';

  @override
  String removeFromAddressBook(String name) {
    return 'Удалить «$name» из адресной книги?';
  }

  @override
  String get delete => 'Удалить';

  @override
  String get edit => 'Редактировать';

  @override
  String get wrkzReceived => 'WRKZ получены';

  @override
  String youReceivedAmount(String amount) {
    return 'Вы получили $amount';
  }

  @override
  String get show => 'Показать';

  @override
  String get exit => 'Выход';

  @override
  String get plutonWallet => 'Кошелёк PLUTON';

  @override
  String get language => 'Язык';

  @override
  String get selectLanguage => 'Выбор языка';

  @override
  String get chooseLanguage => 'Выберите предпочтительный язык';

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
      'Эта транзакция больше не действительна. Вернитесь и создайте её заново.';

  @override
  String get deleteConfirmMismatch => 'Введите точно DELETE для подтверждения.';

  @override
  String get unlockNeedsReopen =>
      'Не удалось проверить пароль на этом устройстве. Используйте «Закрыть кошелёк» и откройте кошелёк заново.';

  @override
  String get exportJsonWarningTitle =>
      'Экспортировать незашифрованный кошелёк?';

  @override
  String get exportJsonWarningBody =>
      'Экспортируемый файл содержит ваш приватный ключ просмотра и приватные ключи траты в открытом виде. Любой, кто его прочитает, сможет потратить ваши средства.\n\nСохраняйте его только в хранилище, которое вы контролируете, и удалите сразу после использования.';

  @override
  String passwordTooShort(int count) {
    return 'Пароль должен содержать не менее $count символов';
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
  String get sectionLocalNode => 'Локальный облегчённый узел';

  @override
  String get localNodeDescription =>
      'Запустите узел на этом компьютере и синхронизируйтесь с ним вместо удалённого сервера. Облегчённый узел хранит только то, что нужно кошельку, но всё равно один раз скачивает всю цепочку.';

  @override
  String get localNodeSetUp => 'Настроить локальный узел';

  @override
  String get localNodeStart => 'Запустить';

  @override
  String get localNodeStop => 'Остановить';

  @override
  String get localNodeUse => 'Использовать этот узел';

  @override
  String get localNodeDelete => 'Удалить данные узла';

  @override
  String get localNodeStateStopped => 'Остановлен';

  @override
  String get localNodeStateStarting => 'Запускается';

  @override
  String get localNodeStateSyncing => 'Синхронизация';

  @override
  String get localNodeStateReady => 'Синхронизирован';

  @override
  String get localNodeStateFailed => 'Ошибка';

  @override
  String localNodeProgress(int height, int network) {
    return 'Блок $height из $network';
  }

  @override
  String localNodePeers(int count) {
    return '$count пиров';
  }

  @override
  String get localNodeNotReadyYet =>
      'Локальный узел ещё догоняет сеть и пока не может обслуживать кошелёк. Он продолжает синхронизацию в фоне — оставайтесь на удалённом узле, пока он не будет готов, затем переключитесь.';

  @override
  String get localNodeInUse => 'Кошелёк подключён к этому узлу.';

  @override
  String localNodeBinaryMissing(String name) {
    return '$name не найден. Поместите исполняемый файл демона рядом с приложением кошелька или в папку «sidecar» рядом с ним и попробуйте снова.';
  }

  @override
  String get localNodeSetupTitle => 'Настройка локального облегчённого узла';

  @override
  String get localNodeSetupCost =>
      'Перед началом:\n• Около 6 ГБ на диске, и вся цепочка скачивается один раз.\n• Первая синхронизация занимает часы. Она идёт в фоне, и пока можно пользоваться удалённым узлом.\n• Начальная высота ниже задаётся навсегда. Чтобы изменить её потом, придётся удалить узел и синхронизировать всё заново.';

  @override
  String get localNodeStartHeightLabel => 'Начальная высота';

  @override
  String localNodeStartHeightHelp(int height) {
    return 'Блоки ниже этой высоты скачиваются и проверяются, но сохраняется только индекс, нужный последующим блокам. Задайте её не выше начальной высоты этого кошелька ($height).';
  }

  @override
  String localNodeStartHeightTooHigh(int height) {
    return 'Выше начальной высоты этого кошелька ($height). Узел никогда не сможет показать его более старые транзакции.';
  }

  @override
  String get localNodeCreate => 'Создать узел';

  @override
  String get localNodeDeleteTitle => 'Удалить данные локального узла?';

  @override
  String get localNodeDeleteWarning =>
      'Это остановит узел и безвозвратно удалит его базу данных цепочки с диска. Кошелёк, его seed-фраза и средства не затрагиваются, но новый локальный узел начнёт синхронизацию с нуля, а это часы.';

  @override
  String get localNodeDeleted => 'Локальный узел удалён.';

  @override
  String localNodeDiskUsage(String size) {
    return '$size на диске';
  }

  @override
  String get nodePresetRemote => 'Удалённый узел';

  @override
  String get nodePresetLocal => 'Локальный облегчённый узел';

  @override
  String get savingWallet => 'Сохранение кошелька…';

  @override
  String get savingWalletBody =>
      'PLUTON записывает кошелёк на диск. На большом кошельке это может занять некоторое время.';

  @override
  String get shutdownTakingLong =>
      'Это занимает больше времени, чем ожидалось. Выход сейчас отменит это сохранение — файл кошелька на диске останется прежним, ничего не испортится, но всё после последнего сохранения пропадёт.';

  @override
  String get quitAnyway => 'Всё равно выйти';

  @override
  String get stillRunningInTray =>
      'PLUTON продолжает работать в системном трее. Нажмите на значок, чтобы вернуть окно, или нажмите правой кнопкой и выберите «Выход».';

  @override
  String get localNodeDataFolder => 'Папка данных';

  @override
  String get localNodeDataFolderHelp =>
      'Сюда будет записано около 6 ГБ. Выберите диск с достаточным местом.';

  @override
  String get localNodeDataFolderInUse =>
      'В этой папке уже есть другие файлы. Выберите пустую или новую папку.';

  @override
  String get localNodeStartHeightRequired =>
      'Укажите высоту, с которой хранить полные блоки. Она должна быть больше нуля — лайт-узел не может начинаться с генезис-блока.';

  @override
  String get nodeExitTitle => 'Локальный узел всё ещё работает';

  @override
  String get nodeExitBodySyncing =>
      'Первая синхронизация не завершена. Она занимает часы и идёт только пока узел запущен — но оставленный работать узел продолжает использовать диск и сеть после закрытия PLUTON.';

  @override
  String get nodeExitBodySynced =>
      'Он на уровне сети. Если оставить его работать, он таким и останется и продолжит тратить процессор, трафик и диск, пока PLUTON закрыт; если остановить — в следующий раз потребуется короткое догоняние.';

  @override
  String get nodeExitKeep => 'Оставить работать';

  @override
  String get nodeExitStop => 'Остановить';

  @override
  String get nodeExitChangeLater =>
      'Можно изменить позже в Настройках, в разделе Локальный лайт-узел.';

  @override
  String get rememberMyChoice => 'Запомнить выбор';

  @override
  String get shutdownStoppingNode => 'Остановка локального узла…';

  @override
  String get shutdownStoppingNodeBody =>
      'Даём ему сбросить базу на диск, чтобы следующий запуск не переигрывал журнал записи.';

  @override
  String get nodeExitPolicyLabel => 'При закрытии кошелька';

  @override
  String get nodeExitPolicyAsk => 'Спрашивать';

  @override
  String get nodeExitPolicyKeep => 'Оставлять узел работать';

  @override
  String get nodeExitPolicyStop => 'Останавливать узел';

  @override
  String get localNodeStillRunningBody =>
      'Локальный лайт-узел продолжает работать в фоне. Откройте PLUTON, чтобы его остановить.';

  @override
  String get syncStoppedTitle => 'Синхронизация остановлена';

  @override
  String syncGapStalled(int covered, int servesFrom) {
    return 'Синхронизация остановлена на блоке $covered. Узел, с которым шёл обмен, отвечает только начиная с блока $servesFrom, поэтому блоки между ними с него не скачать. Баланс неполный, пока вы не подключите узел с полной цепочкой.';
  }

  @override
  String get localNodeNotReadyTitle => 'Этот узел не готов';

  @override
  String localNodeNotReadyBody(int behind) {
    return 'Локальный узел отстаёт от сети ещё на $behind блоков. Если переключить кошелёк на него сейчас, синхронизация встанет до его догона, а баланс будет без всего, чего он ещё не достиг — и ничто на экране этого не объяснит. Остаться на удалённом узле ничего не стоит: узел в любом случае продолжает синхронизацию.';
  }

  @override
  String get switchAnyway => 'Всё равно переключить';

  @override
  String get switchToRemoteNode => 'Переключиться на удалённый узел';

  @override
  String get nodeWillServeFromLabel => 'Будет отдавать блоки с';

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
}
