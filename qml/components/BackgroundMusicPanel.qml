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

    headerTrailing: RowLayout {
        spacing: 4
        Repeater {
            model: [
                { label: "LIBRARY", tab: "library" },
                { label: "PLAYLIST", tab: "playlist" }
            ]
            delegate: Button {
                required property var modelData
                implicitWidth: modelData.tab === "playlist" ? 64 : 58
                implicitHeight: 22
                padding: 0
                contentItem: Label {
                    text: modelData.label
                    color: "#60A5FA"
                    font.family: "Inter"
                    font.pixelSize: 8
                    font.bold: true
                    font.letterSpacing: 0.35
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 5
                    color: parent.hovered ? "#202a3a" : "transparent"
                }
                onClicked: {
                    musicPopup.activeTab = modelData.tab
                    musicPopup.open()
                }
            }
        }
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

        // Drop feedback overlay
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

        // Main column layout
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 8

            // Top section: Album Art + Details Column
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // ── Artwork Card ──
                Rectangle {
                    id: bgmArtworkCard
                    Layout.preferredWidth: 68
                    Layout.preferredHeight: 68
                    radius: 8
                    color: "#12141a"
                    border.color: (MediaFlowBackend || {}).isPlayingBgm ? "#1F4236" : "#222630"
                    border.width: 1
                    clip: true

                    // Fallback disc
                    Rectangle {
                        anchors.centerIn: parent
                        width: 54
                        height: 54
                        radius: 27
                        visible: ((MediaFlowBackend || {}).bgmCoverArt || "") === ""
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#1c2b2a" }
                            GradientStop { position: 1; color: "#0c0e12" }
                        }
                        border.color: "#2a363a"
                        border.width: 1

                        RotationAnimation on rotation {
                            from: 0
                            to: 360
                            duration: 8000
                            loops: Animation.Infinite
                            running: bgmRoot.visible && ((MediaFlowBackend || {}).isPlayingBgm || false)
                        }

                        // Center hub
                        Rectangle {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            radius: 8
                            color: "#101e1d"
                            border.color: Theme.accentEmerald
                            border.width: 1
                        }

                        Label {
                            anchors.centerIn: parent
                            text: "\u266B"
                            color: Theme.accentEmerald
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    // Album Art Image
                    Image {
                        anchors.fill: parent
                        source: (MediaFlowBackend || {}).bgmCoverArt || ""
                        visible: source !== ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize.width: 160
                        sourceSize.height: 160
                    }

                    // Playing equalizer overlay
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 18
                        visible: (MediaFlowBackend || {}).isPlayingBgm || false
                        gradient: Gradient {
                            GradientStop { position: 0; color: "transparent" }
                            GradientStop { position: 1; color: "#E00B1015" }
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 2
                            spacing: 2.5
                            Repeater {
                                model: [6, 12, 15, 9, 13]
                                delegate: Rectangle {
                                    required property int index
                                    required property int modelData
                                    width: 2.5
                                    height: modelData
                                    radius: 1.25
                                    color: index === 2 ? "#D1FAE5" : Theme.accentEmerald
                                    SequentialAnimation on height {
                                        loops: Animation.Infinite
                                        running: bgmRoot.visible && ((MediaFlowBackend || {}).isPlayingBgm || false)
                                        NumberAnimation { to: 3; duration: 170 + index * 26; easing.type: Easing.InOutSine }
                                        NumberAnimation { to: modelData; duration: 210 + index * 22; easing.type: Easing.InOutSine }
                                    }
                                }
                            }
                        }
                    }
                }

                // ── Track Details Column ──
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 4

                    // Status line + Shuffle/Repeat mode buttons
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Rectangle {
                            width: 6; height: 6; radius: 3
                            color: ((MediaFlowBackend || {}).isPlayingBgm || false) ? Theme.accentEmerald : "#6B7280"
                        }

                        Label {
                            text: ((MediaFlowBackend || {}).isPlayingBgm || false) ? "NOW PLAYING" : "PAUSED"
                            color: ((MediaFlowBackend || {}).isPlayingBgm || false) ? Theme.accentEmerald : "#6B7280"
                            font.family: "Inter"
                            font.pixelSize: 9
                            font.bold: true
                            font.letterSpacing: 0.8
                        }

                        Label {
                            text: "\u2022"
                            color: "#4B5563"
                            font.pixelSize: 9
                        }

                        Label {
                            Layout.fillWidth: true
                            text: {
                                let total = (MediaFlowBackend || {}).bgmCount || 0
                                let idx = (MediaFlowBackend || {}).bgmIndex ?? 0
                                return total > 0 ? ("TRACK " + (idx + 1) + " OF " + total) : "NO TRACKS"
                            }
                            color: "#9CA3AF"
                            font.family: "Inter"
                            font.pixelSize: 9
                            font.bold: true
                            font.letterSpacing: 0.5
                            elide: Text.ElideRight
                        }

                        // Mini Mode Buttons: Shuffle & Repeat
                        RowLayout {
                            spacing: 3

                            // Shuffle button
                            Rectangle {
                                width: 22; height: 20; radius: 4
                                color: (MediaFlowBackend || {}).bgmShuffle ? "#1E293B" : (shufMa.containsMouse ? "#202430" : "transparent")
                                border.color: (MediaFlowBackend || {}).bgmShuffle ? "#3B82F6" : "transparent"
                                border.width: 1

                                BroadcastIcon {
                                    anchors.centerIn: parent
                                    name: "shuffle"
                                    iconSize: 11
                                    color: (MediaFlowBackend || {}).bgmShuffle ? "#60A5FA" : (shufMa.containsMouse ? "#E5E7EB" : "#6B7280")
                                }

                                MouseArea {
                                    id: shufMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: (MediaFlowBackend || {}).toggleBgmShuffle()
                                }

                                ToolTip.visible: shufMa.containsMouse
                                ToolTip.delay: 300
                                ToolTip.text: (MediaFlowBackend || {}).bgmShuffle ? "Shuffle: On" : "Shuffle: Off"
                            }

                            // Repeat button
                            Rectangle {
                                width: 22; height: 20; radius: 4
                                readonly property int repMode: (MediaFlowBackend || {}).bgmRepeatMode || 0
                                color: repMode > 0 ? "#1E293B" : (repMa.containsMouse ? "#202430" : "transparent")
                                border.color: repMode > 0 ? "#3B82F6" : "transparent"
                                border.width: 1

                                BroadcastIcon {
                                    anchors.centerIn: parent
                                    name: parent.repMode === 2 ? "repeat-1" : "repeat"
                                    iconSize: 11
                                    color: parent.repMode > 0 ? "#60A5FA" : (repMa.containsMouse ? "#E5E7EB" : "#6B7280")
                                }

                                MouseArea {
                                    id: repMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: (MediaFlowBackend || {}).cycleBgmRepeatMode()
                                }

                                ToolTip.visible: repMa.containsMouse
                                ToolTip.delay: 300
                                ToolTip.text: {
                                    let m = (MediaFlowBackend || {}).bgmRepeatMode || 0
                                    return m === 2 ? "Repeat: Single track" : (m === 1 ? "Repeat: All tracks" : "Repeat: Off")
                                }
                            }
                        }
                    }

                    // Track Title with Marquee support for long titles
                    Item {
                        id: titleClipItem
                        Layout.fillWidth: true
                        Layout.preferredHeight: 18
                        clip: true

                        Label {
                            id: titleLabel
                            text: ((MediaFlowBackend || {}).bgmCount || 0) > 0 ? ((MediaFlowBackend || {}).bgmTrackName || "NO TRACKS") : "NO TRACKS"
                            color: Theme.textPrimary
                            font.family: "Inter"
                            font.pixelSize: 12
                            font.bold: true
                            font.letterSpacing: 0.3
                            anchors.verticalCenter: parent.verticalCenter

                            readonly property bool overflow: width > titleClipItem.width
                            x: overflow && ((MediaFlowBackend || {}).isPlayingBgm || false) ? marqueeAnim.marqueeX : 0

                            SequentialAnimation {
                                id: marqueeAnim
                                property real marqueeX: 0
                                running: titleLabel.overflow && ((MediaFlowBackend || {}).isPlayingBgm || false)
                                loops: Animation.Infinite
                                PauseAnimation { duration: 2500 }
                                NumberAnimation {
                                    target: marqueeAnim
                                    property: "marqueeX"
                                    from: 0
                                    to: -(titleLabel.width - titleClipItem.width + 16)
                                    duration: Math.max(3000, (titleLabel.width - titleClipItem.width) * 35)
                                    easing.type: Easing.Linear
                                }
                                PauseAnimation { duration: 2000 }
                                NumberAnimation {
                                    target: marqueeAnim
                                    property: "marqueeX"
                                    to: 0
                                    duration: 500
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                    }

                    // Progress Scrubber Bar
                    Item {
                        id: progressRow
                        Layout.fillWidth: true
                        Layout.preferredHeight: 12

                        readonly property real ratio: {
                            let d = (MediaFlowBackend || {}).bgmDurationMs || 0
                            let p = (MediaFlowBackend || {}).bgmPositionMs || 0
                            return d > 0 ? Math.max(0, Math.min(1, p / d)) : 0
                        }

                        Rectangle {
                            id: progressTrack
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: scrubMa.containsMouse || scrubMa.pressed ? 6 : 4
                            radius: 2.5
                            color: "#22252e"
                            Behavior on height { NumberAnimation { duration: 100 } }

                            Rectangle {
                                id: progressFill
                                height: parent.height
                                radius: parent.radius
                                color: Theme.accentEmerald
                                width: parent.width * progressRow.ratio
                                Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutQuad } }
                            }

                            // Thumb knob
                            Rectangle {
                                visible: scrubMa.containsMouse || scrubMa.pressed || ((MediaFlowBackend || {}).isPlayingBgm || false)
                                width: scrubMa.containsMouse || scrubMa.pressed ? 12 : 8
                                height: width
                                radius: width / 2
                                color: "white"
                                anchors.verticalCenter: parent.verticalCenter
                                x: Math.max(0, Math.min(parent.width - width, progressFill.width - width / 2))
                                border.color: "#10B981"
                                border.width: 1
                                Behavior on width { NumberAnimation { duration: 100 } }
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
                                let r = Math.max(0, Math.min(1, mouse.x / width))
                                MediaFlowBackend.seekBgm(Math.round(r * d))
                            }
                        }
                    }

                    // Timestamps & Volume Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        // Elapsed / Total Duration (Unified)
                        Label {
                            text: fmtMs((MediaFlowBackend || {}).bgmPositionMs || 0) + " / " + fmtMs((MediaFlowBackend || {}).bgmDurationMs || 0)
                            color: "#9CA3AF"
                            font.family: "JetBrains Mono"
                            font.pixelSize: Theme.textXs
                            font.features: { "tnum": 1 }
                        }

                        Item { Layout.fillWidth: true }

                        // Dedicated Volume Control on the right
                        RowLayout {
                            spacing: 5

                            BroadcastIcon {
                                name: ((MediaFlowBackend || {}).bgmVolume || 0) <= 0.01 ? "mute" : "speaker"
                                iconSize: 12
                                color: volMouse.containsMouse ? Theme.accentEmerald : "#8B95A5"

                                MouseArea {
                                    id: volMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    property real lastNonZeroVol: 0.6
                                    onClicked: {
                                        if (!MediaFlowBackend) return
                                        if (MediaFlowBackend.bgmVolume > 0.01) {
                                            lastNonZeroVol = MediaFlowBackend.bgmVolume
                                            MediaFlowBackend.bgmVolume = 0.0
                                        } else {
                                            MediaFlowBackend.bgmVolume = lastNonZeroVol > 0.05 ? lastNonZeroVol : 0.6
                                        }
                                    }
                                }

                                ToolTip.visible: volMouse.containsMouse
                                ToolTip.delay: 300
                                ToolTip.text: ((MediaFlowBackend || {}).bgmVolume || 0) <= 0.01 ? "Unmute" : "Mute"
                            }

                            Rectangle {
                                id: volTrack
                                width: 50
                                height: 4
                                radius: 2
                                color: "#252833"

                                Rectangle {
                                    height: parent.height
                                    radius: parent.radius
                                    color: volSliderArea.containsMouse || volSliderArea.pressed ? Theme.accentEmerald : "#717B8C"
                                    width: parent.width * Math.max(0, Math.min(1, (MediaFlowBackend || {}).bgmVolume ?? 0.6))
                                }

                                Rectangle {
                                    visible: volSliderArea.containsMouse || volSliderArea.pressed
                                    width: 8; height: 8; radius: 4
                                    color: "white"
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: Math.max(0, Math.min(parent.width - width, (parent.width * Math.max(0, Math.min(1, (MediaFlowBackend || {}).bgmVolume ?? 0.6))) - width / 2))
                                }

                                MouseArea {
                                    id: volSliderArea
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onPositionChanged: (mouse) => {
                                        if (pressed && MediaFlowBackend) {
                                            let r = Math.max(0, Math.min(1, mouse.x / width))
                                            MediaFlowBackend.bgmVolume = r
                                        }
                                    }
                                    onClicked: (mouse) => {
                                        if (MediaFlowBackend) {
                                            let r = Math.max(0, Math.min(1, mouse.x / width))
                                            MediaFlowBackend.bgmVolume = r
                                        }
                                    }
                                }

                                ToolTip.visible: volSliderArea.containsMouse
                                ToolTip.delay: 200
                                ToolTip.text: "Volume: " + Math.round(((MediaFlowBackend || {}).bgmVolume ?? 0.6) * 100) + "%"
                            }

                            Label {
                                text: Math.round(((MediaFlowBackend || {}).bgmVolume ?? 0.6) * 100) + "%"
                                color: "#6B7280"
                                font.family: "JetBrains Mono"
                                font.pixelSize: 9
                                font.features: { "tnum": 1 }
                                Layout.preferredWidth: 24
                            }
                        }
                    }
                }
            }

            // ── Transport Row ──
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                spacing: 6

                // Previous Track Button
                BgmButton {
                    Layout.preferredWidth: 36
                    iconName: "skip-back"
                    onClicked: (MediaFlowBackend || {}).backBgm()
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: "Previous track"
                }

                // Center Hero PLAY / PAUSE Button
                Button {
                    id: playButton
                    Layout.fillWidth: true
                    Layout.minimumWidth: 80
                    Layout.preferredHeight: 38
                    onClicked: (MediaFlowBackend || {}).toggleBgmPlayback()

                    contentItem: RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        BroadcastIcon {
                            name: (MediaFlowBackend || {}).isPlayingBgm ? "pause" : "play"
                            iconSize: 13
                            color: "white"
                        }
                        Label {
                            text: (MediaFlowBackend || {}).isPlayingBgm ? "PAUSE" : "PLAY"
                            color: "#ffffff"
                            font.family: "Inter"
                            font.pixelSize: 11
                            font.bold: true
                            font.letterSpacing: 1.1
                        }
                    }

                    background: Rectangle {
                        radius: 8
                        color: (MediaFlowBackend || {}).isPlayingBgm ? "#1d4ed8" : "#2563eb"
                        border.color: (MediaFlowBackend || {}).isPlayingBgm ? "#3b82f6" : "#60a5fa"
                        border.width: 1
                    }

                    scale: playButton.pressed ? 0.96 : (playButton.hovered ? 1.01 : 1.0)
                    Behavior on scale { SpringAnimation { spring: 5; damping: 0.5 } }
                }

                // Next Track Button
                BgmButton {
                    Layout.preferredWidth: 36
                    iconName: "skip-forward"
                    onClicked: (MediaFlowBackend || {}).nextBgm()
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: "Next track"
                }

                // Fade Out Button (smooth 2.5s fade to stop)
                BgmButton {
                    Layout.preferredWidth: 50
                    label: "FADE"
                    onClicked: {
                        if (MediaFlowBackend && MediaFlowBackend.fadeOutBgm) MediaFlowBackend.fadeOutBgm(2500)
                        else (MediaFlowBackend || {}).stopBgm()
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: "Smooth 2.5s fade out to stop"
                }

                // Stop Button
                BgmButton {
                    Layout.preferredWidth: 50
                    label: "STOP"
                    onClicked: (MediaFlowBackend || {}).stopBgm()
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: "Stop playback"
                }
            }
        }
    }

    BgmLibraryPopup {
        id: musicPopup
    }

    component BgmButton: Control {
        id: control
        property string iconName: ""
        property string label: ""
        property bool active: false
        signal clicked()

        Layout.preferredHeight: 38
        hoverEnabled: true

        background: Rectangle {
            radius: 8
            color: control.active ? "#1E293B" : (control.hovered ? "#222634" : "#171922")
            border.color: control.active ? Theme.accentBlue : (control.hovered ? "#3B4254" : "#242836")
            border.width: 1
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        }

        contentItem: Item {
            BroadcastIcon {
                visible: control.iconName !== ""
                anchors.centerIn: parent
                name: control.iconName
                iconSize: 14
                color: control.active ? Theme.accentBlue : (control.hovered ? Theme.textPrimary : "#9CA3AF")
            }
            Label {
                visible: control.label !== ""
                anchors.centerIn: parent
                text: control.label
                color: control.hovered ? (control.label === "STOP" ? "#EF4444" : Theme.textPrimary) : "#9CA3AF"
                font.family: "Inter"
                font.pixelSize: Theme.textXs
                font.bold: true
                font.letterSpacing: 0.6
            }
        }

        scale: bgmMa.pressed ? 0.94 : 1.0
        Behavior on scale { SpringAnimation { spring: 6; damping: 0.5 } }

        MouseArea {
            id: bgmMa
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clicked()
        }
    }
}
