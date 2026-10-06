#include "BroadcastEngine.h"
#include <QDebug>
#include <QUrl>
#include <QMediaDevices>
#include <QAudioDevice>

BroadcastEngine::BroadcastEngine(QObject *parent)
    : QObject(parent)
{
    // Preview stays dormant -- QML still owns real preview playback
    // independently (out of scope for the shared-decode refactor).
    m_previewAudio = new QAudioOutput(this);
    m_previewAudio->setVolume(0);
    m_previewAudio->setMuted(true);
    m_previewPlayer = new QMediaPlayer(this);
    m_previewPlayer->setAudioOutput(m_previewAudio);

    // Program is a real A/B decoder pair -- every output window (LIVE
    // monitor, Extended Feed, Zoom) binds its own VideoOutput.videoSink
    // directly to programSinkA/B instead of decoding the same file itself.
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

    // VideoOutput.videoSink is read-only in Qt6 -- each output window's
    // VideoOutput owns its own sink, so "sharing" the decode means
    // re-pushing every frame the real players decode into every
    // registered window sink instead of pointing them at one sink object.
    connect(m_programSinkA, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        for (const QPointer<QVideoSink> &out : m_outputSinksA)
            if (out) out->setVideoFrame(frame);
    });
    connect(m_programSinkB, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        for (const QPointer<QVideoSink> &out : m_outputSinksB)
            if (out) out->setVideoFrame(frame);
    });

    // These durations mirror each output window's own crossfade (2000ms)
    // and fade-out (500ms) animations -- the engine only swaps/stops the
    // real players once the matching visual transition has had time to
    // actually cover the cut, so nothing pops or freezes mid-fade.
    m_takeFinishTimer = new QTimer(this);
    m_takeFinishTimer->setSingleShot(true);
    m_takeFinishTimer->setInterval(2000);
    connect(m_takeFinishTimer, &QTimer::timeout, this, &BroadcastEngine::finishTake);

    m_clearFinishTimer = new QTimer(this);
    m_clearFinishTimer->setSingleShot(true);
    m_clearFinishTimer->setInterval(500);
    connect(m_clearFinishTimer, &QTimer::timeout, this, &BroadcastEngine::finishClear);
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
    // A webcam/camera-input asset is preview-only -- Zoom's own fallback
    // (VirtualCameraWindow.qml, gated by webcamFallbackEnabled) is the one
    // and only place a webcam is meant to appear. Without this, tapping the
    // webcam card in Quick Fetch to preview it, then hitting Take Live,
    // would put a live camera feed onto Program and the operator's own LIVE
    // monitor -- never intended.
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

    // Images/other asset types have no real decode to drive here -- every
    // window's own image crossfade (takeImageLive()) handles those, same as
    // before this refactor.
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
