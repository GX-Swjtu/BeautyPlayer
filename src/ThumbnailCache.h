// Copyright (c) 2026 GX-Swjtu. SPDX-License-Identifier: MIT
#pragma once
#include <QUrl>
#include <QImage>
#include <QJsonObject>
#include <QMutex>
#include <functional>
#include <atomic>

struct ThumbnailRequest {
    QUrl url;
    qint64 bytes = 0, modified = 0;
    QString id() const;
    QString key() const;
    static ThumbnailRequest fromId(const QString &id);
    bool valid() const;
};

class ThumbnailCache {
public:
    using Generator = std::function<QImage(const QUrl &)>;
    explicit ThumbnailCache(QString directory);
    QImage load(const ThumbnailRequest &request, const Generator &generate);
    // Only our hash-named cache files are eligible. User media is never removed.
    void prune(qint64 nowMs = 0, qint64 maxBytes = 256 * 1024 * 1024, int maxAgeDays = 30);
    QJsonObject statistics() const;
    QString directory() const { return m_directory; }
private:
    QString m_directory;
    QMutex m_lock;
    qint64 m_lastCleanup = 0, m_lastCleanupGeneration = 0;
    std::atomic<qint64> m_hits{0}, m_generated{0}, m_failed{0}, m_suppressed{0};
};
