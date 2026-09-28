#include "ResumeStore.h"
#include "VideoLibrary.h"
#include "BeautySettings.h"
#include "ThumbnailCache.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>

class LibraryHistoryTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_preferences;
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("BeautyPlayerTests");
        QCoreApplication::setApplicationName("IsolatedLibrarySettings");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_preferences.path());
    }
    void blockedFoldersStayExcludedAfterScanAndRestart() {
        QTemporaryDir dir; QDir().mkpath(dir.filePath("private/child")); QDir().mkpath(dir.filePath("private2"));
        for (const auto *name : {"public.mp4", "private/a.mp4", "private/child/b.mp4", "private2/c.mp4"}) {
            QFile f(dir.filePath(name)); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("fixture");
        }
        QSettings settings; settings.clear(); settings.setValue("library/roots", QStringList{dir.path()});
        const auto hidden = QDir::fromNativeSeparators(dir.filePath("private"));
        {
            VideoLibrary library; QTRY_COMPARE_WITH_TIMEOUT(library.count(), 4, 10000);
            library.setFolder(hidden); QCOMPARE(library.count(), 1);
            library.setFolderBlocked(hidden, true); QCOMPARE(library.count(), 2); QVERIFY(library.folder().isEmpty());
            QCOMPARE(library.folders().size(), 3); // All, root, private2; no prefix collision.
            library.refresh(); QTRY_VERIFY_WITH_TIMEOUT(!library.scanning(), 10000); QCOMPARE(library.count(), 2);
        }
        {
            VideoLibrary restored; QVERIFY(restored.isFolderBlocked(hidden));
            QTRY_COMPARE_WITH_TIMEOUT(restored.count(), 2, 10000);
            restored.setFolderBlocked(hidden, false); QCOMPARE(restored.count(), 4);
            QVERIFY(!restored.isFolderBlocked(hidden));
        }
        settings.clear();
    }
    void blockedFolderPathsUseTheSameIdentity() {
        QTemporaryDir dir; QDir().mkpath(dir.filePath("private/child")); QDir().mkpath(dir.filePath("private2"));
        for (const auto *name : {"public.mp4", "private/a.mp4", "private/child/b.mp4", "private2/c.mp4"}) {
            QFile f(dir.filePath(name)); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("fixture");
        }
        QSettings settings; settings.clear(); settings.setValue("library/roots", QStringList{dir.path()});
        const auto hidden = QDir::fromNativeSeparators(dir.filePath("private"));
        const auto alias = QDir::toNativeSeparators(hidden + "/./");
        {
            VideoLibrary library; QTRY_COMPARE_WITH_TIMEOUT(library.count(), 4, 10000);
            QSignalSpy shortcutsChanged(&library, &VideoLibrary::foldersChanged);
            library.setFolder(hidden); library.setFolderBlocked(alias, true);
            QCOMPARE(library.count(), 2); QVERIFY(library.folder().isEmpty());
            QVERIFY(!shortcutsChanged.isEmpty());
            for (const auto &value : library.folders()) {
                const auto item = value.toMap();
                QVERIFY(!item["path"].toString().startsWith(hidden + '/'));
                QVERIFY(item["path"].toString() != hidden);
                if (item["path"].toString().isEmpty()) QCOMPARE(item["count"].toInt(), 2);
            }
            library.setFolder(hidden); QVERIFY(library.folder().isEmpty()); QCOMPARE(library.count(), 2);
            library.refresh(); QTRY_VERIFY_WITH_TIMEOUT(!library.scanning(), 10000);
            QCOMPARE(library.count(), 2); QCOMPARE(library.folders().size(), 3);
        }
        // Settings written by older versions may contain native separators or a trailing slash.
        settings.setValue("library/blockedFolders", QStringList{alias});
        {
            VideoLibrary restored; QTRY_COMPARE_WITH_TIMEOUT(restored.count(), 2, 10000);
            bool canRestore = false;
            for (const auto &value : restored.folderOptions()) {
                const auto item = value.toMap();
                if (item["path"].toString() == hidden) canRestore = item["explicitBlock"].toBool();
            }
            QVERIFY(canRestore);
#ifdef Q_OS_WIN
            restored.setFolderBlocked(hidden.toUpper(), false);
#else
            restored.setFolderBlocked(hidden, false);
#endif
            QCOMPARE(restored.count(), 4); QCOMPARE(restored.folders().size(), 5);
        }
        settings.clear();
    }
    void presetMigrationKeepsCustomValues() {
        QSettings s; s.clear(); s.setValue("beauty/presetVersion", 2);
        s.setValue("beauty/temperature", -.04); s.setValue("beauty/tint", .025); s.setValue("beauty/whitening", .32); s.setValue("beauty/smoothing", .70);
        { BeautySettings settings(true); QCOMPARE(settings.snapshot().values[0], .2f); QCOMPARE(settings.snapshot().values[2], 0.f); QCOMPARE(settings.snapshot().values[3], 0.f); QCOMPARE(settings.snapshot().values[1], .2f); }
        s.setValue("beauty/presetVersion", 3); s.setValue("beauty/smoothing", .45); s.setValue("beauty/temperature", .13); s.setValue("beauty/tint", -.12);
        { BeautySettings settings(true); QCOMPARE(settings.snapshot().values[0], .45f); QCOMPARE(settings.snapshot().values[2], .13f); QCOMPARE(settings.snapshot().values[3], -.12f); settings.reset(); QCOMPARE(settings.snapshot().values[0], .2f); QCOMPARE(settings.snapshot().values[2], 0.f); QCOMPARE(settings.snapshot().values[3], 0.f); }
        s.setValue("beauty/smoothing", .70);
        { BeautySettings settings(true); QCOMPARE(settings.snapshot().values[0], .7f); } // A new explicit choice survives later launches.
        s.clear();
    }
    void noMediaPrunesDescendantsAndOverlappingRoots() {
        QTemporaryDir dir; QDir().mkpath(dir.filePath("hidden/child"));
        for (const auto *name : {"visible.mp4", "hidden/.nomedia", "hidden/private.mp4", "hidden/child/inside.mp4"}) {
            QFile file(dir.filePath(name)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("fixture");
        }
        VideoLibrary library(false); library.addFolder(QUrl::fromLocalFile(dir.path()));
        library.addFolder(QUrl::fromLocalFile(dir.filePath("hidden/child")));
        QTRY_COMPARE_WITH_TIMEOUT(library.count(), 1, 10000); QTRY_VERIFY_WITH_TIMEOUT(!library.scanning(), 10000);
        library.setRespectNoMedia(false);
        QTRY_COMPARE_WITH_TIMEOUT(library.count(), 3, 10000);
        library.setRespectNoMedia(true);
        QTRY_COMPARE_WITH_TIMEOUT(library.count(), 1, 10000);
        QVERIFY(QFile::remove(dir.filePath("hidden/.nomedia"))); library.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(library.count(), 3, 10000);
    }
    void sortingAndThumbnailIdentity() {
        QTemporaryDir dir;
        for (int n = 0; n < 3; ++n) {
            QFile f(dir.filePath(QString("%1.mp4").arg(QChar('a' + n)))); QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QByteArray(30 - n * 10, 'v'));
            QVERIFY(f.setFileTime(QDateTime::fromSecsSinceEpoch(1700000000 + n * 1000), QFileDevice::FileBirthTime));
        }
        VideoLibrary library(false); library.addFolder(QUrl::fromLocalFile(dir.path()));
        QTRY_COMPARE_WITH_TIMEOUT(library.count(), 3, 10000);
        auto first = [&] { return library.data(library.index(0), VideoLibrary::NameRole).toString(); };
        QCOMPARE(first(), "c.mp4");
        library.setDescending(false); QCOMPARE(first(), "a.mp4");
        library.setSortBy("size"); QCOMPARE(first(), "c.mp4");
        library.setDescending(true); QCOMPARE(first(), "a.mp4");
        library.setSortBy("name"); QCOMPARE(first(), "c.mp4");
        library.setDescending(false); QCOMPARE(first(), "a.mp4");
        library.setSortBy("invalid"); QCOMPARE(library.sortBy(), "name");
        const auto before = library.data(library.index(0), VideoLibrary::ThumbnailRole).toString();
        QFile f(dir.filePath("a.mp4")); QVERIFY(f.open(QIODevice::Append)); f.write("changed"); f.close();
        library.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(before != library.data(library.index(0), VideoLibrary::ThumbnailRole).toString(), 10000);
        library.setTwoColumns(true); QVERIFY(library.twoColumns());
    }
    void thumbnailCacheSurvivesRestartAndInvalidates() {
        QTemporaryDir dir; int calls = 0;
        auto generate = [&](const QUrl &) { ++calls; QImage image(64, 32, QImage::Format_RGB32); image.fill(Qt::green); return image; };
        ThumbnailRequest request{QUrl::fromLocalFile(dir.filePath("movie.mp4")), 100, 200};
        QCOMPARE(ThumbnailRequest::fromId(request.id()).url, request.url);
        QVERIFY(!ThumbnailRequest::fromId("../bad").valid());
        { ThumbnailCache cache(dir.path()); QVERIFY(!cache.load(request, generate).isNull()); QVERIFY(!cache.load(request, generate).isNull()); QCOMPARE(calls, 1); }
        ThumbnailCache restarted(dir.path()); QVERIFY(!restarted.load(request, generate).isNull()); QCOMPARE(calls, 1);
        ++request.modified; QVERIFY(!restarted.load(request, generate).isNull()); QCOMPARE(calls, 2);
        ++request.bytes; QVERIFY(!restarted.load(request, generate).isNull()); QCOMPARE(calls, 3);
        ThumbnailRequest missing{QUrl::fromLocalFile(dir.filePath("missing.mp4")), 0, 0};
        int failures = 0;
        auto fail = [&](const QUrl &) { ++failures; return QImage(); };
        QVERIFY(restarted.load(missing, fail).isNull()); QVERIFY(restarted.load(missing, fail).isNull()); QCOMPARE(failures, 1);
    }
    void thumbnailCacheExpiresByAccessAndBoundsStorage() {
        QTemporaryDir dir; ThumbnailCache cache(dir.path());
        auto generate = [](const QUrl &) { QImage image(32, 32, QImage::Format_RGB32); image.fill(Qt::blue); return image; };
        ThumbnailRequest old{QUrl::fromLocalFile(dir.filePath("old.mp4")), 1, 1}, used{QUrl::fromLocalFile(dir.filePath("used.mp4")), 1, 1};
        cache.load(old, generate); cache.load(used, generate);
        for (const auto &request : {old, used}) {
            QFile file(dir.filePath(request.key() + ".jpg")); QVERIFY(file.open(QIODevice::ReadWrite));
            QVERIFY(file.setFileTime(QDateTime::currentDateTimeUtc().addDays(-31), QFileDevice::FileModificationTime));
        }
        cache.load(used, generate); cache.prune();
        QVERIFY(!QFile::exists(dir.filePath(old.key() + ".jpg"))); QVERIFY(QFile::exists(dir.filePath(used.key() + ".jpg")));
        QFile unrelated(dir.filePath("user-photo.jpg")); QVERIFY(unrelated.open(QIODevice::WriteOnly)); unrelated.write("not a cache file"); unrelated.close();
        cache.prune(0, 0); QVERIFY(!QFile::exists(dir.filePath(used.key() + ".jpg"))); QVERIFY(QFile::exists(unrelated.fileName()));
    }
    void lightweightDefaults() {
        BeautySettings settings(false);
        QVERIFY(settings.snapshot().active()); QVERIFY(!settings.snapshot().needsFace());
        QCOMPARE(settings.snapshot().values[0], .2f);
        settings.setValue("slimming", .8); QVERIFY(settings.snapshot().needsFace());
        settings.reset(); QVERIFY(!settings.snapshot().needsFace());
    }
    void resumeSurvivesRestartAndDetectsFileReplacement() {
        QTemporaryDir dir; const auto media = dir.filePath("movie.mp4"), history = dir.filePath("history.json");
        QFile f(media); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("one"); f.close();
        const auto url = QUrl::fromLocalFile(media);
        { ResumeStore store(history); store.remember(url, 42000, 100000); store.flush(); }
        ResumeStore store(history); QCOMPARE(store.position(url), 42000);
        QVERIFY(f.open(QIODevice::Append)); f.write("changed"); f.close();
        QCOMPARE(store.position(url), 0);
        store.remember(url, 98000, 100000); QCOMPARE(store.position(url), 0);
        store.remember(url, 50000, 100000, true); QCOMPARE(store.position(url), 0);
    }
    void historyIsBoundedAndCorruptFileIsSafe() {
        QTemporaryDir dir; const auto path = dir.filePath("history.json");
        ResumeStore store(path);
        for (int i = 0; i < 250; ++i) store.remember(QUrl(QString("https://example.invalid/%1.mp4").arg(i)), 10000, 100000);
        store.flush(); QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(f.readAll()).object().size(), 200); f.close();
        QVERIFY(f.open(QIODevice::WriteOnly)); f.write("interrupted"); f.close();
        ResumeStore recovered(path); QCOMPARE(recovered.position(QUrl("https://example.invalid/1.mp4")), 0);
    }
    void scannerFiltersDeduplicatesAndRefreshes() {
        QTemporaryDir dir; QDir().mkpath(dir.filePath("child"));
        for (const auto *name : {"one.MP4", "child/two.mkv", "ignore.txt", "child/three.webm"}) {
            QFile f(dir.filePath(name)); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("fixture");
        }
        VideoLibrary library(false);
        library.addFolder(QUrl::fromLocalFile(dir.path()));
        // Overlapping roots and refresh during a scan must cancel/restart safely.
        library.addFolder(QUrl::fromLocalFile(dir.filePath("child")));
        QTRY_COMPARE_WITH_TIMEOUT(library.count(), 3, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!library.scanning(), 10000);
        QCOMPARE(library.folders().size(), 3);
        library.setSearch("TWO"); QCOMPARE(library.count(), 1);
        QCOMPARE(library.data(library.index(0), VideoLibrary::NameRole).toString(), "two.mkv");
        library.setSearch({}); library.setFolder(QFileInfo(dir.filePath("child")).canonicalFilePath()); QCOMPARE(library.count(), 2);
        QVERIFY(QFile::remove(dir.filePath("child/two.mkv")));
        library.refresh(); QTRY_COMPARE_WITH_TIMEOUT(library.count(), 1, 10000);
        library.setFolder({}); QCOMPARE(library.count(), 2);
    }
    void tenThousandFileIndex() {
        const auto artifacts = QDir::current().absoluteFilePath("artifacts");
        QVERIFY(QDir().mkpath(artifacts));
        QTemporaryDir dir(artifacts + "/library-scale-XXXXXX"); QVERIFY(dir.isValid());
        for (int bucket = 0; bucket < 100; ++bucket) {
            const auto folder = dir.filePath(QString::number(bucket)); QVERIFY(QDir().mkpath(folder));
            for (int n = 0; n < 100; ++n) { QFile f(folder + "/clip-" + QString::number(n) + ".mp4"); QVERIFY(f.open(QIODevice::WriteOnly)); }
        }
        VideoLibrary library(false); QElapsedTimer timer; timer.start();
        library.addFolder(QUrl::fromLocalFile(dir.path()));
        QTRY_COMPARE_WITH_TIMEOUT(library.count(), 10000, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!library.scanning(), 15000);
        const auto elapsed = timer.elapsed();
        library.setSearch("clip-99.mp4"); QCOMPARE(library.count(), 100);
        QFile report(artifacts + "/library-scale.json"); QVERIFY(report.open(QIODevice::WriteOnly));
        report.write(QJsonDocument(QJsonObject{{"files", 10000}, {"folders", 100}, {"scanAndPublishMs", elapsed}, {"workerStatus", library.status()}, {"searchMatches", library.count()}}).toJson());
    }
};
QTEST_GUILESS_MAIN(LibraryHistoryTests)
#include "LibraryHistoryTests.moc"
