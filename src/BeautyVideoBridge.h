#pragma once
#include "BeautySettings.h"
#include <QMediaPlayer>
#include <QVideoSink>
#include <QThread>
#include <QElapsedTimer>
#include <QPointer>
#include <QJsonObject>
#include <QVideoFrame>
#include <QImage>
#include <optional>

class BeautyWorker;
class BeautyVideoBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVideoSink* outputSink READ outputSink WRITE setOutputSink NOTIFY outputSinkChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statistics READ statistics NOTIFY statisticsChanged)
    Q_PROPERTY(bool compareOriginal READ compareOriginal WRITE setCompareOriginal NOTIFY compareOriginalChanged)
public:
    BeautyVideoBridge(QMediaPlayer *player, BeautySettings *settings, QObject *parent = nullptr);
    ~BeautyVideoBridge() override;
    QVideoSink *outputSink() const { return m_output; }
    void setOutputSink(QVideoSink *sink);
    QString status() const { return m_status; }
    QString statistics() const { return m_statistics; }
    bool compareOriginal() const { return m_compare; }
    void setCompareOriginal(bool value);
    Q_INVOKABLE void open(const QUrl &url);
    Q_INVOKABLE void seek(qint64 position);
    QJsonObject metrics() const;
    QImage lastImage() const;
    qint64 lastPresentedPts() const { return m_lastPts; }
signals:
    void aboutToOpen();
    void sourceOpened();
    void outputSinkChanged();
    void statusChanged();
    void statisticsChanged();
    void compareOriginalChanged();
    void framePresented(qint64 pts, bool face, double processingMs);
private:
    void invalidate();
    void acceptFrame(const QVideoFrame &frame);
    void dispatch();
    void reprocess();
    QMediaPlayer *m_player;
    BeautySettings *m_settings;
    QVideoSink m_input;
    QPointer<QVideoSink> m_output;
    QThread m_thread;
    QThread m_conversionThread;
    QObject *m_converter;
    BeautyWorker *m_worker;
    struct Prepared {
        QImage image;
        QVideoFrame frame;
        BeautyParameters parameters;
        quint64 epoch, revision;
        double conversionMs;
    };
    std::optional<Prepared> m_prepared;
    QVideoFrame m_pending, m_lastFrame;
    QImage m_lastImage;
    quint64 m_epoch = 0;
    quint64 m_revision = 0;
    bool m_busy = false, m_converting = false, m_compare = false, m_ready = false;
    QString m_status = QStringLiteral("正在初始化美颜…"), m_error, m_renderer, m_statistics;
    qint64 m_received = 0, m_processed = 0, m_dropped = 0, m_stale = 0, m_faceFrames = 0, m_lastPts = -1;
    double m_totalMs = 0, m_maxMs = 0;
    double m_conversionMs = 0, m_gpuMs = 0;
    qint64 m_bypassed = 0, m_hardwareFrames = 0;
    qint64 m_yuvConversions = 0, m_qtConversions = 0;
    QString m_inputFormat;
    QSize m_size;
    QVideoFrame m_originalPresented;
    qint64 m_lastPosition = 0;
    QElapsedTimer m_clock;
    QTimer m_statsTimer;
};
