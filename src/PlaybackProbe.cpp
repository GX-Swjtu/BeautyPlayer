#include "PlaybackProbe.h"
#include "BeautyVideoBridge.h"
#include "PlaybackSession.h"
#include "VideoLibrary.h"
#include <QQuickWindow>
#include <QAudioBufferOutput>
#include <QAudioBuffer>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QQuickItem>
#include <QMouseEvent>
#include <QThreadPool>
#include <cmath>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace {
void saveImageAsync(QImage image, QString path) {
    QThreadPool::globalInstance()->start([image = std::move(image), path = std::move(path)] { image.save(path); });
}
QQuickItem *findVisualItem(QQuickItem *root, const QString &name) {
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = findVisualItem(child, name)) return found;
    return nullptr;
}
}

PlaybackProbe::PlaybackProbe(QMediaPlayer *p, BeautySettings *s, BeautyVideoBridge *b, QQuickWindow *w,
                             const QString &report, int seconds, bool exercise, QString profile,
                             PlaybackSession *session, VideoLibrary *library, QObject *parent)
    : QObject(parent), m_player(p), m_settings(s), m_bridge(b), m_window(w), m_report(report), m_seconds(seconds), m_exercise(exercise), m_profile(std::move(profile)), m_session(session), m_library(library) {
    QDir().mkpath(QFileInfo(report).absolutePath());
    auto *audio = new QAudioBufferOutput(this);
    p->setAudioBufferOutput(audio);
    connect(p, &QMediaPlayer::sourceChanged, this, [this] { m_audioPts = -1; });
    connect(audio, &QAudioBufferOutput::audioBufferReceived, this, [this](const QAudioBuffer &buffer) {
        if (buffer.isValid()) m_audioPts = buffer.startTime();
    });
    p->setLoops(QMediaPlayer::Infinite);
    applyEffects();
    m_timer.setInterval(1000); m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PlaybackProbe::tick);
    m_clock.start(); m_timer.start();
}
void PlaybackProbe::applyEffects() {
    if (m_profile == "default") { m_settings->reset(); return; }
    if (m_profile == "original") { m_settings->setEnabled(false); return; }
    const float values[] = {.6f,.3f,.12f,.08f,.5f,.4f,.6f,.4f};
    for (int i = 0; i < BeautyParameters::Count; ++i) m_settings->setValue(BeautySettings::key(i), values[i]);
    m_settings->setEnabled(true);
}
void PlaybackProbe::check(const QString &name, bool passed) {
    m_checks.append(QJsonObject{{"name", name}, {"passed", passed}}); m_ok &= passed;
}
void PlaybackProbe::tick() {
    ++m_tick;
    auto metrics = m_bridge->metrics();
    const auto elapsed = m_clock.elapsed();
    const auto count = metrics["presented"].toInteger();
    metrics["second"] = elapsed / 1000.0;
    metrics["fpsInterval"] = (count - m_previousCount) * 1000.0 / std::max(qint64(1), elapsed - m_previousElapsed);
    metrics["positionMs"] = m_player->position();
    const bool hasAudioTime = m_player->hasAudio() && m_audioPts >= 0 && m_bridge->lastPresentedPts() >= 0;
    const double rawDifference = (m_audioPts - m_bridge->lastPresentedPts()) / 1000.0;
    metrics["rawAudioMinusVideoPtsMs"] = hasAudioTime ? QJsonValue(rawDifference) : QJsonValue();
    // Qt audio-buffer timestamps continue across loops while video timestamps
    // restart. Compare the same point in the media cycle, retaining the raw value.
    metrics["audioMinusVideoPtsMs"] = hasAudioTime ? QJsonValue(m_player->duration() > 0
        ? std::remainder(rawDifference, double(m_player->duration())) : rawDifference) : QJsonValue();
    metrics["playing"] = m_player->playbackState() == QMediaPlayer::PlayingState;
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb = sizeof(memory);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory), sizeof(memory))) {
        metrics["privateMB"] = memory.PrivateUsage / 1048576.0;
        metrics["workingSetMB"] = memory.WorkingSetSize / 1048576.0;
    }
