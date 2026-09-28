#include "VideoLibrary.h"
#include "PlatformServices.h"
#include "ThumbnailCache.h"
#include <QGuiApplication>
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
#include <QCollator>
#include <algorithm>

VideoLibrary::VideoLibrary(bool persistent, QObject *parent) : QAbstractListModel(parent), m_persistent(persistent) {
    if (persistent) {
        const auto defaults = QStringList{QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)};
        m_roots = QSettings().value("library/roots", defaults).toStringList();
        m_respectNoMedia = QSettings().value("library/respectNoMedia", true).toBool();
        m_twoColumns = QSettings().value("library/twoColumns", false).toBool();
        m_sortBy = QSettings().value("library/sortBy", "created").toString();
        if (m_sortBy != "name" && m_sortBy != "size" && m_sortBy != "created") m_sortBy = "created";
        m_descending = QSettings().value("library/descending", true).toBool();
        m_roots.removeAll(QString()); m_roots.removeDuplicates(); cache(false);
    }
    connect(&m_watcher, &QFutureWatcherBase::finished, this, [this] {
        if (!m_cancel->load()) {
            m_entries = m_watcher.result();
            if (!m_folder.isEmpty() && std::none_of(m_entries.cbegin(), m_entries.cend(), [this](const auto &entry) { return entry.folder == m_folder; })) {
                m_folder.clear(); emit filterChanged();
            }
            filter(); cache(true); emit foldersChanged();
        }
        emit statusChanged();
        if (std::exchange(m_refreshAgain, false)) refresh();
    });
#ifdef Q_OS_ANDROID
    m_roots = PlatformServices::videoFolders();
    // Re-query after permission changes in system settings, including Android
    // 14's selected-videos access. Do not persist inaccessible cached results.
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive && m_persistent) refresh();
    });
    m_permissionTimer.setInterval(500);
    connect(&m_permissionTimer, &QTimer::timeout, this, [this] {
        if (!PlatformServices::videoAccessPending()) { m_permissionTimer.stop(); m_roots = PlatformServices::videoFolders(); emit rootsChanged(); refresh(); }
    });
    if (persistent) QTimer::singleShot(200, this, &VideoLibrary::requestAccess);
#else
    if (persistent) QTimer::singleShot(200, this, &VideoLibrary::refresh);
