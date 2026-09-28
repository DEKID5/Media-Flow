import QtQuick
import QtQuick.Window
import MediaFlow 1.0

// Shown immediately on launch, before the (heavier) main dashboard window
// appears -- covers the gap while the startup library scan runs, since a
// large JW Library folder can take a moment to index and an empty/blank
// main window during that time reads as the app hanging. Closes itself once
// the scan finishes AND a small minimum display time has passed (so it
// never just flashes on a fast machine), with a hard timeout as a safety
// net in case the scan ever takes unusually long.
Window {
    id: splashRoot
    width: 420
    height: 260
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "#0A0A0A"
    visible: true
    title: qsTr("MediaFlow")

    signal finished()

    Component.onCompleted: {
        x = Screen.width / 2 - width / 2
        y = Screen.height / 2 - height / 2
    }

    property bool minTimeElapsed: false
    property bool scanReady: (MediaFlowBackend || {}).initialScanComplete === true
    property bool closed: false

    Timer { interval: 900; running: true; onTriggered: splashRoot.minTimeElapsed = true }
    // Never block startup indefinitely -- an unusually large library or a
    // slow disk still gets the operator into the app within a few seconds.
    Timer { interval: 6000; running: true; onTriggered: splashRoot.requestClose() }

    onScanReadyChanged: maybeClose()
    onMinTimeElapsedChanged: maybeClose()
    function maybeClose() { if (minTimeElapsed && scanReady) requestClose() }

    // Named to avoid shadowing Window's own built-in close() -- this just
    // signals readiness; Main.qml decides when to actually tear the
    // splash window down, after it's shown the main window.
    function requestClose() {
        if (closed) return
        closed = true
        finished()
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: "transparent"
        border.color: "#1AFFFFFF"
        border.width: 1
    }

    Column {
        anchors.centerIn: parent
        spacing: 22

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 64; height: 64; radius: 16
            color: "#3B82F6"
            Text {
                anchors.centerIn: parent
                text: "MF"
                color: "white"
                font.bold: true
                font.pixelSize: 24
                font.letterSpacing: 1
            }

            // Soft pulse -- reads as "working", not just a static logo.
            SequentialAnimation on scale {
                loops: Animation.Infinite
                NumberAnimation { to: 1.06; duration: 900; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1.0; duration: 900; easing.type: Easing.InOutQuad }
            }
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 2
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "MediaFlow"
                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 22
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "BROADCAST SUITE"
                color: "#3B82F6"
                font.bold: true
                font.pixelSize: 10
                font.letterSpacing: 2
            }
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 6
                Repeater {
                    model: 3
                    Rectangle {
                        width: 6; height: 6; radius: 3
                        color: "#3B82F6"
                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            PauseAnimation { duration: index * 150 }
                            NumberAnimation { to: 0.25; duration: 450; easing.type: Easing.InOutQuad }
                            NumberAnimation { to: 1.0; duration: 450; easing.type: Easing.InOutQuad }
                        }
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: (MediaFlowBackend || {}).scanStatus || qsTr("Starting…")
                color: "#6b7280"
                font.pixelSize: 10
                font.letterSpacing: 0.5
            }
        }
    }
}
