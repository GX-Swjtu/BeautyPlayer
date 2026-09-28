#pragma once
#include "ThumbnailCache.h"
#include <QQuickAsyncImageProvider>
#include <QThreadPool>

class ThumbnailProvider : public QQuickAsyncImageProvider {
public:
    explicit ThumbnailProvider(const QString &cacheDirectory);
    ~ThumbnailProvider() override;
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
    QJsonObject statistics() const { return m_cache.statistics(); }
private:
    ThumbnailCache m_cache;
    QThreadPool m_pool;
};
