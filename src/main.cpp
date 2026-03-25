#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSslSocket>
#include <QDebug>

#include "core/settings.h"
#include "core/notificationmanager.h"
#include "network/discovery.h"
#include "network/decenzaclient.h"
#include "network/relayclient.h"
#include "network/connectionmanager.h"

int main(int argc, char *argv[])
{
    // Tell Qt's OpenSSL backend to look for libssl_3.so / libcrypto_3.so
    qputenv("QT_OPENSSL_SUFFIX", "_3");

    QGuiApplication app(argc, argv);
    app.setOrganizationName("Decenza");
    app.setApplicationName("DecenzaPocket");

    // Log SSL status (also triggers library loading on Android)
    qDebug() << "SSL supported:" << QSslSocket::supportsSsl()
             << "build:" << QSslSocket::sslLibraryBuildVersionString()
             << "runtime:" << QSslSocket::sslLibraryVersionString();

    QQuickStyle::setStyle("Material");

    Settings settings;
    Discovery discovery;
    DecenzaClient localClient(&settings);
    RelayClient remoteClient(&settings);
    ConnectionManager connection(&settings, &discovery, &localClient, &remoteClient);
    NotificationManager notifications;

    // Ready notifications from both local and remote
    QObject::connect(&localClient, &DecenzaClient::readyNotification,
                     &notifications, &NotificationManager::playReadySound);
    QObject::connect(&remoteClient, &RelayClient::readyNotification,
                     &notifications, &NotificationManager::playReadySound);
    QObject::connect(&connection, &ConnectionManager::readyNotification,
                     &notifications, &NotificationManager::playReadySound);

    // Save theme data when fetched from the local server
    QObject::connect(&localClient, &DecenzaClient::themeReceived,
                     &settings, &Settings::setThemeData);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Settings", &settings);
    engine.rootContext()->setContextProperty("Discovery", &discovery);
    engine.rootContext()->setContextProperty("Client", &localClient);
    engine.rootContext()->setContextProperty("Connection", &connection);
    engine.rootContext()->setContextProperty("Notifications", &notifications);
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
