#include "network/decenzaclient.h"
#include "core/settings.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslError>
#include <QVariantMap>

DecenzaClient::DecenzaClient(Settings* settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_nam(new QNetworkAccessManager(this))
{
    // ShotServer uses self-signed certificates -- ignore SSL errors globally
    connect(m_nam, &QNetworkAccessManager::sslErrors,
            this, [](QNetworkReply* reply, const QList<QSslError>&) {
        reply->ignoreSslErrors();
    });

    m_pollTimer.setInterval(3000);
    connect(&m_pollTimer, &QTimer::timeout, this, &DecenzaClient::pollStatus);
}

// ---------------------------------------------------------------------------
// Low-level HTTP helpers
// ---------------------------------------------------------------------------

void DecenzaClient::get(const QString& path,
                        std::function<void(int statusCode, const QJsonObject&)> callback)
{
    QUrl url(m_serverUrl + path);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QString cookie = m_settings->sessionCookie();
    if (!cookie.isEmpty()) {
        request.setRawHeader("Cookie", cookie.toUtf8());
    }

    QNetworkReply* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError
            && reply->error() != QNetworkReply::ContentAccessDenied   // 401
            && reply->error() != QNetworkReply::ContentNotFoundError) // 404
        {
            int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QJsonObject empty;
            if (callback) callback(code, empty);
            emit connectionError(reply->errorString());
            return;
        }

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject json = doc.isObject() ? doc.object() : QJsonObject();

        if (callback) callback(statusCode, json);
    });
}

void DecenzaClient::post(const QString& path, const QByteArray& body,
                         std::function<void(int statusCode, const QJsonObject&)> callback)
{
    QUrl url(m_serverUrl + path);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QString cookie = m_settings->sessionCookie();
    if (!cookie.isEmpty()) {
        request.setRawHeader("Cookie", cookie.toUtf8());
    }

    QNetworkReply* reply = m_nam->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError
            && reply->error() != QNetworkReply::ContentAccessDenied   // 401
            && reply->error() != QNetworkReply::ContentNotFoundError) // 404
        {
            int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QJsonObject empty;
            if (callback) callback(code, empty);
            emit connectionError(reply->errorString());
            return;
        }

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject json = doc.isObject() ? doc.object() : QJsonObject();

        // Expose the raw reply for Set-Cookie extraction in the callback
        // We stash set-cookie into the JSON under a special key so login() can read it.
        QByteArray setCookie = reply->rawHeader("Set-Cookie");
        if (!setCookie.isEmpty()) {
            json["__set_cookie"] = QString::fromUtf8(setCookie);
        }

        if (callback) callback(statusCode, json);
    });
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

void DecenzaClient::connectToServer(const QString& serverUrl)
{
    m_serverUrl = serverUrl;

    // Remove trailing slash if present
    if (m_serverUrl.endsWith('/')) {
        m_serverUrl.chop(1);
    }

    m_connected = true;
    emit connectedChanged();

    // Probe the pocket/status endpoint first, fall back to power/status
    get(QStringLiteral("/api/pocket/status"), [this](int statusCode, const QJsonObject& json) {
        if (statusCode == 401) {
            emit loginRequired();
            return;
        }

        if (statusCode == 404 || statusCode == 0) {
            // pocket/status not available -- try legacy endpoint
            get(QStringLiteral("/api/power/status"), [this](int code, const QJsonObject&) {
                if (code == 401) {
                    emit loginRequired();
                    return;
                }
                if (code >= 200 && code < 300) {
                    m_pollTimer.start();
                    emit pollingChanged();
                } else {
                    emit connectionError(QStringLiteral("Server returned status %1").arg(code));
                }
            });
            return;
        }

        if (statusCode >= 200 && statusCode < 300) {
            m_pollTimer.start();
            emit pollingChanged();
        } else {
            emit connectionError(QStringLiteral("Server returned status %1").arg(statusCode));
        }
    });
}

void DecenzaClient::disconnect()
{
    m_pollTimer.stop();
    m_connected = false;
    emit pollingChanged();
    emit connectedChanged();
}

// ---------------------------------------------------------------------------
// Authentication
// ---------------------------------------------------------------------------

void DecenzaClient::login(const QString& totpCode)
{
    QJsonObject body;
    body["code"] = totpCode;
    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    post(QStringLiteral("/api/auth/login"), payload, [this](int statusCode, const QJsonObject& json) {
        if (statusCode == 200) {
            // Extract and persist the session cookie
            QString cookie = json.value("__set_cookie").toString();
            if (!cookie.isEmpty()) {
                // The Set-Cookie header may contain attributes (Path, HttpOnly, etc.).
                // We store the full value; the cookie name=value pair is at the front.
                m_settings->setSessionCookie(cookie);
            }
            emit loginSuccess();

            // Automatically fetch the theme after successful login
            fetchTheme();

            // Start polling now that we are authenticated
            if (!m_pollTimer.isActive()) {
                m_pollTimer.start();
                emit pollingChanged();
            }
        } else {
            QString error = json.value("error").toString();
            if (error.isEmpty()) {
                error = json.value("message").toString();
            }
            if (error.isEmpty()) {
                error = QStringLiteral("Login failed (HTTP %1)").arg(statusCode);
            }
            emit loginFailed(error);
        }
    });
}

