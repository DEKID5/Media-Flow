import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

DockPanel {
    id: timerRoot
    Layout.preferredHeight: 236
    Layout.minimumHeight: 228
    accentColor: Theme.accentBlue
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
            anchors.bottom: sizeRow.top
            anchors.bottomMargin: 6
            spacing: 18

            RoundIconButton {
                iconName: "chevron-down"
                Layout.alignment: Qt.AlignVCenter
                onClicked: TimerBackend.adjustDuration(-1)
            }

            Item {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: timeLabel.implicitHeight

                Label {
                    id: timeLabel
                    anchors.fill: parent
                    text: TimerBackend.displayTime
                    visible: !timeEdit.visible
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    // Traffic-light progress: green for the first half of the
                    // target duration, yellow past the halfway point, red
                    // once time's actually up (Overtime) -- not started yet
                    // just stays neutral white.
                    color: {
                        if (TimerBackend.state === TimerBackend.Overtime) return "#EF4444"
                        if (TimerBackend.state === TimerBackend.Idle) return "#ffffff"
                        let total = TimerBackend.targetDurationSeconds
                        let frac = total > 0 ? TimerBackend.elapsedSeconds / total : 0
                        return frac < 0.5 ? "#10B981" : "#F59E0B"
                    }
                    font.family: "JetBrains Mono"
                    font.pixelSize: 56
                    font.bold: true
                    font.features: { "tnum": 1 }
                    // Large display numerals want negative tracking — at this size the
                    // digits' natural spacing reads as too loose otherwise.
                    font.letterSpacing: -1.5

                    Behavior on color { ColorAnimation { duration: 180 } }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.IBeamCursor
                        onClicked: {
                            // Seed the field with just the target duration (MM:SS),
                            // not the possibly-negative overtime display, since
                            // typing a duration is what adjustDuration/chevrons do too.
                            let total = TimerBackend.targetDurationSeconds
                            let mm = Math.floor(total / 60)
                            let ss = total % 60
                            timeEdit.text = mm + ":" + (ss < 10 ? "0" + ss : ss)
                            timeEdit.visible = true
                            timeEdit.forceActiveFocus()
                            timeEdit.selectAll()
                        }
                    }
                }

                TextField {
                    id: timeEdit
                    anchors.fill: parent
                    visible: false
                    horizontalAlignment: Text.AlignHCenter
                    font.family: "JetBrains Mono"
                    font.pixelSize: 56
                    font.bold: true
                    font.letterSpacing: -1.5
                    color: "#ffffff"
                    background: Rectangle { color: "transparent" }
                    // Accepts "MM:SS", "M", or plain seconds/minutes -- kept
                    // forgiving since this is a quick operator entry field,
                    // not a strict form. Invalid input just cancels the edit.
                    validator: RegularExpressionValidator { regularExpression: /^[0-9]{0,4}(:[0-9]{0,2})?$/ }

                    function commit() {
                        let raw = text.trim()
                        let seconds = -1
                        if (raw.indexOf(":") !== -1) {
                            let parts = raw.split(":")
                            let mm = parseInt(parts[0] || "0", 10)
                            let ss = parseInt(parts[1] || "0", 10)
                            if (!isNaN(mm) && !isNaN(ss)) seconds = mm * 60 + ss
                        } else if (raw.length > 0) {
                            let mm = parseInt(raw, 10)
                            if (!isNaN(mm)) seconds = mm * 60
                        }
                        if (seconds >= 0) TimerBackend.targetDurationSeconds = Math.min(seconds, 359999)
                        visible = false
                    }

                    onAccepted: commit()
                    onActiveFocusChanged: if (!activeFocus) commit()
                    Keys.onEscapePressed: visible = false
                }
            }

            RoundIconButton {
                iconName: "chevron-up"
                Layout.alignment: Qt.AlignVCenter
                onClicked: TimerBackend.adjustDuration(1)
            }
        }

        RowLayout {
            id: sizeRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: controlsRow.top
            anchors.bottomMargin: 8
            height: 24
            spacing: 8

            Label {
                text: "SIZE"
                color: "#71717A"
                font.family: "Inter"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1.0
            }
            Label { text: "A"; color: "#71717A"; font.pixelSize: 10; font.bold: true }
            Slider {
                Layout.fillWidth: true
                from: 0.5; to: 1.0; stepSize: 0.05
                value: TimerBackend.timerScale
                onMoved: TimerBackend.timerScale = value
                ToolTip.visible: hovered || pressed
                ToolTip.text: "Full-screen timer size: " + Math.round(value * 100) + "%"
            }
            Label { text: "A"; color: "#d4d4d8"; font.pixelSize: 16; font.bold: true }
            Label {
                text: Math.round(TimerBackend.timerScale * 100) + "%"
                color: "#a1a1aa"
                font.family: "JetBrains Mono"; font.pixelSize: 10
                Layout.preferredWidth: 34
                horizontalAlignment: Text.AlignRight
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

                scale: startButton.pressed ? 0.97 : 1.0
                Behavior on scale { SpringAnimation { spring: 5; damping: 0.5 } }
            }

            SquareIconButton {
                iconName: "reset"
                onClicked: TimerBackend.reset()
            }

            SquareIconButton {
                iconName: "screen"
                checked: TimerBackend.isStaged
                onClicked: TimerBackend.stage()
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: "Stage — small timer overlay on top of live content"
            }

            SquareIconButton {
                iconName: "expand"
                checked: TimerBackend.fullScreenTimer
                // Turning it ON takes over a whole monitor instantly, so this
                // always confirms first rather than extending the instant the
                // button is pressed. Turning it back OFF needs no
                // confirmation, since that's never surprising. No dedicated
                // screen picked yet in Settings -> explain instead of
                // guessing one (see BroadcastController::setTimerFullScreenActive).
                onClicked: {
                    if (TimerBackend.fullScreenTimer) { TimerBackend.toggleFullScreenTimer(); return }
                    let idx = (MediaFlowBackend || {}).timerScreenIndex
                    if (idx === undefined || idx === -1) noScreenDialog.open()
                    else fullScreenConfirmDialog.open()
                }
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: "Full-screen timer — opens on its own dedicated window/screen"
            }

            ThemedDialog {
                id: noScreenDialog
                title: "No Timer Screen Selected"
                acceptText: "OK"; showCancel: false
                contentItem: Label {
                    text: "The full-screen timer needs a dedicated screen to open on. Pick one in Settings → Displays → Timer, then try again."
                    color: Theme.textPrimary; font.pixelSize: Theme.textMd; wrapMode: Text.WordWrap
                    width: 300
                    leftPadding: 20; rightPadding: 20; topPadding: 4; bottomPadding: 12
                }
            }

            ThemedDialog {
                id: fullScreenConfirmDialog
                title: "Show Full-Screen Timer"
                acceptText: "SHOW"; acceptColor: Theme.accentBlue
                contentItem: Label {
                    text: {
                        let idx = (MediaFlowBackend || {}).timerScreenIndex
                        let screens = (MediaFlowBackend || {}).availableScreens ? MediaFlowBackend.availableScreens() : []
                        let match = screens.find(s => s.index === idx)
                        return "This will open the timer full-screen on " + (match ? match.name : ("display " + (idx + 1))) + ". Continue?"
                    }
                    color: Theme.textPrimary; font.pixelSize: Theme.textMd; wrapMode: Text.WordWrap
                    width: 300
                    leftPadding: 20; rightPadding: 20; topPadding: 4; bottomPadding: 12
                }
                onAccepted: TimerBackend.toggleFullScreenTimer()
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

        scale: roundMa.pressed ? 0.9 : 1.0
        Behavior on scale { SpringAnimation { spring: 6; damping: 0.5 } }

        MouseArea {
            id: roundMa
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

        scale: squareMa.pressed ? 0.92 : 1.0
        Behavior on scale { SpringAnimation { spring: 6; damping: 0.5 } }

        MouseArea {
            id: squareMa
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clicked()
        }
    }
}
