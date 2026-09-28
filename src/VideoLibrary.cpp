#include "VideoLibrary.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include <algorithm>

VideoLibrary::VideoLibrary(bool persistent, QObject *parent) : QAbstractListModel(parent), m_persistent(persistent) {
    if (persistent) {
        const auto defaults = QStringList{QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)};
        m_roots = QSettings().value("library/roots", defaults).toStringList();
        m_roots.removeAll(QString()); m_roots.removeDuplicates(); cache(false);
    }
    connect(&m_watcher, &QFutureWatcherBase::finished, this, [this] {
        if (!m_cancel->load()) {
            m_entries = m_watcher.result(); filter(); cache(true); emit foldersChanged();
        }
        emit statusChanged();
        if (std::exchange(m_refreshAgain, false)) refresh();
    });
    if (persistent) QTimer::singleShot(200, this, &VideoLibrary::refresh);
}
VideoLibrary::~VideoLibrary() {
    if (m_cancel) m_cancel->store(true);
    m_watcher.waitForFinished();
}
int VideoLibrary::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_visible.size(); }
QVariant VideoLibrary::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size()) return {};
    const auto &e = m_entries[m_visible[index.row()]];
    switch (role) {
    case NameRole: return e.name;
    case UrlRole: return e.url;
    case FolderRole: return e.folder;
    case SizeRole: return e.bytes / 1048576.0;
    default: return {};
    }
}
QHash<int, QByteArray> VideoLibrary::roleNames() const {
    return {{NameRole, "videoName"}, {UrlRole, "mediaUrl"}, {FolderRole, "folderPath"}, {SizeRole, "sizeMB"}};
}
QVariantList VideoLibrary::folders() const {
    QMap<QString, int> counts;
    for (const auto &e : m_entries) ++counts[e.folder];
    QVariantList result;
    result.append(QVariantMap{{"name", QStringLiteral("全部视频")}, {"path", QString()}, {"count", m_entries.size()}});
    for (auto i = counts.begin(); i != counts.end(); ++i)
        result.append(QVariantMap{{"name", QDir(i.key()).dirName()}, {"path", i.key()}, {"count", i.value()}});
    return result;
}
void VideoLibrary::filter() {
    beginResetModel(); m_visible.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        const auto &e = m_entries[i];
        if ((m_folder.isEmpty() || e.folder == m_folder) && (m_search.isEmpty() || e.name.contains(m_search, Qt::CaseInsensitive))) m_visible.append(i);
    }
    endResetModel(); emit countChanged();
}
void VideoLibrary::setFolder(const QString &value) { if (value != m_folder) { m_folder = value; filter(); emit filterChanged(); } }
void VideoLibrary::setSearch(const QString &value) { if (value != m_search) { m_search = value; filter(); emit filterChanged(); } }
void VideoLibrary::saveRoots() { if (m_persistent) QSettings().setValue("library/roots", m_roots); emit rootsChanged(); }
void VideoLibrary::addFolder(const QUrl &url) {
    if (!url.isLocalFile()) { m_status = QStringLiteral("请选择本地文件夹"); emit statusChanged(); return; }
    QFileInfo info(url.toLocalFile());
    const auto path = info.canonicalFilePath();
    if (!info.isDir() || path.isEmpty() || m_roots.contains(path, Qt::CaseInsensitive)) return;
    m_roots.append(path); saveRoots(); refresh();
}
void VideoLibrary::removeFolder(const QString &path) { m_roots.removeAll(path); saveRoots(); setFolder({}); refresh(); }
void VideoLibrary::refresh() {
    if (scanning()) { m_refreshAgain = true; m_cancel->store(true); return; }
    if (m_cancel) m_cancel->store(true); // Invalidate queued status from the previous scan.
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = m_cancel; const auto roots = m_roots;
    m_status = QStringLiteral("正在查找视频…");
    m_watcher.setFuture(QtConcurrent::run([this, roots, cancel] {
        QElapsedTimer timer; timer.start(); QVector<VideoEntry> entries;
        const QSet<QString> extensions{"mp4", "mkv", "mov", "avi", "webm", "m4v", "ts", "mts", "m2ts", "wmv", "flv", "mpg", "mpeg", "3gp"};
        QSet<QString> seen; int missing = 0;
        for (const auto &root : roots) {
            if (cancel->load()) break;
            if (!QFileInfo(root).isDir()) { ++missing; continue; }
            // Metadata enumeration only: never open/parse a video or extract thumbnails.
            QDirIterator it(root, QDir::Files | QDir::NoSymLinks | QDir::Readable, QDirIterator::Subdirectories);
            while (it.hasNext() && !cancel->load() && entries.size() < 100000) {
                it.next(); const auto f = it.fileInfo();
                if (!extensions.contains(f.suffix().toLower())) continue;
                const auto path = f.absoluteFilePath();
#ifdef Q_OS_WIN
                const auto unique = path.toCaseFolded();
#else
                const auto unique = path;
#endif
                if (seen.contains(unique)) continue;
                seen.insert(unique);
                entries.append({f.fileName(), f.absolutePath(), QUrl::fromLocalFile(path), f.size(), f.lastModified().toMSecsSinceEpoch()});
            }
        }
        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
            return a.modified == b.modified ? a.url.toString() < b.url.toString() : a.modified > b.modified;
        });
        const auto message = QStringLiteral("%1 个视频 · %2 ms%3%4").arg(entries.size()).arg(timer.elapsed())
            .arg(missing ? QStringLiteral(" · %1 个目录不可访问").arg(missing) : QString())
            .arg(entries.size() == 100000 ? QStringLiteral(" · 已达 10 万条上限") : QString());
        QMetaObject::invokeMethod(this, [this, cancel, message] { if (!cancel->load()) { m_status = message; emit statusChanged(); } }, Qt::QueuedConnection);
        return entries;
    }));
    emit statusChanged();
}
void VideoLibrary::cache(bool write) {
    if (!m_persistent) return;
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation); QDir().mkpath(dir);
    const auto path = dir + "/videos-v1.json";
    if (write) {
        QJsonArray videos;
        for (const auto &e : m_entries) videos.append(QJsonObject{{"name", e.name}, {"folder", e.folder}, {"url", e.url.toString()}, {"bytes", e.bytes}, {"modified", e.modified}});
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) { file.write(QJsonDocument(QJsonObject{{"roots", QJsonArray::fromStringList(m_roots)}, {"videos", videos}}).toJson(QJsonDocument::Compact)); file.commit(); }
    } else {
        QFile file(path); if (!file.open(QIODevice::ReadOnly)) return;
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        if (object["roots"].toArray() != QJsonArray::fromStringList(m_roots)) return;
        for (const auto &value : object["videos"].toArray()) {
            const auto e = value.toObject();
            m_entries.append({e["name"].toString(), e["folder"].toString(), QUrl(e["url"].toString()), e["bytes"].toInteger(), e["modified"].toInteger()});
        }
        filter(); m_status = QStringLiteral("%1 个已缓存视频").arg(m_entries.size());
    }
}
