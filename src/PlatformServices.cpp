#include "PlatformServices.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QCryptographicHash>
#ifdef Q_OS_ANDROID
#include "android/AndroidBeautyResources.h"
#include <QJniObject>
#include <QCoreApplication>
#include <QtCore/qcoreapplication_platform.h>
#include <unistd.h>
namespace {
constexpr auto activityClass = "org/beautyplayer/PlayerActivity";
QJniObject activity() { return QNativeInterface::QAndroidApplication::context(); }
}
#endif
namespace PlatformServices {
QString beautyResourceRoot(QString *error) {
#ifdef Q_OS_ANDROID
    const auto root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/gpupixel-v1.3.1";
    for (const auto *name : androidBeautyResources) {
        const auto relative = QString::fromLatin1(name);
        QFile source(":/beauty/" + relative), existing(root + "/" + relative);
        if (!source.open(QIODevice::ReadOnly)) { *error = "APK 缺少美颜资源：" + relative; return {}; }
        const auto data = source.readAll();
        if (existing.open(QIODevice::ReadOnly) && existing.readAll() == data) continue;
        existing.close();
        const auto path = root + "/" + relative;
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile destination(path);
        if (!destination.open(QIODevice::WriteOnly) || destination.write(data) != data.size() || !destination.commit()) {
            *error = "无法初始化美颜资源：" + destination.errorString(); return {};
        }
    }
    return root;
#else
    Q_UNUSED(error);
    return QCoreApplication::applicationDirPath();
#endif
}
void prepareMedia(const QUrl &url) {
#ifdef Q_OS_ANDROID
    if (url.scheme() == "content") {
        const auto uri = QJniObject::fromString(url.toString());
        QJniObject::callStaticMethod<void>(activityClass, "retainUri", "(Landroid/content/Context;Ljava/lang/String;)V", activity().object(), uri.object());
    }
#else
    Q_UNUSED(url);
#endif
}
std::unique_ptr<QFile> openMediaDevice(const QUrl &url, QString *error) {
#ifdef Q_OS_ANDROID
    if (url.scheme() == "content") {
        // Qt 6.11's DocumentFile checks explicit URI grants. MediaStore's
        // runtime READ_MEDIA_VIDEO grant is different. Let ContentResolver
        // enforce permissions and hand the authorized descriptor to Qt.
        const auto uri = QJniObject::fromString(url.toString());
        const int fd = QJniObject::callStaticMethod<jint>(activityClass, "openReadFd", "(Landroid/content/Context;Ljava/lang/String;)I", activity().object(), uri.object());
        if (fd < 0) { *error = QStringLiteral("无法打开视频，请重新选择文件或检查视频访问权限"); return {}; }
        auto file = std::make_unique<QFile>();
        if (!file->open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
            ::close(fd); *error = file->errorString(); return {};
        }
        return file;
    }
#else
    Q_UNUSED(url); Q_UNUSED(error);
#endif
    return {};
}
QString mediaTitle(const QUrl &url) {
#ifdef Q_OS_ANDROID
    if (url.scheme() == "content") {
        const auto uri = QJniObject::fromString(url.toString());
        const auto title = QJniObject::callStaticObjectMethod(activityClass, "displayName", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", activity().object(), uri.object()).toString();
        if (!title.isEmpty()) return title;
    }
#endif
    return url.fileName().isEmpty() ? url.host() : url.fileName();
}
QJsonObject scanAndroidVideos() {
#ifdef Q_OS_ANDROID
    const auto json = QJniObject::callStaticObjectMethod(activityClass, "scanVideos", "(Landroid/content/Context;)Ljava/lang/String;", activity().object()).toString();
    return QJsonDocument::fromJson(json.toUtf8()).object();
#else
    return {};
#endif
}
void requestVideoAccess() {
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(activityClass, "requestVideoAccess", "(Landroid/content/Context;)V", activity().object());
#endif
}
bool videoAccessPending() {
#ifdef Q_OS_ANDROID
    return QJniObject::getStaticField<jboolean>(activityClass, "permissionPending");
#else
    return false;
#endif
}
void keepScreenOn(bool enabled) {
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(activityClass, "keepScreenOn", "(Landroid/content/Context;Z)V", activity().object(), jboolean(enabled));
#else
    Q_UNUSED(enabled);
#endif
}
}
