#include "ThumbnailCache.h"
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QDateTime>
#include <QRegularExpression>
#include <algorithm>

QString ThumbnailRequest::id() const {
    return QString::fromLatin1(QJsonDocument(QJsonObject{{"url", url.toString(QUrl::FullyEncoded)},
        {"bytes", bytes}, {"modified", modified}, {"version", 2}}).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
QString ThumbnailRequest::key() const {
    return QString::fromLatin1(QCryptographicHash::hash(id().toUtf8(), QCryptographicHash::Sha256).toHex());
}
ThumbnailRequest ThumbnailRequest::fromId(const QString &id) {
    if (id.size() > 32768) return {};
    const auto o = QJsonDocument::fromJson(QByteArray::fromBase64(id.toLatin1(), QByteArray::Base64UrlEncoding)).object();
    if (o["version"].toInt() != 2) return {};
    return {QUrl(o["url"].toString()), o["bytes"].toInteger(), o["modified"].toInteger()};
}
bool ThumbnailRequest::valid() const { return url.isValid() && !url.isEmpty() && (url.isLocalFile() || url.scheme() == "content"); }
ThumbnailCache::ThumbnailCache(QString directory) : m_directory(std::move(directory)) { QDir().mkpath(m_directory); }
QImage ThumbnailCache::load(const ThumbnailRequest &request, const Generator &generate) {
    if (!request.valid()) return {};
    // A single provider worker normally serializes this; the lock also protects tests/other clients.
    QMutexLocker lock(&m_lock);
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastCleanup > 3600000 || m_generated.load() - m_lastCleanupGeneration >= 32) {
        lock.unlock(); prune(now); lock.relock(); m_lastCleanup = now; m_lastCleanupGeneration = m_generated.load();
    }
    const auto base = QDir(m_directory).filePath(request.key());
    QImage cached(base + ".jpg");
    if (!cached.isNull()) {
        ++m_hits;
        // Access recency, not video age. Avoid a metadata write for every repaint.
        QFile file(base + ".jpg");
        if (now - QFileInfo(file).lastModified().toMSecsSinceEpoch() > 3600000 && file.open(QIODevice::ReadWrite))
            file.setFileTime(QDateTime::fromMSecsSinceEpoch(now), QFileDevice::FileModificationTime);
        return cached;
    }
    const QFileInfo failure(base + ".failed");
    if (failure.exists() && now - failure.lastModified().toMSecsSinceEpoch() < 30 * 60000) { ++m_suppressed; return {}; }
    auto image = generate(request.url);
    if (image.isNull()) {
        ++m_failed; QSaveFile failed(base + ".failed");
        if (failed.open(QIODevice::WriteOnly)) { failed.write("retry after 30 minutes"); failed.commit(); }
        return {};
    }
    ++m_generated;
    if (image.width() > 512 || image.height() > 512) image = image.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QSaveFile file(base + ".jpg");
    if (file.open(QIODevice::WriteOnly) && image.save(&file, "JPEG", 80)) file.commit();
    QFile::remove(base + ".failed");
    return image;
}
void ThumbnailCache::prune(qint64 nowMs, qint64 maxBytes, int maxAgeDays) {
    QMutexLocker lock(&m_lock);
    if (!nowMs) nowMs = QDateTime::currentMSecsSinceEpoch();
    const QRegularExpression owned("^[0-9a-f]{64}\\.(jpg|failed)$");
    auto files = QDir(m_directory).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Time | QDir::Reversed);
    qint64 bytes = 0; int count = 0;
    for (const auto &file : files) if (owned.match(file.fileName()).hasMatch()) { bytes += file.size(); ++count; }
    for (const auto &file : files) {
        if (!owned.match(file.fileName()).hasMatch()) continue;
        const auto ageLimit = file.suffix() == "failed" ? qint64(30 * 60000) : qint64(maxAgeDays) * 86400000;
        if (nowMs - file.lastModified().toMSecsSinceEpoch() > ageLimit || bytes > maxBytes || count > 4096) {
            if (QFile::remove(file.absoluteFilePath())) { bytes -= file.size(); --count; }
        }
    }
}
QJsonObject ThumbnailCache::statistics() const {
    return {{"diskHits", m_hits.load()}, {"generated", m_generated.load()}, {"failed", m_failed.load()}, {"retrySuppressed", m_suppressed.load()}};
}
