#include "ResumeStore.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>

namespace {
QString key(const QUrl &url) {
    return QString::fromLatin1(QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex());
}
QJsonObject identity(const QUrl &url) {
    if (!url.isLocalFile()) return {};
    QFileInfo f(url.toLocalFile());
    return {{"size", f.size()}, {"modified", f.lastModified().toMSecsSinceEpoch()}};
}
}
ResumeStore::ResumeStore(QString path) : m_path(std::move(path)) {
    if (m_path.isEmpty()) return;
    QFile file(m_path);
    if (file.open(QIODevice::ReadOnly)) m_entries = QJsonDocument::fromJson(file.readAll()).object();
}
qint64 ResumeStore::position(const QUrl &url) const {
    const auto entry = m_entries.value(key(url)).toObject();
    if (entry.value("identity").toObject() != identity(url)) return 0;
    return std::max(qint64(0), entry.value("position").toInteger());
}
void ResumeStore::remember(const QUrl &url, qint64 pos, qint64 duration, bool finished) {
    if (url.isEmpty() || duration <= 0) return;
    const auto nearEnd = std::min(qint64(5000), duration / 20);
    if (finished || pos >= duration - nearEnd) pos = 0;
    m_entries.insert(key(url), QJsonObject{{"position", std::clamp(pos, qint64(0), duration)},
        {"identity", identity(url)}, {"time", QDateTime::currentMSecsSinceEpoch()}});
    while (m_entries.size() > 200) {
        auto oldest = m_entries.begin();
        for (auto i = m_entries.begin(); i != m_entries.end(); ++i)
            if (i.value().toObject()["time"].toInteger() < oldest.value().toObject()["time"].toInteger()) oldest = i;
        m_entries.erase(oldest);
    }
}
void ResumeStore::flush() {
    if (m_path.isEmpty()) return;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(m_entries).toJson(QJsonDocument::Compact)); file.commit();
    }
}
