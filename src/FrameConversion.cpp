#include "FrameConversion.h"
#include <QTransform>

QImage normalizedFrameImage(const QVideoFrame &frame) {
    // Qt toImage handles surface transforms; frame presentation transforms are
    // separate and must be applied exactly once before finding face landmarks.
    auto image = frame.toImage().convertToFormat(QImage::Format_RGBA8888);
    if (frame.rotation() != QtVideo::Rotation::None)
        image = image.transformed(QTransform().rotate(int(frame.rotation())));
    if (frame.mirrored()) image = image.flipped(Qt::Horizontal);
    return image;
}
