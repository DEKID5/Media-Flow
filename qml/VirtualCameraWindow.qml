import QtQuick
import QtQuick.Window
import QtMultimedia
import MediaFlow 1.0

Window {
    id: zoomRoot
    width: 1920
    height: 1080
    visible: false
    title: qsTr("MediaFlow - Zoom Virtual Camera")
    color: "black"
    flags: Qt.FramelessWindowHint | Qt.Tool

    property bool activeIsA: true

    CaptureSession {
        id: cameraSession
        camera: Camera {
            id: programCamera
            cameraDevice: (MediaFlowBackend || {}).programCameraDevice
            active: {
                let a = (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.programAsset : null
                return !a || !a.absolutePath || a.type === "input"
            }
        }
        videoOutput: cameraOut
    }

    VideoOutput {
        id: cameraOut
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
        z: 0
        visible: programCamera.active
    }

    MediaPlayer {
        id: playerA
        videoOutput: videoOutA
        audioOutput: AudioOutput { muted: true; volume: 0.0 }
    }
    VideoOutput {
        id: videoOutA
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 1.0 : 0.0
        visible: opacity > 0
        z: activeIsA ? 2 : 1
    }

    MediaPlayer {
        id: playerB
        videoOutput: videoOutB
        audioOutput: AudioOutput { muted: true; volume: 0.0 }
    }
    VideoOutput {
        id: videoOutB
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 0.0 : 1.0
        visible: opacity > 0
        z: activeIsA ? 1 : 2
    }

    Image {
        anchors.fill: parent
        z: 3
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        visible: {
            let a = (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.programAsset : null
            return a && a.type === "image" && a.absolutePath
        }
        source: {
            let a = (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.programAsset : null
            return a && a.absolutePath ? "file:///" + a.absolutePath : ""
        }
    }

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
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 300
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: activeIsA ? videoOutA : videoOutB
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 300
            easing.type: Easing.InOutQuad
        }
        onFinished: {
            let prev = activeIsA ? playerA : playerB
            prev.stop()
            prev.source = ""
            activeIsA = !activeIsA
        }
    }

    Connections {
        target: (MediaFlowBackend || {}).broadcastEngine || null

        function onCutExecuted() {
            let a = MediaFlowBackend.broadcastEngine.programAsset
            if (a && a.absolutePath && (a.type === "video" || a.type === "audio")) {
                executeCut("file:///" + a.absolutePath, a.type)
            } else {
                playerA.stop()
                playerA.source = ""
                playerB.stop()
                playerB.source = ""
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
}
