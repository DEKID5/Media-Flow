import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

DockPanel {
    id: timerRoot
    Layout.preferredHeight: 196
    Layout.minimumHeight: 188
    accentColor: "#3B82F6"
    title: "MEETING TIMER"

    headerTrailing: Rectangle {
        width: 74
        height: 22
        radius: 11
        color: "#151519"
        border.color: "#24242a"
        border.width: 1

        Row {
            anchors.centerIn: parent
            spacing: 7

            Rectangle {
                width: 5
                height: 5
                radius: 2.5
                color: TimerBackend.isStaged ? "#9ca3af" : "#4b5563"
                anchors.verticalCenter: parent.verticalCenter
            }

            Label {
                text: "STAGED"
                color: TimerBackend.isStaged ? "#9ca3af" : "#4b5563"
                font.family: "Inter"
                font.pixelSize: 8
                font.bold: true
                font.letterSpacing: 0.8
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    content: Item {
        anchors.fill: parent

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: controlsRow.top
            anchors.bottomMargin: 10
            spacing: 18

            RoundIconButton {
                iconName: "chevron-down"
                Layout.alignment: Qt.AlignVCenter
                onClicked: TimerBackend.adjustDuration(-1)
            }

            Label {
                text: TimerBackend.displayTime
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: {
                    if (TimerBackend.state === TimerBackend.Overtime) return "#EF4444"
                    if (TimerBackend.state === TimerBackend.Paused) return "#F59E0B"
                    return "#ffffff"
                }
                font.family: "JetBrains Mono"
                font.pixelSize: 56
                font.bold: true
                font.features: { "tnum": 1 }

                Behavior on color { ColorAnimation { duration: 180 } }
            }

            RoundIconButton {
                iconName: "chevron-up"
                Layout.alignment: Qt.AlignVCenter
                onClicked: TimerBackend.adjustDuration(1)
            }
        }

        RowLayout {
            id: controlsRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 44
            spacing: 10

            Button {
                id: startButton
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                text: (TimerBackend.state === TimerBackend.Running || TimerBackend.state === TimerBackend.Overtime) ? "PAUSE" : "START TIMER"
                onClicked: {
                    if (TimerBackend.state === TimerBackend.Running || TimerBackend.state === TimerBackend.Overtime) TimerBackend.pause()
                    else TimerBackend.start()
                }

                contentItem: Label {
                    text: startButton.text
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
                    color: (TimerBackend.state === TimerBackend.Running || TimerBackend.state === TimerBackend.Overtime) ? "#f59e0b" : "#10b981"
                    border.color: "#22c58f"
                    border.width: 1
                }
            }

            SquareIconButton {
                iconName: "reset"
                onClicked: TimerBackend.reset()
            }

            SquareIconButton {
                iconName: "screen"
                checked: TimerBackend.isStaged
                onClicked: TimerBackend.stage()
            }
        }
    }

    component RoundIconButton: Control {
        id: control
        property string iconName: ""
        signal clicked()

        Layout.preferredWidth: 34
        Layout.preferredHeight: 34
        hoverEnabled: true

        background: Rectangle {
            radius: 17
            color: control.hovered ? "#151519" : "transparent"
            border.color: "#333333"
            border.width: 1
        }

        contentItem: Item {
            BroadcastIcon {
                anchors.centerIn: parent
                name: control.iconName
                iconSize: 13
                color: "#9ca3af"
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clicked()
        }
    }

    component SquareIconButton: Control {
        id: control
        property string iconName: ""
        property bool checked: false
        signal clicked()

        Layout.preferredWidth: 44
        Layout.preferredHeight: 44
        hoverEnabled: true

        background: Rectangle {
            radius: 8
            color: control.checked ? "#162033" : (control.hovered ? "#202027" : "#1a1a1f")
            border.color: control.checked ? "#3B82F6" : "#2a2a30"
            border.width: 1
        }

        contentItem: Item {
            BroadcastIcon {
                anchors.centerIn: parent
                name: control.iconName
                iconSize: 16
                color: control.checked ? "#93c5fd" : "#9ca3af"
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clicked()
        }
    }
}
