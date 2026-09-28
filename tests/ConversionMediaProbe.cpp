#include "FrameConversion.h"
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QTimer>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <iostream>

// Decode once and compare the exact same frame with both converters. Separate
// playback screenshots can have different timestamps and are not valid A/B inputs.
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (argc != 3) return 2;
    const QString prefix = QString::fromLocal8Bit(argv[2]);
    QDir().mkpath(QFileInfo(prefix).absolutePath());
    QMediaPlayer player; QVideoSink sink; player.setVideoSink(&sink);
    bool done = false;
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &app, [&](const QVideoFrame &frame) {
        if (done || !frame.isValid()) return;
        done = true; player.pause();
        configureFrameConversion("qt"); const auto reference = normalizedFrameImage(frame);
        configureFrameConversion("auto"); bool used = false; const auto candidate = normalizedFrameImage(frame, &used);
        if (reference.isNull() || reference.size() != candidate.size()) { app.exit(3); return; }
        double absolute = 0, squared = 0; int maximum = 0;
        for (int y = 0; y < reference.height(); ++y) for (int x = 0; x < reference.width(); ++x) for (int c = 0; c < 3; ++c) {
            int delta = std::abs(int(reference.constScanLine(y)[x * 4 + c]) - int(candidate.constScanLine(y)[x * 4 + c]));
            maximum = std::max(maximum, delta); absolute += delta; squared += delta * delta;
        }
        const double n = double(reference.width()) * reference.height() * 3;
        const double mae = absolute / n, mse = squared / n;
        const bool saved = reference.save(prefix + "-qt.png") && candidate.save(prefix + "-avx2.png");
        const QJsonObject result{{"usedLibyuv", used}, {"pts", double(frame.startTime())},
            {"width", candidate.width()}, {"height", candidate.height()}, {"meanAbsoluteError", mae},
            {"maxChannelError", maximum}, {"psnrDb", mse > 0 ? 10 * std::log10(255.0 * 255 / mse) : 100},
            {"pixelFormat", QVideoFrameFormat::pixelFormatToString(frame.pixelFormat())}, {"saved", saved}};
        const auto json = QJsonDocument(result).toJson();
        QFile file(prefix + ".json"); if (file.open(QFile::WriteOnly)) file.write(json);
        std::cout << json.constData() << std::endl;
        app.exit(saved && mae < 2 ? 0 : 4);
    });
    QTimer::singleShot(20000, &app, [&] { app.exit(5); });
    player.setSource(QUrl::fromLocalFile(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()));
    player.play(); return app.exec();
}
