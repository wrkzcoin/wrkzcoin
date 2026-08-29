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
  String get browse => 'Обзор';

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
    return 'Версия $version — Веб-кошелёк WRKZ';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 — официальный веб-кошелёк для WrkzCoin (WRKZ), быстрой и лёгкой криптовалюты на основе CryptoNote.\n\nСоздан на Flutter, работает на wallet-api.';

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
  String get pageNotFound => 'Page not found';

  @override
  String get backToWallet => 'Back to wallet';

  @override
  String get hide => 'Hide';

  @override
  String get max => 'MAX';

  @override
  String get viewInExplorer => 'View in explorer';

  @override
  String get pendingConfirmation => 'Pending confirmation';

  @override
  String get addressWrongPrefix => 'A WRKZ address starts with “Wrkz”';

  @override
  String get addressWrongLength =>
      'Wrong length — a WRKZ address is 98 characters (120 or 186 if integrated)';

  @override
  String get addressBadCharacters =>
      'Contains characters that are not valid in an address (0, O, I and l are never used)';

  @override
  String addressAlreadySaved(String name) {
    return 'Already saved as “$name”';
  }

  @override
  String get amountMustBePositive => 'Amount must be greater than zero';

  @override
  String amountTooManyDecimals(int places) {
    return 'At most $places decimal places';
  }

  @override
  String get amountTooLarge => 'Amount is too large';

  @override
  String amountExceedsBalance(String amount, String ticker) {
    return 'More than your available balance ($amount $ticker, plus fee)';
  }

  @override
  String get networkFee => 'Network fee';

  @override
  String get feeFast => 'Fast';

  @override
  String get feeEconomy => 'Economy';

  @override
  String feeFastHint(String amount, String ticker) {
    return 'Pays $amount $ticker to skip the transaction proof of work. Sends in seconds.';
  }

  @override
  String get feeEconomyHint =>
      'Pays the network minimum, but your browser must compute the transaction proof of work — this can take several minutes.';

  @override
  String get paymentIdWithIntegrated =>
      'This address already contains a payment ID — leave the payment ID field empty.';

  @override
  String get sweepConfirmBody =>
      'This sends your entire balance, minus fees, to:';

  @override
  String tooManyAttempts(int seconds) {
    return 'Too many attempts — try again in ${seconds}s';
  }

  @override
  String get sectionSecurity => 'Security';

  @override
  String get autoLock => 'Auto-lock';

  @override
  String get autoLockSubtitle => 'Lock the wallet after a period of inactivity';

  @override
  String get autoLockNever => 'Never';

  @override
  String autoLockMinutes(int minutes) {
    return '$minutes min';
  }

  @override
  String get confirmPassword => 'Confirm password';

  @override
  String get confirmPasswordField => 'Confirm new password';

  @override
  String get newPassword => 'New password';

  @override
  String get changePassword => 'Change Password';

  @override
  String get changePasswordSubtitle =>
      'Re-encrypt this wallet with a new password';

  @override
  String get changePasswordPurpose => 'Enter your current wallet password.';

  @override
  String get passwordChanged => 'Password changed';

  @override
  String get passwordsDoNotMatch => 'Passwords do not match';

  @override
  String passwordTooShort(int length) {
    return 'Use a password of at least $length characters';
  }

  @override
  String get showSeedAndKeys => 'Seed Phrase & Private Keys';

  @override
  String get showSeedAndKeysSubtitle =>
      'Reveal your recovery seed and keys (asks for your password)';

  @override
  String get revealSecretsPurpose =>
      'Your seed phrase and private keys will be shown on screen.';

  @override
  String get secretsWarning =>
      'Anyone with these can spend your funds. Make sure nobody can see your screen, and never share them — no support channel will ever ask for them.';

  @override
  String get tapToReveal => 'Tap to reveal';

  @override
  String get viewOnlyNoSeed =>
      'This is a view-only wallet — it has no seed phrase or spend key.';

  @override
  String get verifySeedTitle => 'Confirm your backup';

  @override
  String get verifySeedSubtitle =>
      'Type the requested words from the phrase you just wrote down.';

  @override
  String wordNumber(int number) {
    return 'Word #$number';
  }

  @override
  String get askDifferentWords => 'Ask me different words';

  @override
  String get downloadWalletFile => 'Download Wallet File';

  @override
  String get downloadWalletFileSubtitle =>
      'Save an encrypted backup of this wallet to your device';

  @override
  String get walletFileDownloaded =>
      'Encrypted wallet file downloaded — keep it somewhere safe.';

  @override
  String get exportJsonWarning =>
      'This file is NOT encrypted. It contains your private keys in plain text — anyone who opens it can spend your funds.\n\nOnly save it somewhere you control, and delete it when you are done.';

  @override
  String autosaveFailed(String error) {
    return 'Could not save the wallet to browser storage: $error';
  }

  @override
  String get walletDataDeleted => 'Wallet data deleted';

  @override
  String deleteFailed(String error) {
    return 'Delete failed: $error';
  }

  @override
  String get notificationsBlocked =>
      'Your browser is blocking notifications for this site. Enable them in site settings.';

  @override
  String get walletNameRequired => 'Enter a name for this wallet';

  @override
  String get daemonHostRequired => 'Enter a daemon host';

  @override
  String get invalidPort => 'Port must be between 1 and 65535';

  @override
  String get invalidSpendKey =>
      'Private spend key must be 64 hexadecimal characters';

  @override
  String get invalidViewKey =>
      'Private view key must be 64 hexadecimal characters';

  @override
  String seedWordCount(int expected, int actual) {
    return 'A seed phrase has $expected words — you entered $actual';
  }
}
