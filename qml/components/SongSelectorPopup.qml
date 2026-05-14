import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

Rectangle {
    id: root
    width: 400
    height: 220
    color: "#0A0A0A"
    radius: 12
    border.color: "#1AFFFFFF"
    
    // Properties
    property string currentLanguageCode: (MediaFlowBackend || {}).currentLanguageCode || "E"
    property string currentLanguageName: ((MediaFlowBackend || {}).currentLanguage || {"name": "English"}).name
    property var searchResult: ({"found": false})
    property int songIndex: -1
    property string targetSegmentId: ""
    signal songLinked()

    function refreshSearch() {
        if (!MediaFlowBackend || songInput.text.length === 0) {
            searchResult = {"found": false}
            return
        }

        let num = parseInt(songInput.text)
        if (!isNaN(num)) {
            console.log("SongSelectorPopup Searching: " + num + " Lang: " + root.currentLanguageCode)
            searchResult = MediaFlowBackend.findSong(num, root.currentLanguageCode, true, "vocal")
        }
    }

    onCurrentLanguageCodeChanged: refreshSearch()
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16
        
        // Header
        RowLayout {
            spacing: 8
            Rectangle { width: 4; height: 16; radius: 2; color: "#3B82F6" }
            Label { 
                text: "MANUAL SONG SELECTOR"; color: "white"
                font.bold: true; font.pixelSize: 12; font.letterSpacing: 1
            }
            Item { Layout.fillWidth: true }
            Label { 
                text: root.currentLanguageName.toUpperCase(); color: "#3B82F6"
                font.bold: true; font.pixelSize: 10
            }
        }
        
        // Input Field
        TextField {
            id: songInput
            Layout.fillWidth: true
            placeholderText: "ENTER SONG NUMBER (E.G. 49)"
            color: "white"
            font.bold: true
            font.pixelSize: 14
            inputMethodHints: Qt.ImhDigitsOnly
            
            background: Rectangle {
                radius: 10
                color: "#0DFFFFFF"
                border.color: songInput.activeFocus ? "#3B82F6" : "#1AFFFFFF"
                border.width: songInput.activeFocus ? 2 : 1
            }
            
            onTextChanged: {
                refreshSearch()
            }
        }
        
        // Result Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            radius: 10
            color: searchResult.found ? "#1A3B82F6" : "#0DFFFFFF"
            border.color: searchResult.found ? "#3B82F6" : "transparent"
            visible: songInput.text.length > 0
            
            RowLayout {
                anchors.fill: parent; anchors.margins: 10; spacing: 12
                
                // Thumbnail / Placeholder
                Rectangle {
                    width: 100; height: 60; radius: 6; color: "#0DFFFFFF"; clip: true
                    Image {
                        anchors.fill: parent; fillMode: Image.PreserveAspectCrop
                        source: searchResult.found ? (searchResult.thumbnailPath || "qrc:/MediaFlow/qml/assets/video_placeholder.png") : ""
                        visible: searchResult.found
                    }
                    Label {
                        anchors.centerIn: parent; text: "!"; color: "#EF4444"
                        visible: !searchResult.found; font.bold: true
                    }
                }
                
                // Details
                ColumnLayout {
                    spacing: 4
                    Label { 
                        text: searchResult.found ? searchResult.name : "SONG NOT FOUND"
                        color: searchResult.found ? "white" : "#EF4444"
                        font.bold: true; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight
                    }
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 24; height: 14; radius: 3; color: "#3B82F6"
                            Label { anchors.centerIn: parent; text: searchResult.code || ""; color: "white"; font.pixelSize: 8; font.bold: true }
                        }
                        Label { 
                            text: searchResult.found ? "READY TO LINK" : "PLEASE CHECK JW LIBRARY"
                            color: "#6b7280"; font.pixelSize: 9; font.bold: true
                        }
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // Link Button
                Rectangle {
                    width: 32; height: 32; radius: 8; color: "#10B981"
                    visible: searchResult.found
                    Label { anchors.centerIn: parent; text: "+"; color: "white"; font.bold: true; font.pixelSize: 16 }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (MediaFlowBackend && root.targetSegmentId !== "") {
                                MediaFlowBackend.findAndStageSong(searchResult.songNumber || parseInt(songInput.text), root.currentLanguageCode, root.targetSegmentId);
                                root.songLinked();
                            }
                        }
                    }
                }
            }
        }
        
        Item { Layout.fillHeight: true }
    }
}
