#include "GPUPixelPipeline.h"
#include "FrameConversion.h"
#include "PlatformServices.h"
#include "../../tests/WhiteningChecks.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QDir>
#include <QFileInfo>
#include <QColor>
#include <sstream>
#include <iostream>
int runFrameConversionTests(const QStringList &arguments);

int runAndroidSelfTest(const QString &prefix) {
    QJsonArray checks;
    QJsonArray whitening;
    bool success = true;
    const auto check = [&](const QString &name, bool value) {
        checks.append(QJsonObject{{"name", name}, {"passed", value}}); success &= value;
    };
    std::ostringstream conversionLog;
    auto *oldOut = std::cout.rdbuf(conversionLog.rdbuf());
    auto *oldErr = std::cerr.rdbuf(conversionLog.rdbuf());
    const int conversionResult = runFrameConversionTests({});
    runFrameConversionTests({"--benchmark"});
    std::cout.rdbuf(oldOut); std::cerr.rdbuf(oldErr);
    check("color_conversion_scalar_neon_and_frame_lifetime", conversionResult == 0);
    configureFrameConversion("auto");
    check("neon_runtime_detected", frameConversionBackend().contains("NEON"));
    QString error, renderer;
    QDir().mkpath(QFileInfo(prefix).absolutePath());
    {
        GPUPixelPipeline pipeline;
        const auto root = PlatformServices::beautyResourceRoot(&error);
        const bool ready = error.isEmpty() && pipeline.initialize(root, &error);
        check("egl_shaders_resources_ready", ready); renderer = pipeline.renderer();
        if (ready) try {
            QImage input(319, 241, QImage::Format_RGBA8888);
            for (int y = 0; y < input.height(); ++y) for (int x = 0; x < input.width(); ++x)
                input.setPixelColor(x, y, QColor(90 + x % 80, 60 + y % 80, 40 + (x + y) % 60));
            auto identity = pipeline.process(input, {}).image;
            check("identity_rgba_orientation_dimensions", identity == input);
            auto next = input.copy(); next.fill(QColor(30, 90, 180));
            auto second = pipeline.process(next, {}).image;
            check("current_frame_no_previous_frame_delay", second == next);
            check("retained_output_not_overwritten", identity == input);
            auto beauty = pipeline.process(input, BeautyParameters::recommended()).image;
            check("default_beauty_changes_pixels", beauty.size() == input.size() && beauty != input);
            whitening = whiteningChecks(pipeline, input, prefix);
            bool continuous = true;
            for (const auto &row : whitening) continuous &= row.toObject()["passed"].toBool();
            check("whitening_continuous_from_zero_with_and_without_smoothing", continuous);
            input.save(prefix + "-original.png"); beauty.save(prefix + "-beauty.png");
            auto heavy = BeautyParameters::recommended();
            for (int i = BeautyParameters::Slimming; i < BeautyParameters::Count; ++i) heavy.values[i] = .4f;
            auto noFace = pipeline.process(next, heavy);
            check("face_models_load_and_no_face_clears_landmarks", !noFace.faceFound && !noFace.image.isNull());
        } catch (const std::exception &e) {
            error = QString::fromUtf8(e.what()); check("pipeline_exception_free", false);
        }
    }
    QSaveFile file(prefix + ".json");
    if (!file.open(QIODevice::WriteOnly)) return 2;
    file.write(QJsonDocument(QJsonObject{{"passed", success}, {"checks", checks}, {"renderer", renderer},
        {"whitening", whitening}, {"converter", frameConversionBackend()}, {"conversionLog", QString::fromStdString(conversionLog.str())}, {"error", error}}).toJson());
    if (!file.commit()) return 2;
    return success ? 0 : 1;
}
