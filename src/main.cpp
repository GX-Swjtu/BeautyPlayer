// Player setup adapted from Qt Multimedia's BSD-3-Clause Media Player example.
#include "BeautyVideoBridge.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QAudioOutput>
#include <QCommandLineParser>
#include <QQuickStyle>
#include <QDir>
#include <QQuickWindow>
#include "PlaybackProbe.h"
#include "PlaybackSession.h"
#include "VideoLibrary.h"
#include "FrameConversion.h"
#include <cstdio>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("BeautyPlayer");
    QCoreApplication::setApplicationName("BeautyPlayer");
    QCoreApplication::setApplicationVersion("0.2.1");
    QQuickStyle::setStyle("Material");
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("video", "Local video or HTTP(S) URL");
    parser.addOption({"fresh", "Do not read or write saved beauty settings"});
    parser.addOption({"probe", "Run playback diagnostics and save JSON/screenshots", "report-prefix"});
    parser.addOption({"seconds", "Diagnostic duration", "seconds", "30"});
    parser.addOption({"exercise", "Exercise pause, beauty, seek, fullscreen and source changes"});
    parser.addOption({"hidden", "Hide window during long diagnostics"});
    parser.addOption({"profile", "Probe beauty profile: default, all, original", "profile", "all"});
    parser.addOption({"phone", "Preview the responsive phone layout (390 x 844)"});
    parser.addOption({"library-folder", "Add a folder to the video library", "folder"});
    parser.addOption({"converter", "Frame conversion: auto (AVX2/SSE fallback), sse, scalar, qt", "mode", "auto"});
    parser.process(app);
    if (!configureFrameConversion(parser.value("converter"))) parser.showHelp(2);
    if (parser.isSet("probe")) qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &message) {
        const auto line = qFormatLogMessage(type, context, message).toUtf8();
        std::fprintf(stderr, "%s\n", line.constData()); std::fflush(stderr);
    });
    QMediaPlayer player; QAudioOutput audio; audio.setVolume(.65f); player.setAudioOutput(&audio);
    BeautySettings settings(!parser.isSet("fresh") && !parser.isSet("probe"));
    BeautyVideoBridge bridge(&player, &settings);
    const bool persistent = !parser.isSet("fresh") && !parser.isSet("probe");
    PlaybackSession session(&player, &bridge, persistent);
    VideoLibrary library(persistent);
    if (parser.isSet("library-folder")) library.addFolder(QUrl::fromLocalFile(parser.value("library-folder")));
    QQmlApplicationEngine engine;
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
