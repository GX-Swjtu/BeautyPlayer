#include "LibraryProbe.h"
#include "VideoLibrary.h"
#include "ThumbnailProvider.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QCoreApplication>

namespace {
void countImages(QQuickItem *item, int &ready, int &loading) {
    if (item->objectName() == "videoThumbnail" && item->isVisible()) {
        if (item->property("status").toInt() == 1) ++ready;
        if (item->property("status").toInt() == 2) ++loading;
    }
    for (auto *child : item->childItems()) countImages(child, ready, loading);
}
}
void runLibraryProbe(QQuickWindow *window, VideoLibrary *library, ThumbnailProvider *thumbnails, const QString &prefix) {
    QDir().mkpath(QFileInfo(prefix).absolutePath());
    auto *timer = new QTimer(window);
    timer->setInterval(500);
    QObject::connect(timer, &QTimer::timeout, window, [=, tick = 0, stage = 0, passed = true]() mutable {
        ++tick; int ready = 0, loading = 0; countImages(window->contentItem(), ready, loading);
        if (stage == 0 && !library->scanning() && ready > 0 && loading == 0) {
            window->grabWindow().save(prefix + "-list.png"); library->setTwoColumns(true); stage = 1; return;
        }
        if (stage == 1 && ready > 0 && loading == 0) {
            window->grabWindow().save(prefix + "-grid.png"); library->setSortBy("size"); library->setDescending(false); stage = 2; return;
        }
        if (stage == 2 || tick >= 90) {
            timer->stop(); passed = stage == 2 && ready > 0 && library->count() > 0;
            QFile file(prefix + ".json");
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(QJsonObject{{"passed", passed}, {"videoCount", library->count()}, {"visibleThumbnails", ready},
                    {"scanStatus", library->status()}, {"thumbnailCache", thumbnails->statistics()}}).toJson()); file.close();
            } else passed = false;
            QCoreApplication::exit(passed ? 0 : 1);
        }
    });
    library->setTwoColumns(false); window->setProperty("libraryOpen", true); library->refresh(); timer->start();
}
