#pragma once
#include <QJsonObject>
#include <QString>
#include <QUrl>

// Bounded local history. Atomic replacement keeps a interrupted save recoverable.
class ResumeStore {
public:
    explicit ResumeStore(QString path = {});
    qint64 position(const QUrl &url) const;
    void remember(const QUrl &url, qint64 position, qint64 duration, bool finished = false);
    void flush();
private:
    QString m_path;
    QJsonObject m_entries;
};
