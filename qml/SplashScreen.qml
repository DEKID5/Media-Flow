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

    Timer {
        interval: 1200
        running: true
        onTriggered: splashRoot.minTimeElapsed = true
    }
    Timer {
        interval: 10000
        running: true
        onTriggered: splashRoot.requestClose()
    }
    Timer {
        id: closeAnimationTimer
        interval: 220
        onTriggered: splashRoot.finished()
    }

    onScanReadyChanged: maybeClose()
    onMinTimeElapsedChanged: maybeClose()
    function maybeClose() {
        if (minTimeElapsed && scanReady)
            requestClose()
    }

    function requestClose() {
        if (closed)
            return
        closed = true
        closeAnimationTimer.start()
        opacity = 0
    }

    Image {
        anchors.fill: parent
        source: "qrc:/MediaFlow/qml/assets/splash_background.jpg"
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        color: "#66070B12"
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
        color: "#C010141D"
        border.color: "#252D3B"
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
        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
    }
}
