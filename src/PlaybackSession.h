#pragma once
#include "ResumeStore.h"
#include <QObject>
#include <QTimer>
#include <QMediaPlayer>
class BeautyVideoBridge;
class PlaybackSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(double holdSpeed READ holdSpeed WRITE setHoldSpeed NOTIFY preferencesChanged)
    Q_PROPERTY(bool holding READ holding NOTIFY holdingChanged)
    Q_PROPERTY(double normalSpeed READ normalSpeed WRITE setNormalSpeed NOTIFY preferencesChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
public:
    PlaybackSession(QMediaPlayer *player, BeautyVideoBridge *bridge, bool persistent, QObject *parent = nullptr);
    ~PlaybackSession() override;
    double holdSpeed() const { return m_holdSpeed; }
    void setHoldSpeed(double speed);
    double normalSpeed() const { return m_normalSpeed; }
    void setNormalSpeed(double speed);
    bool holding() const { return m_holding; }
    QString title() const;
    QString notice() const { return m_notice; }
    Q_INVOKABLE void beginHold();
    Q_INVOKABLE void endHold();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void pauseForLibrary();
    Q_INVOKABLE QUrl firstDroppedMedia(const QList<QUrl> &urls) const;
    Q_INVOKABLE void save();
signals:
    void preferencesChanged();
    void holdingChanged();
    void titleChanged();
    void noticeChanged();
private:
    void restore();
    QMediaPlayer *m_player;
    BeautyVideoBridge *m_bridge;
    bool m_persistent, m_holding = false, m_restorePending = false, m_opening = false;
    double m_holdSpeed = 3, m_normalSpeed = 1;
    qint64 m_resumePosition = 0;
    ResumeStore m_history;
    QTimer m_saveTimer;
    QString m_notice;
};
