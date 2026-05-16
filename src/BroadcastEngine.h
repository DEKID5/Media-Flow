#pragma once

#include <QObject>
#include <QUrl>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include "MediaAsset.h"

/**
 * @brief The BroadcastEngine class manages the core A/V routing.
 * Maintains two independent QMediaPlayer pipelines:
 *   - Preview: Muted, local to operator. Does NOT auto-play.
 *   - Program: Full audio, routed to audience. Auto-plays on cut/take.
 */
class BroadcastEngine : public QObject
{
    Q_OBJECT

    // --- Asset State ---
    Q_PROPERTY(MediaAsset previewAsset READ previewAsset NOTIFY previewAssetChanged)
    Q_PROPERTY(MediaAsset programAsset READ programAsset NOTIFY programAssetChanged)
    Q_PROPERTY(bool programPaused READ isProgramPaused WRITE setProgramPaused NOTIFY isProgramPausedChanged)

    // --- Player Access (for QML VideoOutput binding) ---
    Q_PROPERTY(QMediaPlayer* previewPlayer READ previewPlayer CONSTANT)
    Q_PROPERTY(QMediaPlayer* programPlayer READ programPlayer CONSTANT)

public:
    explicit BroadcastEngine(QObject *parent = nullptr);

    MediaAsset previewAsset() const { return m_previewAsset; }
    MediaAsset programAsset() const { return m_programAsset; }
    bool isProgramPaused() const { return m_programPaused; }

    QMediaPlayer *previewPlayer() const { return m_previewPlayer; }
    QMediaPlayer *programPlayer() const { return m_programPlayer; }

    // Volume control — called by controller when masterVolume/mixerMuted changes
    void setProgramVolume(qreal volume);
    void setProgramMuted(bool muted);
    void setVirtualAudioRouting(bool enabled);

public slots:
    void setPreviewAsset(const MediaAsset &asset);
    void clearPreview();

    /**
     * @brief Instantly promotes the Preview asset to the Live (Program) feed.
     * Program player starts playback immediately. Preview remains loaded.
     */
    void cutLive();

    /**
     * @brief Transitions with a brief crossfade (reserved for future shader logic).
     * Currently identical to cutLive.
     */
    void takeLive();

    /**
     * @brief Clears the live feed — fades to standby/black.
     */
    void clearLive();

    void setProgramPaused(bool paused);
    void toggleProgramPause();

    /**
     * @brief Plays the preview player (operator presses play on preview).
     */
    void playPreview();
    void pausePreview();

signals:
    void previewAssetChanged();
    void programAssetChanged();
    void isProgramPausedChanged();
    void cutExecuted();
    void takeExecuted();

private:
    void loadPlayerSource(QMediaPlayer *player, const MediaAsset &asset);

    MediaAsset m_previewAsset;
    MediaAsset m_programAsset;
    bool m_programPaused = false;

    QMediaPlayer *m_previewPlayer;
    QMediaPlayer *m_programPlayer;
    QAudioOutput *m_previewAudio;
    QAudioOutput *m_programAudio;
};
