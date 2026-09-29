import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0
import "components"

// Deliberately a plain Window (default OS chrome -- minimize/maximize/close
// all work normally) rather than the frameless Qt.Tool windows used for
// VirtualCameraWindow/AudienceWindow, since those need to stay out of the
// operator's way while rendering; Settings is a normal UI surface the user
// explicitly opens and closes like any other app window.
Window {
    id: settingsRoot
    width: 640
    height: 620
    minimumWidth: 560
    minimumHeight: 520
    // QML's Window defaults to visible:true, so without this it opens on
    // every app launch instead of staying hidden until the gear icon is
    // clicked (confirmed live: it appeared unprompted on startup).
    visible: false
    title: qsTr("MediaFlow — Settings")
    color: Theme.bg

    property string newLangName: ""
    property string newLangCode: ""
    property string langError: ""

    // Single source of truth for both the SHORTCUTS (rebindable) and MANUAL
    // (reference) sections below, so the two never drift out of sync.
    readonly property var shortcutActions: [
        { action: "cut", label: "Cut Live", description: "Instantly cut Program to whatever's staged." },
        { action: "take", label: "Take Live", description: "Crossfade Program to whatever's staged." },
        { action: "pauseProgram", label: "Pause / Resume Program", description: "Pause or resume whatever's currently playing live." },
        { action: "goLive", label: "Go Live", description: "Marks the meeting itself as live (header status)." },
        { action: "broadcastZoom", label: "Broadcast to Zoom", description: "Starts/stops sending video to Zoom via the virtual camera." },
        { action: "webcamToggle", label: "Webcam Force-Off", description: "Forces the Zoom feed to black instead of the webcam fallback." },
        { action: "extendFeed", label: "Extend Feed", description: "Opens/closes the audience-facing second-display window." },
    ]

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + Theme.space6 * 2
        clip: true

        ColumnLayout {
            id: contentColumn
            x: Theme.space6; y: Theme.space6
            width: parent.width - Theme.space6 * 2
            spacing: Theme.space6

            // ── Displays ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space3
                Label { text: "DISPLAYS"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textSm; font.letterSpacing: 1 }
                Label {
                    text: "Extended Feed and the full-screen timer are independent -- each can be sent to its own monitor."
                    color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }

                Label { text: "Extended Feed"; color: Theme.textPrimary; font.pixelSize: Theme.textSm }
                ScreenPicker {
                    currentValue: (MediaFlowBackend || {}).extendedFeedScreenIndex
                    onValueChosen: (index) => { MediaFlowBackend.extendedFeedScreenIndex = index }
                }

                Label { text: "Timer (full-screen mode)"; color: Theme.textPrimary; font.pixelSize: Theme.textSm; Layout.topMargin: Theme.space2 }
                Label {
                    text: "No automatic guessing here -- pick the exact screen the full-screen timer should always use."
                    color: Theme.textDim; font.pixelSize: Theme.textXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                ScreenPicker {
                    allowAutomatic: false
                    currentValue: (MediaFlowBackend || {}).timerScreenIndex
                    onValueChosen: (index) => { MediaFlowBackend.timerScreenIndex = index }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder }

            // ── Webcam ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space3
                Label { text: "WEBCAM"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textSm; font.letterSpacing: 1 }
                Label {
                    text: "Which camera feeds the Zoom broadcast whenever nothing else is on Program. Toggle it off entirely from the header."
                    color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                CameraPicker {
                    currentDevice: (MediaFlowBackend || {}).programCameraDevice
                    onDeviceChosen: (device) => { MediaFlowBackend.setProgramCameraDevice(device) }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder }

            // ── Shortcuts ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space3
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "SHORTCUTS"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textSm; font.letterSpacing: 1 }
                    Item { Layout.fillWidth: true }
                    PillButton {
                        text: "RESET TO DEFAULTS"; accentColor: Theme.textSecondary
                        onClicked: MediaFlowBackend.resetShortcutKeys()
                    }
                }
                Label {
                    text: "Click a key to rebind it, then press the new key. Backspace/Delete unbinds it; Escape cancels. Only one action can hold a given key at a time -- claiming a key that's already used moves it here."
                    color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }

                Repeater {
                    model: settingsRoot.shortcutActions
                    delegate: Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 44
                        radius: Theme.radiusSm; color: Theme.surfaceRaised
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 8
                            Label { text: modelData.label; color: Theme.textPrimary; font.pixelSize: Theme.textMd; Layout.fillWidth: true }
                            ShortcutCapture { action: modelData.action }
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder }

            // ── Background Music ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space3
                Label { text: "BACKGROUND MUSIC"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textSm; font.letterSpacing: 1 }
                Label {
                    text: "By default, music is pulled automatically from your Music and JW Library folders. Turn this on to use a separate folder instead."
                    color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.space3
                    Switch {
                        id: bgmSwitch
                        checked: (MediaFlowBackend || {}).bgmUseCustomFolder || false
                        onToggled: MediaFlowBackend.bgmUseCustomFolder = checked
                    }
                    Label { text: "Use a custom folder"; color: Theme.textPrimary; font.pixelSize: Theme.textMd }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.space3
                    visible: bgmSwitch.checked
                    Label {
                        Layout.fillWidth: true
                        text: (MediaFlowBackend || {}).bgmCustomFolder || "No folder chosen"
                        color: (MediaFlowBackend || {}).bgmCustomFolder ? Theme.textPrimary : Theme.textFaint
                        font.pixelSize: Theme.textSm; elide: Text.ElideMiddle
                    }
                    PillButton {
                        text: "BROWSE…"; accentColor: Theme.accentBlue
                        onClicked: MediaFlowBackend.browseBgmFolder()
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder }

            // ── Extended Feed Background ──
            ColumnLayout {
                id: backgroundSection
                Layout.fillWidth: true
                spacing: Theme.space3

                readonly property bool hasBackground: ((MediaFlowBackend || {}).extendedFeedBackgroundPath || "") !== ""
                readonly property bool isVideo: (MediaFlowBackend || {}).extendedFeedBackgroundType === "video"

                Label { text: "EXTENDED FEED BACKGROUND"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textSm; font.letterSpacing: 1 }
                Label {
                    text: "Shown on the Extended Feed screen whenever nothing is live, instead of a plain black screen. Pick an image or a short looping video."
                    color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.space3

                    Rectangle {
                        Layout.preferredWidth: 64; Layout.preferredHeight: 40
                        radius: Theme.radiusSm; color: Theme.panelBgDark; border.color: Theme.panelBorder
                        clip: true
                        Image {
                            anchors.fill: parent
                            visible: backgroundSection.hasBackground && !backgroundSection.isVideo
                            fillMode: Image.PreserveAspectCrop; asynchronous: true
                            source: visible ? "file:///" + (MediaFlowBackend || {}).extendedFeedBackgroundPath : ""
                        }
                        BroadcastIcon {
                            anchors.centerIn: parent
                            visible: backgroundSection.hasBackground && backgroundSection.isVideo
                            name: "video"; iconSize: 18; color: Theme.textSecondary
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: !backgroundSection.hasBackground
                            text: "NONE"; color: Theme.textFaint; font.pixelSize: 9; font.bold: true
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: {
                            let p = (MediaFlowBackend || {}).extendedFeedBackgroundPath || ""
                            if (p === "") return "No background set"
                            return p.substring(Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\")) + 1)
                        }
                        color: ((MediaFlowBackend || {}).extendedFeedBackgroundPath || "") !== "" ? Theme.textPrimary : Theme.textFaint
                        font.pixelSize: Theme.textSm; elide: Text.ElideMiddle
                    }

                    PillButton {
                        text: "BROWSE…"; accentColor: Theme.accentBlue
                        onClicked: MediaFlowBackend.browseExtendedFeedBackground()
                    }
                    PillButton {
                        text: "CLEAR"; accentColor: Theme.accentRed
                        visible: ((MediaFlowBackend || {}).extendedFeedBackgroundPath || "") !== ""
                        onClicked: MediaFlowBackend.clearExtendedFeedBackground()
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder }

            // ── Weekly Workbook ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space3
                Label { text: "WEEKLY WORKBOOK"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textSm; font.letterSpacing: 1 }
                Label {
                    text: "Automatically fetches this week's song numbers, Watchtower Study article title, and links the matching local videos -- checked on launch and periodically after that."
                    color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.space3
                    Label {
                        Layout.fillWidth: true
                        text: (MediaFlowBackend || {}).workbookStatus || "Not checked yet"
                        color: Theme.textPrimary; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap
                    }
                    PillButton {
                        text: "REFRESH NOW"; accentColor: Theme.accentBlue
                        onClicked: MediaFlowBackend.refreshWorkbook()
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder }

            // ── Languages ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space3
                Label { text: "LANGUAGES"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textSm; font.letterSpacing: 1 }
                Label {
                    text: "Media files are filtered by a short code inside the filename, e.g. \"_E_\" for English. Add a language and the code used in your files (1-3 letters, any case) to make it filterable and available in the header's language menu."
                    color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }

                Repeater {
                    model: (MediaFlowBackend || {}).getSupportedLanguages ? MediaFlowBackend.getSupportedLanguages() : []
                    delegate: Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 40
                        radius: Theme.radiusSm; color: Theme.surfaceRaised
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8; spacing: 8
                            Label { text: modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.textMd; Layout.fillWidth: true }
                            Rectangle {
                                width: 40; height: 22; radius: 11; color: Theme.panelBgDark; border.color: Theme.panelBorder
                                Label { anchors.centerIn: parent; text: modelData.code; color: Theme.accentBlue; font.pixelSize: Theme.textXs; font.bold: true }
                            }
                            // Built-ins (from SongSearchUtils::supportedLanguages,
                            // currently 6: English/Ewe/Twi/Ga/French/Spanish)
                            // aren't deletable -- only user-added ones are.
                            Rectangle {
                                width: 26; height: 26; radius: 6; color: "transparent"
                                visible: index >= 6 // built-in count
                                BroadcastIcon { anchors.centerIn: parent; name: "trash"; iconSize: 12; color: Theme.accentRed }
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: MediaFlowBackend.removeCustomLanguage(modelData.code)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 78
                    radius: Theme.radius; color: Theme.panelBgDark; border.color: Theme.panelBorder
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 6
                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            TextField {
                                id: nameField
                                Layout.fillWidth: true
                                placeholderText: "Language name (e.g. Ewe)"
                                color: Theme.textPrimary; font.pixelSize: Theme.textSm
                                background: Rectangle { color: Theme.surfaceRaised; radius: Theme.radiusSm; border.color: Theme.panelBorder }
                            }
                            TextField {
                                id: codeField
                                Layout.preferredWidth: 100
                                placeholderText: "Code (e.g. Ew)"
                                maximumLength: 3
                                color: Theme.textPrimary; font.pixelSize: Theme.textSm
                                background: Rectangle { color: Theme.surfaceRaised; radius: Theme.radiusSm; border.color: Theme.panelBorder }
                            }
                            PillButton {
                                text: "SAVE"; primary: true; accentColor: Theme.accentEmerald
                                onClicked: {
                                    if (nameField.text.trim() === "" || codeField.text.trim() === "") {
                                        settingsRoot.langError = "Enter both a name and a code."
                                        return
                                    }
                                    MediaFlowBackend.addCustomLanguage(nameField.text, codeField.text)
                                    settingsRoot.langError = ""
                                    nameField.text = ""; codeField.text = ""
                                }
                            }
                        }
                        Label {
                            text: settingsRoot.langError
                            color: Theme.accentRed; font.pixelSize: Theme.textXs
                            visible: text.length > 0
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder }

            // ── Manual ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space3
                Label { text: "MANUAL"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textSm; font.letterSpacing: 1 }

                Label { text: "Broadcast controls"; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm; Layout.topMargin: Theme.space2 }
                Repeater {
                    model: settingsRoot.shortcutActions
                    delegate: ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            Label { text: modelData.label; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm }
                            Rectangle {
                                visible: keyLabel.text.length > 0
                                Layout.preferredWidth: keyLabel.implicitWidth + 12; Layout.preferredHeight: 18
                                radius: 9; color: Theme.panelBgDark; border.color: Theme.panelBorder
                                Label {
                                    id: keyLabel
                                    anchors.centerIn: parent
                                    text: {
                                        const keys = (MediaFlowBackend || {}).shortcutKeys
                                        return keys ? (keys[modelData.action] || "") : ""
                                    }
                                    color: Theme.accentBlue; font.pixelSize: Theme.textXs; font.bold: true
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.description
                            color: Theme.textDim; font.pixelSize: Theme.textXs; wrapMode: Text.WordWrap
                        }
                    }
                }

                Label { text: "Quick Fetch & the meeting sequence"; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm; Layout.topMargin: Theme.space3 }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Theme.textDim; font.pixelSize: Theme.textXs
                    text: "Clicking a card in QUICK FETCH or Pins only previews it -- it does not change any segment's linked media. Select a segment in the SEQUENCE panel, then hover a card and click the green \"+\" that appears to actually link it to that segment."
                }

                Label { text: "Zoom broadcasting"; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm; Layout.topMargin: Theme.space3 }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Theme.textDim; font.pixelSize: Theme.textXs
                    text: "Click BROADCAST TO ZOOM, then in Zoom's own camera picker choose \"OBS Virtual Camera.\" Whatever's on Program shows there; when nothing is, it falls back to the webcam unless WEBCAM FORCE-OFF is on. Room audio always stays on your speakers -- nothing is sent to Zoom through this app."
                }

                Label { text: "Folder slideshows"; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm; Layout.topMargin: Theme.space3 }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Theme.textDim; font.pixelSize: Theme.textXs
                    text: "Add several videos and/or images to a pin folder, then drag that folder onto either monitor -- it puts the first one live immediately and keeps going through the rest on its own (a video advances when it ends; an image advances after 6s) until you hit STOP. Every Take, including each step of the slideshow, fades over about 2 seconds instead of cutting instantly; CUT is still instant. Clicking any other card manually stops the slideshow."
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.panelBorder }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "MediaFlow Suite v1.0.0"
                    color: Theme.textFaint; font.pixelSize: Theme.textXs
                }
                Item { Layout.fillWidth: true }
            }
        }
    }

    component ScreenPicker: Rectangle {
        id: pickerRoot
        property int currentValue: -1
        // Extended Feed keeps "Automatic" as a sensible default; the
        // full-screen timer doesn't (see the Timer instance above) -- an
        // unattended guess there can land the countdown on the wrong
        // screen with no visual cue anything's wrong.
        property bool allowAutomatic: true
        signal valueChosen(int index)

        Layout.fillWidth: true; Layout.preferredHeight: 44
        radius: Theme.radius; color: Theme.surfaceRaised; border.color: Theme.panelBorder

        ComboBox {
            id: combo
            anchors.fill: parent; anchors.margins: 6
            flat: true
            textRole: "label"
            model: {
                let list = pickerRoot.allowAutomatic ? [{ index: -1, label: "Automatic (recommended)" }] : []
                let screens = (MediaFlowBackend || {}).availableScreens ? MediaFlowBackend.availableScreens() : []
                for (let i = 0; i < screens.length; i++)
                    list.push({ index: screens[i].index, label: screens[i].name })
                return list
            }
            Component.onCompleted: {
                for (let i = 0; i < model.length; i++) {
                    if (model[i].index === pickerRoot.currentValue) { currentIndex = i; break }
                }
            }
            onActivated: (idx) => pickerRoot.valueChosen(model[idx].index)
            contentItem: Label {
                text: combo.currentText || (pickerRoot.allowAutomatic ? "Automatic (recommended)" : "Choose a screen…")
                color: Theme.textPrimary; font.pixelSize: Theme.textMd
                verticalAlignment: Text.AlignVCenter; leftPadding: 8
            }
            background: Rectangle { color: "transparent" }
        }
    }

    // Mirrors ScreenPicker above, bound to the CameraDeviceModel exposed as
    // MediaFlowBackend.cameraDevices (a real QAbstractListModel, unlike
    // ScreenPicker's plain JS array) instead of a screen index.
    component CameraPicker: Rectangle {
        id: camPickerRoot
        property var currentDevice: null
        signal deviceChosen(var device)

        Layout.fillWidth: true; Layout.preferredHeight: 44
        radius: Theme.radius; color: Theme.surfaceRaised; border.color: Theme.panelBorder

        ComboBox {
            id: camCombo
            anchors.fill: parent; anchors.margins: 6
            flat: true
            textRole: "deviceName"
            model: (MediaFlowBackend || {}).cameraDevices || null

            function syncFromCurrent() {
                if (!camPickerRoot.currentDevice || !model) return
                const targetName = camPickerRoot.currentDevice.description || ""
                for (let i = 0; i < model.rowCount(); i++) {
                    if (model.nameAt(i) === targetName) { currentIndex = i; return }
                }
            }
            Component.onCompleted: syncFromCurrent()
            Connections {
                target: camPickerRoot
                function onCurrentDeviceChanged() { camCombo.syncFromCurrent() }
            }

            onActivated: (idx) => {
                const id = model.deviceIdAt(idx)
                camPickerRoot.deviceChosen(model.deviceForId(id))
            }
            contentItem: Label {
                text: camCombo.currentText || "No camera found"
                color: Theme.textPrimary; font.pixelSize: Theme.textMd
                verticalAlignment: Text.AlignVCenter; leftPadding: 8
            }
            background: Rectangle { color: "transparent" }
        }
    }

    // A small rebindable key button: click to arm it, then press the
    // desired key. Only single letters/digits/function keys (with optional
    // Ctrl/Alt/Shift) are accepted -- arrow keys, Tab, etc. are ignored so a
    // stray press while armed can't silently bind something unusable.
    component ShortcutCapture: Rectangle {
        id: captureRoot
        property string action: ""
        property bool listening: false

        readonly property string currentKey: {
            const keys = (MediaFlowBackend || {}).shortcutKeys
            return keys ? (keys[action] || "") : ""
        }

        width: 96; height: 32; radius: Theme.radiusSm
        color: listening ? Theme.accentBlue : Theme.surfaceRaised
        border.color: listening ? Theme.accentBlue : Theme.panelBorder

        Label {
            anchors.centerIn: parent
            text: captureRoot.listening ? "Press a key…" : (captureRoot.currentKey || "Unbound")
            color: captureRoot.listening ? "white" : (captureRoot.currentKey ? Theme.textPrimary : Theme.textFaint)
            font.pixelSize: Theme.textXs; font.bold: true
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: { captureRoot.listening = true; captureRoot.forceActiveFocus() }
        }

        onActiveFocusChanged: if (!activeFocus) listening = false

        Keys.onPressed: (event) => {
            if (!captureRoot.listening) return
            event.accepted = true

            if (event.key === Qt.Key_Escape) { captureRoot.listening = false; return }
            if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
                MediaFlowBackend.setShortcutKey(captureRoot.action, "")
                captureRoot.listening = false
                return
            }
            if (event.key === Qt.Key_Shift || event.key === Qt.Key_Control
                || event.key === Qt.Key_Alt || event.key === Qt.Key_Meta)
                return // modifier alone -- keep listening for the real key

            let parts = []
            if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl")
            if (event.modifiers & Qt.AltModifier) parts.push("Alt")
            if (event.modifiers & Qt.ShiftModifier) parts.push("Shift")

            let keyName = ""
            if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F12) keyName = "F" + (event.key - Qt.Key_F1 + 1)
            else if (event.key === Qt.Key_Space) keyName = "Space"
            else if (event.text && event.text.trim().length === 1) keyName = event.text.toUpperCase()
            else return // unsupported key (arrows, Tab, ...) -- stay listening

            parts.push(keyName)
            MediaFlowBackend.setShortcutKey(captureRoot.action, parts.join("+"))
            captureRoot.listening = false
        }
    }
}
