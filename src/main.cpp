#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "core/settings.h"
#include "core/notificationmanager.h"
#include "network/discovery.h"
#include "network/decenzaclient.h"
#include "network/relayclient.h"
#include "network/connectionmanager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Decenza");
    app.setApplicationName("DecenzaPocket");

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
    engine.loadFromModule("DecenzaPocket", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
