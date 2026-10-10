import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtMultimedia
import MediaFlow 1.0
import "components"

Item {
    id: root
    anchors.fill: parent

    property string manualSongSegmentId: ""
    // Below this the header's full button labels no longer fit (measured:
    // the header needs ~1900px with every label spelled out) -- collapse
    // the broadcast buttons to icon-only (with tooltips) instead of letting
    // the window's real minimum width stay stuck near ~1900px. The
    // threshold sits above that measured requirement so it always
    // switches to compact before anything would overflow.
    readonly property bool compactHeader: Window.width > 0 && Window.width < 1950
    // A second, lower tier for the controls that still have room to shrink
    // further (branding subtitle, week/meeting-type/language pickers) --
    // lets the window's real floor come down enough to fit a half-screen
    // Windows Snap slot (960px on a standard 1920-wide display) instead of
    // being stuck well above it.
    readonly property bool veryCompactHeader: Window.width > 0 && Window.width < 1100
    signal settingsRequested()

    function takeLive() {
        if (MediaFlowBackend && MediaFlowBackend.broadcastEngine) {
            MediaFlowBackend.broadcastEngine.takeLive()
        }
    }

    function cutLive() {
        if (MediaFlowBackend && MediaFlowBackend.broadcastEngine) {
            MediaFlowBackend.broadcastEngine.cutLive()
        }
    }

    function requestZoomBroadcast() {
        if (!MediaFlowBackend)
            return

        if (MediaFlowBackend.vcamEnabled || MediaFlowBackend.hasVirtualCameraDriver()) {
            MediaFlowBackend.toggleZoomBroadcast()
        } else {
            vcamWarningDialog.open()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    // Global operator shortcuts -- a Shortcut item attaches to the nearest
    // Window ancestor automatically, so these fire regardless of which
    // control currently has focus. Reuse the same guarded wrapper functions
    // the header buttons call (requestZoomBroadcast's driver-check/warning
    // dialog in particular must not be bypassed). Key sequences are bound to
    // MediaFlowBackend.shortcutKeys rather than hardcoded, so rebinding one
    // in Settings (SHORTCUTS section) takes effect immediately -- an empty
    // string just means "unbound" (Shortcut accepts that as a no-op).
    function keyFor(action) {
        const keys = (MediaFlowBackend || {}).shortcutKeys
        return keys ? (keys[action] || "") : ""
    }
    function keyHint(action) {
        const k = root.keyFor(action)
        return k ? ("  (" + k + ")") : ""
    }
    Shortcut { sequence: root.keyFor("cut"); onActivated: root.cutLive() }
    Shortcut { sequence: root.keyFor("take"); onActivated: root.takeLive() }
    Shortcut {
        sequence: root.keyFor("pauseProgram")
        onActivated: {
            if (MediaFlowBackend && MediaFlowBackend.broadcastEngine)
                MediaFlowBackend.broadcastEngine.toggleProgramPause()
        }
    }
    Shortcut { sequence: root.keyFor("goLive"); onActivated: (MediaFlowBackend || {}).toggleMeetingLive() }
    Shortcut { sequence: root.keyFor("broadcastZoom"); onActivated: root.requestZoomBroadcast() }
    Shortcut {
        sequence: root.keyFor("webcamToggle")
        onActivated: { if (MediaFlowBackend) MediaFlowBackend.webcamFallbackEnabled = !MediaFlowBackend.webcamFallbackEnabled }
    }
    Shortcut { sequence: root.keyFor("extendFeed"); onActivated: (MediaFlowBackend || {}).toggleAudienceWindow() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- TOP BAR ---
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.panelBgDark
            border.color: Theme.panelBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                spacing: Theme.space3

                // 1. BRANDING
                RowLayout {
                    spacing: Theme.space3
                    Rectangle {
                        width: 36; height: 36; radius: Theme.radiusSm; color: Theme.accentBlue
                        Label {
                            anchors.centerIn: parent; text: "MF"; color: "white"
                            font.bold: true; font.pixelSize: Theme.textMd; font.letterSpacing: 1
                        }
                    }
                    Column {
                        spacing: -2
                        Label { text: "MediaFlow"; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textMd }
                        Label {
                            text: "BROADCAST SUITE"; color: Theme.accentBlue; font.bold: true; font.pixelSize: 9; font.letterSpacing: 1.5
                            visible: !root.veryCompactHeader
                        }
                    }
                }

                // 1b. SETTINGS
                Rectangle {
                    Layout.preferredHeight: 36; Layout.preferredWidth: 36; radius: Theme.radius
                    color: settingsMa.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised
                    border.color: Theme.panelBorder
                    BroadcastIcon { anchors.centerIn: parent; name: "settings"; iconSize: 15; color: Theme.textPrimary }
                    MouseArea {
                        id: settingsMa
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: root.settingsRequested()
                    }
                    ToolTip.visible: settingsMa.containsMouse
                    ToolTip.delay: 500
                    ToolTip.text: "Settings — displays, background music folder, languages"
                }

                // 1c. REFRESH WORKBOOK
                Rectangle {
                    Layout.preferredHeight: 36; Layout.preferredWidth: 36; radius: Theme.radius
                    color: refreshMa.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised
                    border.color: Theme.panelBorder
                    BroadcastIcon { anchors.centerIn: parent; name: "refresh"; iconSize: 15; color: Theme.textPrimary }
                    MouseArea {
                        id: refreshMa
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: (MediaFlowBackend || {}).refreshWorkbook()
                    }
                    ToolTip.visible: refreshMa.containsMouse
                    ToolTip.delay: 500
                    ToolTip.text: "Refresh weekly workbook — " + ((MediaFlowBackend || {}).workbookStatus || "not checked yet")
                }

                // 1d. MEETING WEEK PICKER
                Rectangle {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: root.veryCompactHeader ? 70 : 180
                    Layout.minimumWidth: root.veryCompactHeader ? 70 : 130
                    radius: Theme.radius; color: Theme.surfaceRaised; border.color: Theme.panelBorder
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 4; spacing: 4
                        ComboBox {
                            id: weekCombo
                            Layout.fillWidth: true
                            flat: true
                            textRole: "label"
                            model: ListModel { id: weekModel }

                            function reload() {
                                weekModel.clear()
                                weekModel.append({label: "Current Week (Auto)", iso: ""})
                                const weeks = (MediaFlowBackend || {}).availableWorkbookWeeks ? MediaFlowBackend.availableWorkbookWeeks() : []
                                for (let i = 0; i < weeks.length; i++)
                                    weekModel.append(weeks[i])
                                currentIndex = 0
                            }

                            Component.onCompleted: reload()

                            onActivated: (index) => {
                                if (MediaFlowBackend) MediaFlowBackend.selectWorkbookWeek(weekModel.get(index).iso)
                            }

                            contentItem: Label {
                                text: weekCombo.currentText
                                font.pixelSize: Theme.textXs; font.bold: true; color: Theme.textPrimary
                                verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignLeft
                                elide: Text.ElideRight
                                leftPadding: 8
                            }
                            background: Rectangle { color: "transparent" }
                        }
                    }
                    ToolTip.visible: weekMa.containsMouse
                    ToolTip.delay: 500
                    ToolTip.text: "Switch which week's meeting media is loaded"
                    MouseArea { id: weekMa; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                }

                // 2. MEETING TYPE SELECTOR
                Rectangle {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: root.veryCompactHeader ? 80 : 190
                    Layout.minimumWidth: root.veryCompactHeader ? 80 : 140
                    radius: Theme.radius; color: Theme.surfaceRaised; border.color: Theme.panelBorder
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 4; spacing: 0
                        Rectangle {
                            Layout.fillWidth: true; Layout.fillHeight: true; radius: Theme.radiusSm + 1
                            color: (MediaFlowBackend || {}).meetingType === "midweek" ? Theme.meetingAccent("midweek") : "transparent"
                            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                            Label {
                                anchors.fill: parent; anchors.margins: 2
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                text: root.veryCompactHeader ? "MW" : "MIDWEEK"
                                color: (MediaFlowBackend || {}).meetingType === "midweek" ? "white" : Theme.textSecondary; font.pixelSize: Theme.textXs; font.bold: true
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: (MediaFlowBackend || {}).setMeetingTypeStr("midweek") }
                        }
                        Rectangle {
                            Layout.fillWidth: true; Layout.fillHeight: true; radius: Theme.radiusSm + 1
                            color: (MediaFlowBackend || {}).meetingType === "weekend" ? Theme.meetingAccent("weekend") : "transparent"
                            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                            Label {
                                anchors.fill: parent; anchors.margins: 2
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                text: root.veryCompactHeader ? "WE" : "WEEKEND"
                                color: (MediaFlowBackend || {}).meetingType === "weekend" ? "white" : Theme.textSecondary; font.pixelSize: Theme.textXs; font.bold: true
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: (MediaFlowBackend || {}).setMeetingTypeStr("weekend") }
                        }
                    }
                }

                // 3. LANGUAGE SELECTOR
                Rectangle {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: root.veryCompactHeader ? 70 : 150
                    Layout.minimumWidth: root.veryCompactHeader ? 70 : 110
                    radius: Theme.radius; color: Theme.surfaceRaised; border.color: Theme.panelBorder
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 4; spacing: 4
                        BroadcastIcon { name: "globe"; iconSize: 13; Layout.leftMargin: 8; opacity: 0.7; color: Theme.textPrimary }
                        ComboBox {
                            id: langCombo
                            property bool syncingFromBackend: false
                            Layout.fillWidth: true
                            flat: true
                            model: (MediaFlowBackend || {}).getSupportedLanguages() || []
                            textRole: "name"
                            Component.onCompleted: syncFromBackend()

                            function indexOfLanguage(code) {
                                let current = (code || "E").toUpperCase()
                                for (let i = 0; i < model.length; i++) {
                                    if ((model[i].code || "").toUpperCase() === current) return i
                                }
                                return 0;
                            }

                            function syncFromBackend() {
                                syncingFromBackend = true
                                currentIndex = indexOfLanguage((MediaFlowBackend || {}).currentLanguageCode || "E")
                                syncingFromBackend = false
                            }

                            onActivated: (index) => {
                                if (MediaFlowBackend && model[index]) {
                                    MediaFlowBackend.setCurrentLanguageCode(model[index].code)
                                }
                            }

                            // getSupportedLanguages() is a plain invokable snapshot,
                            // not a bindable property, so the model won't refresh on
                            // its own when a custom language is added/removed in
                            // Settings -- re-fetch explicitly on that signal instead.
                            Connections {
                                target: MediaFlowBackend || null
                                function onLanguagesChanged() {
                                    langCombo.model = MediaFlowBackend.getSupportedLanguages()
                                    langCombo.syncFromBackend()
                                }
                            }

                            contentItem: Label {
                                text: langCombo.currentText
                                font.pixelSize: Theme.textXs; font.bold: true; color: Theme.textPrimary
                                verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignLeft
                                elide: Text.ElideRight
                            }
                            background: Rectangle { color: "transparent" }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // 4. ROOM AUDIO (the audience window's volume/mute — the single audio channel out of the PC)
                Rectangle {
                    Layout.preferredHeight: 36; Layout.preferredWidth: 150; Layout.minimumWidth: 110; radius: Theme.radius; color: Theme.surfaceRaised; border.color: Theme.panelBorder
                    RowLayout {
                        anchors.centerIn: parent; spacing: Theme.space2
                        BroadcastIcon {
                            name: (MediaFlowBackend || {}).mixerMuted ? "mute" : "speaker"
                            iconSize: 13; opacity: (MediaFlowBackend || {}).mixerMuted ? 1.0 : 0.7
                            color: (MediaFlowBackend || {}).mixerMuted ? Theme.accentRed : Theme.textPrimary
                            MouseArea {
                                anchors.fill: parent; anchors.margins: -4; cursorShape: Qt.PointingHandCursor
                                onClicked: (MediaFlowBackend || {}).setMixerMuted(!(MediaFlowBackend || {}).mixerMuted)
                            }
                        }

                        Label {
                            text: "−"; color: Theme.textSecondary; font.bold: true; font.pixelSize: 16
                            MouseArea {
                                anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.PointingHandCursor
                                onClicked: (MediaFlowBackend || {}).setMasterVolume((MediaFlowBackend || {}).masterVolume - 5)
                            }
                        }

                        Label {
                            text: (MediaFlowBackend || {}).masterVolume + "%"
                            color: Theme.accentBlue; font.pixelSize: Theme.textXs; font.bold: true; Layout.preferredWidth: 32; horizontalAlignment: Text.AlignHCenter
                        }

                        Label {
                            text: "+"; color: Theme.textSecondary; font.bold: true; font.pixelSize: 16
                            MouseArea {
                                anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.PointingHandCursor
                                onClicked: (MediaFlowBackend || {}).setMasterVolume((MediaFlowBackend || {}).masterVolume + 5)
                            }
                        }
                    }
                }

                // 5. SYSTEM STATUS — only real, live-checked state; no decorative fake
                // indicators. Just the dot + a tooltip, not a permanent text label --
                // the header has no room to spare for status text that isn't actionable.
                RowLayout {
                    spacing: Theme.space2
                    Rectangle {
                        width: 6; height: 6; radius: 3
                        color: (MediaFlowBackend || {}).hasSecondaryScreen ? Theme.accentEmerald : Theme.textFaint
                        ToolTip.visible: statusMa.containsMouse
                        ToolTip.delay: 400
                        ToolTip.text: (MediaFlowBackend || {}).hasSecondaryScreen ? "Second display connected" : "No second display connected"
                        MouseArea { id: statusMa; anchors.fill: parent; anchors.margins: -6; hoverEnabled: true }
                    }
                    Rectangle { Layout.leftMargin: Theme.space2; width: 1; height: 16; color: Theme.panelBorder }
                }

                // 6. LIVE & BROADCAST
                RowLayout {
                    spacing: Theme.space3

                    Rectangle {
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: root.compactHeader ? 44 : 130
                        Layout.minimumWidth: root.compactHeader ? 44 : 90
                        radius: Theme.radius
                        color: (MediaFlowBackend || {}).isMeetingLive ? "#1AEF4444" : Theme.surfaceRaised
                        border.color: (MediaFlowBackend || {}).isMeetingLive ? Theme.accentRed : Theme.panelBorder
                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                        // Feedback on press (down), not on release; a critically-damped
                        // spring settles it back so a rapid double-click doesn't stutter.
                        scale: goLiveMa.pressed ? 0.97 : 1.0
                        Behavior on scale { SpringAnimation { spring: 5; damping: 0.6 } }
                        ToolTip.visible: goLiveMa.containsMouse
                        ToolTip.delay: 500
                        ToolTip.text: ((MediaFlowBackend || {}).isMeetingLive ? "Meeting live — click to end" : "Go live") + root.keyHint("goLive")
                        RowLayout {
                            anchors.centerIn: parent; spacing: Theme.space2
                            BroadcastIcon {
                                name: "bolt"; iconSize: 13
                                color: (MediaFlowBackend || {}).isMeetingLive ? Theme.accentRed : Theme.textSecondary
                            }
                            Label {
                                visible: !root.compactHeader
                                text: (MediaFlowBackend || {}).isMeetingLive ? "MEETING LIVE" : "GO LIVE"
                                color: Theme.textPrimary; font.pixelSize: Theme.textXs; font.bold: true
                            }
                        }
                        MouseArea { id: goLiveMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: (MediaFlowBackend || {}).toggleMeetingLive() }
                    }

                    Rectangle {
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: root.compactHeader ? 44 : 190
                        Layout.minimumWidth: root.compactHeader ? 44 : 130
                        radius: Theme.radius
                        color: (MediaFlowBackend || {}).vcamEnabled ? "#1A3B82F6" : Theme.surfaceRaised
                        border.color: (MediaFlowBackend || {}).vcamEnabled ? Theme.accentBlue : Theme.panelBorder
                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                        scale: zoomMa.pressed ? 0.97 : 1.0
                        Behavior on scale { SpringAnimation { spring: 5; damping: 0.6 } }
                        ToolTip.visible: zoomMa.containsMouse
                        ToolTip.delay: 500
                        ToolTip.text: ((MediaFlowBackend || {}).vcamEnabled
                            ? "Broadcasting — click to stop sending video to Zoom."
                            : "1. Click to start.  2. In Zoom's camera picker, choose “OBS Virtual Camera.”  3. Room audio stays on your speakers — nothing is sent to Zoom.") + root.keyHint("broadcastZoom")
                        RowLayout {
                            anchors.centerIn: parent; spacing: Theme.space2
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: (MediaFlowBackend || {}).vcamEnabled ? Theme.accentBlue : Theme.textFaint
                                border.color: "white"; border.width: (MediaFlowBackend || {}).vcamEnabled ? 1 : 0
                                SequentialAnimation on opacity {
                                    running: (MediaFlowBackend || {}).vcamEnabled; loops: Animation.Infinite
                                    NumberAnimation { to: 0.4; duration: 700 }
                                    NumberAnimation { to: 1.0; duration: 700 }
                                }
                            }
                            Label {
                                visible: !root.compactHeader
                                text: "BROADCAST TO ZOOM"
                                color: (MediaFlowBackend || {}).vcamEnabled ? "white" : Theme.textSecondary
                                font.pixelSize: Theme.textXs; font.bold: true
                                elide: Text.ElideRight
                            }
                        }
                        MouseArea { id: zoomMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: requestZoomBroadcast() }
                    }

                    // Forces the Zoom feed's webcam fallback off entirely --
                    // independent of BROADCAST TO ZOOM itself (that starts/
                    // stops sending anything at all; this only controls
                    // what shows when nothing's on Program). See
                    // VirtualCameraWindow.qml's Camera.active.
                    Rectangle {
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: root.compactHeader ? 44 : 100
                        Layout.minimumWidth: root.compactHeader ? 44 : 70
                        radius: Theme.radius
                        color: (MediaFlowBackend || {}).webcamFallbackEnabled ? Theme.surfaceRaised : "#1AEF4444"
                        border.color: (MediaFlowBackend || {}).webcamFallbackEnabled ? Theme.panelBorder : Theme.accentRed
                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                        scale: webcamMa.pressed ? 0.97 : 1.0
                        Behavior on scale { SpringAnimation { spring: 5; damping: 0.6 } }
                        ToolTip.visible: webcamMa.containsMouse
                        ToolTip.delay: 500
                        ToolTip.text: ((MediaFlowBackend || {}).webcamFallbackEnabled
                            ? "Webcam shows on Zoom whenever nothing's on Program. Click to force it off (black instead)."
                            : "Webcam is forced off — Zoom shows black whenever nothing's on Program. Click to re-enable.") + root.keyHint("webcamToggle")
                        Row {
                            anchors.centerIn: parent; spacing: Theme.space1
                            BroadcastIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: (MediaFlowBackend || {}).webcamFallbackEnabled ? "video" : "mute"
                                iconSize: 12
                                color: (MediaFlowBackend || {}).webcamFallbackEnabled ? Theme.textPrimary : Theme.accentRed
                            }
                            Label {
                                visible: !root.compactHeader
                                text: (MediaFlowBackend || {}).webcamFallbackEnabled ? "WEBCAM" : "OFF"
                                color: (MediaFlowBackend || {}).webcamFallbackEnabled ? Theme.textSecondary : Theme.accentRed
                                font.pixelSize: Theme.textXs; font.bold: true
                            }
                        }
                        MouseArea {
                            id: webcamMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (MediaFlowBackend) MediaFlowBackend.webcamFallbackEnabled = !MediaFlowBackend.webcamFallbackEnabled }
                        }
                    }

                    Rectangle {
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: root.compactHeader ? 44 : 144
                        Layout.minimumWidth: root.compactHeader ? 44 : 95
                        radius: Theme.radius
                        color: {
                            let ext = (MediaFlowBackend || {}).feedExtended
                            if (ext) return Theme.accentBlue
                            return extMa.containsMouse ? Theme.surfaceHover : "transparent"
                        }
                        border.color: Theme.accentBlue
                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                        scale: extMa.pressed ? 0.97 : 1.0
                        Behavior on scale { SpringAnimation { spring: 5; damping: 0.6 } }
                        ToolTip.visible: extMa.containsMouse
                        ToolTip.delay: 500
                        ToolTip.text: ((MediaFlowBackend || {}).feedExtended ? "Feed active" : "Extend feed") + root.keyHint("extendFeed")
                        Row {
                            anchors.centerIn: parent; spacing: Theme.space2
                            BroadcastIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: (MediaFlowBackend || {}).feedExtended ? "eye" : "screen"
                                iconSize: 13; color: "white"
                            }
                            Label {
                                visible: !root.compactHeader
                                text: (MediaFlowBackend || {}).feedExtended ? "FEED ACTIVE" : "EXTEND FEED"
                                color: "white"; font.pixelSize: Theme.textXs; font.bold: true
                            }
                        }
                        MouseArea {
                            id: extMa; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { if (MediaFlowBackend) MediaFlowBackend.toggleAudienceWindow() }
                        }
                    }
                }
            }
        }

        // --- MONITOR SECTION ---
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space6
            anchors.margins: Theme.space6

            MonitorView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "PREVIEW"
                showTransitions: true
                acceptsFolderDrop: true
                asset: (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.previewAsset : null
                onTakeClicked: { if (MediaFlowBackend && MediaFlowBackend.broadcastEngine) MediaFlowBackend.broadcastEngine.takeLive() }
                onCutClicked: { if (MediaFlowBackend && MediaFlowBackend.broadcastEngine) MediaFlowBackend.broadcastEngine.cutLive() }
            }

            MonitorView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "LIVE"
                isLive: true
                acceptsFolderDrop: true
                asset: (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.programAsset : null
                cameraDevice: (MediaFlowBackend || {}).programCameraDevice
            }
        }

        // --- DOCK ---
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 400
            Layout.minimumHeight: 400
            color: "transparent"
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.panelBorder }

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.space5
                spacing: Theme.space5

                // MEDIA SOURCE
                MediaSourcePanel {
                    id: mediaSourcePanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 350
                    Layout.minimumWidth: 260
                    Rectangle {
                        anchors.fill: parent; z: -1; color: Theme.panelBgDark; radius: Theme.radiusLg; border.color: Theme.panelBorder
                    }
                }

                // SEQUENCE
                DockPanel {
                    id: sequencePanel
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.preferredWidth: 400
                    Layout.minimumWidth: 280
                    accentColor: Theme.meetingAccent((MediaFlowBackend || {}).meetingType)
                    title: "SEQUENCE"

                    headerTrailing: RowLayout {
                        spacing: Theme.space3
                        PillButton {
                            text: "CLEAR ALL"; accentColor: Theme.accentRed; implicitHeight: 24; implicitWidth: 86; font.pixelSize: 9
                            onClicked: clearDialog.open()
                        }
                        Label {
                            // Bound to the ListView's own reactive count, not rowCount() directly —
                            // a plain method call in a JS binding never re-evaluates on model reset.
                            text: sList.count + " SEGMENTS"
                            font.pixelSize: Theme.textXs; color: Theme.textDim; font.bold: true
                        }
                    }

                    content: ListView {
                        id: sList
                        anchors.fill: parent
                        spacing: Theme.space3; clip: true
                        // Without this, the viewport can come to rest at any
                        // fractional scroll offset -- its top edge slicing
                        // straight through the middle of a row instead of
                        // stopping at a row boundary (confirmed live: a
                        // segment's header text rendered with its top half
                        // cut off after scrolling).
                        snapMode: ListView.SnapToItem
                        model: (MediaFlowBackend || {}).meetingSchedule
                        delegate: Item {
                            id: segmentDelegate
                            width: sList.width; height: isSelected ? 160 : 110
                            property bool isSelected: (MediaFlowBackend || {}).selectedSegmentId === model.id
                            // Captured here because the nested Repeater below has its own
                            // "model" (associatedMediaIds), which shadows this segment's model.id.
                            property string segmentId: model.id
                            readonly property color accent: Theme.meetingAccent((MediaFlowBackend || {}).meetingType)

                            // Critically-damped spring, not a fixed-duration curve — clicking
                            // another segment mid-animation redirects smoothly instead of
                            // restarting or jumping. Critical damping for spring:3 (mass
                            // defaults to 1.0) is 2*sqrt(3) ≈ 3.46 -- the previous 0.6 was
                            // heavily underdamped, overshooting past 110/160 before settling
                            // instead of actually being critically damped as this comment claims.
                            Behavior on height { SpringAnimation { spring: 3; damping: 3.46 } }

                            Rectangle {
                                anchors.fill: parent; anchors.margins: 4
                                radius: Theme.radiusLg; color: isSelected ? Qt.rgba(accent.r, accent.g, accent.b, 0.05) : Theme.panelBgDark
                                border.color: isSelected ? accent : Theme.panelBorder
                                border.width: isSelected ? 2 : 1
                                Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

                                // Selection Glow
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: -2
                                    radius: Theme.radiusLg + 2; color: "transparent"; border.color: segmentDelegate.accent; border.width: 1
                                    opacity: isSelected ? 0.3 : 0
                                }

                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: Theme.space4; spacing: Theme.space2

                                    // Time Row
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: model.time
                                            color: isSelected ? Qt.lighter(segmentDelegate.accent, 1.3) : Theme.textFaint
                                            font.pixelSize: Theme.textSm; font.bold: true
                                        }
                                        Label {
                                            visible: model.durationMinutes > 0
                                            text: model.durationMinutes + " MIN"
                                            color: Theme.textDim; font.pixelSize: Theme.textXs; font.bold: true
                                        }
                                        Item { Layout.fillWidth: true }
                                        BroadcastIcon {
                                            visible: model.isLive; name: "activity"; color: Theme.accentRed; iconSize: 13
                                        }
                                    }

                                    // Title (Editable for Additional Part only)
                                    TextField {
                                        id: titleEdit
                                        text: model.title.toUpperCase()
                                        Layout.fillWidth: true
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.textMd; font.bold: true
                                        background: Rectangle { color: "transparent" }
                                        padding: 0; leftPadding: 0
                                        readOnly: model.id !== "m12"
                                        selectByMouse: !readOnly

                                        onEditingFinished: {
                                            if (MediaFlowBackend && MediaFlowBackend.meetingSchedule) {
                                                MediaFlowBackend.meetingSchedule.updateSegmentTitle(model.id, text.toUpperCase())
                                            }
                                            focus = false
                                        }
                                    }

                                    // Song Selector
                                    RowLayout {
                                        Layout.fillWidth: true
                                        visible: model.type === "song" && isSelected
                                        spacing: Theme.space3

                                        Rectangle {
                                            width: 100; height: 32; radius: Theme.radiusSm; color: Theme.surfaceHover; border.color: Theme.panelBorderStrong
                                            TextInput {
                                                id: songInput
                                                anchors.centerIn: parent; width: parent.width - 16
                                                color: Theme.textPrimary; font.pixelSize: Theme.textMd; font.bold: true; horizontalAlignment: TextInput.AlignHCenter
                                                inputMethodHints: Qt.ImhDigitsOnly

                                                Text {
                                                    text: "Song #"
                                                    anchors.centerIn: parent
                                                    color: Theme.textFaint
                                                    visible: songInput.text === "" && !songInput.activeFocus
                                                    font: songInput.font
                                                }

                                                onAccepted: {
                                                    if (MediaFlowBackend && text !== "") {
                                                        MediaFlowBackend.findAndStageSong(parseInt(text), MediaFlowBackend.currentLanguageCode || "E", model.id)
                                                    }
                                                }
                                            }
                                        }

                                        Rectangle {
                                            width: 32; height: 32; radius: Theme.radiusSm; color: Theme.accentBlue
                                            BroadcastIcon { anchors.centerIn: parent; name: "check"; color: "white"; iconSize: 14 }
                                            MouseArea {
                                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (MediaFlowBackend && songInput.text !== "") {
                                                        MediaFlowBackend.findAndStageSong(parseInt(songInput.text), MediaFlowBackend.currentLanguageCode || "E", model.id)
                                                    }
                                                }
                                            }
                                        }

                                        Label {
                                            text: (typeof associatedMediaIds !== "undefined" && associatedMediaIds && associatedMediaIds.length > 0) ? "LINKED" : "UNLINKED"
                                            font.pixelSize: Theme.textXs; font.bold: true
                                            color: (typeof associatedMediaIds !== "undefined" && associatedMediaIds && associatedMediaIds.length > 0) ? Theme.accentEmerald : Theme.textFaint
                                            font.letterSpacing: 1
                                        }
                                    }

                                    // Media Thumbnails + Add Button
                                    RowLayout {
                                        Layout.fillWidth: true; Layout.preferredHeight: 46; spacing: Theme.space2 + 2
                                        visible: isSelected || (typeof associatedMediaIds !== "undefined" && associatedMediaIds && associatedMediaIds.length > 0)

                                        // Horizontally-scrollable thumbnail strip -- takes whatever
                                        // width is left after the Add button cluster below reserves
                                        // its own, so a long list of linked media never pushes those
                                        // buttons out of the panel's clipped (and otherwise
                                        // non-scrollable) bounds. Mirrors the pinList pattern in
                                        // MediaSourcePanel.qml.
                                        Item {
                                            id: thumbViewport
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 36
                                            Layout.alignment: Qt.AlignVCenter
                                            clip: true

                                            ListView {
                                                id: thumbList
                                                anchors.fill: parent
                                                orientation: ListView.Horizontal
                                                spacing: Theme.space2
                                                model: (typeof associatedMediaIds !== "undefined") ? associatedMediaIds : []
                                                delegate: Rectangle {
                                                    width: 64; height: 36; radius: Theme.radiusSm; color: "black"; clip: true
                                                    property var asset: (MediaFlowBackend || {}).mediaLibrary ? MediaFlowBackend.mediaLibrary.getRowById(modelData) : ({})

                                                    Image {
                                                        anchors.fill: parent; fillMode: Image.PreserveAspectCrop
                                                        source: asset.thumbnailPath || "qrc:/MediaFlow/qml/assets/video_placeholder.png"
                                                        opacity: 0.8
                                                    }

                                                    // Unlink button
                                                    Rectangle {
                                                        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 2
                                                        width: 15; height: 15; radius: 7.5; color: Theme.accentRed
                                                        Label { text: "×"; anchors.centerIn: parent; color: "white"; font.pixelSize: Theme.textSm; font.bold: true }
                                                        MouseArea {
                                                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                            onClicked: (MediaFlowBackend || {}).removeMediaFromSequence(segmentDelegate.segmentId, modelData)
                                                        }
                                                    }
                                                }
                                            }

                                            // Right-edge fade -- only shown while there's actually
                                            // more to scroll to, so it never masks the last tile once
                                            // fully scrolled.
                                            Rectangle {
                                                anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right
                                                width: 18
                                                visible: thumbList.contentWidth > thumbList.width + 1 && !thumbList.atXEnd
                                                gradient: Gradient {
                                                    orientation: Gradient.Horizontal
                                                    GradientStop { position: 0.0; color: "#00000000" }
                                                    GradientStop { position: 1.0; color: "#80000000" }
                                                }
                                            }
                                        }

                                        // Trailing button cluster -- sized to its own content (no
                                        // Layout.fillWidth), so it's always fully reachable at the
                                        // row's trailing edge regardless of thumbnail count. Still a
                                        // RowLayout (not a plain Row) so the mutually-exclusive
                                        // visible bindings below keep collapsing to zero space
                                        // exactly as before.
                                        RowLayout {
                                            id: addButtonCluster
                                            spacing: Theme.space1
                                            Layout.alignment: Qt.AlignVCenter

                                            // ADD VIDEO BUTTON
                                            Rectangle {
                                                visible: isSelected && model.type !== "song"
                                                width: 60; height: 36; radius: Theme.radiusSm; color: "#0D3B82F6"; border.color: Theme.accentBlue
                                                Row {
                                                    anchors.centerIn: parent; spacing: Theme.space1
                                                    BroadcastIcon { anchors.verticalCenter: parent.verticalCenter; name: "video"; color: Theme.accentBlue; iconSize: 13 }
                                                    Label { text: "VIDEO"; color: Theme.accentBlue; font.pixelSize: Theme.textXs - 1; font.bold: true }
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (MediaFlowBackend) MediaFlowBackend.addMediaToSegment(model.id, "video")
                                                    }
                                                }
                                            }

                                            // ADD JW IMAGE BUTTON
                                            Rectangle {
                                                visible: isSelected && model.type !== "song"
                                                width: 60; height: 36; radius: Theme.radiusSm; color: "#0D10B981"; border.color: Theme.accentEmerald
                                                Row {
                                                    anchors.centerIn: parent; spacing: Theme.space1
                                                    BroadcastIcon { anchors.verticalCenter: parent.verticalCenter; name: "image"; color: Theme.accentEmerald; iconSize: 13 }
                                                    Label { text: "IMAGE"; color: Theme.accentEmerald; font.pixelSize: Theme.textXs - 1; font.bold: true }
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (MediaFlowBackend) MediaFlowBackend.addMediaToSegment(model.id, "image")
                                                    }
                                                }
                                            }

                                            // ADD SONG BUTTON
                                            Rectangle {
                                                width: 72; height: 36; radius: Theme.radiusSm; color: "#0D3B82F6"; border.color: Theme.accentBlue
                                                visible: isSelected && model.type === "song"
                                                Row {
                                                    anchors.centerIn: parent; spacing: Theme.space1
                                                    BroadcastIcon { anchors.verticalCenter: parent.verticalCenter; name: "music"; color: Theme.accentBlue; iconSize: 13 }
                                                    Label { text: "SONG"; color: Theme.accentBlue; font.pixelSize: Theme.textXs - 1; font.bold: true }
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        manualSongSegmentId = model.id
                                                        manualSongSelector.open()
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent; z: -1
                                    onClicked: {
                                        if (MediaFlowBackend) MediaFlowBackend.selectSegment(model.id)
                                        // Loads this segment's default length into the timer (e.g.
                                        // Public Talk -> 30:00, Watchtower Study -> 60:00) so the
                                        // dedicated full-screen timer reflects it immediately --
                                        // skipped for songs, which have no duration set.
                                        if (TimerBackend && model.durationMinutes > 0)
                                            TimerBackend.targetDurationSeconds = model.durationMinutes * 60
                                    }
                                }
                            }
                        }
                    }

                    // Clear All Confirmation Dialog
                    ThemedDialog {
                        id: clearDialog
                        title: "Clear All Media"
                        acceptText: "CLEAR"; acceptColor: Theme.accentRed
                        contentItem: Label {
                            text: "Remove all linked media from both the midweek and weekend schedules?"
                            color: Theme.textPrimary; font.pixelSize: Theme.textMd; wrapMode: Text.WordWrap
                            leftPadding: 20; rightPadding: 20; topPadding: 4; bottomPadding: 12
                        }
                        onAccepted: {
                            if (MediaFlowBackend && MediaFlowBackend.meetingSchedule)
                                MediaFlowBackend.meetingSchedule.clearAllMedia()
                        }
                    }
                }

                // TIMERS & BGM
                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 350; Layout.minimumWidth: 260; spacing: Theme.space3
                    TimerPanel { }
                    BackgroundMusicPanel { }
                }
            }
        }
    }

    // Toast Notification
    Rectangle {
        id: toast
        anchors.bottom: parent.bottom; anchors.bottomMargin: 120
        anchors.horizontalCenter: parent.horizontalCenter
        width: toastLabel.width + 40; height: 44; radius: 22
        color: Theme.accentRed; opacity: 0; z: 9999
        border.color: "white"; border.width: 1

        Label {
            id: toastLabel; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: Theme.textMd
        }

        function show(msg) {
            toastLabel.text = msg
            anim.restart()
        }

        SequentialAnimation on opacity {
            id: anim
            NumberAnimation { to: 1; duration: 200 }
            PauseAnimation { duration: 4000 }
            NumberAnimation { to: 0; duration: 800 }
        }
    }

    Connections {
        target: (MediaFlowBackend || null)
        function onCurrentLanguageCodeChanged() {
            langCombo.syncFromBackend()
        }
        function onSongNotFound(num) {
            toast.show("SONG " + num + " NOT FOUND LOCALLY. PLEASE DOWNLOAD IN JW LIBRARY.")
        }
        function onSongNotFoundInLanguage(num, languageName) {
            toast.show("Song " + num + " not found in " + languageName + ".")
        }
    }

    Popup {
        id: manualSongSelector
        anchors.centerIn: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "transparent" }

        SongSelectorPopup {
            targetSegmentId: manualSongSegmentId
            onSongLinked: manualSongSelector.close()
        }
    }

    ThemedDialog {
        id: vcamWarningDialog
        title: "Virtual Camera Driver Not Found"
        showCancel: false
        contentItem: Label {
            text: "Install OBS Studio (obsproject.com) once — it registers the “OBS Virtual Camera” driver. You don't need to open OBS itself; MediaFlow feeds it directly. Then try Broadcast to Zoom again. See BUILD.md for details."
            color: Theme.textPrimary
            font.pixelSize: Theme.textMd
            wrapMode: Text.WordWrap
            width: 320
            leftPadding: 20; rightPadding: 20; topPadding: 4; bottomPadding: 12
        }
    }

}
