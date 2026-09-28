#ifdef _WIN32
#define GPUPIXEL_API __declspec(dllimport)
#endif
#include "GPUPixelPipeline.h"
#include "gpupixel_context.h"
#include "source_raw_data_input.h"
#include "beauty_face_filter.h"
#include "white_balance_filter.h"
#include "face_reshape_filter.h"
#include "lipstick_filter.h"
#include "blusher_filter.h"
#include "face_detector.h"
#include "util.h"
#include "WhiteBalanceShader.h"
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QLibrary>
#include <QFile>
#include <fstream>
#include <stdexcept>

using namespace gpupixel;

namespace {
class DesktopWhiteBalanceFilter final : public WhiteBalanceFilter {
public:
    static std::shared_ptr<DesktopWhiteBalanceFilter> createCompatible() {
        auto result = std::shared_ptr<DesktopWhiteBalanceFilter>(new DesktopWhiteBalanceFilter);
        if (!result->initWithFragmentShaderString(kDesktopWhiteBalanceShader)) return {};
        result->setTemperature(5000.f); result->setTint(0.f);
        return result;
    }
};

void requireLinked(const std::shared_ptr<Filter> &filter) {
    if (!filter || !filter->getProgram()) throw std::runtime_error("Missing GPUPixel shader");
    GLint linked = GL_FALSE;
    glGetProgramiv(filter->getProgram()->getID(), GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[2048]{};
        glGetProgramInfoLog(filter->getProgram()->getID(), sizeof(log), nullptr, log);
        throw std::runtime_error(std::string("GPUPixel shader link failed: ") + log);
    }
}
// The beta3 SDK's PBO output returns the previous image tagged with the current
// timestamp. Read the current FBO instead, and let QImage own the returned bytes.
class CurrentFrameOutput final : public Target {
public:
    QImage image;
    std::array<QImage, 3> pool;
    void update(int64_t) override {
        auto fb = _inputFramebuffers.at(0).frameBuffer;
        image = {};
        // Reuse only storage no longer referenced by Qt's render thread.
        for (auto &candidate : pool) {
            if (!candidate.isNull() && !candidate.isDetached()) continue;
            if (candidate.size() != QSize(fb->getWidth(), fb->getHeight()))
                candidate = QImage(fb->getWidth(), fb->getHeight(), QImage::Format_RGBA8888);
            image.swap(candidate);
            break;
        }
        if (image.isNull()) image = QImage(fb->getWidth(), fb->getHeight(), QImage::Format_RGBA8888);
        if (image.isNull()) throw std::runtime_error("Unable to allocate output frame");
        fb->active();
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, image.width(), image.height(), GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        fb->inactive();
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL frame readback failed");
        for (auto &candidate : pool) if (candidate.isNull()) { candidate = image; break; }
    }
};
}

struct GPUPixelPipeline::Impl {
    bool initialized = false;
    bool ownsContext = false;
    GLint maxSize = 0;
    QString renderer;
    QLibrary library;
    std::shared_ptr<SourceRawDataInput> source;
    std::shared_ptr<BeautyFaceFilter> beauty;
    std::shared_ptr<WhiteBalanceFilter> balance;
    std::shared_ptr<FaceReshapeFilter> reshape;
    std::shared_ptr<LipstickFilter> lipstick;
    std::shared_ptr<BlusherFilter> blush;
    std::shared_ptr<CurrentFrameOutput> output;
    std::unique_ptr<FaceDetector> detector;
    std::vector<float> landmarks;
    int graphMask = -1;
    ~Impl() {
        if (!ownsContext) return;
        // Break references while the GL context is still alive.
        source.reset(); lipstick.reset(); blush.reset(); reshape.reset(); beauty.reset(); balance.reset();
        output.reset(); detector.reset();
        GPUPixelContext::destroy();
    }
    void configureGraph(const BeautyParameters &p) {
        int mask = 0;
        for (int i = 0; i < BeautyParameters::Count; ++i) if (std::abs(p.values[i]) > .0001f) mask |= 1 << i;
        if (mask == graphMask) return;
        source->removeAllTargets(); lipstick->removeAllTargets(); blush->removeAllTargets();
        reshape->removeAllTargets(); beauty->removeAllTargets(); balance->removeAllTargets();
        std::shared_ptr<Source> tail = source;
        const auto append = [&tail](const auto &filter) { tail->addTarget(filter); tail = filter; };
        if (p.values[BeautyParameters::Lipstick] > 0) append(lipstick);
        if (p.values[BeautyParameters::Blush] > 0) append(blush);
        if (p.values[BeautyParameters::Slimming] > 0 || p.values[BeautyParameters::Eyes] > 0) append(reshape);
        if (p.values[BeautyParameters::Smoothing] > 0 || p.values[BeautyParameters::Whitening] > 0) append(beauty);
        if (p.values[BeautyParameters::Temperature] != 0 || p.values[BeautyParameters::Tint] != 0) append(balance);
        tail->addTarget(output);
        graphMask = mask;
    }
};

GPUPixelPipeline::GPUPixelPipeline() : d(std::make_unique<Impl>()) {}
GPUPixelPipeline::~GPUPixelPipeline() = default;
QString GPUPixelPipeline::renderer() const { return d->renderer; }

