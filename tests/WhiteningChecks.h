// Copyright (c) 2026 GX-Swjtu. SPDX-License-Identifier: MIT
#pragma once
#include "GPUPixelPipeline.h"
#include <QJsonArray>
#include <QJsonObject>

// Compare rendered pixels, including the zero boundary with smoothing active.
inline QJsonArray whiteningChecks(GPUPixelPipeline &pipeline, const QImage &input, const QString &prefix) {
    QJsonArray results;
    for (float smoothing : {0.f, .7f}) {
        BeautyParameters p; p.values[BeautyParameters::Smoothing] = smoothing;
        const auto zero = pipeline.process(input, p).image.convertToFormat(QImage::Format_RGBA8888);
        p.values[BeautyParameters::Whitening] = 1;
        const auto full = pipeline.process(input, p).image.convertToFormat(QImage::Format_RGBA8888);
        double previous = 0;
        for (float strength : {.001f, .01f, .1f, .2f, .32f, .5f, 1.f}) {
            p.values[BeautyParameters::Whitening] = strength;
            const auto actual = pipeline.process(input, p).image.convertToFormat(QImage::Format_RGBA8888);
            double delta = 0, linearError = 0;
            for (int y = 0; y < zero.height(); ++y) for (int x = 0; x < zero.width() * 4; ++x) {
                if (x % 4 == 3) continue;
                const int a = actual.constScanLine(y)[x], z = zero.constScanLine(y)[x], f = full.constScanLine(y)[x];
                delta += std::abs(a - z);
                linearError += std::abs(a - (z + strength * (f - z)));
            }
            const double samples = zero.width() * double(zero.height()) * 3;
            delta /= samples; linearError /= samples;
            const bool passed = actual.size() == input.size() && linearError < .8 && delta + .05 >= previous;
            results.append(QJsonObject{{"smoothing", smoothing}, {"strength", strength}, {"difference", delta}, {"linearError", linearError}, {"passed", passed}});
            previous = delta;
            if (!prefix.isEmpty() && smoothing == .7f)
                actual.save(prefix + QString("-white-%1.png").arg(qRound(strength * 1000)));
        }
        if (!prefix.isEmpty() && smoothing == .7f) zero.save(prefix + "-white-0.png");
    }
    return results;
}
