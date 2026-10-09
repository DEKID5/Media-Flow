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
    // WindowStaysOnBottomHint keeps this window rendering (Qt Quick does not
    // fire afterRendering for a window positioned entirely outside every
    // monitor's bounds -- confirmed live: moving it off-screen at
    // (-10000,-10000) left afterRendering never firing at all, so no frame
    // was ever captured) while keeping it out of the way behind every other
    // window on screen, since it must stay on-screen to actually render.
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnBottomHint | Qt.WindowDoesNotAcceptFocus

    property bool activeIsA: true

    // Qt Quick only repaints a window when something in it is actually
    // animating. The live camera feed keeps doing that on its own, but a
    // static program asset (an image, or simply nothing changing between
    // transitions) can let the render loop go idle -- and with it, the
    // VirtualCameraManager's afterRendering-driven capture, freezing Zoom on
    // a stale frame instead of switching to what's actually live. Forcing a
    // steady repaint keeps capture flowing regardless of content.
    //
    // While a video is actively playing its own frames already trigger a
    // render ~30 times a second, so the timer only needs to be a slow
    // safety net there; running it at full rate on top just adds extra,
    // out-of-phase renders of a 1080p window.
    readonly property bool videoPlaying: {
        const e = (MediaFlowBackend || {}).broadcastEngine
        return !!e && !!e.programAsset && e.programAsset.type === "video" && !e.programPaused
    }
    Timer {
        interval: zoomRoot.videoPlaying ? 250 : 33
        running: zoomRoot.visible; repeat: true
        onTriggered: zoomRoot.update()
    }

    CaptureSession {
        id: cameraSession
        camera: Camera {
            id: programCamera
            cameraDevice: (MediaFlowBackend || {}).programCameraDevice
            active: {
                if (!(MediaFlowBackend || {}).webcamFallbackEnabled) return false
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

    // ── Shared program sinks -- BroadcastEngine pushes every frame it
    //    decodes into these sinks (registered below), the same way it feeds
    //    the operator's LIVE monitor and Extended Feed. No local MediaPlayer
    //    here anymore; this window never decodes. Audio was always muted
    //    here regardless (Zoom gets audio via the room output, not this
    //    capture window), so nothing audio-related moves. ──
    VideoOutput {
        id: videoOutA
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 1.0 : 0.0
        visible: opacity > 0
        z: activeIsA ? 2 : 1
    }
    VideoOutput {
        id: videoOutB
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        opacity: activeIsA ? 0.0 : 1.0
        visible: opacity > 0
        z: activeIsA ? 1 : 2
    }

    // videoSink is read-only on VideoOutput in Qt6, so the shared decode is
    // fanned out by the engine pushing frames into each registered sink
    // instead of this window binding to the engine's sink directly -- see
    // BroadcastEngine::registerProgramOutputs().
    Component.onCompleted: {
        if (MediaFlowBackend && MediaFlowBackend.broadcastEngine) {
            MediaFlowBackend.broadcastEngine.registerProgramOutputs(videoOutA.videoSink, videoOutB.videoSink)
            // Capture is sampled at 30 fps (VirtualCameraManager), so don't
            // re-render this 1080p window for every frame of 50/60 fps video.
            MediaFlowBackend.broadcastEngine.limitOutputFrameRate(videoOutA.videoSink, 30)
            MediaFlowBackend.broadcastEngine.limitOutputFrameRate(videoOutB.videoSink, 30)
        }
    }

    // Imperatively driven by takeImageLive()/cutImageLive() (called from
    // onTakeExecuted/onCutExecuted below) instead of a plain reactive
    // binding, so Take fades over 2s like the video crossfade instead of
    // just snapping to the new image.
    Image {
        id: programImage
        anchors.fill: parent
        z: 3
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        // Prevents shimmer/aliasing on fine detail (text, thin lines) when a
        // high-res source photo is minified to fit this window, then
        // minified again by VirtualCameraManager's scale-to-1080p pass.
        mipmap: true
        // Caps decode cost to this window's own 1920x1080 size -- matches
        // VirtualCameraManager's output resolution, so nothing ever decodes
        // bigger than what's actually captured.
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

    // seekMs: only used by syncToCurrentProgram() below, to pick up mid-
    // playback instead of restarting at 0:00 -- a real operator Cut always
    // starts fresh, so every other caller leaves this at its default.
    // Purely visual -- BroadcastEngine's cutLive()/takeLive() already drove
    // the real shared program players before this fired; this just flips
    // which VideoOutput is visible.
    function executeCut() {
        let nextOut = activeIsA ? videoOutB : videoOutA
        let prevOut = activeIsA ? videoOutA : videoOutB
        nextOut.opacity = 1.0
        prevOut.opacity = 0.0
        activeIsA = !activeIsA
    }

    // If Zoom broadcasting is turned on while something is already live,
    // show the side that matches whichever shared sink BroadcastEngine is
    // actually driving right now -- no seek needed, the shared decode is
    // already flowing.
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

    function executeTake() {
        crossfadeAnim.start()
    }

    ParallelAnimation {
        id: crossfadeAnim
        NumberAnimation {
            target: activeIsA ? videoOutB : videoOutA
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 2000
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: activeIsA ? videoOutA : videoOutB
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 2000
            easing.type: Easing.InOutQuad
        }
        // No local flip here: BroadcastEngine's own A/B flag is the single
        // source of truth (onProgramActiveIsAChanged below). A Cut mid-
        // crossfade cancels the engine's flip but not this animation, so a
        // local flip would desync and show the layer that gets no frames.
    }

    Connections {
        target: (MediaFlowBackend || {}).broadcastEngine || null

        function onProgramActiveIsAChanged() {
            activeIsA = MediaFlowBackend.broadcastEngine.programActiveIsA
        }

        function onCutExecuted() {
            let a = MediaFlowBackend.broadcastEngine.programAsset
            if (a && a.absolutePath && (a.type === "video" || a.type === "audio")) {
                executeCut()
            }
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
}
