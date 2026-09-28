#pragma once
#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QUrl>
#include <QVariantList>
#include <QTimer>
#include <atomic>
#include <memory>

struct VideoEntry {
    QString name, folder;
    QUrl url;
    qint64 bytes = 0, modified = 0;
};
class VideoLibrary : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QStringList roots READ roots NOTIFY rootsChanged)
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY filterChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filterChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, UrlRole, FolderRole, SizeRole };
    explicit VideoLibrary(bool persistent = true, QObject *parent = nullptr);
    ~VideoLibrary() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool scanning() const { return m_watcher.isRunning(); }
    QString status() const { return m_status; }
    QVariantList folders() const;
    QStringList roots() const { return m_roots; }
    QString folder() const { return m_folder; }
    void setFolder(const QString &value);
    QString search() const { return m_search; }
    void setSearch(const QString &value);
    int count() const { return m_visible.size(); }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void requestAccess();
    Q_INVOKABLE void addFolder(const QUrl &url);
    Q_INVOKABLE void removeFolder(const QString &path);
signals:
    void statusChanged();
    void foldersChanged();
    void rootsChanged();
    void filterChanged();
    void countChanged();
private:
    void filter();
    void saveRoots();
    void cache(bool write);
    bool m_persistent, m_refreshAgain = false;
    QStringList m_roots;
    QString m_folder, m_search, m_status;
    QVector<VideoEntry> m_entries;
    QVector<int> m_visible;
    QFutureWatcher<QVector<VideoEntry>> m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QTimer m_permissionTimer;
};
