import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import MediaFlow 1.0

Rectangle {
    id: monitor

    property string title: "MONITOR"
    property bool isLive: false
    property var asset: null
    property string mediaType: asset ? (asset.type || "") : ""
    property var cameraDevice: null
    property bool showTransitions: false

    signal takeClicked()
    signal cutClicked()

    color: "#0A0A0A"
    radius: 12
    border.color: isLive ? "#EF4444" : "#1AFFFFFF"
    border.width: isLive ? 2 : 1
    clip: true

    // =====================================================================
    //  DUAL-PLAYER A/B ARCHITECTURE
    //  Two players alternate. Only one is "active" at a time.
    //  CUT  = instant swap (0ms opacity)
    //  TAKE = crossfade (500ms opacity)
    // =====================================================================

    property bool activeIsA: true  // which player is currently showing

    // The audience window is the preferred single audio channel, but it only
    // exists once "Extend Feed" has actually been used to open it. Without
    // that fallback, taking media live before ever extending the feed (no
    // second display, or just not clicked yet) produced no audio anywhere.
    // So: this (LIVE) monitor is the audio source whenever the audience
    // window isn't the one carrying it -- never both, to keep it to one channel.
    readonly property bool isAudioSource: isLive && !((MediaFlowBackend || {}).feedExtended)
    readonly property real roomVolume: {
        let mf = MediaFlowBackend || {}
        return (mf.mixerMuted ? 0 : 1) * ((mf.masterVolume !== undefined ? mf.masterVolume : 100) / 100.0)
    }

    // ── Camera Background ──
    CaptureSession {
        id: monitorCameraSession
        camera: Camera {
            id: monitorCamera
            cameraDevice: monitor.cameraDevice
            active: !monitor.asset || !monitor.asset.absolutePath || monitor.asset.type === "input"
        }
        videoOutput: monitorCameraOut
    }
    VideoOutput {
        id: monitorCameraOut
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
        z: 0
        visible: monitorCamera.active
    }

    // ── Player A ──
    MediaPlayer {
        id: playerA
        videoOutput: videoOutA
        // Normally video-confidence-only (audience window carries the audio);
        // becomes the audio source itself when the audience window isn't active,
        // so program audio is never silently lost. See isAudioSource above.
        audioOutput: AudioOutput { id: audioA; volume: monitor.isAudioSource ? monitor.roomVolume : 0; muted: !monitor.isAudioSource; device: (MediaFlowBackend || {}).roomAudioOutputDevice }
        onMediaStatusChanged: {
            if (isLive && activeIsA && mediaStatus === MediaPlayer.EndOfMedia) {
                if (MediaFlowBackend && MediaFlowBackend.broadcastEngine)
                    MediaFlowBackend.broadcastEngine.clearLive()
            }
        }
    }
    VideoOutput {
        id: videoOutA
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 1.0 : 0.0
        visible: opacity > 0
        z: activeIsA ? 2 : 1
    }

    // ── Player B ──
    MediaPlayer {
        id: playerB
        videoOutput: videoOutB
        audioOutput: AudioOutput { id: audioB; volume: monitor.isAudioSource ? monitor.roomVolume : 0; muted: !monitor.isAudioSource; device: (MediaFlowBackend || {}).roomAudioOutputDevice }
        onMediaStatusChanged: {
            if (isLive && !activeIsA && mediaStatus === MediaPlayer.EndOfMedia) {
                if (MediaFlowBackend && MediaFlowBackend.broadcastEngine)
                    MediaFlowBackend.broadcastEngine.clearLive()
            }
        }
    }
    VideoOutput {
        id: videoOutB
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 0.0 : 1.0
        visible: opacity > 0
        z: activeIsA ? 1 : 2
    }

    // ── Image display (for image assets) ──
    Image {
        id: imageDisplay
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        source: (asset && asset.absolutePath && asset.type === "image") ? ("file:///" + asset.absolutePath) : ""
        visible: mediaType === "image"
        z: 3
    }

    // ── Thumbnail overlay (preview, paused state) ──
    Image {
        id: thumbOverlay
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: 0.9
        visible: !isLive && mediaType === "video" && asset && asset.thumbnailPath && asset.thumbnailPath !== "" && activePlayer.playbackState !== MediaPlayer.PlayingState
        source: (asset && asset.thumbnailPath) ? asset.thumbnailPath : ""
        z: 4
    }

    // ── Helper: get the currently active player ──
    property var activePlayer: activeIsA ? playerA : playerB
    property var inactivePlayer: activeIsA ? playerB : playerA

    // ── Preview: load asset but DON'T play ──
    onAssetChanged: {
        if (isLive) return  // Live is handled by cut/take signals

        if (!asset || !asset.absolutePath || asset.type === "input" || asset.type === "image") {
            playerA.stop(); playerA.source = ""
            playerB.stop(); playerB.source = ""
            return
        }

        let url = "file:///" + asset.absolutePath
        let ap = activeIsA ? playerA : playerB
        if (ap.source != url) {
            ap.source = url
            ap.pause()
            ap.setPosition(0)
        }
    }

    // ── Standby ──
    Rectangle {
        anchors.fill: parent; z: 0
        color: "black"
        visible: (!asset || !asset.absolutePath) && mediaType !== "input" && !monitorCamera.active
        Column {
            anchors.centerIn: parent; spacing: 12
            Rectangle {
                width: 48; height: 48; color: "#0A0A0A"; radius: 4
                anchors.horizontalCenter: parent.horizontalCenter
                BroadcastIcon { anchors.centerIn: parent; name: "video"; iconSize: 20; color: "#1AFFFFFF" }
            }
            Label {
                text: "STANDBY \u25CF NO SIGNAL"
                color: "#1AFFFFFF"; font.bold: true; font.pixelSize: 10; font.letterSpacing: 2
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // =====================================================================
    //  FADE OUT (for Cut Live / Clear)
    // =====================================================================
    function executeFadeOut() {
        fadeOutAnim.start()
    }

    NumberAnimation {
        id: fadeOutAnim
        target: activeIsA ? videoOutA : videoOutB
        property: "opacity"; from: 1.0; to: 0.0
        duration: 500; easing.type: Easing.InOutQuad
        onFinished: {
            playerA.stop(); playerA.source = ""
            playerB.stop(); playerB.source = ""
        }
    }

    // =====================================================================
    //  CUT TRANSITION (instant — 0ms)
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
        prev.stop(); prev.source = ""
    }

    // =====================================================================
    //  TAKE TRANSITION (crossfade — 500ms)
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
            property: "opacity"; from: 0.0; to: 1.0
            duration: 500; easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: activeIsA ? videoOutA : videoOutB
            property: "opacity"; from: 1.0; to: 0.0
            duration: 500; easing.type: Easing.InOutQuad
        }
        onFinished: {
            let prev = activeIsA ? playerA : playerB
            prev.stop(); prev.source = ""
            activeIsA = !activeIsA
        }
    }

    // ── Engine signal handlers ──
    Connections {
        target: (isLive && MediaFlowBackend && MediaFlowBackend.broadcastEngine) ? MediaFlowBackend.broadcastEngine : null

        function onCutExecuted() {
            let a = MediaFlowBackend.broadcastEngine.programAsset
            if (a && a.absolutePath && (a.type === "video" || a.type === "audio")) {
                executeCut("file:///" + a.absolutePath, a.type)
            } else {
                executeFadeOut()
            }
        }

        function onTakeExecuted() {
            let a = MediaFlowBackend.broadcastEngine.programAsset
            if (a && a.absolutePath && (a.type === "video" || a.type === "audio")) {
                executeTake("file:///" + a.absolutePath, a.type)
            }
        }

        function onIsProgramPausedChanged() {
            let ap = activeIsA ? playerA : playerB
            if (MediaFlowBackend.broadcastEngine.programPaused) ap.pause()
            else ap.play()
        }
    }

    // =====================================================================
    //  UI OVERLAYS
    // =====================================================================

    // Header badge
    Rectangle {
        anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 16; z: 20
        width: badgeRow.width + 16; height: 24; radius: 4
        color: isLive ? "#EF4444" : "#1AFFFFFF"
        Row {
            id: badgeRow; anchors.centerIn: parent; spacing: 6
            Rectangle { width: 6; height: 6; radius: 3; color: "white"; visible: isLive
                SequentialAnimation on opacity { running: isLive; loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 800 }
                    NumberAnimation { to: 1.0; duration: 800 }
                }
            }
            Label { text: monitor.title; color: "white"; font.bold: true; font.pixelSize: 9; font.letterSpacing: 1 }
        }
    }

    // Asset name
    Label {
        anchors.bottom: controlRow.top; anchors.left: parent.left; anchors.margins: 12; z: 20
        text: asset ? (asset.name || "") : ""; color: "white"; font.pixelSize: 10; font.bold: true; opacity: 0.7
        visible: asset && asset.name
    }

    // Status
    Label {
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 12; z: 20
        text: isLive ? "PROGRAM OUTPUT" : "PREVIEW BUS"
        color: Theme.textSecondary; font.pixelSize: Theme.textXs; font.bold: true; font.letterSpacing: 1.2
    }

    // Controls
    Row {
        id: controlRow
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.margins: 12
        spacing: Theme.space2; z: 20

        // Play/Pause
        Rectangle {
            visible: mediaType === "video" && asset && asset.absolutePath
            width: 36; height: 36; radius: 18
            color: ppMa.containsMouse ? "#80000000" : "#50000000"
            border.color: ppMa.containsMouse ? "#55FFFFFF" : "#33FFFFFF"
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
            scale: ppMa.pressed ? 0.92 : 1.0
            Behavior on scale { SpringAnimation { spring: 5; damping: 0.5 } }
            BroadcastIcon {
                anchors.centerIn: parent; iconSize: 14
                name: (activeIsA ? playerA : playerB).playbackState === MediaPlayer.PlayingState ? "eye" : "video"
            }
            MouseArea {
                id: ppMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (isLive) {
                        if (MediaFlowBackend && MediaFlowBackend.broadcastEngine)
                            MediaFlowBackend.broadcastEngine.toggleProgramPause()
                    } else {
                        let ap = activeIsA ? playerA : playerB
                        if (ap.playbackState === MediaPlayer.PlayingState) ap.pause()
                        else ap.play()
                    }
                }
            }
        }

        // CUT LIVE
        Rectangle {
            visible: showTransitions && asset && asset.absolutePath
            width: 90; height: 36; radius: Theme.radius
            color: cutMa.containsMouse ? "#33EF4444" : "transparent"
            border.color: cutMa.containsMouse ? Theme.accentRed : "#80EF4444"; border.width: 1.5
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
            scale: cutMa.pressed ? 0.95 : 1.0
            Behavior on scale { SpringAnimation { spring: 5; damping: 0.5 } }
            Row {
                anchors.centerIn: parent; spacing: Theme.space2
                BroadcastIcon { anchors.verticalCenter: parent.verticalCenter; name: "bolt"; iconSize: 11; color: Theme.accentRed }
                Label { text: "CUT LIVE"; color: Theme.accentRed; font.pixelSize: Theme.textXs; font.bold: true }
            }
            MouseArea { id: cutMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: monitor.cutClicked() }
        }

        // TAKE LIVE
        Rectangle {
            visible: showTransitions && asset && asset.absolutePath
            width: 90; height: 36; radius: Theme.radius
            color: takeMa.containsMouse ? "#33FFFFFF" : Theme.surfaceRaised
            border.color: takeMa.containsMouse ? "#55FFFFFF" : Theme.panelBorder; border.width: 1
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
            scale: takeMa.pressed ? 0.95 : 1.0
            Behavior on scale { SpringAnimation { spring: 5; damping: 0.5 } }
            Label { anchors.centerIn: parent; text: "TAKE LIVE"; color: "white"; font.pixelSize: Theme.textXs; font.bold: true }
            MouseArea { id: takeMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: monitor.takeClicked() }
        }
    }
}
