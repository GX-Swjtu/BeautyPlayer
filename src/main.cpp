// Player setup adapted from Qt Multimedia's BSD-3-Clause Media Player example.
#include "BeautyVideoBridge.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QAudioOutput>
#include <QCommandLineParser>
#include <QQuickStyle>
#include <QDir>
#include <QIcon>
#include <QFontDatabase>
#include <QStandardPaths>
#include "ThumbnailProvider.h"
#include "LibraryProbe.h"
#include <QQuickWindow>
#include "PlaybackProbe.h"
#include "PlaybackSession.h"
#include "VideoLibrary.h"
#include "FrameConversion.h"
#include <cstdio>
#ifdef Q_OS_ANDROID
int runAndroidSelfTest(const QString &prefix);
#endif

int main(int argc, char **argv) {
#ifdef Q_OS_ANDROID
    // Qt 6.11 otherwise calls process-wide exit() after main returns. Android's
    // HWUI workers can still be running then, and abort on destroyed libc++
    // mutexes. Let Qt finish the Activity and Android own process reclamation;
    // our player, worker threads and EGL contexts still destruct normally.
    qputenv("QT_ANDROID_NO_EXIT_CALL", "1");
    // Keep Qt's FFmpeg backend: it selects Android MediaCodec for supported
    // streams, with software fallback. Do not force a Qualcomm-only codec name.
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    if (!qEnvironmentVariableIsSet("QT_FFMPEG_DECODING_HW_DEVICE_TYPES"))
        qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "mediacodec");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("BeautyPlayer");
    QCoreApplication::setApplicationName("BeautyPlayer");
    QCoreApplication::setApplicationVersion("0.4.4");
    QGuiApplication::setWindowIcon(QIcon(":/branding/mark.png"));
    QQuickStyle::setStyle("Material");
#ifdef Q_OS_ANDROID
    // Some OEM fallback lists select a Japanese Han font (e.g. MiSans Japanese
    // VF), whose U+590D is visibly narrower. The UI is Simplified Chinese:
    // prefer an installed SC family for Han, retaining the system Latin font.
    const auto installedFonts = QFontDatabase::families();
    QStringList hanFonts;
    for (const auto *family : {"Noto Sans CJK SC", "Noto Sans SC", "MiSans VF", "Droid Sans Fallback"})
        if (installedFonts.contains(QString::fromLatin1(family))) hanFonts.append(QString::fromLatin1(family));
    if (!hanFonts.isEmpty()) QFontDatabase::setApplicationFallbackFontFamilies(QChar::Script_Han, hanFonts);
#endif
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("video", "Local video or HTTP(S) URL");
    parser.addOption({"fresh", "Do not read or write saved beauty settings"});
    parser.addOption({"probe", "Run playback diagnostics and save JSON/screenshots", "report-prefix"});
    parser.addOption({"seconds", "Diagnostic duration", "seconds", "30"});
    parser.addOption({"exercise", "Exercise pause, beauty, seek, fullscreen and source changes"});
    parser.addOption({"hidden", "Hide window during long diagnostics"});
    parser.addOption({"profile", "Probe beauty profile: default, all, original", "profile", "all"});
    parser.addOption({"phone", "Preview the responsive phone layout (390 x 844)"});
#ifdef Q_OS_ANDROID
    parser.addOption({"self-test", "Run ARM64 color/NEON/GLES checks and save JSON", "report-prefix"});
#endif
    parser.addOption({"library-folder", "Add a folder to the video library", "folder"});
    parser.addOption({"library-search", "Initial video library search", "text"});
    parser.addOption({"library-probe", "Validate library thumbnails and layouts", "report-prefix"});
    parser.addOption({"thumbnail-cache", "Override thumbnail cache for isolated diagnostics", "directory"});
    parser.addOption({"converter", "Frame conversion: auto (AVX2/NEON), sse, neon, scalar, qt", "mode", "auto"});
    parser.process(app);
    if (!configureFrameConversion(parser.value("converter"))) parser.showHelp(2);
#ifdef Q_OS_ANDROID
    if (parser.isSet("self-test")) {
        QTimer::singleShot(500, &app, [&] { runAndroidSelfTest(parser.value("self-test")); });
        return app.exec(); // adb collects the report before stopping the test process.
    }
#endif
#ifndef Q_OS_ANDROID
    if (parser.isSet("probe")) qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &message) {
        const auto line = qFormatLogMessage(type, context, message).toUtf8();
        std::fprintf(stderr, "%s\n", line.constData()); std::fflush(stderr);
    });
#endif
    QMediaPlayer player; QAudioOutput audio; audio.setVolume(.65f); player.setAudioOutput(&audio);
    BeautySettings settings(!parser.isSet("fresh") && !parser.isSet("probe") && !parser.isSet("library-probe"));
    BeautyVideoBridge bridge(&player, &settings);
    const bool persistent = !parser.isSet("fresh") && !parser.isSet("probe") && !parser.isSet("library-probe");
    PlaybackSession session(&player, &bridge, persistent);
    VideoLibrary library(persistent);
    if (parser.isSet("library-folder")) library.addFolder(QUrl::fromLocalFile(parser.value("library-folder")));
    library.setSearch(parser.value("library-search"));
    QQmlApplicationEngine engine;
    auto *thumbnails = new ThumbnailProvider(parser.isSet("thumbnail-cache") ? parser.value("thumbnail-cache")
        : QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails-v1");
    engine.addImageProvider("thumbnails", thumbnails);
    engine.rootContext()->setContextProperty("player", &player);
    engine.rootContext()->setContextProperty("audio", &audio);
    engine.rootContext()->setContextProperty("beautySettings", &settings);
    engine.rootContext()->setContextProperty("bridge", &bridge);
    engine.rootContext()->setContextProperty("playbackSession", &session);
    engine.rootContext()->setContextProperty("videoLibrary", &library);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("BeautyPlayer", "Main");
    if (parser.isSet("phone") && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); window->resize(390, 844);
    }
    std::unique_ptr<PlaybackProbe> probe;
    if (!engine.rootObjects().isEmpty() && parser.isSet("library-probe"))
        runLibraryProbe(qobject_cast<QQuickWindow *>(engine.rootObjects().first()), &library, thumbnails, parser.value("library-probe"));
    if (!engine.rootObjects().isEmpty() && parser.isSet("probe")) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (parser.isSet("hidden")) window->hide();
        probe = std::make_unique<PlaybackProbe>(&player, &settings, &bridge, window, parser.value("probe"),
            std::max(4, parser.value("seconds").toInt()), parser.isSet("exercise"), parser.value("profile"), &session, &library);
    }
    if (!parser.positionalArguments().isEmpty()) {
        const auto url = QUrl::fromUserInput(parser.positionalArguments().first(), QDir::currentPath(), QUrl::AssumeLocalFile);
        QTimer::singleShot(500, &bridge, [&bridge, url] { bridge.open(url); });
    }
    const int result = app.exec(); session.save(); player.stop(); return result;
}
