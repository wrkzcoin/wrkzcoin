#include "wallet_bridge.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <limits>

namespace
{
    QString status_to_string(const wallet_status_t status)
    {
        const char *msg = wallet_error_code_to_string(status);
        return msg == nullptr ? QString("Unknown error") : QString::fromUtf8(msg);
    }
}

WalletBridge::WalletBridge(QObject *parent): QObject(parent)
{
    m_pollTimer.setInterval(1500);
    connect(&m_pollTimer, &QTimer::timeout, this, &WalletBridge::pollEvents);
    m_pollTimer.start();
}

WalletBridge::~WalletBridge()
{
    closeWallet();
}

bool WalletBridge::loaded() const
{
    return m_loaded;
}

bool WalletBridge::busy() const
{
    return m_busy;
}

bool WalletBridge::daemonOnline() const
{
    return m_daemonOnline;
}

qulonglong WalletBridge::walletHeight() const
{
    return m_walletHeight;
}

qulonglong WalletBridge::localHeight() const
{
    return m_localHeight;
}

qulonglong WalletBridge::networkHeight() const
{
    return m_networkHeight;
}

qulonglong WalletBridge::unlockedBalance() const
{
    return m_unlockedBalance;
}

qulonglong WalletBridge::lockedBalance() const
{
    return m_lockedBalance;
}

QString WalletBridge::primaryAddress() const
{
    return m_primaryAddress;
}

QString WalletBridge::transactionsJson() const
{
    return m_transactionsJson;
}

QString WalletBridge::nodeInfoJson() const
{
    return m_nodeInfoJson;
}

QString WalletBridge::lastError() const
{
    return m_lastError;
}

QString WalletBridge::lastMessage() const
{
    return m_lastMessage;
}

void WalletBridge::setBusy(bool value)
{
    if (m_busy == value)
    {
        return;
    }

    m_busy = value;
    emit busyChanged();
}

void WalletBridge::setError(const QString &message)
{
    if (m_lastError != message)
    {
        m_lastError = message;
        emit lastErrorChanged();
    }
}

void WalletBridge::setMessage(const QString &message)
{
    if (m_lastMessage != message)
    {
        m_lastMessage = message;
        emit lastMessageChanged();
    }
}

void WalletBridge::clearError()
{
    if (!m_lastError.isEmpty())
    {
        m_lastError.clear();
        emit lastErrorChanged();
    }
}

QString WalletBridge::extractString(wallet_status_t status, char *ptr, size_t len) const
{
    if (status != 0 || ptr == nullptr)
    {
        return {};
    }

    const QString out = QString::fromUtf8(ptr, static_cast<int>(len));
    wallet_string_free(ptr);
    return out;
}

bool WalletBridge::checkLoaded()
{
    if (m_wallet == nullptr)
    {
        setError("Wallet is not loaded");
        return false;
    }

    return true;
}

void WalletBridge::openWallet(
    const QString &filename,
    const QString &password,
    const QString &daemonHost,
    quint16 daemonPort,
    bool daemonSsl,
    quint32 syncThreads)
{
    setBusy(true);
    clearError();

    closeWallet();

    wallet_handle_t *handle = nullptr;
    const wallet_status_t status = wallet_open(
        filename.toUtf8().constData(),
        password.toUtf8().constData(),
        daemonHost.toUtf8().constData(),
        daemonPort,
        daemonSsl,
        syncThreads,
        &handle);

    if (status != 0 || handle == nullptr)
    {
        setError(QString("Open failed: %1").arg(status_to_string(status)));
        setBusy(false);
        return;
    }

    m_wallet = handle;
    m_loaded = true;
    emit loadedChanged();

    setMessage("Wallet opened");
    refresh();
    setBusy(false);
}

void WalletBridge::createWallet(
    const QString &filename,
    const QString &password,
    const QString &daemonHost,
    quint16 daemonPort,
    bool daemonSsl,
    quint32 syncThreads)
{
    setBusy(true);
    clearError();

    closeWallet();

    wallet_handle_t *handle = nullptr;
    const wallet_status_t status = wallet_create(
        filename.toUtf8().constData(),
        password.toUtf8().constData(),
        daemonHost.toUtf8().constData(),
        daemonPort,
        daemonSsl,
        syncThreads,
        &handle);

    if (status != 0 || handle == nullptr)
    {
        setError(QString("Create failed: %1").arg(status_to_string(status)));
        setBusy(false);
        return;
    }

    m_wallet = handle;
    m_loaded = true;
    emit loadedChanged();

    setMessage("Wallet created");
    refresh();
    setBusy(false);
}