bool GPUPixelPipeline::initialize(const QString &root, QString *error) {
    if (d->initialized) return true;
    for (const auto *file : {"models/face_align.mars_model", "models/face_det.mars_model", "res/mouth.png", "res/blusher.png"}) {
        if (!QFileInfo::exists(QDir(root).filePath(QString::fromLatin1(file)))) {
            *error = QStringLiteral("缺少美颜资源：%1").arg(QString::fromLatin1(file));
            return false;
        }
    }
    try {
        d->library.setFileName(QDir(root).filePath(QStringLiteral("gpupixel")));
        if (!d->library.load()) {
            *error = QStringLiteral("美颜动态库加载失败：%1").arg(d->library.errorString());
            return false;
        }
        // beta3's closed model loader uses narrow std::ifstream on Windows.
        // UTF-8 paths fail on a non-UTF-8 Windows ACP and can crash the loader.
        // Match the native filename encoding and verify it before entering it.
        const auto resourcePath = QFile::encodeName(QDir::fromNativeSeparators(root)).toStdString();
        for (const auto *name : {"models/face_det.mars_model", "models/face_align.mars_model", "res/mouth.png", "res/blusher.png"}) {
            std::ifstream test(resourcePath + "/" + name, std::ios::binary);
            if (!test.good()) {
                *error = QStringLiteral("上游美颜库无法读取当前安装路径，请将程序放到英文目录");
                return false;
            }
        }
        Util::setResourceRoot(resourcePath);
        auto *context = GPUPixelContext::getInstance();
        d->ownsContext = true;
        context->useAsCurrent();
        if (!gladLoadGL() || !glGetString(GL_VERSION)) throw std::runtime_error("Cannot create OpenGL context");
        d->renderer = QString::fromLatin1(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &d->maxSize);
        d->source = SourceRawDataInput::create();
        d->beauty = BeautyFaceFilter::create();
        d->balance = DesktopWhiteBalanceFilter::createCompatible();
        d->reshape = FaceReshapeFilter::create();
        d->lipstick = LipstickFilter::create();
        d->blush = BlusherFilter::create();
        d->output = std::make_shared<CurrentFrameOutput>();
        if (!d->source || !d->beauty || !d->balance || !d->reshape || !d->lipstick || !d->blush)
            throw std::runtime_error("GPUPixel filter initialization failed");
        // SourceRawDataInput shadows Filter::_filterProgram in this SDK, so its
        // inherited getter is null. Its output is covered by the identity test.
        requireLinked(d->balance); requireLinked(d->reshape);
        requireLinked(d->lipstick); requireLinked(d->blush);
        d->beauty->setSharpen(0.f);
        d->initialized = true;
        return true;
    } catch (const std::exception &e) {
        *error = QString::fromUtf8(e.what());
        return false;
    }
}

BeautyResult GPUPixelPipeline::process(const QImage &input, const BeautyParameters &p) {
    if (!d->initialized) throw std::runtime_error("Beauty engine is not initialized");
    if (input.isNull()) throw std::runtime_error("Empty frame");
    if (input.width() > d->maxSize || input.height() > d->maxSize)
        throw std::runtime_error("Video exceeds GPU texture size; original resolution cannot be preserved");
    QElapsedTimer timer; timer.start();
    QImage rgba = input.convertToFormat(QImage::Format_RGBA8888);
    // Owned, tightly packed RGBA: the old SDK expects stride in pixels, not bytes.
    if (rgba.bytesPerLine() != rgba.width() * 4) rgba = rgba.copy();
    GPUPixelContext::getInstance()->useAsCurrent();
    d->landmarks.clear();
    if (p.needsFace()) {
        if (!d->detector) {
            d->detector = std::make_unique<FaceDetector>();
            d->detector->RegCallback([this](std::vector<float> points) { d->landmarks = std::move(points); });
        }
        auto small = rgba.scaled(QSize(640, 640), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        d->detector->Detect(small.constBits(), small.width(), small.height(), GPUPIXEL_MODE_FMT_VIDEO, GPUPIXEL_FRAME_TYPE_RGBA8888);
    }
    if (d->landmarks.size() < 222) d->landmarks.clear();
    d->reshape->SetFaceLandmarks(d->landmarks);
    d->lipstick->SetFaceLandmarks(d->landmarks);
    d->blush->SetFaceLandmarks(d->landmarks);
    d->beauty->setBlurAlpha(p.values[BeautyParameters::Smoothing]);
    d->beauty->setWhite(p.values[BeautyParameters::Whitening]);
    const float temperature = p.values[BeautyParameters::Temperature];
    d->balance->setTemperature(5000.f + temperature * (temperature < 0 ? 1000.f : 6000.f));
    d->balance->setTint(p.values[BeautyParameters::Tint] * 100.f);
    d->reshape->setFaceSlimLevel(p.values[BeautyParameters::Slimming] * .08f);
    d->reshape->setEyeZoomLevel(p.values[BeautyParameters::Eyes] * .15f);
    d->lipstick->setBlendLevel(p.values[BeautyParameters::Lipstick] * .5f);
    d->blush->setBlendLevel(p.values[BeautyParameters::Blush] * .5f);
    d->configureGraph(p);
    d->source->uploadBytes(rgba.constBits(), rgba.width(), rgba.height(), rgba.width());
    return {d->output->image, !d->landmarks.empty(), timer.nsecsElapsed() / 1000000.0};
}
