#pragma once
#include <QImage>
#include <QVideoFrame>
// Configure once, before starting conversion threads. Tests may reconfigure only
// when no conversion is in flight. Modes restrict detected CPU features.
bool configureFrameConversion(const QString &mode = QStringLiteral("auto"));
QString frameConversionBackend();
QImage normalizedFrameImage(const QVideoFrame &frame, bool *usedYuv = nullptr);
