#include "BroadcastEngine.h"
#include <QDebug>
#include <QUrl>
#include <QMediaDevices>
#include <QAudioDevice>

BroadcastEngine::BroadcastEngine(QObject *parent)
    : QObject(parent)
{
    // Preview is intentionally quiet; QML handles real preview playback.
    m_previewAudio = new QAudioOutput(this);
    m_previewAudio->setVolume(0);
    m_previewAudio->setMuted(true);
    m_previewPlayer = new QMediaPlayer(this);
    m_previewPlayer->setAudioOutput(m_previewAudio);

    // Use a real A/B program pipeline so each output window gets its own video sink.
    m_programSinkA = new QVideoSink(this);
    m_programSinkB = new QVideoSink(this);

    m_programAudioA = new QAudioOutput(this);
    m_programAudioA->setDevice(QMediaDevices::defaultAudioOutput());
    m_programAudioB = new QAudioOutput(this);
    m_programAudioB->setDevice(QMediaDevices::defaultAudioOutput());

    m_programPlayerA = new QMediaPlayer(this);
    m_programPlayerA->setVideoSink(m_programSinkA);
    m_programPlayerA->setAudioOutput(m_programAudioA);

    m_programPlayerB = new QMediaPlayer(this);
    m_programPlayerB->setVideoSink(m_programSinkB);
    m_programPlayerB->setAudioOutput(m_programAudioB);

    connect(m_programPlayerA, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia && m_programActiveIsA)
            emit programEndOfMedia();
    });
    connect(m_programPlayerB, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia && !m_programActiveIsA)
            emit programEndOfMedia();
    });

    // Each output window owns its own sink, so we fan out every frame to each viewer.
    connect(m_programSinkA, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        for (const QPointer<QVideoSink> &out : m_outputSinksA)
            if (out) pushFrame(out, frame);
    });
    connect(m_programSinkB, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        for (const QPointer<QVideoSink> &out : m_outputSinksB)
            if (out) pushFrame(out, frame);
    });

    // Match the UI transition timing so cuts feel smooth instead of abrupt.
    m_takeFinishTimer = new QTimer(this);
    m_takeFinishTimer->setSingleShot(true);
    m_takeFinishTimer->setInterval(2000);
    connect(m_takeFinishTimer, &QTimer::timeout, this, &BroadcastEngine::finishTake);

    m_clearFinishTimer = new QTimer(this);
    m_clearFinishTimer->setSingleShot(true);
    m_clearFinishTimer->setInterval(500);
    connect(m_clearFinishTimer, &QTimer::timeout, this, &BroadcastEngine::finishClear);
}

void BroadcastEngine::limitOutputFrameRate(QVideoSink *outputSink, int maxFps)
{
    if (!outputSink) return;
    if (maxFps > 0) m_minPushIntervalMs.insert(outputSink, 1000 / maxFps);
    else m_minPushIntervalMs.remove(outputSink);
}

void BroadcastEngine::pushFrame(QVideoSink *out, const QVideoFrame &frame)
{
    const auto it = m_minPushIntervalMs.constFind(out);
    if (it != m_minPushIntervalMs.constEnd()) {
        if (!m_pushClock.isValid()) m_pushClock.start();
        const qint64 now = m_pushClock.elapsed();
        qint64 &last = m_lastPushMs[out];
        // Add a small buffer so the frame cap does not visibly stutter.
        if (last != 0 && now - last < it.value() - 3) return;
        last = now ? now : 1;
    }
    out->setVideoFrame(frame);
}

void BroadcastEngine::registerProgramOutputs(QVideoSink *outputSinkA, QVideoSink *outputSinkB)
{
    if (outputSinkA && !m_outputSinksA.contains(outputSinkA))
        m_outputSinksA.append(outputSinkA);
    if (outputSinkB && !m_outputSinksB.contains(outputSinkB))
        m_outputSinksB.append(outputSinkB);
}

void BroadcastEngine::setPreviewAsset(const MediaAsset &asset)
{
    if (m_previewAsset.id == asset.id && m_previewAsset.absolutePath == asset.absolutePath)
        return;
    m_previewAsset = asset;
    qDebug() << "BroadcastEngine: Preview =" << m_previewAsset.name;
    emit previewAssetChanged();
}

void BroadcastEngine::clearPreview()
{
    m_previewAsset = MediaAsset();
    emit previewAssetChanged();
}

void BroadcastEngine::cutLive()
{
    qDebug() << "BroadcastEngine: CUT LIVE (Fade Stop)";
    clearLive();
}

void BroadcastEngine::takeLive()
{
    // Webcam assets stay preview-only; Zoom handles the live camera fallback.
    if (m_previewAsset.type == "input") {
        qDebug() << "BroadcastEngine: Cannot take webcam live — Zoom-only.";
        return;
    }
    if (m_previewAsset.absolutePath.isEmpty()) {
        qDebug() << "BroadcastEngine: Cannot take — no preview.";
        return;
    }
    m_programAsset = m_previewAsset;
    m_programPaused = false;
    qDebug() << "BroadcastEngine: TAKE LIVE →" << m_programAsset.name;
    emit programAssetChanged();
    emit isProgramPausedChanged();

    // Non-video items are handled by the UI’s own crossfade flow.
    if (m_programAsset.type == "video" || m_programAsset.type == "audio") {
        QMediaPlayer *next = m_programActiveIsA ? m_programPlayerB : m_programPlayerA;
        next->setSource(QUrl::fromLocalFile(m_programAsset.absolutePath));
        next->play();
        m_clearFinishTimer->stop();
        m_takeFinishTimer->start();
    }

    emit takeExecuted();
}

void BroadcastEngine::clearLive()
{
    m_programAsset = MediaAsset();
    m_programPaused = false;
    qDebug() << "BroadcastEngine: CLEAR LIVE → standby";
    emit programAssetChanged();
    emit isProgramPausedChanged();
    m_takeFinishTimer->stop();
    m_clearFinishTimer->start();
    emit cutExecuted();  // triggers fade-out in QML
}

void BroadcastEngine::finishTake()
{
    QMediaPlayer *prev = m_programActiveIsA ? m_programPlayerA : m_programPlayerB;
    prev->stop();
    prev->setSource(QUrl());
    m_programActiveIsA = !m_programActiveIsA;
    emit programActiveIsAChanged();
}

void BroadcastEngine::finishClear()
{
    m_programPlayerA->stop();
    m_programPlayerA->setSource(QUrl());
    m_programPlayerB->stop();
    m_programPlayerB->setSource(QUrl());
}

void BroadcastEngine::setProgramPaused(bool paused)
{
    if (m_programPaused == paused) return;
    m_programPaused = paused;
    QMediaPlayer *active = m_programActiveIsA ? m_programPlayerA : m_programPlayerB;
    if (paused) active->pause();
    else active->play();
    emit isProgramPausedChanged();
}

void BroadcastEngine::toggleProgramPause()
{
    setProgramPaused(!m_programPaused);
}

void BroadcastEngine::playPreview() {}
void BroadcastEngine::pausePreview() {}

void BroadcastEngine::setProgramVolume(int percent)
{
    qreal vol = qBound(0, percent, 100) / 100.0;
    m_programAudioA->setVolume(vol);
    m_programAudioB->setVolume(vol);
}

void BroadcastEngine::setProgramMuted(bool muted)
{
    m_programAudioA->setMuted(muted);
    m_programAudioB->setMuted(muted);
}
