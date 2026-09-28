#include "FrameConversion.h"
#include <QCoreApplication>
#include <QAbstractVideoBuffer>
#include <QElapsedTimer>
#include <QPainter>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using F = QVideoFrameFormat;
class TestBuffer : public QAbstractVideoBuffer {
    F f;
    QByteArray planes[3];
    MapData data;
public:
    explicit TestBuffer(F format) : f(format) {
        const int w = f.frameWidth(), h = f.frameHeight();
        const bool packed = f.pixelFormat() == F::Format_NV12 || f.pixelFormat() == F::Format_NV21;
        data.planeCount = packed ? 2 : 3;
        for (int p = 0; p < data.planeCount; ++p) {
            const int row = p == 0 ? w : ((w + 1) / 2) * (packed ? 2 : 1);
            data.bytesPerLine[p] = ((row + 31) / 32) * 32;
            data.dataSize[p] = data.bytesPerLine[p] * (p == 0 ? h : (h + 1) / 2);
            planes[p].resize(data.dataSize[p]);
            data.data[p] = reinterpret_cast<uchar *>(planes[p].data());
        }
    }
    MapData map(QVideoFrame::MapMode) override { return data; }
    F format() const override { return f; }
};
int failures = 0, checks = 0;
void check(bool pass, const char *name) {
    ++checks;
    if (!pass) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
QVideoFrame fixture(QSize size, F::PixelFormat pixel, F::ColorSpace space, F::ColorRange range, bool flat = false) {
    F format(size, pixel); format.setColorSpace(space); format.setColorRange(range);
    QVideoFrame frame(std::make_unique<TestBuffer>(format));
    if (!frame.map(QVideoFrame::WriteOnly)) return {};
    for (int p = 0; p < frame.planeCount(); ++p) std::fill_n(frame.bits(p), frame.mappedBytes(p), uchar(0xcd));
    for (int y = 0; y < size.height(); ++y) for (int x = 0; x < size.width(); ++x)
        frame.bits(0)[y * frame.bytesPerLine(0) + x] = flat ? 130 : (16 + (x * 13 + y * 3) % 220);
    const bool packed = pixel == F::Format_NV12 || pixel == F::Format_NV21;
    const bool swap = pixel == F::Format_NV21 || pixel == F::Format_YV12;
    for (int y = 0; y < (size.height() + 1) / 2; ++y) for (int x = 0; x < (size.width() + 1) / 2; ++x) {
        const uchar u = flat ? 95 : 32 + (x * 9 + y * 5) % 192;
        const uchar v = flat ? 175 : 32 + (x * 3 + y * 11) % 192;
        if (packed) {
            auto *row = frame.bits(1) + y * frame.bytesPerLine(1);
            row[x * 2] = swap ? v : u; row[x * 2 + 1] = swap ? u : v;
        } else {
            frame.bits(swap ? 2 : 1)[y * frame.bytesPerLine(swap ? 2 : 1) + x] = u;
            frame.bits(swap ? 1 : 2)[y * frame.bytesPerLine(swap ? 1 : 2) + x] = v;
        }
    }
    frame.unmap(); return frame;
}
int maxDifference(const QImage &a, const QImage &b) {
    if (a.size() != b.size() || a.format() != b.format()) return 999;
    int diff = 0;
    for (int y = 0; y < a.height(); ++y) for (int x = 0; x < a.width() * 4; ++x)
        diff = std::max(diff, std::abs(int(a.constScanLine(y)[x]) - int(b.constScanLine(y)[x])));
    return diff;
}
int runFrameConversionTests(const QStringList &arguments) {
    failures = 0; checks = 0;
#ifdef Q_PROCESSOR_ARM
    constexpr auto baselineSimd = "neon";
#else
    constexpr auto baselineSimd = "sse";
#endif
    if (arguments.contains("--benchmark")) {
        for (const QSize size : {QSize(1920, 1080), QSize(2160, 3840)}) {
            auto frame = fixture(size, F::Format_NV12, F::ColorSpace_BT709, F::ColorRange_Video);
            for (const auto mode : {"scalar", baselineSimd, "auto"}) {
                configureFrameConversion(mode);
                for (int i = 0; i < 5; ++i) normalizedFrameImage(frame);
                QElapsedTimer timer; timer.start();
                for (int i = 0; i < 60; ++i) normalizedFrameImage(frame);
                std::cout << size.width() << 'x' << size.height() << ' ' << frameConversionBackend().toStdString()
                    << " meanMs=" << timer.nsecsElapsed() / 60000000.0 << std::endl;
            }
        }
        return 0;
    }
    configureFrameConversion();
    QImage image(321, 239, QImage::Format_RGBA8888); image.fill(Qt::red);
    { QPainter painter(&image); painter.fillRect(160, 0, 161, 119, Qt::green); painter.fillRect(0,119,160,120,Qt::blue); }
    for (int rotation : {0,90,180,270}) for (bool mirror : {false,true}) {
        QVideoFrame frame(image); frame.setRotation(QtVideo::Rotation(rotation)); frame.setMirrored(mirror);
        auto expected = image.transformed(QTransform().rotate(rotation));
        if (mirror) expected = expected.flipped(Qt::Horizontal);
        bool fast = true;
        const auto actual = normalizedFrameImage(frame, &fast);
        // Qt's GLES fallback rounds normalized texture samples to 8-bit, which
        // differs by at most one channel value on Adreno. Check every pixel,
        // including rotated/mirrored edges; CPU conversion remains exact below.
#ifdef Q_OS_ANDROID
        check(maxDifference(actual, expected) <= 1 && !fast, "RGBA GLES fallback transforms within 1/255");
#else
        check(actual == expected && !fast, "RGBA fallback transforms");
#endif
    }
    int worst = 0;
    for (auto size : {QSize(2,2), QSize(3,3), QSize(17,9), QSize(321,239), QSize(640,360)})
    for (auto pixel : {F::Format_YUV420P, F::Format_YV12, F::Format_NV12, F::Format_NV21})
    for (auto space : {F::ColorSpace_BT601, F::ColorSpace_BT709})
    for (auto range : {F::ColorRange_Video, F::ColorRange_Full}) {
        auto frame = fixture(size, pixel, space, range);
        configureFrameConversion("scalar");
        bool fast = false;
        const auto reference = normalizedFrameImage(frame, &fast);
        check(fast && reference.size() == size, "mapped YUV dimensions / padded strides");
        for (auto mode : {baselineSimd, "auto"}) {
            configureFrameConversion(mode);
            const auto result = normalizedFrameImage(frame, &fast);
            const int diff = maxDifference(reference, result); worst = std::max(worst, diff);
            check(fast && diff <= 1, "SIMD agrees with scalar");
        }
        auto flat = normalizedFrameImage(fixture(size, pixel, space, range, true));
        const double y = range == F::ColorRange_Video ? (130 - 16) * 255.0 / 219 : 130;
        const double u = (95 - 128) * (range == F::ColorRange_Video ? 255.0 / 224 : 1);
        const double v = (175 - 128) * (range == F::ColorRange_Video ? 255.0 / 224 : 1);
        const bool hd = space == F::ColorSpace_BT709;
        const double expected[] = {y + (hd ? 1.5748 : 1.402) * v,
            y - (hd ? .187324 : .344136) * u - (hd ? .468124 : .714136) * v,
            y + (hd ? 1.8556 : 1.772) * u, 255};
        for (int c = 0; c < 4; ++c) {
            const int delta = std::abs(int(flat.constBits()[c]) - std::clamp(int(std::round(expected[c])), 0, 255));
            if (delta > 2) std::cerr << "matrix pixel=" << int(pixel) << " space=" << int(space) << " range=" << int(range) << " channel=" << c << " actual=" << int(flat.constBits()[c]) << " expected=" << expected[c] << '\n';
            check(delta <= 2, "independent matrix / RGBA channel reference");
        }
    }
    auto frame = fixture(QSize(321,239), F::Format_NV12, F::ColorSpace_BT709, F::ColorRange_Video);
    const auto unrotated = normalizedFrameImage(frame);
    for (int rotation : {0,90,180,270}) for (bool mirror : {false,true}) {
        frame.setRotation(QtVideo::Rotation(rotation)); frame.setMirrored(mirror);
        auto expected = unrotated.transformed(QTransform().rotate(rotation));
        if (mirror) expected = expected.flipped(Qt::Horizontal);
        check(normalizedFrameImage(frame) == expected, "YUV presentation transforms");
    }
    std::vector<QImage> held, snapshots;
    for (int i = 0; i < 9; ++i) {
        auto input = fixture(QSize(320,240), F::Format_NV12, F::ColorSpace_BT709, F::ColorRange_Video, i % 2);
        held.push_back(normalizedFrameImage(input)); snapshots.push_back(held.back().copy());
    }
    for (int i = 0; i < 9; ++i) check(held[i] == snapshots[i], "pooled frame lifetime");
    for (int kind = 0; kind < 5; ++kind) {
        F format(QSize(32,24), F::Format_NV12); format.setColorSpace(F::ColorSpace_BT709); format.setColorRange(F::ColorRange_Video);
        if (kind == 0) format.setColorRange(F::ColorRange_Unknown);
        if (kind == 1) format.setColorTransfer(F::ColorTransfer_ST2084);
        if (kind == 2) format.setRotation(QtVideo::Rotation::Clockwise90);
        if (kind == 3) format.setViewport(QRect(1,1,30,22));
        if (kind == 4) format.setColorSpace(F::ColorSpace_Undefined);
        QVideoFrame unsupported(format); unsupported.map(QVideoFrame::WriteOnly);
        for (int p = 0; p < unsupported.planeCount(); ++p) std::fill_n(unsupported.bits(p), unsupported.mappedBytes(p), uchar(128));
        unsupported.unmap(); bool fast = true;
        normalizedFrameImage(unsupported, &fast); check(!fast, "unsupported metadata uses Qt");
    }
    check(!configureFrameConversion("invalid"), "invalid dispatch mode rejected");
    std::cout << checks << " checks, " << failures << " failures; scalar/SIMD max channel difference=" << worst << std::endl;
    return failures ? 1 : 0;
}
#ifndef BEAUTY_EMBEDDED_TESTS
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return runFrameConversionTests(app.arguments());
}
#endif
