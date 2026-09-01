// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Vietnamese (`vi`).
class SVi extends S {
  SVi([String locale = 'vi']) : super(locale);

  @override
  String get appTitle => 'PLUTON Mobile';

  @override
  String get tabOverview => 'Tổng quan';

  @override
  String get tabReceive => 'Nhận';

  @override
  String get tabSend => 'Gửi';

  @override
  String get tabHistory => 'Lịch sử';

  @override
  String get tabSettings => 'Cài đặt';

  @override
  String get send => 'Gửi';

  @override
  String get receive => 'Nhận';

  @override
  String get available => 'Khả dụng';

  @override
  String get locked => 'Đã khóa';

  @override
  String get total => 'Tổng cộng';

  @override
  String lockedAmount(String amount) {
    return 'Đã khóa: $amount';
  }

  @override
  String totalAmount(String amount) {
    return 'Tổng cộng: $amount';
  }

  @override
  String get recentTransactions => 'Giao dịch gần đây';

  @override
  String get viewAll => 'Xem tất cả';

  @override
  String get noTransactionsYet => 'Chưa có giao dịch nào';

  @override
  String get noMatchingTransactions => 'Không tìm thấy giao dịch phù hợp';

  @override
  String get pending => 'Đang chờ...';

  @override
  String get justNow => 'Vừa xong';

  @override
  String minutesAgo(int count) {
    return '$count phút trước';
  }

  @override
  String hoursAgo(int count) {
    return '$count giờ trước';
  }

  @override
  String daysAgo(int count) {
    return '$count ngày trước';
  }

  @override
  String get received => 'Đã nhận';

  @override
  String get sent => 'Đã gửi';

  @override
  String get networkStatus => 'Trạng thái mạng';

  @override
  String get node => 'Nút mạng';

  @override
  String get status => 'Trạng thái';

  @override
  String get connected => 'Đã kết nối';

  @override
  String get disconnected => 'Mất kết nối';

  @override
  String get walletHeight => 'Chiều cao ví';

  @override
  String get networkHeight => 'Chiều cao mạng';

  @override
  String get peers => 'Đồng nghiệp';

  @override
  String get type => 'Loại';

  @override
  String get viewOnly => 'Chỉ xem';

  @override
  String get couldNotFetchStatus =>
      'Không thể lấy trạng thái. Kiểm tra nút mạng trong Cài đặt.';

  @override
  String errorPrefix(String message) {
    return 'Lỗi: $message';
  }

  @override
  String get seedBackupWarning =>
      'Hãy sao lưu cụm từ hạt giống trong Cài đặt để bảo vệ tài sản của bạn.';

  @override
  String get noConnectionToDaemon => 'Không có kết nối tới daemon';

  @override
  String syncingPercent(String percent) {
    return 'Đang đồng bộ $percent%';
  }

  @override
  String get yourAddress => 'Địa chỉ của bạn';

  @override
  String get errorLoadingAddress => 'Lỗi khi tải địa chỉ';

  @override
  String get integratedAddress => 'Địa chỉ tích hợp';

  @override
  String get embedPaymentId => 'Nhúng mã thanh toán vào địa chỉ của bạn';

  @override
  String get randomShort => 'Ngẫu nhiên ngắn (16)';

  @override
  String get randomLong => 'Ngẫu nhiên dài (64)';

  @override
  String get enterCustomPaymentId =>
      'Hoặc nhập mã thanh toán tùy chỉnh (16 hoặc 64 ký tự hex)';

  @override
  String get enterPaymentId => 'Nhập mã thanh toán';

  @override
  String get paymentIdInvalid => 'Mã thanh toán phải có 16 hoặc 64 ký tự hex';

  @override
  String get shortPid => 'PID ngắn';

  @override
  String get longPid => 'PID dài';

  @override
  String get share => 'Chia sẻ';

  @override
  String get copy => 'Sao chép';

  @override
  String get sweepAllFunds => 'Quét toàn bộ số dư';

  @override
  String get normalSend => 'Gửi thông thường';

  @override
  String get sweep => 'Quét';

