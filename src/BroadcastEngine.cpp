#include "BroadcastEngine.h"
#include <QDebug>
#include <QUrl>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioDevice>

BroadcastEngine::BroadcastEngine(QObject *parent)
    : QObject(parent)
{
    // Engine players are dormant — actual playback is in QML
    m_previewAudio = new QAudioOutput(this);
    m_previewAudio->setVolume(0);
    m_previewAudio->setMuted(true);
    m_previewPlayer = new QMediaPlayer(this);
    m_previewPlayer->setAudioOutput(m_previewAudio);

    m_programAudio = new QAudioOutput(this);
    m_programAudio->setVolume(0);
    m_programAudio->setMuted(true);
    m_programPlayer = new QMediaPlayer(this);
    m_programPlayer->setAudioOutput(m_programAudio);
}

void BroadcastEngine::loadPlayerSource(QMediaPlayer *, const MediaAsset &)
{
    // No-op: QML handles playback
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
    if (m_previewAsset.absolutePath.isEmpty() && m_previewAsset.type != "input") {
        qDebug() << "BroadcastEngine: Cannot take — no preview.";
        return;
    }
    m_programAsset = m_previewAsset;
    m_programPaused = false;
    qDebug() << "BroadcastEngine: TAKE LIVE →" << m_programAsset.name;
    emit programAssetChanged();
    emit isProgramPausedChanged();
    emit takeExecuted();
}

void BroadcastEngine::clearLive()
{
    m_programAsset = MediaAsset();
    m_programPaused = false;
    qDebug() << "BroadcastEngine: CLEAR LIVE → standby";
    emit programAssetChanged();
    emit isProgramPausedChanged();
    emit cutExecuted();  // triggers fade-out in QML
}

void BroadcastEngine::setProgramPaused(bool paused)
{
    if (m_programPaused == paused) return;
    m_programPaused = paused;
    emit isProgramPausedChanged();
}

void BroadcastEngine::toggleProgramPause()
{
    setProgramPaused(!m_programPaused);
}

void BroadcastEngine::playPreview() {}
void BroadcastEngine::pausePreview() {}

void BroadcastEngine::setProgramVolume(qreal v) { Q_UNUSED(v); }
void BroadcastEngine::setProgramMuted(bool m) { Q_UNUSED(m); }

void BroadcastEngine::setVirtualAudioRouting(bool enabled)
{
    if (enabled) {
        const auto devices = QMediaDevices::audioOutputs();
        for (const auto &dev : devices) {
            QString name = dev.description().toLower();
            if (name.contains("cable") || name.contains("blackhole") || name.contains("virtual")) {
                m_programAudio->setDevice(dev);
                qDebug() << "BroadcastEngine: Audio routed to Virtual Cable ->" << dev.description();
                return;
            }
        }
        qDebug() << "BroadcastEngine: Virtual Audio Cable not found! Falling back to default.";
    } else {
        m_programAudio->setDevice(QMediaDevices::defaultAudioOutput());
        qDebug() << "BroadcastEngine: Audio routed to default device.";
    }
}
