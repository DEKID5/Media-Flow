import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

Rectangle {
    id: timerRoot
    Layout.fillWidth: true
    Layout.preferredHeight: 180
    color: "#0A0A0A"
    radius: 12
    border.color: "#1AFFFFFF"
    clip: true

    readonly property var theme: (typeof Theme !== "undefined") ? Theme : {
        bg: "#050505",
        panelBg: "#0A0A0A",
        accentBlue: "#3B82F6",
        accentEmerald: "#10B981",
        accentAmber: "#F59E0B",
        accentRed: "#EF4444",
        textPrimary: "#FFFFFF",
        textSecondary: "#A1A1AA"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // Header
        RowLayout {
            Layout.fillWidth: true
            RowLayout {
                spacing: 8
                Rectangle { width: 3; height: 14; color: theme.accentBlue; radius: 1.5 }
                Label {
                    text: "MEETING TIMER"
                    font.bold: true; font.pixelSize: 11; color: "white"; font.letterSpacing: 1.5
                }
            }
            Item { Layout.fillWidth: true }
            
            // Staged Status
            Rectangle {
                width: 70; height: 22; radius: 11
                color: TimerBackend.isStaged ? "#1A3B82F6" : "#0DFFFFFF"
                border.color: TimerBackend.isStaged ? theme.accentBlue : "#1AFFFFFF"
                Row {
                    anchors.centerIn: parent; spacing: 6
                    Rectangle { width: 5; height: 5; radius: 2.5; color: TimerBackend.isStaged ? theme.accentBlue : "#4DA1A1AA"; anchors.verticalCenter: parent.verticalCenter }
                    Label {
                        text: "STAGED"
                        font.pixelSize: 8; font.bold: true; color: TimerBackend.isStaged ? "white" : "#4DA1A1AA"; anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // Timer Display Area (Perfectly centered)
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            
            Row {
                anchors.centerIn: parent
                spacing: 40

                // Adjust Down
                Control {
                    id: downBtn
                    width: 32; height: 32; anchors.verticalCenter: parent.verticalCenter
                    background: Rectangle {
                        radius: 16; color: downBtn.hovered ? "#1AFFFFFF" : "transparent"; border.color: "#1AFFFFFF"; border.width: 1
                    }
                    contentItem: BroadcastIcon { name: "chevron-down"; iconSize: 12; color: "#A1A1AA"; anchors.centerIn: parent }
                    MouseArea { anchors.fill: parent; onClicked: TimerBackend.adjustDuration(-1) }
                    hoverEnabled: true
                }

                // MAIN TIME
                Label {
                    id: timeLabel
                    text: TimerBackend.displayTime
                    font.pixelSize: 56; font.bold: true; font.family: "JetBrains Mono"; font.features: { "tnum": 1 }
                    color: {
                        if (TimerBackend.state === TimerBackend.Overtime) return theme.accentRed
                        if (TimerBackend.state === TimerBackend.Paused) return theme.accentAmber
                        return "white"
                    }
                    anchors.verticalCenter: parent.verticalCenter
                    
                    Behavior on color { ColorAnimation { duration: 300 } }
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite; running: TimerBackend.state === TimerBackend.Overtime
                        NumberAnimation { to: 0.5; duration: 500; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: 500; easing.type: Easing.InOutQuad }
                    }
                }

                // Adjust Up
                Control {
                    id: upBtn
                    width: 32; height: 32; anchors.verticalCenter: parent.verticalCenter
                    background: Rectangle {
                        radius: 16; color: upBtn.hovered ? "#1AFFFFFF" : "transparent"; border.color: "#1AFFFFFF"; border.width: 1
                    }
                    contentItem: BroadcastIcon { name: "chevron-up"; iconSize: 12; color: "#A1A1AA"; anchors.centerIn: parent }
                    MouseArea { anchors.fill: parent; onClicked: TimerBackend.adjustDuration(1) }
                    hoverEnabled: true
                }
            }
        }

        // Control Buttons (Uniform sizing)
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            
            PillButton {
                id: mainActionBtn
                Layout.fillWidth: true; Layout.preferredHeight: 44
                text: (TimerBackend.state === TimerBackend.Running || TimerBackend.state === TimerBackend.Overtime) ? "PAUSE" : "START TIMER"
                primary: true
                accentColor: (TimerBackend.state === TimerBackend.Running || TimerBackend.state === TimerBackend.Overtime) ? theme.accentAmber : theme.accentEmerald
                iconName: (TimerBackend.state === TimerBackend.Running || TimerBackend.state === TimerBackend.Overtime) ? "pause" : "play"
                onClicked: {
                    if (TimerBackend.state === TimerBackend.Running || TimerBackend.state === TimerBackend.Overtime) TimerBackend.pause()
                    else TimerBackend.start()
                }
            }

            PillButton {
                Layout.preferredWidth: 44; Layout.preferredHeight: 44
                iconName: "refresh"; iconSize: 16
                onClicked: TimerBackend.reset()
            }

            PillButton {
                Layout.preferredWidth: 44; Layout.preferredHeight: 44
                iconName: "screen"; iconSize: 16
                accentColor: theme.accentBlue
                primary: TimerBackend.isStaged
                onClicked: TimerBackend.stage()
            }
        }
    }
}
