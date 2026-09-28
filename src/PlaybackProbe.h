#pragma once
#include <QObject>
#include <QTimer>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QImage>
#include <QUrl>
class QMediaPlayer;
class QQuickWindow;
class BeautySettings;
class BeautyVideoBridge;
class QAudioBufferOutput;
class PlaybackSession;
class VideoLibrary;

// Opt-in diagnostics exercising the same application path as normal playback.
class PlaybackProbe : public QObject {
public:
    PlaybackProbe(QMediaPlayer *player, BeautySettings *settings, BeautyVideoBridge *bridge,
                  QQuickWindow *window, const QString &report, int seconds, bool exercise, QString profile,
                  PlaybackSession *session, VideoLibrary *library, QObject *parent = nullptr);
private:
    void tick();
    void check(const QString &name, bool passed);
    void applyEffects();
    bool clickItem(const QString &name, qreal horizontal = .5);
    void checkBeautyLayout(const QString &captureName);
    QMediaPlayer *m_player;
    BeautySettings *m_settings;
    BeautyVideoBridge *m_bridge;
    QQuickWindow *m_window;
    QString m_report;
    int m_seconds, m_tick = 0;
    bool m_exercise;
    QString m_profile;
    PlaybackSession *m_session;
    VideoLibrary *m_library;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QJsonArray m_samples, m_checks;
    QImage m_before, m_compare;
    QUrl m_source;
    qint64 m_audioPts = -1, m_previousCount = 0;
    qint64 m_previousElapsed = 0;
    qint64 m_libraryPausePosition = 0;
    bool m_ok = true;
};