// ---------------------------------------------------------------------------
// Polling
// ---------------------------------------------------------------------------

void DecenzaClient::pollStatus()
{
    get(QStringLiteral("/api/pocket/status"), [this](int statusCode, const QJsonObject& json) {
        if (statusCode == 401) {
            m_pollTimer.stop();
            emit pollingChanged();
            emit loginRequired();
            return;
        }

        if (statusCode == 404 || statusCode == 0) {
            // pocket/status not available -- combine /api/state + /api/telemetry
            get(QStringLiteral("/api/state"), [this](int stateCode, const QJsonObject& stateJson) {
                if (stateCode == 401) {
                    m_pollTimer.stop();
                    emit pollingChanged();
                    emit loginRequired();
                    return;
                }
                if (stateCode < 200 || stateCode >= 300) {
                    return; // silently skip this poll cycle
                }

                // Apply state fields
                m_machineState = stateJson.value("state").toString();
                m_machinePhase = stateJson.value("phase").toString();
                m_isHeating = stateJson.value("isHeating").toBool();
                m_isReady = stateJson.value("isReady").toBool();
                m_isAwake = stateJson.value("connected").toBool();

                // Now fetch telemetry
                get(QStringLiteral("/api/telemetry"), [this](int telCode, const QJsonObject& telJson) {
                    if (telCode >= 200 && telCode < 300) {
                        m_temperature = telJson.value("temperature").toDouble();
                        m_waterLevelMl = telJson.value("waterLevelMl").toDouble();
                    }

                    // Ready notification edge detection
                    if (m_wasHeating && m_isReady) {
                        emit readyNotification();
                    }
                    m_wasHeating = m_isHeating;

                    emit statusUpdated();
                });
            });
            return;
        }

        if (statusCode >= 200 && statusCode < 300) {
            // Parse the unified pocket/status response
            m_machineState = json.value("state").toString();
            m_machinePhase = json.value("phase").toString();
            m_temperature = json.value("temperature").toDouble();
            m_waterLevelMl = json.value("waterLevelMl").toDouble();
            m_isHeating = json.value("isHeating").toBool();
            m_isReady = json.value("isReady").toBool();
            m_isAwake = json.value("awake").toBool();
            if (json.contains("connected")) {
                m_isAwake = json.value("connected").toBool() && m_isAwake;
            }

            // Ready notification edge detection
            if (m_wasHeating && m_isReady) {
                emit readyNotification();
            }
            m_wasHeating = m_isHeating;

            emit statusUpdated();
        }
    });
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void DecenzaClient::wake()
{
    get(QStringLiteral("/api/power/wake"), [this](int statusCode, const QJsonObject&) {
        bool success = (statusCode >= 200 && statusCode < 300);
        emit commandSent(QStringLiteral("wake"), success);
    });
}

void DecenzaClient::sleep()
{
    get(QStringLiteral("/api/power/sleep"), [this](int statusCode, const QJsonObject&) {
        bool success = (statusCode >= 200 && statusCode < 300);
        emit commandSent(QStringLiteral("sleep"), success);
    });
}

// ---------------------------------------------------------------------------
// Pairing
// ---------------------------------------------------------------------------

void DecenzaClient::pair(const QString& pairingToken)
{
    QJsonObject body;
    body["pairingToken"] = pairingToken;
    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    post(QStringLiteral("/api/pocket/pair"), payload, [this, pairingToken](int statusCode, const QJsonObject& json) {
        if (statusCode == 200 && json.value("success").toBool()) {
            QString deviceId = json.value("deviceId").toString();
            QString deviceName = json.value("deviceName").toString();
            m_settings->setPairedDevice(deviceId, deviceName, m_serverUrl, pairingToken);
            emit pairingComplete(deviceId, deviceName);
            qDebug() << "DecenzaClient: Pairing complete, deviceId:" << deviceId;
        } else {
            QString error = json.value("error").toString("Pairing failed");
            emit connectionError(error);
        }
    });
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

void DecenzaClient::fetchTheme()
{
    get(QStringLiteral("/api/theme"), [this](int statusCode, const QJsonObject& json) {
        if (statusCode >= 200 && statusCode < 300) {
            QVariantMap colors = json.value("colors").toObject().toVariantMap();
            QVariantMap fonts = json.value("fonts").toObject().toVariantMap();
            emit themeReceived(colors, fonts);
        }
    });
}
