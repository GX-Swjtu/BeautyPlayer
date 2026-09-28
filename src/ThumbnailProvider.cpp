#include "ThumbnailProvider.h"
#include "PlatformServices.h"
#include <QQuickTextureFactory>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#endif

namespace {
class Response : public QQuickImageResponse {
public:
    QImage image;
    std::atomic_bool cancelled{false};
    void cancel() override { cancelled.store(true); }
    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(image); }
    void finish(QImage result) { image = std::move(result); emit finished(); }
};
#ifdef Q_OS_WIN
QImage shellThumbnail(const QUrl &url) {
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(initialized)) return {};
    IShellItemImageFactory *factory = nullptr; QImage image;
    const auto path = QDir::toNativeSeparators(url.toLocalFile()).toStdWString();
    if (SUCCEEDED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&factory)))) {
        HBITMAP bitmap = nullptr;
        if (SUCCEEDED(factory->GetImage({512, 512}, SIIGBF_THUMBNAILONLY, &bitmap))) {
            BITMAP info{}; GetObject(bitmap, sizeof(info), &info);
            if (info.bmWidth > 0 && info.bmHeight > 0 && info.bmWidth <= 8192 && info.bmHeight <= 8192) {
                image = QImage(info.bmWidth, info.bmHeight, QImage::Format_RGB32);
                BITMAPINFO format{}; format.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                format.bmiHeader.biWidth = info.bmWidth; format.bmiHeader.biHeight = -info.bmHeight;
                format.bmiHeader.biPlanes = 1; format.bmiHeader.biBitCount = 32; format.bmiHeader.biCompression = BI_RGB;
                auto dc = GetDC(nullptr);
                if (!GetDIBits(dc, bitmap, 0, info.bmHeight, image.bits(), &format, DIB_RGB_COLORS)) image = {};
                ReleaseDC(nullptr, dc);
            }
            DeleteObject(bitmap);
        }
        factory->Release();
    }
    CoUninitialize(); return image;
}
QImage decodedThumbnail(const QUrl &url) {
    // Fallback for codecs without a Shell thumbnail handler. One silent decoder,
    // on the provider worker, seeking beyond opening titles/black first frames.
    QEventLoop loop; QMediaPlayer player; QVideoSink sink; QTimer timeout;
    QImage image; qint64 target = 0; bool started = false;
    player.setVideoSink(&sink); timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop, &QEventLoop::quit);
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia && !started) {
            started = true; target = std::min(std::max(qint64(0), player.duration() - 100), std::min(qint64(10000), player.duration() / 10) + 137);
            player.setPosition(target); player.play();
        }
    });
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &loop, [&](const QVideoFrame &frame) {
        if (frame.isValid() && (frame.startTime() < 0 || frame.startTime() / 1000 >= target - 100)) {
            image = frame.toImage(); if (!image.isNull()) loop.quit();
        }
    });
    timeout.start(8000); player.setSource(url); loop.exec(); player.stop(); player.setVideoSink(nullptr);
    return image;
}
bool blankThumbnail(const QImage &image) {
    if (image.isNull()) return true;
    int minimum = 255, maximum = 0;
    for (int y = 1; y < 9; ++y) for (int x = 1; x < 9; ++x) {
        const auto c = image.pixelColor(x * image.width() / 10, y * image.height() / 10);
        const int light = (c.red() + c.green() + c.blue()) / 3;
        minimum = std::min(minimum, light); maximum = std::max(maximum, light);
    }
    return maximum < 8 || minimum > 247;
}
#endif
QImage generate(const QUrl &url) {
#ifdef Q_OS_ANDROID
    return PlatformServices::videoThumbnail(url);
#else
    auto image = shellThumbnail(url);
    if (blankThumbnail(image)) {
        auto fallback = decodedThumbnail(url);
        if (!fallback.isNull()) return fallback;
    }
    return image;
#endif
}
}
ThumbnailProvider::ThumbnailProvider(const QString &directory) : m_cache(directory) {
    m_pool.setMaxThreadCount(1); m_pool.setExpiryTimeout(15000);
    m_pool.start([this] { m_cache.prune(); });
}
ThumbnailProvider::~ThumbnailProvider() { m_pool.waitForDone(); }
QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &) {
    auto *response = new Response;
    m_pool.start([this, response, request = ThumbnailRequest::fromId(id)] {
        auto image = response->cancelled.load() ? QImage() : m_cache.load(request, generate);
        // Always finish after requestImageResponse returns, including a fast
        // cancellation/cache hit, so the engine has connected its listener.
        QMetaObject::invokeMethod(response, [response, image = std::move(image)]() mutable {
            response->finish(response->cancelled.load() ? QImage() : std::move(image));
        }, Qt::QueuedConnection);
    });
    return response;
}
