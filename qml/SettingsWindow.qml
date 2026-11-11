import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0
import "components"

// A same-window overlay instead of a separate OS Window. Opening/closing a
// real Window -- even a small one like Settings previously was -- is an
// OS-level activation event, and Qt Quick's threaded render loop syncs
// every top-level window against one shared GUI thread once per frame, so
// that event could stall rendering across every OTHER window in the
// process too (confirmed this session as a real contributor to Extended
// Feed stutter/blank-frame flashes). A Popup lives entirely inside the
// main window's own scene graph -- opening/closing it never touches the
// OS at all, eliminating that cause outright for Settings specifically.
Popup {
    id: settingsRoot
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: Math.min(820, (Overlay.overlay ? Overlay.overlay.width : 880) - 48)
    height: Math.min(620, (Overlay.overlay ? Overlay.overlay.height : 680) - 56)
    visible: false
    modal: true
    dim: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    Overlay.modal: Rectangle { color: "#BD000000" }
    background: Rectangle {
        color: "#0C0E14"
        radius: 14
        border.color: "#2C3140"
        border.width: 1
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutQuad }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 110; easing.type: Easing.InQuad }
    }

    property int activeSection: 0
    property string newLangName: ""
    property string newLangCode: ""
    property string langError: ""

    readonly property var navItems: [
        { label: "Displays", icon: "screen" },
        { label: "Webcam", icon: "webcam" },
        { label: "Shortcuts", icon: "bolt" },
        { label: "Music & Feed", icon: "music" },
        { label: "App Updates", icon: "refresh" },
        { label: "Workbook & Langs", icon: "globe" },
        { label: "User Guide", icon: "book" }
    ]

    function formatDisplayName(rawName) {
        if (!rawName) return ""
        let cleaned = rawName.replace(/^\\\\\.\\/, "")
        cleaned = cleaned.replace(/^DISPLAY(\d+)/i, "Display $1")
        cleaned = cleaned.replace(/(\d+)[x\u00d7](\d+)/, "$1 \u00d7 $2")
        return cleaned
    }

    // Single source of truth for both the SHORTCUTS (rebindable) and MANUAL
    // (reference) sections below, so the two never drift out of sync.
    readonly property var shortcutActions: [
        { action: "cut", label: "Cut Live", description: "Instantly cut Program to whatever's staged." },
        { action: "take", label: "Take Live", description: "Crossfade Program to whatever's staged." },
        { action: "pauseProgram", label: "Pause / Resume Program", description: "Pause or resume whatever's currently playing live." },
        { action: "goLive", label: "Go Live", description: "Marks the meeting itself as live (header status)." },
        { action: "broadcastZoom", label: "Broadcast to Zoom", description: "Starts/stops sending video to Zoom via the virtual camera." },
        { action: "webcamToggle", label: "Webcam Force-Off", description: "Forces the Zoom feed to black instead of the webcam fallback." },
        { action: "extendFeed", label: "Extend Feed", description: "Opens/closes the audience-facing second-display window." }
    ]

    contentItem: ColumnLayout {
        spacing: 0

        // Replaces the OS title bar a real Window would have had -- a
        // Popup has no window chrome of its own, so Settings needs its own
        // title/close affordance now.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 14

                Label {
                    text: "SETTINGS"
                    color: Theme.textPrimary
                    font.family: Theme.sansFont
                    font.bold: true
                    font.pixelSize: Theme.textMd
                    font.letterSpacing: 1.2
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: 30; height: 30; radius: 8
                    color: closeMa.containsMouse ? "#1E222E" : "#13161F"
                    border.color: closeMa.containsMouse ? "#383F52" : "#252936"
                    border.width: 1
                    BroadcastIcon { anchors.centerIn: parent; name: "close"; iconSize: 11; color: Theme.textSecondary }
                    MouseArea {
                        id: closeMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsRoot.close()
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#1F2430"
            }
        }

        // Two-column layout: left navigation rail + right scrollable section cards
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left Category Rail
            Rectangle {
                Layout.preferredWidth: 196
                Layout.fillHeight: true
                color: "#090B10"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Repeater {
                        model: settingsRoot.navItems
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            readonly property bool active: settingsRoot.activeSection === index

                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            radius: 8
                            color: active ? "#172542" : (navMa.containsMouse ? "#141722" : "transparent")
                            border.color: active ? "#3B82F6" : "transparent"
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 120 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 10
                                spacing: 10

                                BroadcastIcon {
                                    name: modelData.icon
                                    iconSize: 13
                                    color: active ? "#60A5FA" : (navMa.containsMouse ? Theme.textPrimary : Theme.textSecondary)
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: active ? "#FFFFFF" : (navMa.containsMouse ? Theme.textPrimary : Theme.textSecondary)
                                    font.family: Theme.sansFont
                                    font.pixelSize: Theme.textSm
                                    font.bold: active
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: navMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsRoot.activeSection = index
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#1B202C"
                        Layout.bottomMargin: 6
                    }

                    Label {
                        Layout.leftMargin: 6
                        Layout.bottomMargin: 4
                        text: "MediaFlow v" + ((MediaFlowBackend || {}).updater
                                                ? MediaFlowBackend.updater.currentVersion : "")
                        color: Theme.textDim
                        font.family: Theme.monoFont
                        font.pixelSize: Theme.textXs
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: "#1F2430"
                }
            }

            // Right Category Pages
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: settingsRoot.activeSection

                // ── 0: Displays ──
                SettingsPage {
                    SettingsCard {
                        Label { text: "DISPLAYS"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }
                        Label {
                            text: "Extended Feed and the full-screen timer are independent — each can be sent to its own monitor."
                            color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }

                        Label { text: "Extended Feed"; color: Theme.textPrimary; font.pixelSize: Theme.textSm; font.bold: true; Layout.topMargin: 4 }
                        ScreenPicker {
                            currentValue: (MediaFlowBackend || {}).extendedFeedScreenIndex
                            onValueChosen: (index) => { MediaFlowBackend.extendedFeedScreenIndex = index }
                        }

                        Label { text: "Timer (full-screen mode)"; color: Theme.textPrimary; font.pixelSize: Theme.textSm; font.bold: true; Layout.topMargin: 8 }
                        Label {
                            text: "Pick the exact screen the full-screen timer should always use."
                            color: Theme.textDim; font.pixelSize: Theme.textXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                        ScreenPicker {
                            allowAutomatic: false
                            currentValue: (MediaFlowBackend || {}).timerScreenIndex
                            onValueChosen: (index) => { MediaFlowBackend.timerScreenIndex = index }
                        }
                    }
                }

                // ── 1: Webcam ──
                SettingsPage {
                    SettingsCard {
                        Label { text: "WEBCAM FALLBACK"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }
                        Label {
                            text: "Choose which camera feeds the Zoom broadcast whenever nothing else is on Program. Toggle it off anytime from the top header."
                            color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                        CameraPicker {
                            Layout.topMargin: 4
                            currentDevice: (MediaFlowBackend || {}).programCameraDevice
                            onDeviceChosen: (device) => { MediaFlowBackend.setProgramCameraDevice(device) }
                        }
                    }
                }

                // ── 2: Shortcuts ──
                SettingsPage {
                    SettingsCard {
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "KEYBOARD SHORTCUTS"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }
                            Item { Layout.fillWidth: true }
                            PillButton {
                                text: "RESET TO DEFAULTS"; accentColor: Theme.textSecondary
                                implicitHeight: 28; implicitWidth: 140
                                onClicked: MediaFlowBackend.resetShortcutKeys()
                            }
                        }
                        Label {
                            text: "Click a key to rebind it, then press the new key. Backspace/Delete unbinds it; Escape cancels. Claiming an already-used key moves it here."
                            color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }

                        Repeater {
                            model: settingsRoot.shortcutActions
                            delegate: Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 44
                                radius: Theme.radiusSm; color: "#191C27"; border.color: "#262B38"; border.width: 1
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 10; spacing: 8
                                    Label { text: modelData.label; color: Theme.textPrimary; font.pixelSize: Theme.textMd; Layout.fillWidth: true }
                                    ShortcutCapture { action: modelData.action }
                                }
                            }
                        }
                    }
                }

                // ── 3: Music & Feed ──
                SettingsPage {
                    SettingsCard {
                        Label { text: "BACKGROUND MUSIC"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }
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
                                text: "BROWSE\u2026"; accentColor: Theme.accentBlue; implicitHeight: 32
                                onClicked: MediaFlowBackend.browseBgmFolder()
                            }
                        }
                    }

                    SettingsCard {
                        id: backgroundSection
                        readonly property bool hasBackground: ((MediaFlowBackend || {}).extendedFeedBackgroundPath || "") !== ""
                        readonly property bool isVideo: (MediaFlowBackend || {}).extendedFeedBackgroundType === "video"

                        Label { text: "EXTENDED FEED BACKGROUND"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }
                        Label {
                            text: "Shown on the Extended Feed screen whenever nothing is live, instead of a plain black screen. Pick an image or a short looping video."
                            color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true; spacing: Theme.space3; Layout.topMargin: 4

                            Rectangle {
                                Layout.preferredWidth: 68; Layout.preferredHeight: 42
                                radius: Theme.radiusSm; color: "#0B0D12"; border.color: "#282D3B"; border.width: 1
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    visible: backgroundSection.hasBackground && !backgroundSection.isVideo
                                    fillMode: Image.PreserveAspectCrop; asynchronous: true
                                    sourceSize.width: 136; sourceSize.height: 84
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
                                    text: "NONE"; color: Theme.textDim; font.pixelSize: Theme.textXs; font.bold: true
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: {
                                    let p = (MediaFlowBackend || {}).extendedFeedBackgroundPath || ""
                                    if (p === "") return "No background set"
                                    return p.substring(Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\")) + 1)
                                }
                                color: backgroundSection.hasBackground ? Theme.textPrimary : Theme.textDim
                                font.pixelSize: Theme.textSm; elide: Text.ElideMiddle
                            }

                            PillButton {
                                text: "BROWSE\u2026"; accentColor: Theme.accentBlue; implicitHeight: 32
                                onClicked: MediaFlowBackend.browseExtendedFeedBackground()
                            }
                            PillButton {
                                text: "CLEAR"; accentColor: Theme.accentRed; implicitHeight: 32; implicitWidth: 76
                                visible: backgroundSection.hasBackground
                                onClicked: MediaFlowBackend.clearExtendedFeedBackground()
                            }
                        }
                    }
                }

                // ── 4: App Updates ──
                SettingsPage {
                    SettingsCard {
                        id: updateSection
                        readonly property var updater: (MediaFlowBackend || {}).updater

                        Label { text: "APP UPDATES"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }
                        Label {
                            Layout.fillWidth: true
                            text: "Current version " + (updateSection.updater ? updateSection.updater.currentVersion : "")
                            color: Theme.textDim; font.pixelSize: Theme.textSm
                        }
                        Label {
                            Layout.fillWidth: true
                            text: updateSection.updater ? updateSection.updater.status : "Update checker unavailable"
                            color: updateSection.updater && (updateSection.updater.updateAvailable || updateSection.updater.downloadReady)
                                   ? Theme.accentBlue
                                   : (updateSection.updater && updateSection.updater.downloadFailed ? Theme.accentRed : Theme.textPrimary)
                            font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap
                        }
                        ProgressBar {
                            id: updateProgress
                            Layout.fillWidth: true
                            implicitHeight: 8
                            visible: updateSection.updater ? updateSection.updater.downloading : false
                            from: 0; to: 100
                            indeterminate: updateSection.updater ? (updateSection.updater.downloading && updateSection.updater.downloadProgress <= 0) : false
                            value: updateSection.updater ? Math.max(0, updateSection.updater.downloadProgress) : 0

                            background: Rectangle {
                                radius: 4
                                color: "#1E222E"
                            }
                            contentItem: Item {
                                Rectangle {
                                    width: updateProgress.indeterminate ? parent.width * 0.35 : updateProgress.visualPosition * parent.width
                                    height: parent.height
                                    radius: 4
                                    color: Theme.accentBlue
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: updateSection.updater ? (updateSection.updater.downloading && updateSection.updater.downloadDetail.length > 0) : false
                            text: updateSection.updater ? updateSection.updater.downloadDetail : ""
                            color: Theme.textSecondary
                            font.pixelSize: Theme.textXs
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space3
                            PillButton {
                                text: updateSection.updater && updateSection.updater.checking ? "CHECKING\u2026" : "CHECK FOR UPDATES"
                                accentColor: Theme.accentBlue
                                implicitWidth: 156; implicitHeight: 34
                                enabled: updateSection.updater && !updateSection.updater.checking
                                         && !updateSection.updater.downloading && !updateSection.updater.downloadReady
                                onClicked: updateSection.updater.checkForUpdates()
                            }
                            PillButton {
                                visible: updateSection.updater ? updateSection.updater.updateAvailable && !updateSection.updater.downloadReady && !updateSection.updater.downloading && !updateSection.updater.downloadFailed : false
                                text: "DOWNLOAD UPDATE"; primary: true; accentColor: Theme.accentBlue; implicitWidth: 146; implicitHeight: 34
                                onClicked: updateSection.updater.downloadUpdate()
                            }
                            PillButton {
                                visible: updateSection.updater ? updateSection.updater.downloadFailed && !updateSection.updater.downloading : false
                                text: "RETRY DOWNLOAD"; primary: true; accentColor: Theme.accentBlue; implicitWidth: 146; implicitHeight: 34
                                onClicked: updateSection.updater.downloadUpdate()
                            }
                            PillButton {
                                visible: updateSection.updater ? updateSection.updater.downloading : false
                                text: "CANCEL"; accentColor: Theme.accentRed; implicitWidth: 88; implicitHeight: 34
                                onClicked: updateSection.updater.cancelDownload()
                            }
                            PillButton {
                                visible: updateSection.updater ? updateSection.updater.downloadReady : false
                                text: "INSTALL & RESTART"; primary: true; accentColor: Theme.accentEmerald; implicitWidth: 152; implicitHeight: 34
                                onClicked: installUpdateDialog.open()
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: updateSection.updater ? updateSection.updater.updateAvailable : false
                            text: updateSection.updater ? "Available version: " + updateSection.updater.latestVersion : ""
                            color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm
                        }
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 110
                            visible: updateSection.updater ? updateSection.updater.updateAvailable : false
                            clip: true
                            ScrollBar.vertical: SlimScrollBar { }
                            TextArea {
                                readOnly: true
                                wrapMode: TextEdit.Wrap
                                text: updateSection.updater ? updateSection.updater.releaseNotes : ""
                                color: Theme.textSecondary
                                font.pixelSize: Theme.textXs
                                background: Rectangle { color: "#0B0D12"; radius: Theme.radiusSm; border.color: "#222633"; border.width: 1 }
                            }
                        }
                    }
                }

                // ── 5: Workbook & Languages ──
                SettingsPage {
                    SettingsCard {
                        Label { text: "WEEKLY WORKBOOK"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }
                        Label {
                            text: "Automatically fetches this week's song numbers, Watchtower Study article title, and links the matching local videos — checked on launch and periodically after that."
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
                                text: "REFRESH NOW"; accentColor: Theme.accentBlue; implicitHeight: 32; implicitWidth: 118
                                onClicked: MediaFlowBackend.refreshWorkbook()
                            }
                        }
                    }

                    SettingsCard {
                        Label { text: "LANGUAGES"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }
                        Label {
                            text: "Media files are filtered by a short code inside the filename, e.g. \"_E_\" for English. Add a language and its code (1-3 letters) to make it selectable."
                            color: Theme.textDim; font.pixelSize: Theme.textSm; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }

                        Repeater {
                            model: (MediaFlowBackend || {}).getSupportedLanguages ? MediaFlowBackend.getSupportedLanguages() : []
                            delegate: Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 40
                                radius: Theme.radiusSm; color: "#191C27"; border.color: "#262B38"; border.width: 1
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8; spacing: 8
                                    Label { text: modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.textMd; Layout.fillWidth: true }
                                    Rectangle {
                                        width: 40; height: 22; radius: 11; color: "#0D0F16"; border.color: "#2A3040"
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
                            Layout.fillWidth: true; Layout.preferredHeight: 68
                            radius: Theme.radius; color: "#0D0F16"; border.color: "#262B38"; border.width: 1
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 8
                                    TextField {
                                        id: nameField
                                        Layout.fillWidth: true
                                        implicitHeight: 34
                                        leftPadding: 10
                                        placeholderText: "Language name (e.g. Ewe)"
                                        placeholderTextColor: "#71717A"
                                        color: Theme.textPrimary; font.pixelSize: Theme.textSm
                                        background: Rectangle { color: "#181B25"; radius: Theme.radiusSm; border.color: nameField.activeFocus ? Theme.accentBlue : "#2A3040" }
                                    }
                                    TextField {
                                        id: codeField
                                        Layout.preferredWidth: 108
                                        implicitHeight: 34
                                        leftPadding: 10
                                        placeholderText: "Code (e.g. Ew)"
                                        placeholderTextColor: "#71717A"
                                        maximumLength: 3
                                        color: Theme.textPrimary; font.pixelSize: Theme.textSm
                                        background: Rectangle { color: "#181B25"; radius: Theme.radiusSm; border.color: codeField.activeFocus ? Theme.accentBlue : "#2A3040" }
                                    }
                                    PillButton {
                                        text: "SAVE"; primary: true; accentColor: Theme.accentEmerald; implicitHeight: 34; implicitWidth: 74
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
                }

                // ── 6: User Guide ──
                SettingsPage {
                    SettingsCard {
                        Label { text: "BROADCAST CONTROLS"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }
                        Repeater {
                            model: settingsRoot.shortcutActions
                            delegate: ColumnLayout {
                                Layout.fillWidth: true; spacing: 3
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 8
                                    Label { text: modelData.label; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm }
                                    Rectangle {
                                        visible: keyLabel.text.length > 0
                                        Layout.preferredWidth: keyLabel.implicitWidth + 12; Layout.preferredHeight: 18
                                        radius: 6; color: "#0D0F16"; border.color: "#282D3B"
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
                    }

                    SettingsCard {
                        Label { text: "WORKFLOW GUIDE"; color: Theme.textSecondary; font.bold: true; font.pixelSize: Theme.textXs; font.letterSpacing: 1 }

                        Label { text: "Quick Fetch & the meeting sequence"; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            color: Theme.textDim; font.pixelSize: Theme.textXs
                            text: "Clicking a card in QUICK FETCH or Pins only previews it — it does not change any segment's linked media. Select a segment in SEQUENCE, then hover a card and click the green \"+\" that appears to link it."
                        }

                        Label { text: "Zoom broadcasting"; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm; Layout.topMargin: 6 }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            color: Theme.textDim; font.pixelSize: Theme.textXs
                            text: "Click BROADCAST TO ZOOM, then in Zoom's own camera picker choose \"OBS Virtual Camera.\" Whatever's on Program shows there; when idle, it falls back to the webcam unless WEBCAM FORCE-OFF is on."
                        }

                        Label { text: "Folder slideshows"; color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.textSm; Layout.topMargin: 6 }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            color: Theme.textDim; font.pixelSize: Theme.textXs
                            text: "Add several videos and/or images to a pin folder, then drag that folder onto either monitor — it takes the first live immediately and auto-advances until you hit STOP."
                        }
                    }
                }
            }
        }
    }

    ThemedDialog {
        id: installUpdateDialog
        title: "Install Update and Restart?"
        acceptText: "INSTALL & RESTART"
        acceptColor: Theme.accentEmerald
        contentItem: Label {
            width: 340
            leftPadding: 20; rightPadding: 20; topPadding: 4; bottomPadding: 12
            text: "MediaFlow will close and open the installer for version "
                  + (updateSection.updater ? updateSection.updater.latestVersion : "")
                  + ". Approve the Windows prompt and leave “Launch MediaFlow” selected to restart."
            color: Theme.textPrimary
            font.pixelSize: Theme.textMd
            wrapMode: Text.WordWrap
        }
        onAccepted: if (updateSection.updater) updateSection.updater.installAndRestart()
    }

    component SettingsPage: Flickable {
        id: pageFlick
        default property alias items: pageColumn.data
        clip: true
        contentWidth: width
        contentHeight: pageColumn.implicitHeight + 36
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: SlimScrollBar { }

        ColumnLayout {
            id: pageColumn
            x: 18
            y: 18
            width: parent.width - 36
            spacing: 14
        }
    }

    component SettingsCard: Rectangle {
        default property alias body: cardLayout.data
        Layout.fillWidth: true
        implicitHeight: cardLayout.implicitHeight + 32
        radius: Theme.radiusLg
        color: "#13161F"
        border.color: "#252936"
        border.width: 1

        ColumnLayout {
            id: cardLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 10
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

        Layout.fillWidth: true; Layout.preferredHeight: 42
        radius: Theme.radius; color: "#181B25"; border.color: "#2A3040"; border.width: 1

        DarkComboBox {
            id: combo
            anchors.fill: parent; anchors.margins: 4
            textLeftPadding: 8
            fontPixelSize: Theme.textMd
            fontBold: false
            placeholderLabel: pickerRoot.allowAutomatic ? "Automatic (recommended)" : "Choose a screen\u2026"
            textRole: "label"
            model: {
                let list = pickerRoot.allowAutomatic ? [{ index: -1, label: "Automatic (recommended)" }] : []
                let screens = (MediaFlowBackend || {}).availableScreens ? MediaFlowBackend.availableScreens() : []
                for (let i = 0; i < screens.length; i++)
                    list.push({ index: screens[i].index, label: settingsRoot.formatDisplayName(screens[i].name) })
                return list
            }
            Component.onCompleted: {
                for (let i = 0; i < model.length; i++) {
                    if (model[i].index === pickerRoot.currentValue) { currentIndex = i; break }
                }
            }
            onActivated: (idx) => pickerRoot.valueChosen(model[idx].index)
        }
    }

    // Mirrors ScreenPicker above, bound to the CameraDeviceModel exposed as
    // MediaFlowBackend.cameraDevices (a real QAbstractListModel, unlike
    // ScreenPicker's plain JS array) instead of a screen index.
    component CameraPicker: Rectangle {
        id: camPickerRoot
        property var currentDevice: null
        signal deviceChosen(var device)

        Layout.fillWidth: true; Layout.preferredHeight: 42
        radius: Theme.radius; color: "#181B25"; border.color: "#2A3040"; border.width: 1

        DarkComboBox {
            id: camCombo
            anchors.fill: parent; anchors.margins: 4
            textLeftPadding: 8
            fontPixelSize: Theme.textMd
            fontBold: false
            placeholderLabel: "No camera found"
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

        width: 96; height: 30; radius: Theme.radiusSm
        color: listening ? Theme.accentBlue : "#12151E"
        border.color: listening ? Theme.accentBlue : "#2D3342"

        Label {
            anchors.centerIn: parent
            text: captureRoot.listening ? "Press a key\u2026" : (captureRoot.currentKey || "Unbound")
            color: captureRoot.listening ? "white" : (captureRoot.currentKey ? Theme.textPrimary : Theme.textDim)
            font.family: Theme.monoFont
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