  @override
  String get recipientAddress => 'Địa chỉ người nhận';

  @override
  String get scanQr => 'Quét QR';

  @override
  String get amount => 'Số lượng';

  @override
  String availableBalance(String amount) {
    return 'Khả dụng: $amount';
  }

  @override
  String sweepInfo(String amount) {
    return 'Quét sẽ gộp tất cả UTXO và gửi toàn bộ số dư đã mở khóa ($amount) trừ phí.';
  }

  @override
  String get paymentIdOptional => 'Mã thanh toán (tùy chọn)';

  @override
  String get hexCharacters => '16 hoặc 64 ký tự hex';

  @override
  String get mustBeHex => 'Phải có 16 hoặc 64 ký tự hex';

  @override
  String get recipientRequired => 'Địa chỉ người nhận là bắt buộc';

  @override
  String get invalidAddress => 'Địa chỉ WRKZ không hợp lệ';

  @override
  String get enterValidAmount => 'Nhập số lượng hợp lệ';

  @override
  String get reviewTransaction => 'Xem lại giao dịch';

  @override
  String get to => 'Đến';

  @override
  String get fee => 'Phí';

  @override
  String get totalDeducted => 'Tổng khấu trừ';

  @override
  String get paymentId => 'Mã thanh toán';

  @override
  String get transactionsIrreversible =>
      'Giao dịch không thể hoàn tác. Vui lòng kiểm tra lại thông tin.';

  @override
  String get back => 'Quay lại';

  @override
  String get confirmAndSend => 'Xác nhận & Gửi';

  @override
  String get transactionSent => 'Giao dịch đã gửi!';

  @override
  String get transactionHash => 'Mã băm giao dịch';

  @override
  String get sendAnother => 'Gửi thêm';

  @override
  String get scanQrCode => 'Quét mã QR';

  @override
  String get scannedAddress => 'Địa chỉ đã quét';

  @override
  String get cancel => 'Hủy';

  @override
  String get useThisAddress => 'Dùng địa chỉ này';

  @override
  String get sweepFailed => 'Quét thất bại';

  @override
  String get searchPlaceholder => 'Tìm theo mã băm, địa chỉ, mã thanh toán...';

  @override
  String get all => 'Tất cả';

  @override
  String get filterReceived => 'Đã nhận';

  @override
  String get filterSent => 'Đã gửi';

  @override
  String get hash => 'Mã băm';

  @override
  String get address => 'Địa chỉ';

  @override
  String get block => 'Khối';

  @override
  String get confirmed => 'Đã xác nhận';

  @override
  String get password => 'Mật khẩu';

  @override
  String get unlock => 'Mở khóa';

  @override
  String get switchWallet => 'Đổi ví';

  @override
  String get enterPasswordToUnlock => 'Nhập mật khẩu để mở khóa';

  @override
  String get incorrectPassword => 'Mật khẩu không đúng';

  @override
  String get enterYourPassword => 'Nhập mật khẩu của bạn';

  @override
  String get plutonMobile => 'PLUTON Mobile';

  @override
  String get createFirstWalletSubtitle => 'Tạo ví đầu tiên của bạn để bắt đầu';

  @override
  String get selectWalletSubtitle => 'Chọn một ví để mở';

  @override
  String get yourWallets => 'Ví của bạn';

  @override
  String get noWalletsYet => 'Chưa có ví nào';

  @override
  String get lastOpened => 'Lần mở gần nhất';

  @override
  String createdDate(String date) {
    return 'Đã tạo $date';
  }

  @override
  String get createFirstWallet => 'Tạo ví đầu tiên';

  @override
  String get addWallet => 'Thêm ví';

  @override
  String get deleteWallet => 'Xóa ví';

  @override
  String deleteWalletConfirm(String name) {
    return 'Xóa \"$name\"?\n\nĐiều này sẽ xóa vĩnh viễn tệp ví và các khóa. Hãy chắc chắn bạn đã sao lưu cụm từ hạt giống.';
  }

  @override
  String get delete => 'Xóa';

  @override
  String get createNewWallet => 'Tạo ví mới';

