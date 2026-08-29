// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Vietnamese (`vi`).
class SVi extends S {
  SVi([String locale = 'vi']) : super(locale);

  @override
  String get appTitle => 'PLUTON v2';

  @override
  String get tabOverview => 'Tổng quan';

  @override
  String get tabReceive => 'Nhận';

  @override
  String get tabTransfer => 'Chuyển';

  @override
  String get tabHistory => 'Lịch sử';

  @override
  String get tabAddressBook => 'Danh bạ';

  @override
  String get tabSettings => 'Cài đặt';

  @override
  String get tabAbout => 'Giới thiệu';

  @override
  String get lockWallet => 'Khóa ví';

  @override
  String get send => 'Gửi';

  @override
  String get receive => 'Nhận';

  @override
  String get transfer => 'Chuyển';

  @override
  String get available => 'Khả dụng';

  @override
  String get locked => 'Đã khóa';

  @override
  String get total => 'Tổng';

  @override
  String get availableBalance => 'Số dư khả dụng';

  @override
  String lockedUnconfirmed(String amount, String ticker) {
    return 'Đã khóa (chưa xác nhận): $amount $ticker';
  }

  @override
  String totalBalance(String amount, String ticker) {
    return 'Tổng: $amount $ticker';
  }

  @override
  String get balanceIncompleteWhileSyncing =>
      'Số dư có thể chưa đầy đủ trong khi đồng bộ';

  @override
  String errorPrefix(String message) {
    return 'Lỗi: $message';
  }

  @override
  String get network => 'Mạng';

  @override
  String get syncStatus => 'Trạng thái đồng bộ';

  @override
  String get synced => 'Đã đồng bộ';

  @override
  String get syncing => 'Đang đồng bộ…';

  @override
  String get walletBlock => 'Block ví';

  @override
  String get networkBlock => 'Block mạng';

  @override
  String get peers => 'Nút ngang hàng';

  @override
  String get walletType => 'Loại ví';

  @override
  String get viewOnly => 'Chỉ xem';

  @override
  String get full => 'Đầy đủ';

  @override
  String get nodeConnectionIssue => 'Sự cố kết nối nút';

  @override
  String get switchNodeInSettings => 'Chuyển nút trong Cài đặt →';

  @override
  String get recentTransactions => 'Giao dịch gần đây';

  @override
  String get viewAll => 'Xem tất cả →';

  @override
  String get noTransactionsYet => 'Chưa có giao dịch nào';

  @override
  String get received => 'Đã nhận';

  @override
  String get sent => 'Đã gửi';

  @override
  String syncingProgress(String pct, int wallet, int network) {
    return 'Đang đồng bộ $pct% (block $wallet / $network)';
  }

  @override
  String get shareAddressSubtitle => 'Chia sẻ địa chỉ của bạn để nhận WRKZ';

  @override
  String get yourAddress => 'Địa chỉ của bạn';

  @override
  String get generateIntegratedAddress => 'Tạo địa chỉ tích hợp';

  @override
  String get integratedAddressDescription =>
      'Kết hợp địa chỉ của bạn với mã thanh toán. Nhấn nút ngẫu nhiên để tạo mã mới, hoặc nhập mã riêng bên dưới.';

  @override
  String get randomShort16 => 'Ngẫu nhiên ngắn (16)';

  @override
  String get randomLong64 => 'Ngẫu nhiên dài (64)';

  @override
  String get customPaymentIdLabel =>
      'Mã thanh toán tùy chỉnh (16 hoặc 64 ký tự hex)';

  @override
  String get generate => 'Tạo';

  @override
  String get integratedAddress => 'Địa chỉ tích hợp';

  @override
  String get paymentIdShort => 'Ngắn (16)';

  @override
  String get paymentIdLong => 'Dài (64)';

  @override
  String paymentIdLabel(String label) {
    return 'Mã thanh toán · $label';
  }

  @override
  String get enterPaymentIdError => 'Nhập mã thanh toán (16 hoặc 64 ký tự hex)';

  @override
  String get paymentIdInvalidError =>
      'Mã thanh toán phải có 16 hoặc 64 ký tự hex';

  @override
  String get copyAddress => 'Sao chép địa chỉ';

  @override
  String get copyPaymentId => 'Sao chép mã thanh toán';

  @override
  String get copy => 'Sao chép';

  @override
  String get copied => 'Đã sao chép!';

  @override
  String get sendWrkzToAny => 'Gửi WRKZ đến bất kỳ địa chỉ nào';

  @override
  String get sweepAllDescription =>
      'Gửi toàn bộ số dư đến một địa chỉ (gộp UTXO)';

  @override
  String get sweepAll => 'Quét tất cả';

