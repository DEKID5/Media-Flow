import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

DockPanel {
    id: bgmRoot
    Layout.fillHeight: true
    Layout.minimumHeight: 180
    accentColor: "#10B981"
    title: "BACKGROUND MUSIC"

    headerTrailing: Label {
        text: ((MediaFlowBackend || {}).bgmCount || 0) + " TRACKS"
        color: "#3B82F6"
        font.family: "Inter"
        font.pixelSize: 9
        font.bold: true
        font.letterSpacing: 0.5
        verticalAlignment: Text.AlignVCenter
    }

    content: Item {
        anchors.fill: parent
        clip: true

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

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 44
            spacing: 10

            BgmButton {
                Layout.preferredWidth: 44
                iconName: "skip-back"
                onClicked: (MediaFlowBackend || {}).backBgm()
            }

            Button {
                id: playButton
                Layout.fillWidth: true
                Layout.preferredHeight: 44
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
            }

            BgmButton {
                Layout.preferredWidth: 44
                iconName: "skip-forward"
                onClicked: (MediaFlowBackend || {}).nextBgm()
            }

            BgmButton {
                Layout.preferredWidth: 54
                label: "STOP"
                onClicked: (MediaFlowBackend || {}).stopBgm()
            }
        }
    }

    component BgmButton: Control {
        id: control
        property string iconName: ""
        property string label: ""
        signal clicked()

        Layout.preferredHeight: 44
        hoverEnabled: true

        background: Rectangle {
            radius: 8
            color: control.hovered ? "#202027" : "#1a1a1f"
            border.color: "#2a2a30"
            border.width: 1
        }

        contentItem: Item {
            BroadcastIcon {
                visible: control.iconName !== ""
                anchors.centerIn: parent
                name: control.iconName
                iconSize: 16
                color: "#9ca3af"
            }
            Label {
                visible: control.label !== ""
                anchors.centerIn: parent
                text: control.label
                color: "#9ca3af"
                font.family: "Inter"
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 0.6
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clicked()
        }
    }
}
