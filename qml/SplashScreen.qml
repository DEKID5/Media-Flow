import QtQuick
import MediaFlow 1.0

Item {
    id: splashRoot
    anchors.fill: parent
    opacity: 1

    signal finished()

    property bool minTimeElapsed: false
    property bool scanReady: (MediaFlowBackend || {}).initialScanComplete === true
    property bool closed: false

    // Guaranteed minimum display time so the splash screen is stable and consistent on all machines
    Timer {
        interval: 1500
        running: true
        onTriggered: splashRoot.minTimeElapsed = true
    }

    // Safety timeout: never block startup indefinitely
    Timer {
        interval: 12000
        running: true
        onTriggered: splashRoot.requestClose()
    }

    // Brief settle pause when scan completes so "Scan complete." is readably presented before fade-out
    Timer {
        id: settleTimer
        interval: 350
        onTriggered: splashRoot.requestClose()
    }

    Timer {
        id: closeAnimationTimer
        interval: 280
        onTriggered: splashRoot.finished()
    }

    onScanReadyChanged: maybeClose()
    onMinTimeElapsedChanged: maybeClose()

    function maybeClose() {
        if (minTimeElapsed && scanReady) {
            if (!settleTimer.running && !closed) {
                settleTimer.start()
            }
        }
    }

    function requestClose() {
        if (closed)
            return
        closed = true
        closeAnimationTimer.start()
        opacity = 0
    }

    // 1. Opaque solid backing so the dashboard can NEVER bleed through during startup
    Rectangle {
        anchors.fill: parent
        color: "#060A12"
    }

    // 2. Synchronously loaded splash artwork (20KB local resource, loads in <1ms)
    Image {
        id: bgImage
        anchors.fill: parent
        source: "assets/splash_background.jpg"
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: false
        cache: true
        onStatusChanged: {
            if (status === Image.Error && source.toString() !== "qrc:/MediaFlow/qml/assets/splash_background.jpg") {
                source = "qrc:/MediaFlow/qml/assets/splash_background.jpg"
            }
        }
    }

    // 3. Subtle translucent tint to elevate the central card and maintain contrast
    Rectangle {
        anchors.fill: parent
        color: "#50070B12"
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 500)
        height: 280
        radius: 20
        color: "#D010141D"
        border.color: "#2A3546"
        border.width: 1

        Column {
            anchors.centerIn: parent
            width: parent.width - 72
            spacing: 26

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Getting everything ready")
                color: "#F5F7FA"
                font.bold: true
                font.pixelSize: 20
            }

            Rectangle {
                id: progressTrack
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width, 300)
                height: 4
                radius: 2
                color: "#252D3B"
                clip: true

                Rectangle {
                    id: progressBar
                    width: 92
                    height: parent.height
                    radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#2563EB" }
                        GradientStop { position: 1.0; color: "#60A5FA" }
                    }
                    SequentialAnimation on x {
                        loops: Animation.Infinite
                        NumberAnimation {
                            from: -progressBar.width
                            to: progressTrack.width
                            duration: 1250
                            easing.type: Easing.InOutSine
                        }
                        PauseAnimation { duration: 100 }
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                text: (MediaFlowBackend || {}).scanStatus || qsTr("Preparing your media library…")
                color: "#9AA5B5"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("MEDIAFLOW  •  BROADCAST SUITE")
                color: "#647084"
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 1.5
            }
        }
    }

    Behavior on opacity {
        NumberAnimation { duration: 280; easing.type: Easing.InOutCubic }
    }
}
