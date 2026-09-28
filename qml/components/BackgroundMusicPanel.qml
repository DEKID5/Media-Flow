import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

DockPanel {
    id: bgmRoot
    Layout.fillHeight: true
    Layout.minimumHeight: 180
    accentColor: Theme.accentEmerald
    title: "BACKGROUND MUSIC"

    headerTrailing: Label {
        text: ((MediaFlowBackend || {}).bgmCount || 0) + " TRACKS"
        color: Theme.accentBlue
        font.family: "Inter"
        font.pixelSize: Theme.textXs
        font.bold: true
        font.letterSpacing: 0.5
        verticalAlignment: Text.AlignVCenter
    }

    function fmtMs(ms) {
        if (!ms || ms <= 0) return "0:00"
        let totalSec = Math.floor(ms / 1000)
        let m = Math.floor(totalSec / 60)
        let s = totalSec % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    content: Item {
        anchors.fill: parent
        clip: true

        DropArea {
            id: bgmDropArea
            anchors.fill: parent
            onDropped: (drop) => {
                if (drop.hasUrls) {
                    let paths = []
                    for (let i = 0; i < drop.urls.length; i++) paths.push(drop.urls[i].toString())
                    MediaFlowBackend.addFilesToBgm(paths)
                }
            }
        }

        // Drop feedback -- a dashed-style highlight border + hint text,
        // shown only while actually dragging files over the panel.
        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 8
            color: "#1A10B981"
            border.color: Theme.accentEmerald
            border.width: 2
            visible: bgmDropArea.containsDrag
            z: 10

            Label {
                anchors.centerIn: parent
                text: "DROP AUDIO FILES OR A FOLDER"
                color: Theme.accentEmerald
                font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: 2
            anchors.bottomMargin: 46
            width: 70
            height: 70
            radius: 8
            color: "transparent"
            clip: true

            Image {
                anchors.fill: parent
                source: (MediaFlowBackend || {}).bgmCoverArt || ""
                visible: source !== ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }

            Rectangle {
                anchors.fill: parent
                visible: ((MediaFlowBackend || {}).bgmCoverArt || "") !== ""
                color: "transparent"
                border.color: "#2a2a30"
                border.width: 1
                radius: 8
            }

            Label {
                anchors.centerIn: parent
                visible: ((MediaFlowBackend || {}).bgmCoverArt || "") === ""
                text: "\uD83C\uDFB5"
                color: "#2a2a30"
                opacity: 0.55
                font.pixelSize: 64
            }
        }

        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -18
            text: ((MediaFlowBackend || {}).bgmCount || 0) > 0 ? ((MediaFlowBackend || {}).bgmTrackName || "NO TRACKS") : "NO TRACKS"
            horizontalAlignment: Text.AlignHCenter
            color: "#6b7280"
            font.family: "Inter"
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 0.7
            elide: Text.ElideRight
        }

        // ── Progress scrubber ──
        Item {
            id: progressRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: timeRow.top
            anchors.bottomMargin: 4
            height: 16

            readonly property real ratio: {
                let d = (MediaFlowBackend || {}).bgmDurationMs || 0
                let p = (MediaFlowBackend || {}).bgmPositionMs || 0
                return d > 0 ? Math.max(0, Math.min(1, p / d)) : 0
            }

            Rectangle {
                id: progressTrack
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width; height: 4; radius: 2
                color: "#242429"

                Rectangle {
                    id: progressFill
                    height: parent.height; radius: parent.radius
                    color: Theme.accentEmerald
                    width: parent.width * progressRow.ratio
                    // Animates smoothly between position updates instead of
                    // jumping in fixed increments -- a real progress motion.
                    Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    visible: scrubMa.containsMouse || scrubMa.pressed
                    width: 10; height: 10; radius: 5; color: "white"
                    anchors.verticalCenter: parent.verticalCenter
                    x: Math.max(0, Math.min(parent.width - width, progressFill.width - width / 2))
                }
            }

            MouseArea {
                id: scrubMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: (mouse) => {
                    let d = (MediaFlowBackend || {}).bgmDurationMs || 0
                    if (d <= 0) return
                    let ratio = Math.max(0, Math.min(1, mouse.x / width))
                    MediaFlowBackend.seekBgm(Math.round(ratio * d))
                }
            }
        }

        RowLayout {
            id: timeRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: transportRow.top
            anchors.bottomMargin: 8
            Label {
                text: fmtMs((MediaFlowBackend || {}).bgmPositionMs || 0)
                color: "#6b7280"; font.family: "JetBrains Mono"; font.pixelSize: Theme.textXs
            }
            Item { Layout.fillWidth: true }
            Label {
                text: fmtMs((MediaFlowBackend || {}).bgmDurationMs || 0)
                color: "#6b7280"; font.family: "JetBrains Mono"; font.pixelSize: Theme.textXs
            }
        }

        RowLayout {
            id: transportRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 40
            spacing: 8

            BgmButton {
                Layout.preferredWidth: 36
                iconName: "shuffle"
                active: (MediaFlowBackend || {}).bgmShuffle
                onClicked: (MediaFlowBackend || {}).toggleBgmShuffle()
            }

            BgmButton {
                Layout.preferredWidth: 40
                iconName: "skip-back"
                onClicked: (MediaFlowBackend || {}).backBgm()
            }

            Button {
                id: playButton
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: (MediaFlowBackend || {}).isPlayingBgm ? "PAUSE" : "PLAY"
                onClicked: (MediaFlowBackend || {}).toggleBgmPlayback()

                contentItem: Label {
                    text: playButton.text
                    color: "#ffffff"
                    font.family: "Inter"
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.2
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 8
                    color: "#2563eb"
                    border.color: "#3B82F6"
                    border.width: 1
                }

                scale: playButton.pressed ? 0.97 : 1.0
                Behavior on scale { SpringAnimation { spring: 5; damping: 0.5 } }
            }

            BgmButton {
                Layout.preferredWidth: 40
                iconName: "skip-forward"
                onClicked: (MediaFlowBackend || {}).nextBgm()
            }

            BgmButton {
                Layout.preferredWidth: 50
                label: "STOP"
                onClicked: (MediaFlowBackend || {}).stopBgm()
            }
        }
    }

    component BgmButton: Control {
        id: control
        property string iconName: ""
        property string label: ""
        property bool active: false
        signal clicked()

        Layout.preferredHeight: 40
        hoverEnabled: true

        background: Rectangle {
            radius: 8
            color: control.active ? "#1a3B82F6" : (control.hovered ? "#202027" : "#1a1a1f")
            border.color: control.active ? Theme.accentBlue : "#2a2a30"
            border.width: 1
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        }

        contentItem: Item {
            BroadcastIcon {
                visible: control.iconName !== ""
                anchors.centerIn: parent
                name: control.iconName
                iconSize: 16
                color: control.active ? Theme.accentBlue : "#9ca3af"
            }
            Label {
                visible: control.label !== ""
                anchors.centerIn: parent
                text: control.label
                color: "#9ca3af"
                font.family: "Inter"
                font.pixelSize: Theme.textXs
                font.bold: true
                font.letterSpacing: 0.6
            }
        }

        scale: bgmMa.pressed ? 0.92 : 1.0
        Behavior on scale { SpringAnimation { spring: 6; damping: 0.5 } }

        MouseArea {
            id: bgmMa
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clicked()
        }
    }
}
