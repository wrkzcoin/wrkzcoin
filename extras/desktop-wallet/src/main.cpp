#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "wallet_bridge.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Wrkz");
    QCoreApplication::setApplicationName("DesktopWallet");

    WalletBridge walletBridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("walletBridge", &walletBridge);
    engine.load(QUrl("qrc:/qml/Main.qml"));

    if (engine.rootObjects().isEmpty())
    {
        return 1;
    }

    return app.exec();
}

