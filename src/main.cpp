#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Decenza");
    app.setApplicationName("DecenzaPocket");

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;
    engine.loadFromModule("DecenzaPocket", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