  @override
  String get createNewWalletSubtitle => 'Tạo ví mới với cụm từ hạt giống mới';

  @override
  String get importFromSeed => 'Nhập từ cụm từ hạt giống';

  @override
  String get importFromSeedSubtitle => 'Khôi phục ví bằng cụm từ gợi nhớ 25 từ';

  @override
  String get importFromKeys => 'Nhập từ khóa riêng tư';

  @override
  String get importFromKeysSubtitle =>
      'Khôi phục bằng khóa chi tiêu và khóa xem';

  @override
  String get viewOnlyWallet => 'Ví chỉ xem';

  @override
  String get viewOnlyWalletSubtitle => 'Ví theo dõi dùng khóa xem và địa chỉ';

  @override
  String get createWallet => 'Tạo ví';

  @override
  String get importWallet => 'Nhập ví';

  @override
  String get walletName => 'Tên ví';

  @override
  String get walletNameHint => 'VD: Ví chính';

  @override
  String get passwordLabel => 'Mật khẩu';

  @override
  String get enterPassword => 'Nhập mật khẩu';

  @override
  String get confirmPassword => 'Xác nhận mật khẩu';

  @override
  String get seedPhrase => 'Cụm từ hạt giống (25 từ)';

  @override
  String get enterSeedPhrase => 'Nhập cụm từ hạt giống của bạn...';

  @override
  String get scanHeight => 'Chiều cao quét (tùy chọn)';

  @override
  String get scanHeightHint => '0 = quét từ đầu';

  @override
  String get privateSpendKey => 'Khóa chi tiêu riêng tư';

  @override
  String get privateViewKey => 'Khóa xem riêng tư';

  @override
  String get walletAddress => 'Địa chỉ ví';

  @override
  String get walletAddressHint => 'Địa chỉ Wrkz...';

  @override
  String get hexKey => '64 ký tự hex';

  @override
  String get daemonNode => 'Nút daemon';

  @override
  String get custom => 'Tùy chỉnh';

  @override
  String get host => 'Máy chủ';

  @override
  String get hostHint => 'Máy chủ / IP';

  @override
  String get port => 'Cổng';

  @override
  String get ssl => 'SSL';

  @override
  String get walletNameRequired => 'Tên ví là bắt buộc';

  @override
  String get passwordRequired => 'Mật khẩu là bắt buộc';

  @override
  String passwordTooShort(int count) {
    return 'Mật khẩu phải có ít nhất $count ký tự';
  }

  @override
  String get passwordsDoNotMatch => 'Mật khẩu không khớp';

  @override
  String get seedRequired => 'Cụm từ hạt giống là bắt buộc';

  @override
  String get spendKeyRequired => 'Khóa chi tiêu là bắt buộc';

  @override
  String get viewKeyRequired => 'Khóa xem là bắt buộc';

  @override
  String get addressRequired => 'Địa chỉ là bắt buộc';

  @override
  String get daemonHostRequired => 'Máy chủ daemon là bắt buộc';

  @override
  String get backupSeedTitle => 'Sao lưu hạt giống';

  @override
  String get backupWarning =>
      'Ghi lại cụm từ hạt giống và lưu trữ an toàn. Nếu bạn mất nó, tài sản của bạn sẽ mất vĩnh viễn.';

  @override
  String get seedPhraseLabel => 'Cụm từ hạt giống';

  @override
  String get privateViewKeyLabel => 'Khóa xem riêng tư';

  @override
  String get privateSpendKeyLabel => 'Khóa chi tiêu riêng tư';

  @override
  String get backupConfirmCheck => 'Tôi đã sao lưu cụm từ hạt giống an toàn';

  @override
  String get continueToWallet => 'Tiếp tục vào ví';

  @override
  String get sectionDaemonNode => 'Nút daemon';

  @override
  String get apply => 'Áp dụng';

  @override
  String nodeUpdated(String host, int port) {
    return 'Nút đã cập nhật thành $host:$port';
  }

  @override
  String get hostRequired => 'Máy chủ là bắt buộc';

  @override
  String currentWallet(String name) {
    return 'Ví hiện tại — $name';
  }

