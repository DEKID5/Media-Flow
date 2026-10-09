#pragma once

#include <QObject>
#include <QUrl>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QVideoFrame>
#include <QTimer>
#include <QPointer>
#include <QVector>
#include <QHash>
#include <QElapsedTimer>
#include "MediaAsset.h"

/**
 * @brief The BroadcastEngine class manages the core A/V routing.
 * Maintains:
 *   - Preview: Muted, local to operator. Does NOT auto-play. (dormant --
 *     QML still owns real preview playback independently.)
 *   - Program: a real A/B decoder pair, shared by every output window
 *     (operator LIVE monitor, Extended Feed, Zoom) -- the same video is
 *     decoded once instead of once per window. VideoOutput.videoSink is
 *     read-only in Qt6 (each VideoOutput owns its own sink), so sharing
 *     works by re-forwarding each decoded QVideoFrame from the engine's
 *     own master sink into every window's sink instead -- see
 *     registerProgramOutputs().
 */
class BroadcastEngine : public QObject
{
    Q_OBJECT

    // --- Asset State ---
    Q_PROPERTY(MediaAsset previewAsset READ previewAsset NOTIFY previewAssetChanged)
    Q_PROPERTY(MediaAsset programAsset READ programAsset NOTIFY programAssetChanged)
    Q_PROPERTY(bool programPaused READ isProgramPaused WRITE setProgramPaused NOTIFY isProgramPausedChanged)
    // Which of programPlayerA/programSinkA ("A") vs the B side is currently
    // showing live content. Flips only once a Take's crossfade finishes (or
    // immediately after a Cut) -- every output window reads this to know
    // which of its two VideoOutputs should be the visible one.
    Q_PROPERTY(bool programActiveIsA READ programActiveIsA NOTIFY programActiveIsAChanged)

    // --- Player Access (for QML, e.g. the play/pause icon's playbackState) ---
    Q_PROPERTY(QMediaPlayer* previewPlayer READ previewPlayer CONSTANT)
    Q_PROPERTY(QMediaPlayer* programPlayerA READ programPlayerA CONSTANT)
    Q_PROPERTY(QMediaPlayer* programPlayerB READ programPlayerB CONSTANT)

public:
    explicit BroadcastEngine(QObject *parent = nullptr);

    MediaAsset previewAsset() const { return m_previewAsset; }
    MediaAsset programAsset() const { return m_programAsset; }
    bool isProgramPaused() const { return m_programPaused; }
    bool programActiveIsA() const { return m_programActiveIsA; }

    QMediaPlayer *previewPlayer() const { return m_previewPlayer; }
    QMediaPlayer *programPlayerA() const { return m_programPlayerA; }
    QMediaPlayer *programPlayerB() const { return m_programPlayerB; }

    // Called once by each output window (LIVE monitor, Extended Feed, Zoom)
    // at Component.onCompleted, passing its own VideoOutput.videoSink
    // (readable, but not writable, from QML). Every decoded frame the
    // engine's real program players produce then gets pushed into all
    // registered sinks -- this is how one shared decode fans out to
    // however many windows are currently open.
    Q_INVOKABLE void registerProgramOutputs(QVideoSink *outputSinkA, QVideoSink *outputSinkB);

    // Caps how often decoded frames are pushed into one output sink. Used for
    // the Zoom capture window, which is only sampled at 30 fps anyway -- every
    // extra frame pushed into it is a full extra render of a 1080p window.
    Q_INVOKABLE void limitOutputFrameRate(QVideoSink *outputSink, int maxFps);

public slots:
    void setPreviewAsset(const MediaAsset &asset);
    void clearPreview();

    /**
     * @brief "Cut Live" button -- currently just an alias for clearLive()
     * (fades Program to standby/black). Named separately from clearLive()
     * because it's the one QML's Shortcut/button wiring calls directly.
     */
    void cutLive();

    /**
     * @brief Promotes the Preview asset to the Live (Program) feed with a
     * ~2s crossfade (matches every window's own crossfade animation).
     */
    void takeLive();

    /**
     * @brief Clears the live feed -- fades to standby/black. Same as
     * cutLive(); kept as a separate slot since EndOfMedia handling and
     * cutLive() both call it, and "clear" states intent more clearly there.
     */
    void clearLive();

    void setProgramPaused(bool paused);
    void toggleProgramPause();

    /**
     * @brief Plays the preview player (operator presses play on preview).
     */
    void playPreview();
    void pausePreview();

    // Pushed from BroadcastController's own masterVolume/mixerMuted
    // properties -- both program audio outputs always mirror the same
    // room-volume state (matching the previous single-audience-audio-
    // output design), so a single pair of setters covers both.
    void setProgramVolume(int percent);
    void setProgramMuted(bool muted);

signals:
    void previewAssetChanged();
    void programAssetChanged();
    void isProgramPausedChanged();
    void programActiveIsAChanged();
    void cutExecuted();
    void takeExecuted();
    // Fires when the currently-active program player reaches EndOfMedia --
    // lets BroadcastController advance a live playlist or clear to standby,
    // without any output window needing its own EndOfMedia handling.
    void programEndOfMedia();

private:
    void finishTake();   // stops the previous player once the crossfade completes
    void finishClear();  // stops both players once the fade-out completes

    MediaAsset m_previewAsset;
    MediaAsset m_programAsset;
    bool m_programPaused = false;
    bool m_programActiveIsA = true;

    QMediaPlayer *m_previewPlayer;
    QAudioOutput *m_previewAudio;

    QMediaPlayer *m_programPlayerA;
    QMediaPlayer *m_programPlayerB;
    QVideoSink *m_programSinkA;  // master sink each real player decodes into
    QVideoSink *m_programSinkB;
    QAudioOutput *m_programAudioA;
    QAudioOutput *m_programAudioB;

    // Every output window's own VideoOutput sink, registered via
    // registerProgramOutputs() -- QPointer so a closed window's destroyed
    // sink just drops out silently instead of needing explicit unregister.
    QVector<QPointer<QVideoSink>> m_outputSinksA;
    QVector<QPointer<QVideoSink>> m_outputSinksB;

    void pushFrame(QVideoSink *out, const QVideoFrame &frame);
    QHash<const QVideoSink *, qint64> m_minPushIntervalMs; // only sinks with a fps cap
    QHash<const QVideoSink *, qint64> m_lastPushMs;
    QElapsedTimer m_pushClock;

    // Matches the 2000ms ParallelAnimation crossfade / 500ms fade-out
    // duration every output window already animates on its own side --
    // keeping these in lockstep is what lets each window's purely-visual
    // opacity animation line up with when the engine actually swaps players.
    QTimer *m_takeFinishTimer;
    QTimer *m_clearFinishTimer;
};