#endif
}
VideoLibrary::~VideoLibrary() {
    if (m_cancel) m_cancel->store(true);
    if (scanning()) PlatformServices::cancelVideoScan();
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
    case CreatedRole: return QDateTime::fromMSecsSinceEpoch(e.created).toString("yyyy-MM-dd HH:mm");
    case ThumbnailRole: return "image://thumbnails/" + ThumbnailRequest{e.url, e.bytes, e.modified}.id();
    default: return {};
    }
}
QHash<int, QByteArray> VideoLibrary::roleNames() const {
    return {{NameRole, "videoName"}, {UrlRole, "mediaUrl"}, {FolderRole, "folderPath"}, {SizeRole, "sizeMB"},
        {CreatedRole, "createdText"}, {ThumbnailRole, "thumbnailUrl"}};
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
    QCollator collator; collator.setNumericMode(true); collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(m_visible.begin(), m_visible.end(), [this, &collator](int a, int b) {
        const auto &left = m_entries[a], &right = m_entries[b];
        int order = 0;
        if (m_sortBy == "name") order = collator.compare(left.name, right.name);
        else {
            const auto lv = m_sortBy == "size" ? left.bytes : left.created;
            const auto rv = m_sortBy == "size" ? right.bytes : right.created;
            order = lv == rv ? 0 : lv < rv ? -1 : 1;
        }
        if (order == 0) return left.url.toString() < right.url.toString();
        return m_descending ? order > 0 : order < 0;
    });
    endResetModel(); emit countChanged();
}
void VideoLibrary::savePreferences() {
    if (m_persistent) {
        QSettings settings;
        settings.setValue("library/respectNoMedia", m_respectNoMedia);
        settings.setValue("library/twoColumns", m_twoColumns);
        settings.setValue("library/sortBy", m_sortBy);
        settings.setValue("library/descending", m_descending);
    }
    emit preferencesChanged();
}
void VideoLibrary::setRespectNoMedia(bool value) {
    if (m_respectNoMedia == value) return;
    m_respectNoMedia = value; savePreferences();
    // Hide results from the old policy immediately, even while a scan is cancelled.
    m_entries.clear(); filter(); emit foldersChanged(); refresh();
}
void VideoLibrary::setTwoColumns(bool value) { if (m_twoColumns != value) { m_twoColumns = value; savePreferences(); } }
void VideoLibrary::setSortBy(const QString &value) {
    if (value == m_sortBy || (value != "name" && value != "size" && value != "created")) return;
    m_sortBy = value; savePreferences(); filter();
}
void VideoLibrary::setDescending(bool value) { if (m_descending != value) { m_descending = value; savePreferences(); filter(); } }
void VideoLibrary::chooseFolder() {
#ifdef Q_OS_ANDROID
    PlatformServices::chooseVideoFolder(); m_permissionTimer.start();
#endif
}
QString VideoLibrary::rootLabel(const QString &path) const {
    return path.startsWith("content:") ? PlatformServices::mediaTitle(QUrl(path)) : path;
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
void VideoLibrary::removeFolder(const QString &path) {
#ifdef Q_OS_ANDROID
    PlatformServices::removeVideoFolder(path);
#endif
    m_roots.removeAll(path); saveRoots(); setFolder({}); refresh();
}
void VideoLibrary::requestAccess() {
#ifdef Q_OS_ANDROID
    PlatformServices::requestVideoAccess();
    m_permissionTimer.start();
#endif
    refresh();
}
void VideoLibrary::refresh() {
    if (scanning()) { m_refreshAgain = true; m_cancel->store(true); PlatformServices::cancelVideoScan(); return; }
    if (m_cancel) m_cancel->store(true); // Invalidate queued status from the previous scan.
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = m_cancel; const auto roots = m_roots; const auto respectNoMedia = m_respectNoMedia;
    m_status = QStringLiteral("正在查找视频…");
    m_watcher.setFuture(QtConcurrent::run([this, roots, cancel, respectNoMedia] {
        QElapsedTimer timer; timer.start(); QVector<VideoEntry> entries;
#ifdef Q_OS_ANDROID
        const auto result = PlatformServices::scanAndroidVideos(respectNoMedia);
        for (const auto &value : result["videos"].toArray()) {
            if (cancel->load()) break;
            const auto entry = value.toObject();
            entries.append({entry["name"].toString(), entry["folder"].toString(), QUrl(entry["url"].toString()), entry["bytes"].toInteger(), entry["modified"].toInteger(), entry["created"].toInteger()});
        }
        const auto message = result.contains("error") ? result["error"].toString()
            : QStringLiteral("%1 个视频 · %2 ms%3").arg(entries.size()).arg(timer.elapsed())
                .arg(result["limited"].toBool() ? QStringLiteral(" · 仅获授权的视频") : QString());
#else
        const QSet<QString> extensions{"mp4", "mkv", "mov", "avi", "webm", "m4v", "ts", "mts", "m2ts", "wmv", "flv", "mpg", "mpeg", "3gp"};
        QSet<QString> seen, visited; int missing = 0;
        for (const auto &root : roots) {
            if (cancel->load()) break;
            if (!QFileInfo(root).isDir()) { ++missing; continue; }
            bool excluded = false;
            if (respectNoMedia) {
                QDir ancestor(root);
                do { if (QFileInfo(ancestor.filePath(".nomedia")).isFile()) { excluded = true; break; } } while (ancestor.cdUp());
            }
            if (excluded) continue;
            QStringList pending{root};
            while (!pending.isEmpty() && !cancel->load() && entries.size() < 100000) {
                const auto directory = pending.takeLast();
                const auto canonical = QFileInfo(directory).canonicalFilePath().toCaseFolded();
                if (visited.contains(canonical)) continue;
                visited.insert(canonical);
                if (respectNoMedia && QFileInfo(QDir(directory).filePath(".nomedia")).isFile()) continue;
                // Prune excluded subtrees before enumerating; thumbnails are separate, on demand.
                QDirIterator it(directory, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Readable);
                while (it.hasNext() && !cancel->load() && entries.size() < 100000) {
                it.next(); const auto f = it.fileInfo();
                if (f.isDir()) { pending.append(f.absoluteFilePath()); continue; }
                if (!extensions.contains(f.suffix().toLower())) continue;
                const auto path = f.absoluteFilePath();
#ifdef Q_OS_WIN
                const auto unique = path.toCaseFolded();
#else
                const auto unique = path;
#endif
                if (seen.contains(unique)) continue;
                seen.insert(unique);
                const auto created = f.birthTime().isValid() ? f.birthTime() : f.lastModified();
                entries.append({f.fileName(), f.absolutePath(), QUrl::fromLocalFile(path), f.size(), f.lastModified().toMSecsSinceEpoch(), created.toMSecsSinceEpoch()});
                }
            }
        }
        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
            return a.modified == b.modified ? a.url.toString() < b.url.toString() : a.modified > b.modified;
        });
        const auto message = QStringLiteral("%1 个视频 · %2 ms%3%4").arg(entries.size()).arg(timer.elapsed())
            .arg(missing ? QStringLiteral(" · %1 个目录不可访问").arg(missing) : QString())
            .arg(entries.size() == 100000 ? QStringLiteral(" · 已达 10 万条上限") : QString());
#endif
        QMetaObject::invokeMethod(this, [this, cancel, message] { if (!cancel->load()) { m_status = message; emit statusChanged(); } }, Qt::QueuedConnection);
        return entries;
    }));
    emit statusChanged();
}
void VideoLibrary::cache(bool write) {
#ifdef Q_OS_ANDROID
    // MediaStore is already an OS-maintained index; stale JSON can reveal names
    // of media for which the user has subsequently revoked access.
    Q_UNUSED(write); return;
#endif
    if (!m_persistent) return;
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation); QDir().mkpath(dir);
    const auto path = dir + "/videos-v2.json";
    if (write) {
        QJsonArray videos;
        for (const auto &e : m_entries) videos.append(QJsonObject{{"name", e.name}, {"folder", e.folder}, {"url", e.url.toString()}, {"bytes", e.bytes}, {"modified", e.modified}, {"created", e.created}});
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) { file.write(QJsonDocument(QJsonObject{{"roots", QJsonArray::fromStringList(m_roots)}, {"respectNoMedia", m_respectNoMedia}, {"videos", videos}}).toJson(QJsonDocument::Compact)); file.commit(); }
    } else {
        QFile file(path); if (!file.open(QIODevice::ReadOnly)) return;
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        if (object["roots"].toArray() != QJsonArray::fromStringList(m_roots) || object["respectNoMedia"].toBool() != m_respectNoMedia) return;
        for (const auto &value : object["videos"].toArray()) {
            const auto e = value.toObject();
            m_entries.append({e["name"].toString(), e["folder"].toString(), QUrl(e["url"].toString()), e["bytes"].toInteger(), e["modified"].toInteger(), e["created"].toInteger()});
        }
        filter(); m_status = QStringLiteral("%1 个已缓存视频").arg(m_entries.size());
    }
}
