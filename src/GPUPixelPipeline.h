#pragma once
#include "BeautyParameters.h"
#include <QImage>
#include <QString>
#include <memory>

struct BeautyResult {
    QImage image;
    bool faceFound = false;
    double milliseconds = 0;
};

// Construct, initialize, process and destroy on one dedicated thread.
// No Qt graphics handles cross this boundary; Android can use the same RGBA contract.
class GPUPixelPipeline {
public:
    GPUPixelPipeline();
    ~GPUPixelPipeline();
    bool initialize(const QString &resourceRoot, QString *error);
    BeautyResult process(const QImage &image, const BeautyParameters &parameters);
    QString renderer() const;
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