void WalletBridge::restoreFromSeed(
    const QString &seed,
    const QString &filename,
    const QString &password,
    qulonglong scanHeight,
    const QString &daemonHost,
    quint16 daemonPort,
    bool daemonSsl,
    quint32 syncThreads)
{
    setBusy(true);
    clearError();

    closeWallet();

    wallet_handle_t *handle = nullptr;
    const wallet_status_t status = wallet_restore_from_seed(
        seed.toUtf8().constData(),
        filename.toUtf8().constData(),
        password.toUtf8().constData(),
        scanHeight,
        daemonHost.toUtf8().constData(),
        daemonPort,
        daemonSsl,
        syncThreads,
        &handle);

    if (status != 0 || handle == nullptr)
    {
        setError(QString("Restore failed: %1").arg(status_to_string(status)));
        setBusy(false);
        return;
    }

    m_wallet = handle;
    m_loaded = true;
    emit loadedChanged();

    setMessage("Wallet restored from seed");
    refresh();
    setBusy(false);
}

void WalletBridge::closeWallet()
{
    if (m_wallet != nullptr)
    {
        wallet_close(m_wallet);
        m_wallet = nullptr;
    }

    if (m_loaded)
    {
        m_loaded = false;
        emit loadedChanged();
    }
}

void WalletBridge::saveWallet()
{
    if (!checkLoaded())
    {
        return;
    }

    const wallet_status_t status = wallet_save(m_wallet);
    if (status != 0)
    {
        setError(QString("Save failed: %1").arg(status_to_string(status)));
        return;
    }

    setMessage("Wallet saved");
}

void WalletBridge::updateSync()
{
    if (!checkLoaded())
    {
        return;
    }

    uint64_t w = 0;
    uint64_t l = 0;
    uint64_t n = 0;

    const wallet_status_t status = wallet_get_sync_status(m_wallet, &w, &l, &n);
    if (status != 0)
    {
        setError(QString("Sync status failed: %1").arg(status_to_string(status)));
        return;
    }

    m_walletHeight = w;
    m_localHeight = l;
    m_networkHeight = n;
    emit syncChanged();
}

void WalletBridge::updateBalances()
{
    if (!checkLoaded())
    {
        return;
    }

    uint64_t unlocked = 0;
    uint64_t locked = 0;
    const wallet_status_t status = wallet_get_total_balance(m_wallet, &unlocked, &locked);
    if (status != 0)
    {
        setError(QString("Balance failed: %1").arg(status_to_string(status)));
        return;
    }

    m_unlockedBalance = unlocked;
    m_lockedBalance = locked;
    emit balanceChanged();
}

void WalletBridge::updatePrimaryAddress()
{
    if (!checkLoaded())
    {
        return;
    }

    char *out = nullptr;
    size_t outLen = 0;
    const wallet_status_t status = wallet_get_primary_address(m_wallet, &out, &outLen);
    const QString value = extractString(status, out, outLen);
    if (status != 0)
    {
        setError(QString("Primary address failed: %1").arg(status_to_string(status)));
        return;
    }

    m_primaryAddress = value;
    emit primaryAddressChanged();
}

void WalletBridge::updateTransactions()
{
    if (!checkLoaded())
    {
        return;
    }

    char *out = nullptr;
    size_t outLen = 0;
    const wallet_status_t status = wallet_get_transactions_json(
        m_wallet,
        0,
        std::numeric_limits<uint64_t>::max(),
        true,
        &out,
        &outLen);
    const QString value = extractString(status, out, outLen);
    if (status != 0)
    {
        setError(QString("Transactions failed: %1").arg(status_to_string(status)));
        return;
    }

    m_transactionsJson = value;
    emit transactionsChanged();
}

