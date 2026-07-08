#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSslSocket>
#include <QKeyEvent>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QDebug>

#include "core/settings.h"
#include "core/nativebridge.h"
#include "core/notificationmanager.h"
#include "network/discovery.h"
#include "network/decenzaclient.h"
#include "network/relayclient.h"
#include "network/connectionmanager.h"
#include "network/remotecontrolclient.h"
#include "version.h"

int main(int argc, char *argv[])
{
    // Tell Qt's OpenSSL backend to look for libssl_3.so / libcrypto_3.so
    qputenv("QT_OPENSSL_SUFFIX", "_3");

    // Parse --ss WxH[@S] for App Store screenshot sizing. WxH is the final PNG pixel
    // size; S is the device pixel ratio (default 3, matching iPhone @3x). The window
    // is sized at logical W/S × H/S so QML lays out at iPhone-point dimensions, then
    // Qt renders at device pixels. Must run before QGuiApplication so QT_SCALE_FACTOR
    // takes effect.
    QSize screenshotLogical;
    int screenshotScale = 3;
    for (int i = 1; i < argc - 1; ++i) {
        if (QByteArray(argv[i]) != "--ss") continue;
        QByteArray spec(argv[i + 1]);
        int scale = 3;
        const int at = spec.indexOf('@');
        if (at > 0) {
            scale = qMax(1, spec.mid(at + 1).toInt());
            spec = spec.left(at);
        }
        const int x = spec.indexOf('x');
        if (x <= 0) break;
        bool wOk = false, hOk = false;
        const int w = spec.left(x).toInt(&wOk);
        const int h = spec.mid(x + 1).toInt(&hOk);
        if (!wOk || !hOk || w <= 0 || h <= 0) break;
        screenshotLogical = QSize(w / scale, h / scale);
        screenshotScale = scale;
        qputenv("QT_SCALE_FACTOR", QByteArray::number(scale));
        break;
    }

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
    RemoteControlClient remoteControl(remoteClient.socket());
    ConnectionManager connection(&settings, &remoteClient);
    NotificationManager notifications;

    // Ready notifications from relay
    QObject::connect(&remoteClient, &RelayClient::readyNotification,
                     &notifications, &NotificationManager::playReadySound);

    // Forward binary WebSocket messages to remote control client
    QObject::connect(&remoteClient, &RelayClient::binaryMessageReceived,
                     &remoteControl, &RemoteControlClient::handleBinaryMessage);

    // Save theme data when fetched from the local server
    QObject::connect(&localClient, &DecenzaClient::themeReceived,
                     &settings, &Settings::setThemeData);

    QQmlApplicationEngine engine;
    engine.addImageProvider("remoteframe", new RemoteFrameProvider(&remoteControl));
    engine.rootContext()->setContextProperty("RemoteControl", &remoteControl);
    engine.rootContext()->setContextProperty("Settings", &settings);
    engine.rootContext()->setContextProperty("Discovery", &discovery);
    engine.rootContext()->setContextProperty("Client", &localClient);
    engine.rootContext()->setContextProperty("Connection", &connection);
    engine.rootContext()->setContextProperty("Notifications", &notifications);
    engine.rootContext()->setContextProperty("AppVersion", QStringLiteral(VERSION_STRING));
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    // App Store screenshot mode. The window is locked to an exact device size; F5–F8
    // re-lock it to another size and F12 saves a PNG at the current size. Locking
    // (min == max) is what makes portrait work: a freely-resizable window defaults its
    // maximum to the screen work area, so Windows silently clamps a taller-than-screen
    // portrait (e.g. 1242x2688 on a 4K display) and the capture comes out short. The lock
    // overrides that cap, and grabWindow() renders off-screen so the full size is captured
    // even where it extends past the screen. It also blocks stray drag-resizes to invalid
    // sizes.
    if (screenshotLogical.isValid()) {
        if (auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {

            class ScreenshotHelper : public QObject {
            public:
                ScreenshotHelper(QQuickWindow *w, int scale) : QObject(w), m_win(w), m_scale(scale) {}

                // Lock to the logical points that render to the given device-pixel size.
                void lockTo(int pxW, int pxH) {
                    const QSize logical(pxW / m_scale, pxH / m_scale);
                    m_win->setMinimumSize(logical);
                    m_win->setMaximumSize(logical);
                    m_win->resize(logical);
                    qInfo().noquote() << QString("Window locked to %1x%2 — press F12 to save").arg(pxW).arg(pxH);
                }

                bool eventFilter(QObject *, QEvent *e) override {
                    if (e->type() != QEvent::KeyPress)
                        return false;
                    switch (static_cast<QKeyEvent *>(e)->key()) {
                    case Qt::Key_F4:  // 13" iPad landscape — needs @2 (iPad renders at 2x, not 3x)
                        if (m_scale != 2)
                            qWarning().noquote() << "iPad size needs @2 — relaunch with --ss 2752x2064@2";
                        lockTo(2752, 2064);
                        return true;
                    case Qt::Key_F5:  lockTo(2868, 1320); return true; // 6.9" landscape
                    case Qt::Key_F6:  lockTo(2688, 1242); return true; // 6.5" landscape
                    case Qt::Key_F7:  lockTo(1320, 2868); return true; // 6.9" portrait
                    case Qt::Key_F8:  lockTo(1242, 2688); return true; // 6.5" portrait
                    case Qt::Key_F12: capture();          return true;
                    default:          return false;                    // let other keys through (e.g. TOTP entry)
                    }
                }
            private:
                void capture() {
                    // Drop the alpha channel — App Store Connect rejects screenshots that
                    // carry one (the UI is fully opaque anyway).
                    const QImage img = m_win->grabWindow().convertToFormat(QImage::Format_RGB888);
                    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
                    QDir().mkpath(dir);
                    const QString path = QString("%1/decenza-pocket-%2x%3-%4.png")
                        .arg(dir).arg(img.width()).arg(img.height())
                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
                    if (img.save(path))
                        qInfo().noquote() << "Saved screenshot:" << path;
                    else
                        qWarning() << "Failed to save screenshot to" << path;
                }
                QQuickWindow *m_win;
                int m_scale;
            };

            auto *helper = new ScreenshotHelper(win, screenshotScale);
            win->installEventFilter(helper);
            helper->lockTo(screenshotLogical.width() * screenshotScale, screenshotLogical.height() * screenshotScale);

            qInfo().noquote() << "Screenshot mode — snap to a device size, then capture:\n"
                                 "  F4 = 2752x2064 (13\" iPad landscape — launch this session with @2)\n"
                                 "  F5 = 2868x1320 (6.9\" landscape)   F6 = 2688x1242 (6.5\" landscape)\n"
                                 "  F7 = 1320x2868 (6.9\" portrait)    F8 = 1242x2688 (6.5\" portrait)\n"
                                 "  F12 = save PNG at the current size";
        }
    }

    // Sync pairing data to native platform storage for widget access
    if (settings.isPaired()) {
        NativeBridge::syncPairingData(settings.pairedDeviceId(), settings.pairingToken());
    }

    return app.exec();
}