  @override
  String get sweepWarning =>
      'Quét sẽ gộp tất cả UTXO thành một đầu ra. Sử dụng khi giao dịch thất bại do quá nhiều đầu vào.';

  @override
  String sweepAvailableBalance(String amount, String ticker) {
    return 'Khả dụng: $amount $ticker (toàn bộ số dư sẽ được gửi trừ phí)';
  }

  @override
  String get destinationAddress => 'Địa chỉ đích';

  @override
  String get addressBook => 'Danh bạ';

  @override
  String get sweepAllFunds => 'Quét toàn bộ số dư';

  @override
  String get recipientAddress => 'Địa chỉ người nhận';

  @override
  String get amount => 'Số lượng';

  @override
  String get paymentIdOptional => 'Mã thanh toán (tùy chọn)';

  @override
  String get hexCharacters => '16 hoặc 64 ký tự hex';

  @override
  String get reviewTransaction => 'Xem lại giao dịch';

  @override
  String get reviewAndConfirm => 'Xem lại & Xác nhận';

  @override
  String get to => 'Đến';

  @override
  String get fee => 'Phí';

  @override
  String get totalDeducted => 'Tổng trừ';

  @override
  String get paymentId => 'Mã thanh toán';

  @override
  String get transactionsIrreversible =>
      'Giao dịch không thể đảo ngược. Kiểm tra địa chỉ trước khi xác nhận.';

  @override
  String get back => 'Quay lại';

  @override
  String get confirmAndSend => 'Xác nhận & Gửi';

  @override
  String get transactionSent => 'Đã gửi giao dịch!';

  @override
  String get transactionBroadcast =>
      'Giao dịch của bạn đã được phát đến mạng lưới.';

  @override
  String get transactionHash => 'Mã giao dịch';

  @override
  String get sendAnother => 'Gửi giao dịch khác';

  @override
  String get enterDestinationAddress => 'Nhập địa chỉ đích';

  @override
  String get enterValidAmount => 'Nhập số lượng hợp lệ';

  @override
  String computingPow(int seconds) {
    return 'Đang tính PoW... ${seconds}s';
  }

  @override
  String get stepFillDetails => 'Điền thông tin';

  @override
  String get stepReview => 'Xem lại';

  @override
  String get stepDone => 'Hoàn tất';

  @override
  String get sweepFailed => 'Quét thất bại';

  @override
  String get addressBookTitle => 'Danh bạ';

  @override
  String get transactionHistory => 'Lịch sử giao dịch';

  @override
  String get searchByHash => 'Tìm theo mã, địa chỉ hoặc mã thanh toán…';

  @override
  String get all => 'Tất cả';

  @override
  String get filterReceived => 'Đã nhận';

  @override
  String get filterSent => 'Đã gửi';

  @override
  String get refresh => 'Làm mới';

  @override
  String get noTransactionsFound => 'Không tìm thấy giao dịch';

  @override
  String get confirmed => 'Đã xác nhận';

  @override
  String get pending => 'Đang chờ';

  @override
  String get hash => 'Mã';

  @override
  String get address => 'Địa chỉ';

  @override
  String get block => 'Block';

  @override
  String showingRange(int start, int end, int total) {
    return 'Hiển thị $start–$end / $total';
  }

  @override
  String get previous => 'Trước';

  @override
  String get next => 'Tiếp';

  @override
  String get walletLocked => 'Ví đã khóa';

  @override
  String get enterPasswordToContinue => 'Nhập mật khẩu ví để tiếp tục';

  @override
  String get password => 'Mật khẩu';

  @override
  String get incorrectPassword => 'Sai mật khẩu';

  @override
  String get unlock => 'Mở khóa';

  @override
  String get closeWalletInstead => 'Đóng ví thay thế';

  @override
  String get closeWallet => 'Đóng ví';

  @override
  String get closeWalletDescription =>
      'Thao tác này sẽ lưu và đóng ví.\n\nBạn sẽ được đưa về màn hình đăng nhập.';

  @override
  String get cancel => 'Hủy';

  @override
  String get welcomeToPluton => 'Chào mừng đến PLUTON v2';

  @override
  String get selectOptionToStart => 'Chọn một tùy chọn để bắt đầu';

  @override
  String get createNewWallet => 'Tạo ví mới';

  @override
  String get openExistingWallet => 'Mở ví hiện có';

  @override
  String get importFromSeed => 'Nhập từ cụm từ hạt giống';

  @override
  String get importFromKeys => 'Nhập từ khóa riêng';

  @override
  String get openWallet => 'Mở ví';

  @override
  String get importFromSeedTitle => 'Nhập từ hạt giống';

  @override
  String get importFromKeysTitle => 'Nhập từ khóa';

