#include "PlatformServices.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
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
QJsonObject scanAndroidVideos(bool respectNoMedia) {
#ifdef Q_OS_ANDROID
    const auto json = QJniObject::callStaticObjectMethod(activityClass, "scanVideos", "(Landroid/content/Context;Z)Ljava/lang/String;", activity().object(), jboolean(respectNoMedia)).toString();
    return QJsonDocument::fromJson(json.toUtf8()).object();
#else
    Q_UNUSED(respectNoMedia);
    return {};
#endif
}
QStringList videoFolders() {
#ifdef Q_OS_ANDROID
    const auto json = QJniObject::callStaticObjectMethod(activityClass, "videoFolders", "(Landroid/content/Context;)Ljava/lang/String;", activity().object()).toString();
    QStringList result;
    for (const auto &value : QJsonDocument::fromJson(json.toUtf8()).array()) result.append(value.toString());
    return result;
#else
    return {};
#endif
}
void cancelVideoScan() {
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("org/beautyplayer/VideoIndex", "cancel", "()V");
#endif
}
void chooseVideoFolder() {
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(activityClass, "chooseVideoFolder", "(Landroid/content/Context;)V", activity().object());
#endif
}
void removeVideoFolder(const QString &uri) {
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(activityClass, "removeVideoFolder", "(Landroid/content/Context;Ljava/lang/String;)V", activity().object(), QJniObject::fromString(uri).object());
#else
    Q_UNUSED(uri);
#endif
}
QImage videoThumbnail(const QUrl &url) {
#ifdef Q_OS_ANDROID
    const auto bytes = QJniObject::callStaticObjectMethod("org/beautyplayer/MediaThumbnails", "load", "(Landroid/content/Context;Ljava/lang/String;)[B", activity().object(), QJniObject::fromString(url.toString()).object());
    if (!bytes.isValid()) return {};
    QJniEnvironment env;
    auto array = bytes.object<jbyteArray>(); const auto length = env->GetArrayLength(array);
    if (length <= 0 || length > 8 * 1024 * 1024) return {};
    QByteArray data(length, Qt::Uninitialized);
    env->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte *>(data.data()));
    return QImage::fromData(data);
#else
    Q_UNUSED(url); return {};
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
