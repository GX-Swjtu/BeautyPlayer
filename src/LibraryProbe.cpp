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
#include <QFontDatabase>
#include <QTextLayout>
#include <QGlyphRun>
#include <QJsonArray>

namespace {
void countImages(QQuickItem *item, int &ready, int &loading) {
    if (item->objectName() == "videoThumbnail" && item->isVisible()) {
        if (item->property("status").toInt() == 1) ++ready;
        if (item->property("status").toInt() == 2) ++loading;
    }
    for (auto *child : item->childItems()) countImages(child, ready, loading);
}
QJsonArray glyphs(const QFont &font) {
    QJsonArray result;
    for (const auto ch : QStringLiteral("恢复推荐屏蔽")) {
        QTextLayout layout(QString(ch), font);
        layout.beginLayout(); layout.createLine(); layout.endLayout();
        for (const auto &run : layout.glyphRuns()) {
            const auto raw = run.rawFont();
            if (run.glyphIndexes().isEmpty()) continue;
            const auto bounds = raw.boundingRect(run.glyphIndexes().first());
            result.append(QJsonObject{{"character", QString(ch)}, {"family", raw.familyName()},
                {"inkWidth", bounds.width()}, {"pixelSize", raw.pixelSize()}});
        }
    }
    return result;
}
QJsonObject typography(QQuickWindow *window) {
    auto *reset = window->findChild<QObject *>("beautyReset");
    const auto font = reset ? reset->property("font").value<QFont>() : QFont();
    QJsonObject result{{"resetGlyphs", glyphs(font)}};
    result["hanFallbackFamilies"] = QJsonArray::fromStringList(QFontDatabase::applicationFallbackFontFamilies(QChar::Script_Han));
    QJsonArray families;
    for (const auto &family : QFontDatabase::families())
        if (family.contains("CJK") || family.contains("MiSans") || family.contains("Droid Sans Fallback")) families.append(family);
    result["cjkFamilies"] = families;
    return result;
}
bool shortcutPresent(QQuickItem *item, const QString &path) {
    if (item->objectName() == "folderShortcut" && item->property("folderPath").toString() == path) return true;
    for (auto *child : item->childItems()) if (shortcutPresent(child, path)) return true;
    return false;
}
bool modelContainsFolder(QObject *shortcuts, const QString &path) {
    if (!shortcuts) return false;
    for (const auto &value : shortcuts->property("model").toList())
        if (value.toMap()["path"].toString() == path) return true;
    return false;
}
}
void runLibraryProbe(QQuickWindow *window, VideoLibrary *library, ThumbnailProvider *thumbnails, const QString &prefix) {
    QDir().mkpath(QFileInfo(prefix).absolutePath());
    auto *timer = new QTimer(window);
    timer->setInterval(500);
    QObject::connect(timer, &QTimer::timeout, window, [=, tick = 0, stage = 0, checks = QJsonObject{}, blockedPath = QString(), beforeCount = 0]() mutable {
        ++tick; int ready = 0, loading = 0; countImages(window->contentItem(), ready, loading);
        if (stage == 0 && !library->scanning() && ready > 0 && loading == 0) {
            window->grabWindow().save(prefix + "-list.png"); library->setTwoColumns(true); stage = 1; return;
        }
        if (stage == 1 && ready > 0 && loading == 0) {
            window->grabWindow().save(prefix + "-grid.png"); library->setSortBy("size"); library->setDescending(false); stage = 2; return;
        }
        if (stage == 2) {
            blockedPath = library->data(library->index(0), VideoLibrary::FolderRole).toString();
            beforeCount = library->count();
            library->setFolder(blockedPath);
            auto *shortcuts = window->findChild<QObject *>("folderShortcuts");
            checks["shortcutInitiallyPresent"] = modelContainsFolder(shortcuts, blockedPath);
            library->setFolderBlocked(QDir::toNativeSeparators(blockedPath + "/./"), true);
            stage = 3; return;
        }
        if (stage == 3) {
            auto *shortcuts = window->findChild<QObject *>("folderShortcuts");
            checks["blockedShortcutRemoved"] = !modelContainsFolder(shortcuts, blockedPath) && !shortcutPresent(window->contentItem(), blockedPath);
            checks["shortcutModelUpdated"] = shortcuts && shortcuts->property("count").toInt() == library->folders().size();
            checks["selectedBlockedFolderCleared"] = library->folder().isEmpty();
            checks["blockedVideosRemoved"] = library->count() < beforeCount;
            window->grabWindow().save(prefix + "-blocked.png");
            library->setFolderBlocked(blockedPath, false); stage = 4; return;
        }
        if (stage == 4 || tick >= 90) {
            timer->stop(); bool passed = stage == 4 && ready > 0 && library->count() > 0;
            checks["videosRestored"] = library->count() == beforeCount;
            checks["restoredShortcutModel"] = modelContainsFolder(window->findChild<QObject *>("folderShortcuts"), blockedPath);
            const auto fontReport = typography(window);
            const auto resetGlyphs = fontReport["resetGlyphs"].toArray();
            checks["chineseGlyphWidth"] = resetGlyphs.size() == 6 && resetGlyphs[1].toObject()["inkWidth"].toDouble()
                >= .8 * resetGlyphs[0].toObject()["inkWidth"].toDouble();
#ifdef Q_OS_ANDROID
            const auto fallbacks = QFontDatabase::applicationFallbackFontFamilies(QChar::Script_Han);
            checks["simplifiedChineseFont"] = !fallbacks.isEmpty() && resetGlyphs.size() == 6
                && resetGlyphs[1].toObject()["family"].toString() == fallbacks.first();
#endif
            for (auto i = checks.begin(); i != checks.end(); ++i) passed = passed && i.value().toBool();
            QFile file(prefix + ".json");
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(QJsonObject{{"passed", passed}, {"videoCount", library->count()}, {"visibleThumbnails", ready},
                    {"scanStatus", library->status()}, {"thumbnailCache", thumbnails->statistics()},
                    {"checks", checks}, {"typography", fontReport}}).toJson()); file.close();
            } else passed = false;
            QCoreApplication::exit(passed ? 0 : 1);
        }
    });
    library->setTwoColumns(false); window->setProperty("libraryOpen", true); library->refresh(); timer->start();
}