  @override
  String get saveWalletTo => 'Lưu ví vào';

  @override
  String get walletFile => 'Tệp ví';

  @override
  String get walletPassword => 'Mật khẩu ví';

  @override
  String get mnemonicSeedPhrase => 'Cụm từ hạt giống ghi nhớ';

  @override
  String get scanFromHeight => 'Quét từ block (0 = quét toàn bộ)';

  @override
  String get daemonHost => 'Máy chủ daemon';

  @override
  String get port => 'Cổng';

  @override
  String get continueButton => 'Tiếp tục';

  @override
  String get browse => 'Duyệt';

  @override
  String get backupWarning =>
      'Sao lưu ví trước khi tiếp tục.\nCác khóa này không thể khôi phục nếu bị mất.';

  @override
  String get yourWalletAddress => 'Địa chỉ ví của bạn';

  @override
  String get seedPhrase25Words => 'Cụm từ hạt giống (25 từ)';

  @override
  String get privateViewKey => 'Khóa xem riêng';

  @override
  String get privateSpendKey => 'Khóa chi tiêu riêng';

  @override
  String get seedBackupConfirm =>
      'Tôi đã ghi lại cụm từ hạt giống và khóa riêng ở nơi an toàn.';

  @override
  String get backedUpContinue => 'Tôi đã sao lưu ví — Tiếp tục';

  @override
  String get settings => 'Cài đặt';

  @override
  String get sectionDaemonNode => 'Nút Daemon';

  @override
  String get nodeDescription =>
      'Kết nối đến nút daemon cục bộ hoặc từ xa. Thay đổi có hiệu lực ngay lập tức.';

  @override
  String get hostIpAddress => 'Máy chủ / Địa chỉ IP';

  @override
  String get ssl => 'SSL';

  @override
  String get apply => 'Áp dụng';

  @override
  String get nodeUpdatedSuccess => 'Đã cập nhật nút thành công';

  @override
  String get nodeUnreachable =>
      'Không thể kết nối đến nút hiện tại. Nhập địa chỉ nút mới bên dưới và nhấn Áp dụng.';

  @override
  String get sectionWallet => 'Ví';

  @override
  String get saveWallet => 'Lưu ví';

  @override
  String get saveWalletSubtitle => 'Ghi trạng thái hiện tại ra đĩa';

  @override
  String get walletSaved => 'Đã lưu ví';

  @override
  String get exportToJson => 'Xuất ra JSON';

  @override
  String get exportToJsonSubtitle => 'Lưu dữ liệu ví dưới dạng tệp JSON';

  @override
  String get exportJsonTitle => 'Xuất JSON ví';

  @override
  String exportedTo(String path) {
    return 'Đã xuất ra $path';
  }

  @override
  String exportFailed(String error) {
    return 'Xuất thất bại: $error';
  }

  @override
  String get resetScanHeight => 'Đặt lại chiều cao quét';

  @override
  String get resetScanHeightSubtitle =>
      'Quét lại blockchain từ một chiều cao cụ thể';

  @override
  String get resetScanHeightDescription =>
      'Nhập chiều cao block để quét lại. Dùng 0 để quét lại toàn bộ.';

  @override
  String get scanHeight => 'Chiều cao quét';

  @override
  String get reset => 'Đặt lại';

  @override
  String get autosave => 'Tự động lưu';

  @override
  String get autosaveSubtitle => 'Lưu ví ra đĩa sau khi đồng bộ và mỗi 5 phút';

  @override
  String get scanCoinbaseTx => 'Quét giao dịch Coinbase';

  @override
  String get scanCoinbaseSubtitle =>
      'Bao gồm phần thưởng đào khi đồng bộ (mặc định tắt)';

  @override
  String get sectionAppearance => 'Giao diện';

  @override
  String get theme => 'Chủ đề';

  @override
  String get themeSubtitle => 'Chọn bảng màu ứng dụng';

  @override
  String get themeSystem => 'Hệ thống';

  @override
  String get themeLight => 'Sáng';

  @override
  String get themeDark => 'Tối';

  @override
  String get sectionNotifications => 'Thông báo';

  @override
  String get incomingTxAlerts => 'Cảnh báo giao dịch đến';

  @override
  String get incomingTxAlertsSubtitle =>
      'Hiển thị thông báo trên màn hình khi nhận được WRKZ';

  @override
  String get sectionDebugLogs => 'Gỡ lỗi & Nhật ký';

  @override
  String get logLevel => 'Mức nhật ký';

  @override
  String get logLevelSubtitle => 'Điều chỉnh mức chi tiết của thư viện ví';

  @override
  String get viewLogs => 'Xem nhật ký';

