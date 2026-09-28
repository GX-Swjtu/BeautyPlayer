#include "BeautyVideoBridge.h"
#include "GPUPixelPipeline.h"
#include "FrameConversion.h"
#include <QCoreApplication>
#include <exception>
#include <utility>

class BeautyWorker : public QObject {
public:
    std::unique_ptr<GPUPixelPipeline> pipeline;
    QString error;
    void initialize() {
        pipeline = std::make_unique<GPUPixelPipeline>();
        pipeline->initialize(QCoreApplication::applicationDirPath(), &error);
    }
};

BeautyVideoBridge::BeautyVideoBridge(QMediaPlayer *player, BeautySettings *settings, QObject *parent)
    : QObject(parent), m_player(player), m_settings(settings), m_converter(new QObject), m_worker(new BeautyWorker) {
    m_player->setVideoSink(&m_input);
    m_worker->moveToThread(&m_thread); m_converter->moveToThread(&m_conversionThread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(&m_conversionThread, &QThread::finished, m_converter, &QObject::deleteLater);
    connect(&m_input, &QVideoSink::videoFrameChanged, this, &BeautyVideoBridge::acceptFrame);
    connect(&m_input, &QVideoSink::subtitleTextChanged, this, [this](const QString &text) { if (m_output) m_output->setSubtitleText(text); });
    connect(settings, &BeautySettings::changed, this, &BeautyVideoBridge::reprocess);
    connect(player, &QMediaPlayer::sourceChanged, this, [this] { invalidate(); m_lastPosition = 0; });
    connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (position + 500 < m_lastPosition) invalidate();
        m_lastPosition = position;
    });
    m_thread.setObjectName("BeautyGL"); m_thread.start();
    m_conversionThread.setObjectName("VideoConversion"); m_conversionThread.start();
    QMetaObject::invokeMethod(m_worker, [this] {
        m_worker->initialize();
        const auto error = m_worker->error;
        const auto renderer = m_worker->pipeline->renderer();
        QMetaObject::invokeMethod(this, [this, error, renderer] {
            m_error = error; m_renderer = renderer; m_ready = error.isEmpty();
            m_status = m_ready ? QStringLiteral("轻盈美颜已开启") : QStringLiteral("美颜不可用：%1 · 原图播放").arg(error);
            emit statusChanged(); reprocess();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    m_clock.start(); m_statsTimer.setInterval(1000);
    connect(&m_statsTimer, &QTimer::timeout, this, [this] {
        const auto count = m_processed - m_bypassed;
        m_statistics = QStringLiteral("%1 × %2 · 转换 %3 ms · 美颜 %4 ms · 丢帧 %5")
            .arg(m_size.width()).arg(m_size.height())
            .arg(count ? m_conversionMs / count : 0, 0, 'f', 1)
            .arg(count ? m_gpuMs / count : 0, 0, 'f', 1).arg(m_dropped);
        emit statisticsChanged();
    });
    m_statsTimer.start();
}
BeautyVideoBridge::~BeautyVideoBridge() {
    m_player->setVideoSink(nullptr);
    m_conversionThread.quit(); m_conversionThread.wait();
    m_thread.quit(); m_thread.wait();
}
QImage BeautyVideoBridge::lastImage() const {
    // Only diagnostics request CPU images; ordinary bypass never reads back.
    return m_originalPresented.isValid() ? normalizedFrameImage(m_originalPresented) : m_lastImage;
}
void BeautyVideoBridge::setOutputSink(QVideoSink *sink) {
    if (m_output == sink) return;
    m_output = sink; emit outputSinkChanged(); reprocess();
}
void BeautyVideoBridge::invalidate() {
    ++m_epoch; m_pending = {}; m_prepared.reset(); m_lastFrame = {}; m_lastImage = {}; m_originalPresented = {}; m_lastPts = -1;
    if (m_output) { m_output->setVideoFrame({}); m_output->setSubtitleText({}); }
}
void BeautyVideoBridge::open(const QUrl &url) {
    if (!url.isValid() || url.isEmpty()) return;
    emit aboutToOpen();
    m_player->stop(); invalidate(); m_player->setSource(url); emit sourceOpened(); m_player->play();
}
void BeautyVideoBridge::seek(qint64 position) {
    if (!m_player->isSeekable()) return;
    invalidate(); m_lastPosition = std::clamp(position, qint64(0), m_player->duration());
    m_player->setPosition(m_lastPosition);
}
void BeautyVideoBridge::setCompareOriginal(bool value) {
    if (m_compare == value) return;
    m_compare = value; emit compareOriginalChanged(); reprocess();
}
void BeautyVideoBridge::reprocess() {
    ++m_revision; m_prepared.reset();
    if (m_lastFrame.isValid()) { m_pending = m_lastFrame; dispatch(); }
}
void BeautyVideoBridge::acceptFrame(const QVideoFrame &frame) {
    if (!frame.isValid()) return;
    const auto format = frame.surfaceFormat();
    m_inputFormat = QStringLiteral("%1 space=%2 range=%3 transfer=%4")
        .arg(QVideoFrameFormat::pixelFormatToString(format.pixelFormat()))
        .arg(int(format.colorSpace())).arg(int(format.colorRange())).arg(int(format.colorTransfer()));
    ++m_received;
    if (frame.handleType() == QVideoFrame::RhiTextureHandle) ++m_hardwareFrames;
    if (m_pending.isValid()) ++m_dropped;
    m_lastFrame = frame; m_pending = frame; dispatch();
}
void BeautyVideoBridge::dispatch() {
    const auto parameters = m_settings->snapshot();
    if ((!parameters.active() || m_compare || !m_ready) && m_pending.isValid()) {
        // Preserve native textures, rotation and timing in original mode.
        auto frame = std::exchange(m_pending, {}); m_prepared.reset();
        m_lastImage = {}; m_originalPresented = frame; m_size = frame.size();
        m_lastPts = frame.startTime(); ++m_processed; ++m_bypassed;
        if (m_output) m_output->setVideoFrame(frame);
        if (m_error.isEmpty()) { m_status = QStringLiteral("原图"); emit statusChanged(); }
        emit framePresented(frame.startTime(), false, 0);
        return;
    }
    if (!m_busy && m_prepared) {
        auto job = std::move(*m_prepared); m_prepared.reset(); m_busy = true;
        QMetaObject::invokeMethod(m_worker, [this, job = std::move(job)]() mutable {
            BeautyResult result; QString error;
            QElapsedTimer timer; timer.start();
            try { result = m_worker->pipeline->process(job.image, job.parameters); }
            catch (const std::exception &e) { error = QString::fromUtf8(e.what()); result.image = job.image; }
            const auto gpuMs = timer.nsecsElapsed() / 1000000.0;
            QMetaObject::invokeMethod(this, [this, result = std::move(result), job = std::move(job), error, gpuMs] {
                m_busy = false;
                if (job.epoch != m_epoch || job.revision != m_revision) { ++m_stale; dispatch(); return; }
                if (!result.image.isNull()) {
                    const double ms = job.conversionMs + gpuMs;
                    ++m_processed; m_totalMs += ms; m_conversionMs += job.conversionMs; m_gpuMs += gpuMs;
                    m_maxMs = std::max(m_maxMs, ms); if (result.faceFound) ++m_faceFrames;
                    m_lastImage = result.image; m_originalPresented = {}; m_size = result.image.size();
                    m_lastPts = job.frame.startTime();
                    QVideoFrame output(result.image); output.setStartTime(m_lastPts); output.setEndTime(job.frame.endTime());
                    if (m_output) m_output->setVideoFrame(output);
                    emit framePresented(m_lastPts, result.faceFound, ms);
                }
                m_error = error;
                if (!error.isEmpty()) { m_ready = false; m_prepared.reset(); }
                const QString next = !error.isEmpty() ? QStringLiteral("美颜不可用：%1 · 原图播放").arg(error)
                    : result.faceFound ? QStringLiteral("五官与美妆已开启")
                    : job.parameters.needsFace() ? QStringLiteral("未发现人脸 · 基础美颜继续") : QStringLiteral("轻盈美颜已开启");
                if (m_status != next) { m_status = next; emit statusChanged(); }
                dispatch();
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }
    // One conversion, one prepared slot, one GPU job and only the newest input.
    // Conversion overlaps GL work without an unbounded event backlog.
    if (m_converting || m_prepared || !m_pending.isValid()) return;
    auto frame = std::exchange(m_pending, {});
    const auto epoch = m_epoch, revision = m_revision; m_converting = true;
    QMetaObject::invokeMethod(m_converter, [this, frame, parameters, epoch, revision] {
        QElapsedTimer timer; timer.start();
        bool usedYuv = false;
        auto image = normalizedFrameImage(frame, &usedYuv);
        const auto ms = timer.nsecsElapsed() / 1000000.0;
        QMetaObject::invokeMethod(this, [this, image = std::move(image), frame, parameters, epoch, revision, ms, usedYuv]() mutable {
            m_converting = false;
            if (usedYuv) ++m_yuvConversions; else ++m_qtConversions;
            if (epoch != m_epoch || revision != m_revision) ++m_stale;
            else if (image.isNull()) { m_error = QStringLiteral("视频帧无法转换为 RGBA"); m_status = m_error; emit statusChanged(); }
            else m_prepared = Prepared{std::move(image), frame, parameters, epoch, revision, ms};
            dispatch();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
QJsonObject BeautyVideoBridge::metrics() const {
    const auto count = m_processed - m_bypassed;
    return {{"ready", m_ready}, {"error", m_error}, {"renderer", m_renderer}, {"received", m_received},
        {"converter", frameConversionBackend()}, {"yuvConversions", m_yuvConversions},
        {"qtConversions", m_qtConversions}, {"inputFormat", m_inputFormat},
        {"presented", m_processed}, {"droppedPending", m_dropped}, {"discardedStale", m_stale}, {"faceFrames", m_faceFrames},
        {"bypassFrames", m_bypassed}, {"textureBackedInputFrames", m_hardwareFrames},
        {"meanConversionMs", count ? m_conversionMs / count : 0}, {"meanGpuStageMs", count ? m_gpuMs / count : 0},
        {"meanProcessingMs", count ? m_totalMs / count : 0}, {"maxProcessingMs", m_maxMs},
        {"width", m_size.width()}, {"height", m_size.height()}, {"wallSeconds", m_clock.elapsed() / 1000.0}};
}
