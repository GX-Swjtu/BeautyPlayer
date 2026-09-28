#pragma once
#include <QString>
#include <QUrl>
#include <QJsonObject>
#include <QFile>
#include <memory>
namespace PlatformServices {
QString beautyResourceRoot(QString *error);
void prepareMedia(const QUrl &url);
std::unique_ptr<QFile> openMediaDevice(const QUrl &url, QString *error);
QString mediaTitle(const QUrl &url);
QJsonObject scanAndroidVideos();
void requestVideoAccess();
bool videoAccessPending();
void keepScreenOn(bool enabled);
}
