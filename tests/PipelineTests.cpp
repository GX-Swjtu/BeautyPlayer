#include "GPUPixelPipeline.h"
#include <QGuiApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QPainter>
#include <iostream>

double difference(const QImage &a, const QImage &b) {
    auto x = a.convertToFormat(QImage::Format_RGBA8888), y = b.convertToFormat(QImage::Format_RGBA8888);
    if (x.size() != y.size()) return 1e9;
    double sum = 0;
    for (int row = 0; row < x.height(); ++row) for (int col = 0; col < x.width() * 4; ++col)
        sum += std::abs(int(x.constScanLine(row)[col]) - int(y.constScanLine(row)[col]));
    return sum / (x.width() * double(x.height()) * 4);
}
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QDir work(QDir::currentPath());
    const QString out = work.filePath(argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("artifacts/pipeline")) + "/";
    work.mkpath(out);
    GPUPixelPipeline pipeline; QString error;
    if (!pipeline.initialize(QCoreApplication::applicationDirPath(), &error)) {
        std::cerr << error.toStdString() << std::endl; return 1;
    }
    QJsonArray results; bool ok = true;
    auto check = [&](QString name, bool passed, double delta = 0) {
        results.append(QJsonObject{{"test", name}, {"passed", passed}, {"difference", delta}});
        std::cout << name.toStdString() << ": " << passed << " delta=" << delta << std::endl;
        ok &= passed;
    };
    BeautyParameters neutral;
    QImage colors(321, 239, QImage::Format_RGBA8888); colors.fill(Qt::black);
    { QPainter p(&colors); p.fillRect(0,0,160,119,Qt::red); p.fillRect(160,0,161,119,Qt::green); p.fillRect(0,119,160,120,Qt::blue); p.fillRect(160,119,161,120,Qt::yellow); }
    auto identity = pipeline.process(colors, neutral);
    check("identity_odd_size_rgba_orientation", difference(colors, identity.image) < .01, difference(colors, identity.image));
    auto portrait = QImage(work.filePath(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("third_party/gpupixel/app/windows/demo.png")));
    if (portrait.isNull()) return 2;
    portrait.save(out + "original.png");
    const char *names[] = {"smoothing", "whitening", "temperature", "tint", "slimming", "eyes", "lipstick", "blush"};
    for (int i = 0; i < BeautyParameters::Count; ++i) {
        BeautyParameters p; p.values[i] = 1;
        auto result = pipeline.process(portrait, p);
        result.image.save(out + names[i] + ".png");
        const auto delta = difference(portrait, result.image);
        double light = 0;
        for (int y = 0; y < result.image.height(); y += 8) for (int x = 0; x < result.image.width(); x += 8)
            light += qGray(result.image.pixel(x, y));
        check(names[i], light > 1000 && delta > .02 && result.image.size() == portrait.size() && (!p.needsFace() || result.faceFound), delta);
    }
    BeautyParameters all; all.values = {.6f,.3f,.12f,.08f,.5f,.4f,.6f,.4f};
    auto combined = pipeline.process(portrait, all); combined.image.save(out + "combined.png");
    check("all_effects_face", combined.faceFound);
    auto absent = pipeline.process(colors, all); check("face_disappeared", !absent.faceFound);
    check("face_returns", pipeline.process(portrait, all).faceFound);
    auto reset = pipeline.process(colors, neutral);
    check("neutral_after_effects_no_stale_frame", difference(colors, reset.image) < .01, difference(colors, reset.image));
    QImage uhd = colors.scaled(3840,2160);
    auto full = pipeline.process(uhd, all); check("4k_preserved", full.image.size() == uhd.size());
    QJsonObject report{{"renderer", pipeline.renderer()}, {"tests", results}, {"passed", ok}, {"4k_ms", full.milliseconds}};
    QFile file(out + "results.json");
    if (!file.open(QIODevice::WriteOnly)) return 3;
    file.write(QJsonDocument(report).toJson());
    return ok ? 0 : 1;
}