  @override
  String get saveWallet => 'Lưu ví';

  @override
  String get walletSaved => 'Đã lưu ví';

  @override
  String saveFailed(String error) {
    return 'Lưu thất bại: $error';
  }

  @override
  String get backupSeed => 'Sao lưu hạt giống';

  @override
  String get changePassword => 'Đổi mật khẩu';

  @override
  String get resetScanHeight => 'Đặt lại chiều cao quét';

  @override
  String get reset => 'Đặt lại';

  @override
  String resetScanConfirm(int height) {
    return 'Thao tác này sẽ quét lại blockchain từ khối $height. Có thể mất một lúc. Tiếp tục?';
  }

  @override
  String scanResetTo(int height) {
    return 'Đặt lại quét về khối $height';
  }

  @override
  String resetFailed(String error) {
    return 'Đặt lại thất bại: $error';
  }

  @override
  String get enterPasswordTitle => 'Nhập mật khẩu';

  @override
  String get confirm => 'Xác nhận';

  @override
  String get seedBackup => 'Sao lưu hạt giống';

  @override
  String get seedPhraseColon => 'Cụm từ hạt giống:';

  @override
  String get privateViewKeyColon => 'Khóa xem riêng tư:';

  @override
  String get iveBackedUp => 'Tôi đã sao lưu';

  @override
  String get currentPasswordLabel => 'Mật khẩu hiện tại';

  @override
  String get newPasswordLabel => 'Mật khẩu mới';

  @override
  String get confirmNewPasswordLabel => 'Xác nhận mật khẩu mới';

  @override
  String get change => 'Thay đổi';

  @override
  String get currentPasswordIncorrect => 'Mật khẩu hiện tại không đúng';

  @override
  String get newPasswordsDoNotMatch => 'Mật khẩu mới không khớp';

  @override
  String get passwordChanged => 'Đã đổi mật khẩu';

  @override
  String get walletManagement => 'Quản lý ví';

  @override
  String get switchWalletSubtitle => 'Lưu & đóng, chọn ví khác';

  @override
  String get manageWallets => 'Quản lý ví';

  @override
  String get manageWalletsSubtitle => 'Đổi tên hoặc xóa ví';

  @override
  String get currentlyOpen => '(đang mở)';

  @override
  String get close => 'Đóng';

  @override
  String get renameWallet => 'Đổi tên ví';

  @override
  String get newName => 'Tên mới';

  @override
  String get rename => 'Đổi tên';

  @override
  String deleteWalletConfirmShort(String name) {
    return 'Xóa \"$name\"? Không thể hoàn tác.';
  }

  @override
  String get security => 'Bảo mật';

  @override
  String get biometricUnlock => 'Mở khóa sinh trắc học';

  @override
  String get biometricSubtitle => 'Vân tay / Nhận diện khuôn mặt';

  @override
  String get biometricNotAvailable => 'Sinh trắc học không khả dụng';

  @override
  String get autoLock => 'Tự động khóa';

  @override
  String get appearance => 'Giao diện';

  @override
  String get theme => 'Chủ đề';

  @override
  String get themeAuto => 'Tự động';

  @override
  String get themeLight => 'Sáng';

  @override
  String get themeDark => 'Tối';

  @override
  String get preferences => 'Tùy chọn';

  @override
  String get transactionNotifications => 'Thông báo giao dịch';

  @override
  String get notificationsSubtitle => 'Cảnh báo khi có giao dịch đến';

  @override
  String get autosave => 'Tự động lưu';

  @override
  String get autosaveSubtitle => 'Lưu sau khi đồng bộ, sau đó mỗi 5 phút';

  @override
  String get scanCoinbaseTx => 'Quét giao dịch Coinbase';

  @override
  String get scanCoinbaseSubtitle =>
      'Bao gồm phần thưởng đào (tắt theo mặc định)';

  @override
  String get dangerZone => 'Vùng nguy hiểm';

  @override
  String get deleteCurrentWallet => 'Xóa ví hiện tại';

  @override
  String get deleteCurrentWalletSubtitle => 'Xóa vĩnh viễn dữ liệu ví';

