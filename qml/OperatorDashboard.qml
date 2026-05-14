import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import MediaFlow 1.0
import "components"

Item {
    id: root
    anchors.fill: parent

    function fmtTime(sec) {
        var m = Math.floor(sec / 60)
        var s = sec % 60
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    readonly property var theme: (typeof Theme !== "undefined" && Theme.bg !== undefined) ? Theme : {
        bg: "#050505",
        panelBg: "#0A0A0A",
        panelBorder: "#1AFFFFFF",
        accentBlue: "#3B82F6",
        accentEmerald: "#10B981",
        accentRed: "#EF4444",
        textPrimary: "#FFFFFF",
        textSecondary: "#A1A1AA",
        textMuted: "#4DA1A1AA",
        radius: 12,
        monoFont: "JetBrains Mono",
        sansFont: "Inter"
    }
    property string manualSongSegmentId: ""

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

    Rectangle {
        anchors.fill: parent
        color: "#050505"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- TOP BAR (RE-DESIGNED) ---
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: "#0A0A0A"
            border.color: "#1AFFFFFF"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 24

                // 1. BRANDING
                RowLayout {
                    spacing: 12
                    Rectangle {
                        width: 36; height: 36; radius: 8; color: "#3B82F6"
                        Label { 
                            anchors.centerIn: parent; text: "MF"; color: "white"
                            font.bold: true; font.pixelSize: 14; font.letterSpacing: 1
                        }
                    }
                    Column {
                        spacing: -2
                        Label { text: "MediaFlow"; color: "white"; font.bold: true; font.pixelSize: 14 }
                        Label { text: "BROADCAST SUITE"; color: "#3B82F6"; font.bold: true; font.pixelSize: 8; font.letterSpacing: 1.5 }
                    }
                }

                // 2. MEETING TYPE SELECTOR
                Rectangle {
                    Layout.preferredHeight: 36; width: 180; radius: 10; color: "#0DFFFFFF"; border.color: "#1AFFFFFF"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 4; spacing: 0
                        Rectangle {
                            Layout.fillWidth: true; Layout.fillHeight: true; radius: 7
                            color: (MediaFlowBackend || {}).meetingType === "midweek" ? "#3B82F6" : "transparent"
                            Label { anchors.centerIn: parent; text: "MIDWEEK"; color: (MediaFlowBackend || {}).meetingType === "midweek" ? "white" : "#A1A1AA"; font.pixelSize: 9; font.bold: true }
                            MouseArea { anchors.fill: parent; onClicked: (MediaFlowBackend || {}).setMeetingTypeStr("midweek") }
                        }
                        Rectangle {
                            Layout.fillWidth: true; Layout.fillHeight: true; radius: 7
                            color: (MediaFlowBackend || {}).meetingType === "weekend" ? "#3B82F6" : "transparent"
                            Label { anchors.centerIn: parent; text: "WEEKEND"; color: (MediaFlowBackend || {}).meetingType === "weekend" ? "white" : "#A1A1AA"; font.pixelSize: 9; font.bold: true }
                            MouseArea { anchors.fill: parent; onClicked: (MediaFlowBackend || {}).setMeetingTypeStr("weekend") }
                        }
                    }
                }

                // 3. LANGUAGE SELECTOR
                Rectangle {
                    Layout.preferredHeight: 36; width: 140; radius: 10; color: "#0DFFFFFF"; border.color: "#1AFFFFFF"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 4; spacing: 4
                        BroadcastIcon { name: "globe"; iconSize: 12; Layout.leftMargin: 8; opacity: 0.7 }
                        ComboBox {
                            id: langCombo
                            Layout.fillWidth: true
                            flat: true
                            model: (MediaFlowBackend || {}).getSupportedLanguages() || []
                            textRole: "name"
                            currentIndex: {
                                let current = (MediaFlowBackend || {}).currentLanguageCode || "E";
                                for (let i = 0; i < model.length; i++) {
                                    if (model[i].code === current) return i;
                                }
                                return 0;
                            }
                            onActivated: (index) => { if (MediaFlowBackend) MediaFlowBackend.currentLanguage = model[index] }
                            
                            contentItem: Label {
                                text: langCombo.currentText
                                font.pixelSize: 10; font.bold: true; color: "white"
                                verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignLeft
                                elide: Text.ElideRight
                            }
                            background: Rectangle { color: "transparent" }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // 4. AUDIO & OPERATOR STATUS
                RowLayout {
                    spacing: 12
                    Rectangle {
                        Layout.preferredHeight: 36; width: 140; radius: 10; color: "#0DFFFFFF"; border.color: "#1AFFFFFF"
                        RowLayout {
                            anchors.centerIn: parent; spacing: 6
                            BroadcastIcon { 
                                name: (MediaFlowBackend || {}).mixerMuted ? "mute" : "speaker"
                                iconSize: 12; opacity: (MediaFlowBackend || {}).mixerMuted ? 1.0 : 0.7 
                                color: (MediaFlowBackend || {}).mixerMuted ? theme.accentRed : "white"
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: (MediaFlowBackend || {}).setMixerMuted(!(MediaFlowBackend || {}).mixerMuted)
                                }
                            }
                            
                            // VOLUME DOWN
                            Label { 
                                text: "−"; color: "#A1A1AA"; font.bold: true; font.pixelSize: 14
                                MouseArea { 
                                    anchors.fill: parent; anchors.margins: -5
                                    onClicked: (MediaFlowBackend || {}).setMasterVolume((MediaFlowBackend || {}).masterVolume - 5)
                                }
                            }
                            
                            Label { 
                                text: (MediaFlowBackend || {}).masterVolume + "%"
                                color: "#3B82F6"; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 30; horizontalAlignment: Text.AlignHCenter
                            }
                            
                            // VOLUME UP
                            Label { 
                                text: "+"; color: "#A1A1AA"; font.bold: true; font.pixelSize: 14
                                MouseArea { 
                                    anchors.fill: parent; anchors.margins: -5
                                    onClicked: (MediaFlowBackend || {}).setMasterVolume((MediaFlowBackend || {}).masterVolume + 5)
                                }
                            }
                            
                            BroadcastIcon { name: "chevron-down"; iconSize: 8; opacity: 0.5 }
                        }
                    }
                    Rectangle {
                        Layout.preferredHeight: 36; width: 110; radius: 10; color: "#0D00FF00"; border.color: "#3300FF00"
                        RowLayout {
                            anchors.centerIn: parent; spacing: 8
                            BroadcastIcon { name: "speaker"; iconSize: 12; color: "#10B981" }
                            Label { text: "OPERATOR"; color: "#10B981"; font.pixelSize: 9; font.bold: true }
                            Rectangle { width: 6; height: 6; radius: 3; color: "#10B981" }
                        }
                    }
                }

                // 5. SYSTEM STATUS
                RowLayout {
                    spacing: 16
                    RowLayout {
                        spacing: 6
                        Rectangle { 
                            width: 6; height: 6; radius: 3; 
                            color: (MediaFlowBackend || {}).hasSecondaryScreen ? "#10B981" : "#EF4444" 
                        }
                        Label { 
                            text: (MediaFlowBackend || {}).hasSecondaryScreen ? "DISPLAY 2: ONLINE" : "DISPLAY 2: OFFLINE"
                            color: (MediaFlowBackend || {}).hasSecondaryScreen ? "#FFFFFF" : "#A1A1AA"
                            font.pixelSize: 8; font.bold: true 
                        }
                    }
                    
                    // EXTEND FEED TOGGLE
                    Rectangle {
                        Layout.preferredHeight: 24; Layout.preferredWidth: 100; radius: 6
                        color: (MediaFlowBackend || {}).feedExtended ? "#1A10B981" : "#0DFFFFFF"
                        border.color: (MediaFlowBackend || {}).feedExtended ? "#10B981" : "#33FFFFFF"
                        RowLayout {
                            anchors.centerIn: parent; spacing: 6
                            Rectangle { width: 4; height: 4; radius: 2; color: (MediaFlowBackend || {}).feedExtended ? "#10B981" : "#A1A1AA" }
                            Label { text: "EXTEND FEED"; color: (MediaFlowBackend || {}).feedExtended ? "#FFFFFF" : "#A1A1AA"; font.pixelSize: 8; font.bold: true }
                        }
                        MouseArea { 
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: (MediaFlowBackend || {}).toggleAudienceWindow()
                        }
                    }

                    RowLayout {
                        spacing: 6
                        Rectangle { width: 6; height: 6; radius: 3; color: "#10B981" }
                        Label { text: "SYNC: OK"; color: "#A1A1AA"; font.pixelSize: 8; font.bold: true }
                    }

                    // RESET APP
                    Rectangle {
                        Layout.preferredHeight: 24; Layout.preferredWidth: 80; radius: 6
                        color: "#1AEF4444"; border.color: "#EF4444"
                        Label { 
                            anchors.centerIn: parent; text: "RESET APP"; color: "#EF4444"
                            font.pixelSize: 8; font.bold: true 
                        }
                        MouseArea { 
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: resetConfirmDialog.open()
                        }
                    }

                    Rectangle { width: 1; height: 16; color: "#1AFFFFFF" }
                }

                // 6. LIVE & BROADCAST
                RowLayout {
                    spacing: 12
                    Rectangle {
                        Layout.preferredHeight: 36; width: 160; radius: 10; color: (MediaFlowBackend || {}).isMeetingLive ? "#1A000000" : "#0DFFFFFF"; border.color: (MediaFlowBackend || {}).isMeetingLive ? "#EF4444" : "#1AFFFFFF"
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 4
                            RowLayout {
                                Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter; anchors.leftMargin: 8; spacing: 8
                                BroadcastIcon { 
                                    name: "bolt"; iconSize: 12
                                    color: (MediaFlowBackend || {}).isMeetingLive ? "#EF4444" : "#A1A1AA" 
                                }
                                Label { text: "GO LIVE"; color: "white"; font.pixelSize: 10; font.bold: true }
                            }
                            BroadcastIcon { name: "settings"; iconSize: 12; opacity: 0.5 }
                            BroadcastIcon { name: "refresh"; iconSize: 12; opacity: 0.5; Layout.rightMargin: 8 }
                            MouseArea { anchors.fill: parent; onClicked: (MediaFlowBackend || {}).toggleMeetingLive() }
                        }
                    }

                    Rectangle {
                        Layout.preferredHeight: 36; width: 220; radius: 10 
                        color: (MediaFlowBackend || {}).vcamEnabled ? "#1A3B82F6" : "#1AFFFFFF"
                        border.color: (MediaFlowBackend || {}).vcamEnabled ? "#3B82F6" : "#1AFFFFFF"
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 4
                            Rectangle {
                                width: 140; Layout.fillHeight: true; radius: 8; color: "#0DFFFFFF"
                                RowLayout {
                                    anchors.centerIn: parent; spacing: 8
                                    Rectangle { width: 8; height: 8; radius: 4; color: (MediaFlowBackend || {}).vcamEnabled ? "#3B82F6" : "#A1A1AA"; border.color: "white"; border.width: 1 }
                                    Label { text: "BROADCAST TO ZOOM"; color: (MediaFlowBackend || {}).vcamEnabled ? "white" : "#A1A1AA"; font.pixelSize: 9; font.bold: true }
                                }
                                MouseArea { anchors.fill: parent; onClicked: (MediaFlowBackend || {}).toggleZoomBroadcast() }
                            }
                            RowLayout {
                                spacing: 4; Layout.alignment: Qt.AlignVCenter
                                Rectangle { width: 24; height: 16; radius: 4; color: "#3B82F6"; Label { anchors.centerIn: parent; text: "AUTO"; color: "white"; font.pixelSize: 6; font.bold: true } }
                                Label { text: "CAM"; color: "#A1A1AA"; font.pixelSize: 6; font.bold: true }
                                Label { text: "MEDIA"; color: "#A1A1AA"; font.pixelSize: 6; font.bold: true }
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredHeight: 36; width: 140; radius: 10
                        color: {
                            let ext = (MediaFlowBackend || {}).feedExtended
                            if (ext) return "#3B82F6"
                            return extMa.containsMouse ? "#1AFFFFFF" : "transparent"
                        }
                        border.color: (MediaFlowBackend || {}).feedExtended ? "#3B82F6" : "#3B82F6"
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Row {
                            anchors.centerIn: parent; spacing: 6
                            BroadcastIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: (MediaFlowBackend || {}).feedExtended ? "eye" : "screen"
                                iconSize: 12; color: "white"
                            }
                            Label {
                                text: (MediaFlowBackend || {}).feedExtended ? "FEED ACTIVE" : "EXTEND FEED"
                                color: "white"; font.pixelSize: 9; font.bold: true
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
            spacing: 24
            anchors.margins: 24

            MonitorView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "PREVIEW"
                showTransitions: true
                asset: (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.previewAsset : null
                onTakeClicked: { if (MediaFlowBackend && MediaFlowBackend.broadcastEngine) MediaFlowBackend.broadcastEngine.takeLive() }
                onCutClicked: { if (MediaFlowBackend && MediaFlowBackend.broadcastEngine) MediaFlowBackend.broadcastEngine.cutLive() }
            }

            MonitorView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "LIVE"
                isLive: true
                asset: (MediaFlowBackend || {}).broadcastEngine ? MediaFlowBackend.broadcastEngine.programAsset : null
                cameraDevice: (MediaFlowBackend || {}).programCameraDevice
            }
        }

        // --- DOCK ---
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            color: "transparent"
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: "#1AFFFFFF" }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                // MEDIA SOURCE
                // MEDIA SOURCE
                MediaSourcePanel {
                    id: mediaSourcePanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 350
                    // Dock styling
                    Rectangle { 
                        anchors.fill: parent; z: -1; color: "#0A0A0A"; radius: 12; border.color: "#1AFFFFFF" 
                    }
                }

                // SEQUENCE
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.preferredWidth: 400
                    color: "#0A0A0A"; radius: 12; border.color: "#1AFFFFFF"; clip: true
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 16; spacing: 12

                        // Header
                        RowLayout {
                            Label { text: "SEQUENCE"; font.bold: true; font.pixelSize: 10; color: "white"; font.letterSpacing: 2 }
                            PillButton {
                                text: "CLEAR ALL"; accentColor: theme.accentRed; implicitHeight: 22; implicitWidth: 80; font.pixelSize: 8
                                onClicked: clearDialog.open()
                            }
                            Label {
                                text: ((MediaFlowBackend || {}).meetingSchedule ? (MediaFlowBackend || {}).meetingSchedule.rowCount() : 0) + " SEGMENTS"
                                font.pixelSize: 8; color: "#4DA1A1AA"; font.bold: true
                            }
                        }

                        // Segment ListView
                        ListView {
                            id: sList
                            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12; clip: true
                            model: (MediaFlowBackend || {}).meetingSchedule
                            delegate: Item {
                                id: segmentDelegate
                                width: sList.width; height: isSelected ? 160 : 110
                                property bool isSelected: (MediaFlowBackend || {}).selectedSegmentId === model.id

                                Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }

                                Rectangle {
                                    anchors.fill: parent; anchors.margins: 4
                                    radius: 12; color: isSelected ? "#0D3B82F6" : "#0A0A0A"
                                    border.color: isSelected ? "#3B82F6" : "#1AFFFFFF"
                                    border.width: isSelected ? 2 : 1

                                    // Selection Glow
                                    Rectangle {
                                        anchors.fill: parent; anchors.margins: -2
                                        radius: 14; color: "transparent"; border.color: "#3B82F6"; border.width: 1
                                        opacity: isSelected ? 0.3 : 0
                                    }

                                    ColumnLayout {
                                        anchors.fill: parent; anchors.margins: 16; spacing: 8

                                        // Time Row
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label {
                                                text: model.time
                                                color: isSelected ? "#60a5fa" : "#4b5563"
                                                font.pixelSize: 11; font.bold: true
                                            }
                                            Item { Layout.fillWidth: true }
                                            BroadcastIcon {
                                                visible: model.isLive; name: "activity"; color: "#ef4444"; iconSize: 12
                                            }
                                        }

                                        // Title (Editable for Additional Part only)
                                        TextField {
                                            id: titleEdit
                                            text: model.title.toUpperCase()
                                            Layout.fillWidth: true
                                            color: "white"
                                            font.pixelSize: 13; font.bold: true
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
                                            spacing: 12
                                            
                                            Rectangle {
                                                width: 100; height: 32; radius: 8; color: "#1AFFFFFF"; border.color: "#33FFFFFF"
                                                TextInput {
                                                    id: songInput
                                                    anchors.centerIn: parent; width: parent.width - 16
                                                    color: "white"; font.pixelSize: 12; font.bold: true; horizontalAlignment: TextInput.AlignHCenter
                                                    inputMethodHints: Qt.ImhDigitsOnly
                                                    
                                                    Text {
                                                        text: "Song #"
                                                        anchors.centerIn: parent
                                                        color: "#4b5563"
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
                                                width: 32; height: 32; radius: 8; color: theme.accentBlue
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
                                                font.pixelSize: 8; font.bold: true; color: (typeof associatedMediaIds !== "undefined" && associatedMediaIds && associatedMediaIds.length > 0) ? theme.accentEmerald : "#4b5563"; font.letterSpacing: 1
                                            }
                                        }

                                        // Media Thumbnails + Add Button
                                        RowLayout {
                                            Layout.fillWidth: true; Layout.preferredHeight: 46; spacing: 10
                                            visible: isSelected || (typeof associatedMediaIds !== "undefined" && associatedMediaIds && associatedMediaIds.length > 0)

                                            Repeater {
                                                model: (typeof associatedMediaIds !== "undefined") ? associatedMediaIds : []
                                                delegate: Rectangle {
                                                    width: 64; height: 36; radius: 6; color: "black"; clip: true
                                                    property var asset: (MediaFlowBackend || {}).mediaLibrary ? MediaFlowBackend.mediaLibrary.getRowById(modelData) : ({})
                                                    
                                                    Image {
                                                        anchors.fill: parent; fillMode: Image.PreserveAspectCrop
                                                        source: asset.thumbnailPath || "qrc:/MediaFlow/qml/assets/video_placeholder.png"
                                                        opacity: 0.8
                                                    }
                                                    
                                                    // Unlink button
                                                    Rectangle {
                                                        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 2
                                                        width: 14; height: 14; radius: 7; color: "#EF4444"
                                                        Label { text: "×"; anchors.centerIn: parent; color: "white"; font.pixelSize: 10; font.bold: true }
                                                        MouseArea {
                                                            anchors.fill: parent
                                                            onClicked: (MediaFlowBackend || {}).removeMediaFromSequence(model.id, modelData)
                                                        }
                                                    }
                                                }
                                            }

                                            // ADD VIDEO BUTTON
                                            Rectangle {
                                                visible: isSelected && model.type !== "song"
                                                width: 60; height: 36; radius: 6; color: "#0D3B82F6"; border.color: "#3B82F6"
                                                Row {
                                                    anchors.centerIn: parent; spacing: 4
                                                    BroadcastIcon { anchors.verticalCenter: parent.verticalCenter; name: "video"; color: "#3B82F6"; iconSize: 12 }
                                                    Label { text: "VIDEO"; color: "#3B82F6"; font.pixelSize: 8; font.bold: true }
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
                                                width: 60; height: 36; radius: 6; color: "#0D10B981"; border.color: "#10B981"
                                                Row {
                                                    anchors.centerIn: parent; spacing: 4
                                                    BroadcastIcon { anchors.verticalCenter: parent.verticalCenter; name: "image"; color: "#10B981"; iconSize: 12 }
                                                    Label { text: "IMAGE"; color: "#10B981"; font.pixelSize: 8; font.bold: true }
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
                                                width: 72; height: 36; radius: 6; color: "#0D3B82F6"; border.color: "#3B82F6"
                                                visible: isSelected && model.type === "song"
                                                Row {
                                                    anchors.centerIn: parent; spacing: 4
                                                    BroadcastIcon { anchors.verticalCenter: parent.verticalCenter; name: "music"; color: "#3B82F6"; iconSize: 12 }
                                                    Label { text: "SONG"; color: "#3B82F6"; font.pixelSize: 8; font.bold: true }
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

                                    MouseArea {
                                        anchors.fill: parent; z: -1
                                        onClicked: {
                                            if (MediaFlowBackend) MediaFlowBackend.selectSegment(model.id)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Clear All Confirmation Dialog
                    Dialog {
                        id: clearDialog
                        title: "Clear All Media"
                        modal: true; anchors.centerIn: Overlay.overlay
                        standardButtons: Dialog.Ok | Dialog.Cancel
                        background: Rectangle { color: "#1a1a1e"; radius: 12; border.color: "#333" }
                        contentItem: Label {
                            text: "Remove all linked media from the current meeting schedule?"
                            color: "white"; font.pixelSize: 12; wrapMode: Text.WordWrap
                        }
                        onAccepted: {
                            if (MediaFlowBackend && MediaFlowBackend.meetingSchedule)
                                MediaFlowBackend.meetingSchedule.clearAllMedia()
                        }
                    }
                }

                // TIMERS & BGM
                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 350; spacing: 20
                    TimerPanel { }

                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        color: "#0A0A0A"; radius: 12; border.color: "#1AFFFFFF"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 20; spacing: 16
                            
                            // Header
                            RowLayout {
                                RowLayout {
                                    spacing: 8
                                    Rectangle { width: 3; height: 14; color: theme.accentEmerald; radius: 1.5 }
                                    Label { text: "BACKGROUND MUSIC"; font.bold: true; font.pixelSize: 11; color: "white"; font.letterSpacing: 1.5 }
                                }
                                Item { Layout.fillWidth: true }
                                Label { 
                                    text: ((MediaFlowBackend || {}).bgmCount || 0) + " TRACKS"
                                    color: theme.accentBlue; font.pixelSize: 9; font.bold: true 
                                }
                            }
                            
                            Item { Layout.fillHeight: true }

                            // Center Info
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 12
                                BroadcastIcon { 
                                    name: "music"; iconSize: 48; Layout.alignment: Qt.AlignHCenter; opacity: 0.1 
                                }
                                Label { 
                                    text: (MediaFlowBackend || {}).bgmTrackName || "NO TRACKS DETECTED"
                                    font.pixelSize: 10; color: "#A1A1AA"; Layout.alignment: Qt.AlignHCenter; font.bold: true; font.letterSpacing: 0.5
                                }
                            }

                            Item { Layout.fillHeight: true }

                            // CONTROLS (Uniform and Professional)
                            RowLayout {
                                Layout.fillWidth: true; spacing: 12
                                
                                PillButton { 
                                    Layout.preferredWidth: 44; Layout.preferredHeight: 44
                                    iconName: "skip-back"; iconSize: 16
                                    onClicked: (MediaFlowBackend || {}).backBgm() 
                                }
                                
                                PillButton { 
                                    id: playBtn
                                    Layout.fillWidth: true; Layout.preferredHeight: 44
                                    text: (MediaFlowBackend || {}).isPlayingBgm ? "PAUSE" : "PLAY"
                                    primary: true
                                    accentColor: theme.accentBlue
                                    iconName: (MediaFlowBackend || {}).isPlayingBgm ? "pause" : "play"
                                    onClicked: (MediaFlowBackend || {}).toggleBgmPlayback() 
                                }
                                
                                PillButton { 
                                    Layout.preferredWidth: 44; Layout.preferredHeight: 44
                                    iconName: "skip-forward"; iconSize: 16
                                    onClicked: (MediaFlowBackend || {}).nextBgm() 
                                }
                                
                                PillButton { 
                                    Layout.preferredWidth: 60; Layout.preferredHeight: 44
                                    text: "STOP"
                                    onClicked: (MediaFlowBackend || {}).stopBgm() 
                                }
                            }
                        }
                    }
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
        color: theme.accentRed; opacity: 0; z: 9999
        border.color: "white"; border.width: 1
        
        Label {
            id: toastLabel; anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 12
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

    Dialog {
        id: resetConfirmDialog
        title: "Reset Application?"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        
        background: Rectangle {
            color: "#0A0A0A"
            radius: 12
            border.color: "#33FFFFFF"
        }

        header: Rectangle {
            height: 40; color: "transparent"
            Label {
                anchors.centerIn: parent; text: "RESET APPLICATION"; color: "white"; font.bold: true; font.pixelSize: 12
            }
        }

        contentItem: Label {
            text: "This will clear all media links, imported files, and thumbnail cache.\nAre you sure?"
            color: "#A1A1AA"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
            topPadding: 20; bottomPadding: 20
        }
        
        onAccepted: {
            if (MediaFlowBackend) MediaFlowBackend.resetApp()
        }
    }
}
