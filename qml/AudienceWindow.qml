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
    // =====================================================================
    //  DUAL-PLAYER A/B — mirrors the operator's Live monitor
    //  Audio output comes from HERE (the audience display)
    // =====================================================================

    property bool activeIsA: true

    // Audience window is the single authoritative audio output for the whole
    // app — the operator's monitors are always muted (see MonitorView.qml) —
    // so room volume/mute are wired in here.
    readonly property real roomVolume: {
        let mf = MediaFlowBackend || {}
        return (mf.mixerMuted ? 0 : 1) * ((mf.masterVolume !== undefined ? mf.masterVolume : 100) / 100.0)
    }

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

    // ── Player A ──
    MediaPlayer {
        id: playerA
        videoOutput: videoOutA
        audioOutput: AudioOutput { id: audioA; volume: audienceRoot.roomVolume; device: (MediaFlowBackend || {}).roomAudioOutputDevice }
    }
    VideoOutput {
        id: videoOutA; anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 1.0 : 0.0; visible: opacity > 0
        z: activeIsA ? 2 : 1
    }

    // ── Player B ──
    MediaPlayer {
        id: playerB
        videoOutput: videoOutB
        audioOutput: AudioOutput { id: audioB; volume: audienceRoot.roomVolume; device: (MediaFlowBackend || {}).roomAudioOutputDevice }
    }
    VideoOutput {
        id: videoOutB; anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 0.0 : 1.0; visible: opacity > 0
        z: activeIsA ? 1 : 2
    }

    // ── Image display ──
    Image {
        anchors.fill: parent; z: 3
        fillMode: Image.PreserveAspectFit; asynchronous: true
        visible: {
            let a = (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.programAsset : null;
            return a && a.type === "image" && a.absolutePath;
        }
        source: {
            let a = (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.programAsset : null;
            return a && a.absolutePath ? "file:///" + a.absolutePath : "";
        }
    }

    // =====================================================================
    //  CUT (instant swap)
    // =====================================================================
    function executeCut(url, type) {
        let next = activeIsA ? playerB : playerA
        let prev = activeIsA ? playerA : playerB
        let nextOut = activeIsA ? videoOutB : videoOutA
        let prevOut = activeIsA ? videoOutA : videoOutB

        if (type === "video" || type === "audio") {
            next.source = url
            next.play()
        }

        nextOut.opacity = 1.0
        prevOut.opacity = 0.0
        activeIsA = !activeIsA

        prev.stop()
        prev.source = ""
    }

    // If a video/audio is already live when this window is (re-)shown --
    // e.g. the operator took media live before ever clicking "Extend Feed",
    // or after re-opening it -- start playing immediately instead of
    // sitting black/silent until the next Cut/Take. (Starts from the
    // beginning rather than the operator monitor's exact position, which
    // isn't currently tracked centrally -- close enough to avoid dead air.)
    function syncToCurrentProgram() {
        let a = (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.programAsset : null
        if (a && a.absolutePath && (a.type === "video" || a.type === "audio")) {
            executeCut("file:///" + a.absolutePath, a.type)
        }
    }
    onVisibleChanged: if (visible) syncToCurrentProgram()

    // =====================================================================
    //  TAKE (500ms crossfade)
    // =====================================================================
    function executeTake(url, type) {
        let next = activeIsA ? playerB : playerA
        if (type === "video" || type === "audio") {
            next.source = url
            next.play()
        }
        crossfadeAnim.start()
    }

    ParallelAnimation {
        id: crossfadeAnim
        NumberAnimation {
            target: activeIsA ? videoOutB : videoOutA
            property: "opacity"; from: 0.0; to: 1.0; duration: 300; easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: activeIsA ? videoOutA : videoOutB
            property: "opacity"; from: 1.0; to: 0.0; duration: 300; easing.type: Easing.InOutQuad
        }
        onFinished: {
            let prev = activeIsA ? playerA : playerB
            prev.stop(); prev.source = ""
            activeIsA = !activeIsA
        }
    }

    // =====================================================================
    //  ENGINE SYNC
    // =====================================================================
    Connections {
        target: (MediaFlowBackend || {}).broadcastEngine || null

        function onCutExecuted() {
            let a = MediaFlowBackend.broadcastEngine.programAsset
            if (a && a.absolutePath && (a.type === "video" || a.type === "audio"))
                executeCut("file:///" + a.absolutePath, a.type)
            else {
                playerA.stop(); playerA.source = ""
                playerB.stop(); playerB.source = ""
            }
        }

        function onTakeExecuted() {
            let a = MediaFlowBackend.broadcastEngine.programAsset
            if (a && a.absolutePath && (a.type === "video" || a.type === "audio")) {
                executeTake("file:///" + a.absolutePath, a.type)
            } else {
                // Taking live to an image (or camera input) — stop any video/audio
                // that was previously live so its sound doesn't keep playing under it.
                playerA.stop(); playerA.source = ""
                playerB.stop(); playerB.source = ""
            }
        }

        function onIsProgramPausedChanged() {
            let ap = activeIsA ? playerA : playerB
            if (MediaFlowBackend.broadcastEngine.programPaused) ap.pause()
            else ap.play()
        }
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
        border.color: {
            if (TimerBackend.state === TimerBackend.Overtime) return "#EF4444"
            if (TimerBackend.state === TimerBackend.Paused) return "#F59E0B"
            return "#1AFFFFFF"
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
                color: (TimerBackend.state === TimerBackend.Overtime) ? "#EF4444" : "white"
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