  @override
  String get deleteWalletTypeCaps =>
      'Thao tác này sẽ xóa vĩnh viễn tệp ví và các khóa. Hãy chắc chắn bạn đã sao lưu cụm từ hạt giống.\n\nGõ DELETE để xác nhận:';

  @override
  String get deleteHint => 'DELETE';

  @override
  String get language => 'Ngôn ngữ';

  @override
  String get selectLanguage => 'Chọn ngôn ngữ';

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
  String get autoLockImmediately => 'Ngay lập tức';

  @override
  String get autoLock1Min => '1 phút';

  @override
  String get autoLock5Min => '5 phút';

  @override
  String get autoLockNever => 'Không bao giờ';

  @override
  String get preparedTransactionExpired =>
      'Giao dịch này không còn hợp lệ. Hãy quay lại và tạo lại.';

  @override
  String get deleteConfirmMismatch => 'Hãy gõ chính xác DELETE để xác nhận.';

  @override
  String get seedNotBackedUpWarning =>
      'Bạn chưa xác nhận sao lưu cụm từ hạt giống của ví này. Xóa ngay bây giờ đồng nghĩa với việc không thể khôi phục tiền.';

  @override
  String get wrkzReceived => 'Đã nhận WRKZ';

  @override
  String get retry => 'Thử lại';

  @override
  String youReceivedAmount(String amount) {
    return 'Bạn đã nhận $amount';
  }

  @override
  String get ringSize => 'Kích thước vòng';

  @override
  String ringSizeReduced(int actual, int normal) {
    return 'Kích thước vòng giảm xuống $actual (thông thường là $normal). Các khoản tiền được gửi không có đủ đầu ra trên chuỗi để tạo thành một vòng đầy đủ, vì vậy giao dịch này kém riêng tư hơn bình thường.';
  }

  @override
  String get liteNodeTitle => 'Node rút gọn';

  @override
  String liteNodeServesFrom(int height) {
    return 'Node này chỉ lưu các khối từ $height trở đi. Không thể tìm thấy giao dịch trước khối đó qua node này.';
  }

  @override
  String liteNodeMissesHistory(int nodeHeight, int walletHeight) {
    return 'Node này bắt đầu từ khối $nodeHeight, nhưng ví này bắt đầu từ khối $walletHeight. Mọi khoản nhận được ở khoảng giữa đều không hiển thị ở đây, nên số dư có thể thấp hơn thực tế. Hãy kết nối tới node giữ toàn bộ chuỗi để xem.';
  }

  @override
  String liteNodeSyncStalled(int wallet, int node) {
    return 'Đồng bộ đã dừng ở khối $wallet. Node này không giữ gì dưới khối $node, nên không thể tải các khối ở giữa từ nó. Số dư sẽ chưa đầy đủ cho tới khi bạn kết nối node giữ toàn bộ chuỗi.';
  }

  @override
  String get liteNodeRescanRefusedTitle =>
      'Node này không thể quét lại từ mức đó';

  @override
  String liteNodeRescanRefused(int height) {
    return 'Node đang kết nối là node rút gọn, không giữ dữ liệu khối dưới $height. Quét lại từ mức thấp hơn sẽ làm mất các giao dịch ví đã tìm thấy, và không thể tìm lại qua node này. Chưa có gì bị thay đổi.';
  }

  @override
  String liteNodeRescanFromInstead(int height) {
    return 'Quét lại từ $height';
  }

  @override
  String liteNodeRescanHint(int height) {
    return 'Node đang kết nối chỉ có thể quét lại từ khối $height trở lên.';
  }

  @override
  String get nodeServesFromLabel => 'Cung cấp khối từ';

  @override
  String get nodeFullChain => 'Toàn bộ chuỗi';

  @override
  String get localNodeMobileFuture =>
      'Chạy node ngay trên điện thoại đã có trong kế hoạch nhưng chưa khả dụng — một node cần vài GB dung lượng và nhiều giờ đồng bộ. Trong lúc đó, hãy trỏ ví này tới node do bạn tự chạy.';
}
