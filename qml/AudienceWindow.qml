import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QtMultimedia
import MediaFlow 1.0

Window {
    id: audienceRoot
    width: 1920; height: 1080; visible: false
    title: qsTr("MediaFlow — Audience Display")
    color: "black"
    // This is a pure display output for the audience -- the operator never
    // types or clicks into it directly, so it should never hold or request
    // OS input focus. Without this, opening/closing another one of this
    // app's own windows (or even switching to a different application
    // entirely) can trigger a Win32 focus/activation event that briefly
    // stalls Qt Quick's shared render thread across every top-level window
    // in the process, including this one's continuously-playing video --
    // confirmed live as the stutter/black-frame the operator reported.
    // Frameless too, matching VirtualCameraWindow's pattern for the same
    // reason: no title bar needed for a full-screen audience display.
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    // =====================================================================
    //  DUAL-PLAYER A/B — mirrors the operator's Live monitor
    //  Audio output comes from HERE (the audience display)
    // =====================================================================

    property bool activeIsA: true

    // Audio is no longer owned here -- BroadcastEngine's own programAudioA/B
    // are now the single authoritative audio output for the whole app,
    // driven by BroadcastController's masterVolume/mixerMuted. This window
    // just displays the shared decoded video, same as Zoom and the
    // operator's own LIVE monitor.

    // Nothing currently live -- the same "no program asset" condition the
    // Image element below already keys off of, hoisted here so the
    // standby background can share it too.
    readonly property bool isStandby: {
        let a = (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.programAsset : null
        return !a || !a.absolutePath
    }

    // ── Standby background (image or looping video) ──
    // z:0, behind every program-content layer below, so it's automatically
    // covered the instant something goes live and reappears the instant
    // things go back to standby -- no extra show/hide logic needed here.
    Image {
        anchors.fill: parent; z: 0
        fillMode: Image.PreserveAspectCrop; asynchronous: true
        sourceSize.width: 1920
        sourceSize.height: 1080
        visible: audienceRoot.isStandby && (MediaFlowBackend || {}).extendedFeedBackgroundType === "image"
        source: visible ? "file:///" + (MediaFlowBackend || {}).extendedFeedBackgroundPath : ""
    }
    MediaPlayer {
        id: backgroundVideoPlayer
        loops: MediaPlayer.Infinite
        videoOutput: backgroundVideoOut
        audioOutput: AudioOutput { muted: true; volume: 0 } // decorative only, never audible
        source: {
            let bg = (MediaFlowBackend || {})
            return (bg.extendedFeedBackgroundType === "video" && bg.extendedFeedBackgroundPath)
                ? "file:///" + bg.extendedFeedBackgroundPath : ""
        }
        onSourceChanged: if (source.toString() !== "") play()
    }
    VideoOutput {
        id: backgroundVideoOut
        anchors.fill: parent; z: 0
        fillMode: VideoOutput.PreserveAspectCrop
        visible: audienceRoot.isStandby && (MediaFlowBackend || {}).extendedFeedBackgroundType === "video"
    }

    // ── Shared program sinks -- BroadcastEngine pushes every frame it
    //    decodes into these sinks (registered below), the same way it
    //    feeds the operator's LIVE monitor and Zoom. No local MediaPlayer
    //    here anymore; this window never decodes. ──
    VideoOutput {
        id: videoOutA; anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 1.0 : 0.0; visible: opacity > 0
        z: activeIsA ? 2 : 1
    }
    VideoOutput {
        id: videoOutB; anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 0.0 : 1.0; visible: opacity > 0
        z: activeIsA ? 1 : 2
    }

    // videoSink is read-only on VideoOutput in Qt6, so the shared decode is
    // fanned out by the engine pushing frames into each registered sink
    // instead of this window binding to the engine's sink directly -- see
    // BroadcastEngine::registerProgramOutputs().
    Component.onCompleted: {
        if (MediaFlowBackend && MediaFlowBackend.broadcastEngine) {
            MediaFlowBackend.broadcastEngine.registerProgramOutputs(videoOutA.videoSink, videoOutB.videoSink)
        }
    }

    // ── Image display -- imperatively driven by takeImageLive()/
    // cutImageLive() (called from onTakeExecuted/onCutExecuted below)
    // instead of a plain reactive binding, so Take fades over 2s like the
    // video crossfade instead of just snapping to the new image. ──
    Image {
        id: programImage
        anchors.fill: parent; z: 3
        fillMode: Image.PreserveAspectFit; asynchronous: true
        // Prevents shimmer/aliasing on fine detail (text, thin lines) when a
        // high-res source photo is minified to fit this window.
        mipmap: true
        sourceSize.width: 1920
        sourceSize.height: 1080
        visible: opacity > 0
        opacity: 0
        Behavior on opacity {
            id: imageFade
            NumberAnimation { duration: 1000; easing.type: Easing.InOutQuad }
        }
    }

    Timer {
        id: imageSwapTimer
        interval: 1000
        property string pendingSource: ""
        onTriggered: {
            programImage.source = pendingSource
            programImage.opacity = 1
        }
    }

    // Fades the current live image out, swaps to the new one, then fades it
    // in -- ~2s total, matching the video crossfade's duration.
    function takeImageLive(path) {
        if (programImage.opacity > 0) {
            programImage.opacity = 0
            imageSwapTimer.pendingSource = path
            imageSwapTimer.restart()
        } else {
            programImage.source = path
            programImage.opacity = 1
        }
    }

    // Cut is instant, unlike Take -- bypass the opacity Behavior entirely.
    function cutImageLive() {
        imageFade.enabled = false
        programImage.opacity = 0
        programImage.source = ""
        imageFade.enabled = true
    }

    // =====================================================================
    //  CUT (instant swap) -- purely visual. BroadcastEngine's cutLive()/
    //  takeLive() already drove the real shared program players before
    //  this fired; this just flips which VideoOutput is visible.
    // =====================================================================
    function executeCut() {
        let nextOut = activeIsA ? videoOutB : videoOutA
        let prevOut = activeIsA ? videoOutA : videoOutB
        nextOut.opacity = 1.0
        prevOut.opacity = 0.0
        activeIsA = !activeIsA
    }

    // If this window is (re-)shown while something is already live -- e.g.
    // the operator took media live before ever clicking "Extend Feed", or
    // after re-opening it -- show the side that matches whichever shared
    // sink BroadcastEngine is actually driving right now, instead of
    // restarting or guessing. No seek needed -- the shared decode is
    // already flowing; this window just needs to display the right half.
    function syncToCurrentProgram() {
        let be = (MediaFlowBackend || {}).broadcastEngine
        let a = be ? be.programAsset : null
        if (a && a.absolutePath && (a.type === "video" || a.type === "audio")) {
            activeIsA = be.programActiveIsA
        } else if (a && a.absolutePath && a.type === "image") {
            programImage.source = "file:///" + a.absolutePath
            programImage.opacity = 1
        }
    }
    onVisibleChanged: if (visible) syncToCurrentProgram()

    // =====================================================================
    //  TAKE (2s crossfade) -- purely visual, same reasoning as executeCut.
    // =====================================================================
    function executeTake() {
        crossfadeAnim.start()
    }

    ParallelAnimation {
        id: crossfadeAnim
        NumberAnimation {
            target: activeIsA ? videoOutB : videoOutA
            property: "opacity"; from: 0.0; to: 1.0; duration: 2000; easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: activeIsA ? videoOutA : videoOutB
            property: "opacity"; from: 1.0; to: 0.0; duration: 2000; easing.type: Easing.InOutQuad
        }
        // No local flip here: BroadcastEngine's own A/B flag is the single
        // source of truth (onProgramActiveIsAChanged below). A Cut mid-
        // crossfade cancels the engine's flip but not this animation, so a
        // local flip would desync and show the layer that gets no frames.
    }

    // =====================================================================
    //  ENGINE SYNC
    // =====================================================================
    Connections {
        target: (MediaFlowBackend || {}).broadcastEngine || null

        function onProgramActiveIsAChanged() {
            activeIsA = MediaFlowBackend.broadcastEngine.programActiveIsA
        }

        function onCutExecuted() {
            let a = MediaFlowBackend.broadcastEngine.programAsset
            if (a && a.absolutePath && (a.type === "video" || a.type === "audio"))
                executeCut()
            cutImageLive()
        }

        function onTakeExecuted() {
            let a = MediaFlowBackend.broadcastEngine.programAsset
            if (a && a.absolutePath && (a.type === "video" || a.type === "audio")) {
                executeTake()
            } else if (a && a.absolutePath && a.type === "image") {
                takeImageLive("file:///" + a.absolutePath)
            }
        }

        // No onIsProgramPausedChanged handler needed -- BroadcastEngine
        // pauses/resumes its own real program player directly now.
    }

    // =====================================================================
    //  TIMER OVERLAY (STAGE) — small corner overlay, unchanged. The
    //  full-screen timer mode now lives in its own dedicated TimerWindow.qml
    //  (see BroadcastController::setTimerFullScreenActive), not here, so it
    //  can target a different monitor than this Extended Feed window.
    // =====================================================================
    Rectangle {
        id: timerOverlay
        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 40
        width: 240; height: 100; radius: 16
        color: "#CC000000"
        // Traffic-light progress: green for the first half of the target
        // duration, yellow past the halfway point, red once time's up.
        border.color: {
            if (TimerBackend.state === TimerBackend.Overtime) return "#EF4444"
            if (TimerBackend.state === TimerBackend.Idle) return "#1AFFFFFF"
            let total = TimerBackend.targetDurationSeconds
            let frac = total > 0 ? TimerBackend.elapsedSeconds / total : 0
            return frac < 0.5 ? "#10B981" : "#F59E0B"
        }
        border.width: 2
        opacity: TimerBackend.isStaged ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 400 } }
        Behavior on border.color { ColorAnimation { duration: 300 } }

        ColumnLayout {
            anchors.centerIn: parent; spacing: 4
            Label {
                text: "REMAINING TIME"
                font.pixelSize: 10; font.bold: true; color: "#A1A1AA"; font.letterSpacing: 1
                Layout.alignment: Qt.AlignHCenter
            }
            Label {
                text: TimerBackend.displayTime
                font.pixelSize: 44; font.bold: true; font.family: "JetBrains Mono"
                color: {
                    if (TimerBackend.state === TimerBackend.Overtime) return "#EF4444"
                    if (TimerBackend.state === TimerBackend.Idle) return "white"
                    let total = TimerBackend.targetDurationSeconds
                    let frac = total > 0 ? TimerBackend.elapsedSeconds / total : 0
                    return frac < 0.5 ? "#10B981" : "#F59E0B"
                }
                Behavior on color { ColorAnimation { duration: 180 } }
                Layout.alignment: Qt.AlignHCenter
                
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: TimerBackend.state === TimerBackend.Overtime
                    NumberAnimation { to: 0.6; duration: 500; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 1.0; duration: 500; easing.type: Easing.InOutQuad }
                }
            }
        }
    }
}
