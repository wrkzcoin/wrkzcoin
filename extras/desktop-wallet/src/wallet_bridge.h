#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <QTimer>

extern "C" {
#include <walletcapi/wallet_capi.h>
}

class WalletBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool daemonOnline READ daemonOnline NOTIFY daemonOnlineChanged)
    Q_PROPERTY(qulonglong walletHeight READ walletHeight NOTIFY syncChanged)
    Q_PROPERTY(qulonglong localHeight READ localHeight NOTIFY syncChanged)
    Q_PROPERTY(qulonglong networkHeight READ networkHeight NOTIFY syncChanged)
    Q_PROPERTY(qulonglong unlockedBalance READ unlockedBalance NOTIFY balanceChanged)
    Q_PROPERTY(qulonglong lockedBalance READ lockedBalance NOTIFY balanceChanged)
    Q_PROPERTY(QString primaryAddress READ primaryAddress NOTIFY primaryAddressChanged)
    Q_PROPERTY(QString transactionsJson READ transactionsJson NOTIFY transactionsChanged)
    Q_PROPERTY(QString nodeInfoJson READ nodeInfoJson NOTIFY nodeInfoChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)

  public:
    explicit WalletBridge(QObject *parent = nullptr);
    ~WalletBridge() override;

    bool loaded() const;
    bool busy() const;
    bool daemonOnline() const;
    qulonglong walletHeight() const;
    qulonglong localHeight() const;
    qulonglong networkHeight() const;
    qulonglong unlockedBalance() const;
    qulonglong lockedBalance() const;
    QString primaryAddress() const;
    QString transactionsJson() const;
    QString nodeInfoJson() const;
    QString lastError() const;
    QString lastMessage() const;

    Q_INVOKABLE void openWallet(
        const QString &filename,
        const QString &password,
        const QString &daemonHost,
        quint16 daemonPort,
        bool daemonSsl,
        quint32 syncThreads);
    Q_INVOKABLE void createWallet(
        const QString &filename,
        const QString &password,
        const QString &daemonHost,
        quint16 daemonPort,
        bool daemonSsl,
        quint32 syncThreads);
    Q_INVOKABLE void restoreFromSeed(
        const QString &seed,
        const QString &filename,
        const QString &password,
        qulonglong scanHeight,
        const QString &daemonHost,
        quint16 daemonPort,
        bool daemonSsl,
        quint32 syncThreads);
    Q_INVOKABLE void closeWallet();
    Q_INVOKABLE void saveWallet();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void swapNode(const QString &daemonHost, quint16 daemonPort, bool daemonSsl);
    Q_INVOKABLE void resetWallet(qulonglong scanHeight, qulonglong timestamp);
    Q_INVOKABLE void sendBasic(
        const QString &destination,
        qulonglong amount,
        const QString &paymentId,
        bool sendAll);
    Q_INVOKABLE void createIntegratedAddress(const QString &address, const QString &paymentId);
    Q_INVOKABLE void deleteWalletFile(const QString &filename);

  signals:
    void loadedChanged();
    void busyChanged();
    void daemonOnlineChanged();
    void syncChanged();
    void balanceChanged();
    void primaryAddressChanged();
    void transactionsChanged();
    void nodeInfoChanged();
    void lastErrorChanged();
    void lastMessageChanged();
    void sendResult(const QString &result);
    void integratedAddressReady(const QString &address);

  private:
    void setBusy(bool value);
    void setError(const QString &message);
    void setMessage(const QString &message);
    void clearError();
    void updateSync();
    void updateBalances();
    void updatePrimaryAddress();
    void updateTransactions();
    void updateNodeInfo();
    void pollEvents();
    bool checkLoaded();
    QString extractString(wallet_status_t status, char *ptr, size_t len) const;

  private:
    mutable QMutex m_mutex;
    wallet_handle_t *m_wallet = nullptr;
    QTimer m_pollTimer;
    bool m_busy = false;
    bool m_loaded = false;
    bool m_daemonOnline = false;
    qulonglong m_walletHeight = 0;
    qulonglong m_localHeight = 0;
    qulonglong m_networkHeight = 0;
    qulonglong m_unlockedBalance = 0;
    qulonglong m_lockedBalance = 0;
    QString m_primaryAddress;
    QString m_transactionsJson = "[]";
    QString m_nodeInfoJson = "{}";
    QString m_lastError;
    QString m_lastMessage;
};

