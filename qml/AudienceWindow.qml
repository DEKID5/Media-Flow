import QtQuick
import QtQuick.Window
import QtMultimedia
import MediaFlow 1.0

Window {
    id: audienceRoot
    width: 1920; height: 1080; visible: false
    title: qsTr("MediaFlow — Audience Display")
    color: "black"
    
    // Automatic screen binding (can also be overridden by C++ setScreen)
    screen: (Qt.application.screens.length > 1) ? Qt.application.screens[1] : Qt.application.screens[0]

    // =====================================================================
    //  DUAL-PLAYER A/B — mirrors the operator's Live monitor
    //  Audio output comes from HERE (the audience display)
    // =====================================================================

    property bool activeIsA: true

    // ── Player A ──
    MediaPlayer {
        id: playerA
        videoOutput: videoOutA
        audioOutput: AudioOutput { id: audioA; volume: 1.0 }
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
        audioOutput: AudioOutput { id: audioB; volume: 1.0 }
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
            if (a && a.absolutePath && (a.type === "video" || a.type === "audio"))
                executeTake("file:///" + a.absolutePath, a.type)
        }

        function onIsProgramPausedChanged() {
            let ap = activeIsA ? playerA : playerB
            if (MediaFlowBackend.broadcastEngine.programPaused) ap.pause()
            else ap.play()
        }
    }

    // =====================================================================
    //  TIMER OVERLAY
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