  @override
  String get viewLogsSubtitle => 'Đầu ra nhật ký thư viện ví trực tiếp';

  @override
  String get walletLogs => 'Nhật ký ví';

  @override
  String logEntries(int count) {
    return '$count mục';
  }

  @override
  String get autoScroll => 'Tự động cuộn';

  @override
  String get copyAll => 'Sao chép tất cả';

  @override
  String get clear => 'Xóa';

  @override
  String get close => 'Đóng';

  @override
  String get noLogsYet =>
      'Chưa có nhật ký. Đặt mức nhật ký cao hơn Tắt để xem đầu ra.';

  @override
  String get logsCopied => 'Đã sao chép nhật ký vào bộ nhớ tạm';

  @override
  String get sectionDangerZone => 'Khu vực nguy hiểm';

  @override
  String get deleteWalletData => 'Xóa dữ liệu ví';

  @override
  String get deleteWalletDataSubtitle => 'Xóa vĩnh viễn tệp ví khỏi đĩa';

  @override
  String get deleteWalletWarning =>
      'Thao tác này sẽ xóa vĩnh viễn tệp ví khỏi đĩa.\n\nHãy chắc chắn bạn đã sao lưu cụm từ hạt giống và khóa riêng trước khi tiếp tục. Hành động này không thể hoàn tác.';

  @override
  String get iUnderstandContinue => 'Tôi hiểu, tiếp tục';

  @override
  String get finalConfirmation => 'Xác nhận lần cuối';

  @override
  String get typeDeleteToConfirm => 'Nhập DELETE để xác nhận:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get deletePermanently => 'Xóa vĩnh viễn';

  @override
  String get aboutTitle => 'Giới thiệu';

  @override
  String versionInfo(String version) {
    return 'Phiên bản $version — Ví web WRKZ';
  }

  @override
  String get aboutDescription =>
      'PLUTON v2 là ví web chính thức cho WrkzCoin (WRKZ), một loại tiền mã hóa nhanh và nhẹ dựa trên CryptoNote.\n\nĐược xây dựng bằng Flutter, vận hành bởi wallet-api.';

  @override
  String get github => 'GitHub';

  @override
  String get githubSubtitle => 'Xem mã nguồn và các bản phát hành';

  @override
  String get discord => 'Discord';

  @override
  String get discordSubtitle => 'Tham gia cộng đồng';

  @override
  String get twitterX => 'Twitter / X';

  @override
  String get twitterXSubtitle => 'Theo dõi @wrkzcoin';

  @override
  String get website => 'Trang web';

  @override
  String get websiteSubtitle => 'wrkz.work';

  @override
  String get license => 'Giấy phép';

  @override
  String get licenseText =>
      'Phát hành theo Giấy phép MIT.\nSử dụng theo rủi ro của bạn. Luôn sao lưu cụm từ hạt giống.';

  @override
  String get addButton => 'Thêm';

  @override
  String get noSavedAddresses => 'Chưa có địa chỉ đã lưu';

  @override
  String get tapAddToSave => 'Nhấn Thêm để lưu địa chỉ thường dùng.';

  @override
  String get addAddress => 'Thêm địa chỉ';

  @override
  String get nameLabel => 'Tên / nhãn';

  @override
  String get addressLabel => 'Địa chỉ';

  @override
  String get noteOptional => 'Ghi chú (tùy chọn)';

  @override
  String get nameAndAddressRequired => 'Tên và địa chỉ là bắt buộc';

  @override
  String get invalidWrkzAddress =>
      'Địa chỉ WRKZ không hợp lệ. Phải có 98 (tiêu chuẩn), 120 (tích hợp ngắn), hoặc 186 (tích hợp dài) ký tự bắt đầu bằng \"Wrkz\".';

  @override
  String get save => 'Lưu';

  @override
  String get editEntry => 'Chỉnh sửa mục';

  @override
  String get deleteEntry => 'Xóa mục';

  @override
  String removeFromAddressBook(String name) {
    return 'Xóa \"$name\" khỏi danh bạ của bạn?';
  }

  @override
  String get delete => 'Xóa';

  @override
  String get edit => 'Sửa';

  @override
  String get wrkzReceived => 'Đã nhận WRKZ';

  @override
  String youReceivedAmount(String amount) {
    return 'Bạn đã nhận $amount';
  }

  @override
  String get show => 'Hiện';

  @override
  String get exit => 'Thoát';

  @override
  String get plutonWallet => 'Ví PLUTON';

  @override
  String get language => 'Ngôn ngữ';

  @override
  String get selectLanguage => 'Chọn ngôn ngữ';

  @override
  String get chooseLanguage => 'Chọn ngôn ngữ ưa thích của bạn';

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
