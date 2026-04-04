#include "nativebridge.h"
#include <QDebug>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

namespace NativeBridge {

void syncPairingData(const QString& deviceId, const QString& pairingToken)
{
#ifdef Q_OS_ANDROID
    auto context = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "getContext",
        "()Landroid/content/Context;");
    if (!context.isValid()) return;

    auto prefs = context.callObjectMethod(
        "getSharedPreferences",
        "(Ljava/lang/String;I)Landroid/content/SharedPreferences;",
        QJniObject::fromString("Decenza.DecenzaPocket").object<jstring>(),
        0);
    if (!prefs.isValid()) return;

    auto editor = prefs.callObjectMethod(
        "edit",
        "()Landroid/content/SharedPreferences$Editor;");
    if (!editor.isValid()) return;

    editor.callObjectMethod(
        "putString",
        "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/SharedPreferences$Editor;",
        QJniObject::fromString("device/id").object<jstring>(),
        QJniObject::fromString(deviceId).object<jstring>());

    editor.callObjectMethod(
        "putString",
        "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/SharedPreferences$Editor;",
        QJniObject::fromString("device/pairingToken").object<jstring>(),
        QJniObject::fromString(pairingToken).object<jstring>());

    editor.callObjectMethod("apply", "()V");
    qDebug() << "NativeBridge: synced pairing to Android SharedPreferences";
#else
    Q_UNUSED(deviceId);
    Q_UNUSED(pairingToken);
#endif
}

void registerFcmToken(const QString& deviceId, const QString& pairingToken)
{
    // Android handles this from Java (DecenzaApplication.java)
    Q_UNUSED(deviceId);
    Q_UNUSED(pairingToken);
}

}
