#include "FrameConversion.h"
#include <QTransform>
#include <array>
#include <libyuv/convert_argb.h>
#include <libyuv/cpu_id.h>
#include <libyuv/planar_functions.h>
#include <libyuv/scale.h>

namespace {
bool useQt = false;
QString backend = QStringLiteral("libyuv auto");
using Format = QVideoFrameFormat;

QImage convertYuv(const QVideoFrame &input) {
    const auto format = input.surfaceFormat();
    const auto pixel = format.pixelFormat();
    const bool packed = pixel == Format::Format_NV12 || pixel == Format::Format_NV21;
    if (!packed && pixel != Format::Format_YUV420P && pixel != Format::Format_YV12) return {};
    // Other transfers, unspecified metadata and surface transforms stay with
    // Qt. This fast path is deliberately limited to known 8-bit SDR matrices.
    if ((format.colorSpace() != Format::ColorSpace_BT601 && format.colorSpace() != Format::ColorSpace_BT709)
        || format.colorRange() == Format::ColorRange_Unknown
        || (format.colorTransfer() != Format::ColorTransfer_Unknown
            && format.colorTransfer() != Format::ColorTransfer_BT601
            && format.colorTransfer() != Format::ColorTransfer_BT709)
        || format.rotation() != QtVideo::Rotation::None || format.isMirrored()
        || format.scanLineDirection() != Format::TopToBottom
        || format.viewport() != QRect(QPoint(), format.frameSize())) return {};
    const int w = input.width(), h = input.height(), cw = (w + 1) / 2, ch = (h + 1) / 2;
    if (w < 2 || h < 2 || w > 16384 || h > 16384) return {};
    auto frame = input;
    if (!frame.map(QVideoFrame::ReadOnly)) return {};
    struct Unmap { QVideoFrame &frame; ~Unmap() { frame.unmap(); } } unmap{frame};
    auto validPlane = [&frame](int plane, int rowBytes, int rows) {
        return plane < frame.planeCount() && frame.bits(plane) && frame.bytesPerLine(plane) >= rowBytes
            && frame.mappedBytes(plane) >= qint64(rows - 1) * frame.bytesPerLine(plane) + rowBytes;
    };
    if (!validPlane(0, w, h) || !validPlane(1, packed ? cw * 2 : cw, ch)
        || (!packed && !validPlane(2, cw, ch))) return {};
    // Thread-local scratch never aliases returned images. Reuse an output only
    // after all Qt/GL consumers have released it; a slow consumer cannot be overwritten.
    struct Scratch { QByteArray u, v; std::array<QImage, 3> images; };
    thread_local Scratch scratch;
    QImage unpooled;
    QImage *result = nullptr;
    for (auto &slot : scratch.images) {
        if (!slot.isNull() && !slot.isDetached()) continue;
        if (slot.size() != QSize(w, h)) slot = QImage(w, h, QImage::Format_RGBA8888);
        result = &slot; break;
    }
    if (!result) { unpooled = QImage(w, h, QImage::Format_RGBA8888); result = &unpooled; }
    auto *out = result->bits();
    if (!out) return {};
    const uchar *u = nullptr, *v = nullptr;
    int us = cw, vs = cw;
    if (packed) {
        scratch.u.resize(cw * ch); scratch.v.resize(cw * ch);
        libyuv::SplitUVPlane(frame.bits(1), frame.bytesPerLine(1),
            reinterpret_cast<uchar *>(scratch.u.data()), cw,
            reinterpret_cast<uchar *>(scratch.v.data()), cw, cw, ch);
        u = reinterpret_cast<const uchar *>(scratch.u.constData());
        v = reinterpret_cast<const uchar *>(scratch.v.constData());
        if (pixel == Format::Format_NV21) std::swap(u, v);
    } else {
        const int up = pixel == Format::Format_YV12 ? 2 : 1, vp = 3 - up;
        u = frame.bits(up); v = frame.bits(vp); us = frame.bytesPerLine(up); vs = frame.bytesPerLine(vp);
    }
    const bool full = format.colorRange() == Format::ColorRange_Full;
    const auto *matrix = format.colorSpace() == Format::ColorSpace_BT709
        ? (full ? &libyuv::kYvuF709Constants : &libyuv::kYvuH709Constants)
        : (full ? &libyuv::kYvuJPEGConstants : &libyuv::kYvuI601Constants);
    // libyuv ARGB means BGRA bytes on little endian. Swapped UV plus the Yvu
    // matrix gives RGBA. Preserve bilinear chroma interpolation (no nearest shortcut).
    if (libyuv::I420ToARGBMatrixFilter(frame.bits(0), frame.bytesPerLine(0), v, vs, u, us,
        out, result->bytesPerLine(), matrix, w, h, libyuv::kFilterBilinear) != 0) return {};
    return *result;
}
}

bool configureFrameConversion(const QString &mode) {
    if (mode != "auto" && mode != "sse" && mode != "scalar" && mode != "qt") return false;
    useQt = mode == "qt";
    int mask = libyuv::kCpuInitialized;
    if (mode == "auto" || mode == "sse") mask |= libyuv::kCpuHasX86 | libyuv::kCpuHasSSE2 | libyuv::kCpuHasSSSE3 | libyuv::kCpuHasSSE41 | libyuv::kCpuHasSSE42;
    if (mode == "auto") mask |= libyuv::kCpuHasAVX | libyuv::kCpuHasAVX2;
    const int enabled = libyuv::MaskCpuFlags(mask); // intersects real CPUID/OS support
    backend = useQt ? "Qt" : (enabled & libyuv::kCpuHasAVX2) ? "libyuv AVX2"
        : (enabled & libyuv::kCpuHasSSSE3) ? "libyuv SSSE3" : (enabled & libyuv::kCpuHasSSE2) ? "libyuv SSE2" : "libyuv scalar";
    return true;
}
QString frameConversionBackend() { return backend; }

QImage normalizedFrameImage(const QVideoFrame &frame, bool *usedYuv) {
    auto image = useQt ? QImage() : convertYuv(frame);
    if (usedYuv) *usedYuv = !image.isNull();
    if (image.isNull()) image = frame.toImage().convertToFormat(QImage::Format_RGBA8888);
    // Qt handles surface transforms; presentation transforms are separate.
    if (frame.rotation() != QtVideo::Rotation::None)
        image = image.transformed(QTransform().rotate(int(frame.rotation())));
    if (frame.mirrored()) image = image.flipped(Qt::Horizontal);
    return image;
}
