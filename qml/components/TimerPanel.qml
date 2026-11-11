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
        width: 78
        height: 22
        radius: 11
        color: TimerBackend.isStaged ? "#132C22" : "#151821"
        border.color: TimerBackend.isStaged ? "#23523E" : "#272C3A"
        border.width: 1

        Row {
            anchors.centerIn: parent
            spacing: 6

            Rectangle {
                width: 6
                height: 6
                radius: 3
                color: TimerBackend.isStaged ? Theme.accentEmerald : "#4b5563"
                anchors.verticalCenter: parent.verticalCenter
            }

            Label {
                text: "STAGED"
                color: TimerBackend.isStaged ? "#D1FAE5" : "#71717A"
                font.family: "Inter"
                font.pixelSize: Theme.textXs
                font.bold: true
                font.letterSpacing: 0.7
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
            anchors.bottom: presetRow.top
            anchors.bottomMargin: 6
            spacing: 14

            Item { Layout.fillWidth: true }

            RoundIconButton {
                iconName: "chevron-down"
                Layout.alignment: Qt.AlignVCenter
                onClicked: TimerBackend.adjustDuration(-1)
            }

            Item {
                Layout.preferredWidth: Math.max(176, timeLabel.implicitWidth + 12)
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

            Item { Layout.fillWidth: true }
        }

        RowLayout {
            id: presetRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: sizeRow.top
            anchors.bottomMargin: 6
            height: 22
            spacing: 6

            Label {
                text: "PRESETS"
                color: "#71717A"
                font.family: "Inter"; font.pixelSize: Theme.textXs; font.bold: true; font.letterSpacing: 0.7
            }

            Item { Layout.fillWidth: true }

            Repeater {
                model: [
                    { label: "-1m", delta: -1 },
                    { label: "+1m", delta: 1 },
                    { label: "+5m", delta: 5 }
                ]
                delegate: Rectangle {
                    required property var modelData
                    width: 42; height: 22; radius: 6
                    color: presetMa.containsMouse ? "#232838" : "#151821"
                    border.color: presetMa.containsMouse ? Theme.accentBlue : "#282D3B"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                    Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

                    Label {
                        anchors.centerIn: parent
                        text: parent.modelData.label
                        color: presetMa.containsMouse ? "#ffffff" : "#CBD5E1"
                        font.family: "JetBrains Mono"
                        font.pixelSize: Theme.textXs
                        font.bold: true
                    }

                    MouseArea {
                        id: presetMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: TimerBackend.adjustDuration(parent.modelData.delta)
                    }
                }
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
                font.family: "Inter"; font.pixelSize: Theme.textXs; font.bold: true; font.letterSpacing: 0.9
            }
            Label { text: "A"; color: "#71717A"; font.pixelSize: Theme.textXs; font.bold: true }
            Slider {
                id: scaleSlider
                Layout.fillWidth: true
                implicitHeight: 20
                from: 0.5; to: 1.0; stepSize: 0.05
                value: TimerBackend.timerScale
                onMoved: TimerBackend.timerScale = value
                ToolTip.visible: hovered || pressed
                ToolTip.text: "Full-screen timer size: " + Math.round(value * 100) + "%"

                background: Rectangle {
                    x: scaleSlider.leftPadding
                    y: scaleSlider.topPadding + scaleSlider.availableHeight / 2 - height / 2
                    width: scaleSlider.availableWidth
                    height: 4
                    radius: 2
                    color: "#252833"

                    Rectangle {
                        width: scaleSlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: scaleSlider.hovered || scaleSlider.pressed ? "#60A5FA" : Theme.accentBlue
                    }
                }

                handle: Rectangle {
                    x: scaleSlider.leftPadding + scaleSlider.visualPosition * (scaleSlider.availableWidth - width)
                    y: scaleSlider.topPadding + scaleSlider.availableHeight / 2 - height / 2
                    width: scaleSlider.hovered || scaleSlider.pressed ? 14 : 12
                    height: width
                    radius: width / 2
                    color: "#F8FAFC"
                    border.color: scaleSlider.hovered || scaleSlider.pressed ? Theme.accentBlue : "#64748B"
                    border.width: 1.5
                    Behavior on width { NumberAnimation { duration: 100 } }
                }
            }
            Label { text: "A"; color: "#d4d4d8"; font.pixelSize: 15; font.bold: true }
            Label {
                text: Math.round(TimerBackend.timerScale * 100) + "%"
                color: "#a1a1aa"
                font.family: "JetBrains Mono"; font.pixelSize: Theme.textXs
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
                    font.pixelSize: Theme.textSm
                    font.bold: true
                    font.letterSpacing: 1.1
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    readonly property bool activeState: TimerBackend.state === TimerBackend.Running || TimerBackend.state === TimerBackend.Overtime
                    radius: 8
                    color: activeState ? "#F59E0B" : "#10B981"
                    border.color: activeState ? "#FBBF24" : "#34D399"
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
            color: control.hovered ? "#202534" : "#151822"
            border.color: control.hovered ? Theme.accentBlue : "#2A2F3D"
            border.width: 1
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
            Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
        }

        contentItem: Item {
            BroadcastIcon {
                anchors.centerIn: parent
                name: control.iconName
                iconSize: 13
                color: control.hovered ? Theme.textPrimary : "#CBD5E1"
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
            color: control.checked ? "#162033" : (control.hovered ? "#232838" : "#161922")
            border.color: control.checked ? "#3B82F6" : (control.hovered ? "#3E4659" : "#2A2F3D")
            border.width: 1
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
            Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
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