void WalletBridge::updateNodeInfo()
{
    if (!checkLoaded())
    {
        return;
    }

    bool online = false;
    wallet_status_t status = wallet_daemon_online(m_wallet, &online);
    if (status != 0)
    {
        setError(QString("Daemon status failed: %1").arg(status_to_string(status)));
        return;
    }

    if (m_daemonOnline != online)
    {
        m_daemonOnline = online;
        emit daemonOnlineChanged();
    }

    char *out = nullptr;
    size_t outLen = 0;
    status = wallet_get_node_info_json(m_wallet, &out, &outLen);
    const QString value = extractString(status, out, outLen);
    if (status != 0)
    {
        setError(QString("Node info failed: %1").arg(status_to_string(status)));
        return;
    }

    if (m_nodeInfoJson != value)
    {
        m_nodeInfoJson = value;
        emit nodeInfoChanged();
    }
}

void WalletBridge::pollEvents()
{
    if (!m_loaded || m_wallet == nullptr || m_busy)
    {
        return;
    }

    uint32_t eventType = WALLET_EVENT_NONE;
    char *out = nullptr;
    size_t outLen = 0;
    const wallet_status_t status = wallet_poll_event(m_wallet, 0, &eventType, &out, &outLen);
    if (status != 0)
    {
        if (status != 0)
        {
            setError(QString("Event poll failed: %1").arg(status_to_string(status)));
        }
        return;
    }

    if (out != nullptr)
    {
        wallet_string_free(out);
    }

    if (eventType == WALLET_EVENT_SYNCED || eventType == WALLET_EVENT_TRANSACTION)
    {
        refresh();
    }
}

void WalletBridge::refresh()
{
    if (!checkLoaded())
    {
        return;
    }

    clearError();
    updateSync();
    updateBalances();
    updatePrimaryAddress();
    updateTransactions();
    updateNodeInfo();
}

void WalletBridge::swapNode(const QString &daemonHost, quint16 daemonPort, bool daemonSsl)
{
    if (!checkLoaded())
    {
        return;
    }

    const wallet_status_t status = wallet_swap_node(
        m_wallet,
        daemonHost.toUtf8().constData(),
        daemonPort,
        daemonSsl);
    if (status != 0)
    {
        setError(QString("Swap node failed: %1").arg(status_to_string(status)));
        return;
    }

    setMessage("Node switched");
    refresh();
}

void WalletBridge::resetWallet(qulonglong scanHeight, qulonglong timestamp)
{
    if (!checkLoaded())
    {
        return;
    }

    const wallet_status_t status = wallet_reset(m_wallet, scanHeight, timestamp);
    if (status != 0)
    {
        setError(QString("Reset failed: %1").arg(status_to_string(status)));
        return;
    }

    setMessage("Wallet reset requested");
    refresh();
}

void WalletBridge::sendBasic(
    const QString &destination,
    qulonglong amount,
    const QString &paymentId,
    bool sendAll)
{
    if (!checkLoaded())
    {
        return;
    }

    char *out = nullptr;
    size_t outLen = 0;
    const wallet_status_t status = wallet_send_basic(
        m_wallet,
        destination.toUtf8().constData(),
        amount,
        paymentId.toUtf8().constData(),
        sendAll,
        true,
        &out,
        &outLen);

    if (status != 0)
    {
        setError(QString("Send failed: %1").arg(status_to_string(status)));
        return;
    }

    const QString hash = extractString(status, out, outLen);
    setMessage("Transaction sent");
    emit sendResult(hash);
    refresh();
}

void WalletBridge::createIntegratedAddress(const QString &address, const QString &paymentId)
{
    char *out = nullptr;
    size_t outLen = 0;
    const wallet_status_t status = wallet_create_integrated_address(
        address.toUtf8().constData(),
        paymentId.toUtf8().constData(),
        &out,
        &outLen);

    if (status != 0)
    {
        setError(QString("Integrated address failed: %1").arg(status_to_string(status)));
        return;
    }

    const QString integrated = extractString(status, out, outLen);
    emit integratedAddressReady(integrated);
}

void WalletBridge::deleteWalletFile(const QString &filename)
{
    const wallet_status_t status = wallet_delete_file(filename.toUtf8().constData());
    if (status != 0)
    {
        setError(QString("Delete wallet file failed: %1").arg(status_to_string(status)));
        return;
    }

    setMessage("Wallet file deleted");
}

