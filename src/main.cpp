#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "core/settings.h"
#include "network/discovery.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Decenza");
    app.setApplicationName("DecenzaPocket");

    QQuickStyle::setStyle("Material");

    Settings settings;
    Discovery discovery;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Settings", &settings);
    engine.rootContext()->setContextProperty("Discovery", &discovery);
    engine.loadFromModule("DecenzaPocket", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
