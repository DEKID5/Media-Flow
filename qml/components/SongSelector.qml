import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitWidth: 350
    implicitHeight: 100
    color: "transparent"

    property int songIndex: -1
    property var searchResult: ({"found": false, "file_missing": true})

    function updateSearch() {
        if (!MediaFlowBackend) return;
        let model = langCombo.model;
        if (!model || model.length === 0) return;
        let lang = model[langCombo.currentIndex].code;
        let num = parseInt(songNumInput.text);
        if (isNaN(num)) return;
        let prefVideo = videoToggle.checked;
        let track = vocalToggle.checked ? "vocal" : "instrumental";
        
        console.log("SongSelector Searching: Num=" + num + " Lang=" + lang + " Video=" + prefVideo);
        searchResult = MediaFlowBackend.findSong(num, lang, prefVideo, track);
        if (searchResult.found) {
            MediaFlowBackend.setLinkedMedia(songIndex, searchResult.path, searchResult.type);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        RowLayout {
            spacing: 16
            
            // SONG NUMBER
            ColumnLayout {
                Label { text: "SONG #"; font.pixelSize: 9; color: "#A1A1AA"; font.bold: true }
                TextField {
                    id: songNumInput
                    placeholderText: "000"
                    color: "white"
                    font.bold: true
                    background: Rectangle { radius: 8; color: "#0DFFFFFF"; border.color: "#1AFFFFFF" }
                    onTextChanged: updateSearch()
                }
            }

            // LANGUAGE
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "LANGUAGE"; font.pixelSize: 9; color: "#A1A1AA"; font.bold: true }
                ComboBox {
                    id: langCombo
                    Layout.fillWidth: true
                    model: (MediaFlowBackend || {}).getSupportedLanguages() || []
                    textRole: "name"
                    currentIndex: {
                        let current = (MediaFlowBackend || {}).currentLanguageCode || "E";
                        for (let i = 0; i < model.length; i++) {
                            if (model[i].code === current) return i;
                        }
                        return 0;
                    }
                    onCurrentIndexChanged: updateSearch()
                }
            }
        }

        Connections {
            target: MediaFlowBackend || null
            function onCurrentLanguageCodeChanged() {
                updateSearch()
            }
        }

        RowLayout {
            spacing: 20
            
            // VOCAL VS INSTRUMENTAL
            RowLayout {
                spacing: 8
                Label { text: vocalToggle.checked ? "VOCAL" : "INSTR."; font.pixelSize: 9; color: "white"; font.bold: true }
                Switch { id: vocalToggle; checked: true; onCheckedChanged: updateSearch() }
            }

            // VIDEO VS AUDIO
            RowLayout {
                spacing: 8
                Label { text: videoToggle.checked ? "VIDEO" : "AUDIO"; font.pixelSize: 9; color: "white"; font.bold: true }
                Switch { id: videoToggle; checked: true; onCheckedChanged: updateSearch() }
            }

            Item { Layout.fillWidth: true }

            // STATUS FEEDBACK
            Rectangle {
                width: 32; height: 32; radius: 16
                color: searchResult.found ? "#1A10B981" : "#1AEF4444"
                border.color: searchResult.found ? "#10B981" : "#EF4444"
                Label {
                    anchors.centerIn: parent
                    text: searchResult.found ? "✓" : "!"
                    color: parent.border.color
                    font.bold: true
                }
            }
        }

        Label {
            text: searchResult.found ? "File: " + searchResult.name : "Status: Media not found locally"
            font.pixelSize: 9
            color: searchResult.found ? "#10B981" : "#EF4444"
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        
        Button {
            text: "OPEN ADVANCED SEARCH"
            Layout.fillWidth: true
            flat: true
            onClicked: advSearchPopup.open()
            contentItem: Label {
                text: parent.text; color: "#3B82F6"; font.bold: true; font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Popup {
        id: advSearchPopup
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        background: Rectangle { color: "transparent" }
        
        SongSelectorPopup {
            songIndex: root.songIndex
            currentLanguage: langCombo.currentText
        }
    }
}
