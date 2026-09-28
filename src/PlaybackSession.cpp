#include "PlaybackSession.h"
#include "BeautyVideoBridge.h"
#include "PlatformServices.h"
#include <QGuiApplication>
#include <QStandardPaths>
#include <QSettings>
#include <QFileInfo>
#include <QSet>
#include <cmath>

PlaybackSession::PlaybackSession(QMediaPlayer *player, BeautyVideoBridge *bridge, bool persistent, QObject *parent)
    : QObject(parent), m_player(player), m_bridge(bridge), m_persistent(persistent),
      m_history(persistent ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/playback.json" : QString()) {
    if (persistent) setHoldSpeed(QSettings().value("playback/holdSpeed", 3).toDouble());
    connect(bridge, &BeautyVideoBridge::aboutToOpen, this, [this] { endHold(); save(); m_opening = true; });
    connect(bridge, &BeautyVideoBridge::sourceOpened, this, [this] {
        m_opening = false;
        endHold(); m_resumePosition = m_history.position(m_player->source());
        m_restorePending = m_resumePosition > 0; m_notice.clear(); emit noticeChanged(); emit titleChanged();
        restore();
    });
    connect(player, &QMediaPlayer::durationChanged, this, &PlaybackSession::restore);
    connect(player, &QMediaPlayer::seekableChanged, this, &PlaybackSession::restore);
    connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) restore();
        if (status == QMediaPlayer::EndOfMedia) {
            endHold(); m_history.remember(m_player->source(), 0, m_player->duration(), true); m_history.flush();
        }
    });
    connect(player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        PlatformServices::keepScreenOn(state == QMediaPlayer::PlayingState);
        if (state != QMediaPlayer::PlayingState) { endHold(); save(); }
    });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) { endHold(); save(); }
#ifdef Q_OS_ANDROID
        // Release decoder pressure and stop audio while backgrounded. Resume
        // remains a user action, including after screen locking or a call.
        if (state == Qt::ApplicationSuspended || state == Qt::ApplicationHidden) m_player->pause();
#endif
    });
    connect(qGuiApp, &QCoreApplication::aboutToQuit, this, &PlaybackSession::save);
    m_saveTimer.setInterval(5000); connect(&m_saveTimer, &QTimer::timeout, this, &PlaybackSession::save); m_saveTimer.start();
}
PlaybackSession::~PlaybackSession() { endHold(); save(); }
void PlaybackSession::save() {
    // Loading a remembered source must not overwrite its bookmark with position 0.
    if (m_restorePending || m_opening) return;
    if (m_player->playbackState() == QMediaPlayer::StoppedState && m_player->position() == 0 && m_player->mediaStatus() != QMediaPlayer::EndOfMedia) return;
    m_history.remember(m_player->source(), m_player->position(), m_player->duration(), m_player->mediaStatus() == QMediaPlayer::EndOfMedia);
    m_history.flush();
}
void PlaybackSession::restore() {
    if (m_opening || !m_restorePending || !m_player->isSeekable() || m_player->duration() <= 0) return;
    // A replacement QIODevice loads asynchronously while the previous source's
    // duration/seekable values may still be visible. Do not consume the bookmark
    // until Qt confirms that the new media is loaded.
    if (m_player->mediaStatus() != QMediaPlayer::LoadedMedia
        && m_player->mediaStatus() != QMediaPlayer::BufferedMedia) return;
    m_restorePending = false;
    if (m_resumePosition >= m_player->duration()) return;
    m_bridge->seek(m_resumePosition);
    m_notice = QStringLiteral("已续播至 %1:%2").arg(m_resumePosition / 60000).arg((m_resumePosition / 1000) % 60, 2, 10, QLatin1Char('0'));
    emit noticeChanged();
}
QString PlaybackSession::title() const {
    const auto url = m_player->source();
    return url.isEmpty() ? QStringLiteral("映颜") : PlatformServices::mediaTitle(url);
}
void PlaybackSession::setHoldSpeed(double value) {
    if (!std::isfinite(value)) return;
    m_holdSpeed = std::clamp(value, 1.5, 5.0);
    if (m_holding) m_player->setPlaybackRate(m_holdSpeed);
    if (m_persistent) QSettings().setValue("playback/holdSpeed", m_holdSpeed);
    emit preferencesChanged();
}
void PlaybackSession::setNormalSpeed(double value) {
    if (!std::isfinite(value)) return;
    m_normalSpeed = std::clamp(value, .25, 3.0);
    if (!m_holding) m_player->setPlaybackRate(m_normalSpeed);
    emit preferencesChanged();
}
void PlaybackSession::beginHold() {
    if (m_holding || !m_player->isSeekable() || m_player->playbackState() != QMediaPlayer::PlayingState) return;
    m_normalSpeed = m_player->playbackRate(); m_holding = true;
    m_player->setPlaybackRate(m_holdSpeed); emit holdingChanged();
}
void PlaybackSession::endHold() {
    if (!m_holding) return;
    m_holding = false; m_player->setPlaybackRate(m_normalSpeed); emit holdingChanged();
}
void PlaybackSession::togglePlayback() {
    endHold();
    m_player->playbackState() == QMediaPlayer::PlayingState ? m_player->pause() : m_player->play();
}
void PlaybackSession::pauseForLibrary() {
    endHold(); m_player->pause(); save();
}
QUrl PlaybackSession::firstDroppedVideo(const QList<QUrl> &urls) const {
    static const QSet<QString> extensions{"mp4", "mkv", "mov", "avi", "webm", "m4v", "ts", "mts", "m2ts", "wmv", "flv", "mpg", "mpeg", "3gp"};
    for (const auto &url : urls) {
        const QFileInfo file(url.toLocalFile());
        if (url.isLocalFile() && file.isFile() && extensions.contains(file.suffix().toLower())) return url;
    }
    return {};
}
