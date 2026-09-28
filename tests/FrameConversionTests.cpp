#include "FrameConversion.h"
#include <QCoreApplication>
#include <QPainter>
#include <QTransform>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QImage image(321, 239, QImage::Format_RGBA8888); image.fill(Qt::red);
    { QPainter painter(&image); painter.fillRect(160, 0, 161, 119, Qt::green); painter.fillRect(0,119,160,120,Qt::blue); }
    bool ok = true;
    for (int rotation : {0,90,180,270}) for (bool mirror : {false,true}) {
        QVideoFrame frame(image); frame.setRotation(QtVideo::Rotation(rotation)); frame.setMirrored(mirror);
        auto expected = image.transformed(QTransform().rotate(rotation));
        if (mirror) expected = expected.flipped(Qt::Horizontal);
        bool passed = normalizedFrameImage(frame) == expected;
        ok &= passed; std::cout << rotation << " mirror=" << mirror << " passed=" << passed << std::endl;
    }
    return ok ? 0 : 1;
}