#endif
#ifdef Q_OS_ANDROID
    QFile status("/proc/self/status");
    if (status.open(QIODevice::ReadOnly)) {
        for (const auto &line : status.readAll().split('\n'))
            if (line.startsWith("VmRSS:")) metrics["workingSetMB"] = line.simplified().split(' ').value(1).toDouble() / 1024.;
    }
#endif
    m_samples.append(metrics);
    m_previousCount = count; m_previousElapsed = elapsed;
    if (m_tick % 30 == 0) {
        QFile progress(m_report + "-progress.json");
        if (progress.open(QIODevice::WriteOnly)) progress.write(QJsonDocument(metrics).toJson());
    }
    if (m_tick == 2) m_source = m_player->source();
    if (m_tick == 3) {
        // PNG encoding a native 4K frame can block the GUI for seconds. Keep
        // diagnostic file I/O out of the playback timing path.
        auto image = m_bridge->lastImage();
        auto path = m_report + "-frame.png";
        QThreadPool::globalInstance()->start([image, path] { image.save(path); });
        if (m_window->isVisible()) {
            auto capture = m_window->grabWindow(); check("window_capture", !capture.isNull());
            saveImageAsync(capture, m_report + "-window.png");
        }
    }
    if (m_exercise) {
        switch (m_tick) {
        case 3: m_player->pause(); break;
        case 4: m_before = m_bridge->lastImage(); saveImageAsync(m_before, m_report + "-paused-before.png"); m_settings->setValue("whitening", 1); break;
        case 5: check("paused_parameter_refresh", !m_before.isNull() && m_before != m_bridge->lastImage()); saveImageAsync(m_bridge->lastImage(), m_report + "-paused-after.png"); m_bridge->setCompareOriginal(true); break;
        case 6: m_compare = m_bridge->lastImage(); saveImageAsync(m_compare, m_report + "-original.png"); m_bridge->setCompareOriginal(false); break;
        case 7: check("compare_restores_beauty", m_compare != m_bridge->lastImage()); for (int i = 0; i < BeautyParameters::Count; ++i) m_settings->setValue(BeautySettings::key(i), 0); break;
        case 8: check("reset_matches_original", !m_compare.isNull() && m_compare == m_bridge->lastImage()); applyEffects(); m_player->play(); break;
        case 10: m_bridge->seek(m_player->duration() / 2); break;
        case 11: m_bridge->seek(500); break;
        case 12: if (m_window->isVisible()) m_window->showFullScreen(); break;
        case 13: if (m_window->isVisible()) { check("fullscreen", m_window->visibility() == QWindow::FullScreen); m_window->showNormal(); } break;
        case 14: m_window->setProperty("panelOpen", false); break;
        case 15: {
            m_window->setProperty("panelOpen", true);
            if (m_window->isVisible()) {
                // Showing the panel schedules a layout polish. Wait for it before
                // targeting actual window coordinates; delegates have visual parents.
                QTimer::singleShot(150, this, [this] {
                auto *slider = findVisualItem(m_window->contentItem(), "smoothingSlider");
                if (slider) {
                    const auto before = m_settings->snapshot().values[0];
                    const auto point = slider->mapToScene(QPointF(slider->width() * .85, slider->height() * .5));
                    const auto global = m_window->mapToGlobal(point.toPoint());
                    QMouseEvent press(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(m_window, &press);
                    QMouseEvent release(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(m_window, &release);
                    check("qml_slider_mouse_input", std::abs(m_settings->snapshot().values[0] - before) > .05);
                } else check("qml_slider_mouse_input", false);
                });
            }
            break;
        }
        case 16: m_player->setPlaybackRate(1.5); break;
        case 18: m_player->setPlaybackRate(1); break;
        case 19: m_bridge->open(m_source); break;
        case 21: {
            const auto other = QFileInfo(m_source.toLocalFile()).dir().filePath("sync-1080.mp4");
            if (QFileInfo::exists(other)) m_bridge->open(QUrl::fromLocalFile(other));
            break;
        }
        case 23: m_bridge->open(m_source); break;
        case 24:
            m_window->setProperty("panelOpen", false);
            if (m_window->isVisible()) m_window->resize(390, 844);
            break;
        case 25: {
            if (!m_window->isVisible()) break;
            const auto point = QPointF(m_window->width() * .5, m_window->height() * .4);
            const auto global = m_window->mapToGlobal(point.toPoint());
            const auto speed = m_player->playbackRate();
            QMouseEvent press(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(m_window, &press);
            QTimer::singleShot(700, this, [this, point, global, speed] {
                check("long_press_3x", m_session->holding() && std::abs(m_player->playbackRate() - 3) < .01);
                QMouseEvent release(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(m_window, &release);
                check("release_restores_speed", !m_session->holding() && std::abs(m_player->playbackRate() - speed) < .01);
                saveImageAsync(m_window->grabWindow(), m_report + "-phone-player.png");
            });
            break;
        }
        case 26: {
            if (!m_window->isVisible()) break;
            const auto point = QPointF(m_window->width() * .65, m_window->height() * .4);
            const auto target = point - QPointF(m_window->width() * .25, 0);
            const auto before = m_player->position();
            QMouseEvent press(QEvent::MouseButtonPress, point, m_window->mapToGlobal(point.toPoint()), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(m_window, &press);
            QMouseEvent move(QEvent::MouseMove, target, m_window->mapToGlobal(target.toPoint()), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(m_window, &move);
            QMouseEvent release(QEvent::MouseButtonRelease, target, m_window->mapToGlobal(target.toPoint()), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(m_window, &release);
            QTimer::singleShot(200, this, [this, before] { check("swipe_seeks_back", m_player->position() < before - 500); });
            break;
        }
        case 27: m_player->pause(); m_bridge->seek(3000); break;
        case 28:
            m_session->save(); m_bridge->open(m_source);
            QTimer::singleShot(300, this, [this] {
                check("reopen_resumes_position", std::abs(m_player->position() - 3000) < 900);
                if (m_seconds <= 35) m_player->pause();
            });
            break;
        case 29:
#ifndef Q_OS_ANDROID
            // This probe uses app-private fixtures. MediaStore and permission
            // flows are validated separately, without indexing private media.
            if (m_window->isVisible() && !m_library->roots().isEmpty()) {
                check("library_scan_found_videos", m_library->count() > 0 && !m_library->scanning());
                m_window->setProperty("libraryOpen", true);
            }
#endif
            break;
        case 30:
            if (m_window->isVisible()) saveImageAsync(m_window->grabWindow(), m_report + "-phone-library.png");
            break;
        default:
            if (m_tick > 30 && m_tick % 45 == 0) m_bridge->seek(m_player->duration() / 3);
            break;
        }
    }
    if (m_tick >= m_seconds) {
        m_timer.stop();
        check("engine_ready", metrics["ready"].toBool());
        check("no_beauty_error", metrics["error"].toString().isEmpty());
        check("no_player_error", m_player->error() == QMediaPlayer::NoError);
        check("frames_presented", count > 10);
        if (m_window->isVisible()) m_window->grabWindow().save(m_report + "-final-window.png");
        QFile out(m_report + ".json");
        if (!out.open(QIODevice::WriteOnly)) { QCoreApplication::exit(2); return; }
        out.write(QJsonDocument(QJsonObject{{"passed", m_ok}, {"checks", m_checks}, {"samples", m_samples}, {"final", metrics}, {"source", m_source.toString()}}).toJson());
        out.close(); m_player->stop(); QCoreApplication::exit(m_ok ? 0 : 1);
    }
}
