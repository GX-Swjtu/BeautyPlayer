#include "GPUPixelPipeline.h"
#include "AndroidBeautyResources.h"
#include "gpupixel/gpupixel.h"
#include "core/gpupixel_context.h"
#include "mars_vision/mars_face_landmarker.h"
#include <libyuv/scale_argb.h>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <array>
#include <cmath>
#include <stdexcept>

using namespace gpupixel;
namespace {
// Upload directly to a persistent input texture, eliminating the SDK's
// per-frame glTexImage2D allocation and an extra pass-through shader draw.
class RgbaSource final : public Source {
public:
    void upload(const QImage &image) {
        if (!framebuffer_ || framebuffer_->GetWidth() != image.width() || framebuffer_->GetHeight() != image.height())
            SetFramebuffer(std::make_shared<GPUPixelFramebuffer>(image.width(), image.height()));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, framebuffer_->GetTexture());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, image.bytesPerLine() / 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image.width(), image.height(), GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        DoRender(true);
    }
};
class CurrentFrameOutput final : public Sink {
public:
    QImage image;
    std::array<QImage, 3> pool;
    void Render() override {
        const auto fb = input_framebuffers_.at(0).frame_buffer;
        image = {};
        for (auto &slot : pool) {
            if (!slot.isNull() && !slot.isDetached()) continue;
            if (slot.size() != QSize(fb->GetWidth(), fb->GetHeight()))
                slot = QImage(fb->GetWidth(), fb->GetHeight(), QImage::Format_RGBA8888);
            image.swap(slot); break;
        }
        if (image.isNull()) image = QImage(fb->GetWidth(), fb->GetHeight(), QImage::Format_RGBA8888);
        if (image.isNull()) throw std::runtime_error("Cannot allocate beauty output");
        fb->Activate(); glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, image.width(), image.height(), GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        fb->Deactivate();
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL ES beauty processing failed");
        for (auto &slot : pool) if (slot.isNull()) { slot = image; break; }
    }
};
void requireLinked(const std::shared_ptr<Filter> &filter) {
    if (!filter || !filter->GetGlProgram()) throw std::runtime_error("Missing beauty shader");
    GLint linked = GL_FALSE;
    glGetProgramiv(filter->GetGlProgram()->GetProgram(), GL_LINK_STATUS, &linked);
    if (!linked) throw std::runtime_error("OpenGL ES beauty shader link failed");
}
}
struct GPUPixelPipeline::Impl {
    bool initialized = false, ownsContext = false;
    GLint maxSize = 0;
    QString renderer, resourceRoot;
    std::shared_ptr<RgbaSource> source;
    std::shared_ptr<BeautyFaceFilter> beauty;
    std::shared_ptr<WhiteBalanceFilter> balance;
    std::shared_ptr<FaceReshapeFilter> reshape;
    std::shared_ptr<LipstickFilter> lipstick;
    std::shared_ptr<BlusherFilter> blush;
    std::shared_ptr<CurrentFrameOutput> output;
    std::shared_ptr<mars_vision::MarsFaceLandmarker> detector;
    QImage detectionImage;
    qint64 timestamp = 0;
    int graphMask = -1;
    ~Impl() {
        if (!ownsContext) return;
        source.reset(); lipstick.reset(); blush.reset(); reshape.reset(); beauty.reset(); balance.reset(); output.reset(); detector.reset();
        GPUPixelContext::Destroy();
    }
    void configureGraph(const BeautyParameters &p) {
        int mask = 0;
        for (int i = 0; i < BeautyParameters::Count; ++i) if (std::abs(p.values[i]) > .0001f) mask |= 1 << i;
        if (mask == graphMask) return;
        source->RemoveAllSinks(); lipstick->RemoveAllSinks(); blush->RemoveAllSinks();
        reshape->RemoveAllSinks(); beauty->RemoveAllSinks(); balance->RemoveAllSinks();
        std::shared_ptr<Source> tail = source;
        const auto append = [&tail](const auto &filter) { tail->AddSink(filter); tail = filter; };
        if (p.values[BeautyParameters::Lipstick] > 0) append(lipstick);
        if (p.values[BeautyParameters::Blush] > 0) append(blush);
        if (p.values[BeautyParameters::Slimming] > 0 || p.values[BeautyParameters::Eyes] > 0) append(reshape);
        if (p.values[BeautyParameters::Smoothing] > 0 || p.values[BeautyParameters::Whitening] > 0) append(beauty);
        if (p.values[BeautyParameters::Temperature] != 0 || p.values[BeautyParameters::Tint] != 0) append(balance);
        tail->AddSink(output); graphMask = mask;
    }
};
GPUPixelPipeline::GPUPixelPipeline() : d(std::make_unique<Impl>()) {}
GPUPixelPipeline::~GPUPixelPipeline() = default;
QString GPUPixelPipeline::renderer() const { return d->renderer; }
bool GPUPixelPipeline::initialize(const QString &root, QString *error) {
    if (d->initialized) return true;
    try {
        for (const auto *name : androidBeautyResources) {
            const auto path = QDir(root).filePath(QString::fromLatin1(name));
            if (!QFileInfo::exists(path))
                throw std::runtime_error(std::string("Missing Android beauty resource: ") + name);
            if (path.endsWith(".png") && QImage(path).isNull())
                throw std::runtime_error(std::string("Invalid Android beauty resource: ") + name);
        }
        d->resourceRoot = root;
        GPUPixel::SetResourcePath(root.toStdString());
        auto *context = GPUPixelContext::GetInstance(); d->ownsContext = true; context->UseAsCurrent();
        if (!glGetString(GL_VERSION)) throw std::runtime_error("OpenGL ES unavailable");
        d->renderer = QString::fromLatin1(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &d->maxSize);
        d->source = std::make_shared<RgbaSource>(); d->beauty = BeautyFaceFilter::Create();
        d->balance = WhiteBalanceFilter::Create(); d->reshape = FaceReshapeFilter::Create();
        d->lipstick = LipstickFilter::Create(); d->blush = BlusherFilter::Create();
        d->output = std::make_shared<CurrentFrameOutput>();
        if (!d->beauty) throw std::runtime_error("Cannot initialize GPUPixel beauty");
        requireLinked(d->balance); requireLinked(d->reshape); requireLinked(d->lipstick); requireLinked(d->blush);
        d->beauty->SetSharpen(0); d->initialized = true; return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}
BeautyResult GPUPixelPipeline::process(const QImage &input, const BeautyParameters &p) {
    if (!d->initialized || input.isNull()) throw std::runtime_error("Beauty pipeline not ready");
    if (input.width() > d->maxSize || input.height() > d->maxSize) throw std::runtime_error("Video exceeds GPU texture limit");
    QElapsedTimer timer; timer.start();
    auto rgba = input.convertToFormat(QImage::Format_RGBA8888);
    GPUPixelContext::GetInstance()->UseAsCurrent();
    std::vector<float> landmarks;
    if (p.needsFace()) {
        if (!d->detector) {
            auto detector = mars_vision::MarsFaceLandmarker::Create();
            mars_vision::FaceLandmarkerOptions options;
            options.model_path = (d->resourceRoot + "/models").toStdString();
            // IMAGE avoids carrying tracking state across seeks/source switches.
            options.running_mode = mars_vision::RunningMode::IMAGE; options.num_faces = 1;
            if (!detector || detector->Init(options) < 0) throw std::runtime_error("Mars face model initialization failed");
            d->detector = std::move(detector);
        }
        auto size = rgba.size();
        if (size.width() > 640 || size.height() > 640) size.scale(640, 640, Qt::KeepAspectRatio);
        if (d->detectionImage.size() != size) d->detectionImage = QImage(size, QImage::Format_RGBA8888);
        if (d->detectionImage.isNull()) throw std::runtime_error("Cannot allocate detection frame");
        // libyuv scales all four channels independently; RGBA is valid here.
        if (libyuv::ARGBScale(rgba.constBits(), rgba.bytesPerLine(), rgba.width(), rgba.height(),
            d->detectionImage.bits(), d->detectionImage.bytesPerLine(), size.width(), size.height(), libyuv::kFilterBilinear))
            throw std::runtime_error("NEON detection resize failed");
        mars_vision::MarsImage image{};
        image.data = d->detectionImage.bits(); image.width = size.width(); image.height = size.height();
        image.stride = d->detectionImage.bytesPerLine(); image.format = mars_vision::MarsImageFormat::RGBA;
        image.rotate_type = mars_vision::RotateType::CLOCKWISE_0; image.timestamp = ++d->timestamp;
        std::vector<mars_vision::FaceLandmarkerResult> faces;
        if (d->detector->Detect(image, faces) < 0) throw std::runtime_error("Mars face detection failed");
        if (!faces.empty() && faces.front().key_points.size() == 111)
            for (const auto &point : faces.front().key_points) { landmarks.push_back(point.x / size.width()); landmarks.push_back(point.y / size.height()); }
    }
    d->reshape->SetFaceLandmarks(landmarks); d->lipstick->SetFaceLandmarks(landmarks); d->blush->SetFaceLandmarks(landmarks);
    d->beauty->SetBlurAlpha(p.values[BeautyParameters::Smoothing]); d->beauty->SetWhite(p.values[BeautyParameters::Whitening]);
    const float temperature = p.values[BeautyParameters::Temperature];
    d->balance->setTemperature(5000.f + temperature * (temperature < 0 ? 1000.f : 6000.f));
    d->balance->setTint(p.values[BeautyParameters::Tint] * 100.f);
    d->reshape->SetFaceSlimLevel(p.values[BeautyParameters::Slimming] * .08f);
    d->reshape->SetEyeZoomLevel(p.values[BeautyParameters::Eyes] * .15f);
    d->lipstick->SetBlendLevel(p.values[BeautyParameters::Lipstick] * .5f); d->blush->SetBlendLevel(p.values[BeautyParameters::Blush] * .5f);
    d->configureGraph(p); d->source->upload(rgba);
    return {d->output->image, !landmarks.empty(), timer.nsecsElapsed() / 1000000.0};
}
