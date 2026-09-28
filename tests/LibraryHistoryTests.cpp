#include "ResumeStore.h"
#include "VideoLibrary.h"
#include "BeautySettings.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

class LibraryHistoryTests : public QObject {
    Q_OBJECT
private slots:
    void lightweightDefaults() {
        BeautySettings settings(false);
        QVERIFY(settings.snapshot().active()); QVERIFY(!settings.snapshot().needsFace());
        QVERIFY(settings.snapshot().values[0] >= .6f);
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
