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
}
