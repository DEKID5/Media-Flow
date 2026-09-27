import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    // Internal State
    property string currentView: "segment" // "segment" or "library"
    property string selectedCategory: ""

    // Header
    Rectangle {
        id: header
        width: parent.width; height: 60; color: "transparent"
        RowLayout {
            anchors.fill: parent; anchors.margins: 15; spacing: 12
            
            // View Toggle
            Row {
                spacing: 8
                Repeater {
                    model: [
                        { id: "segment", label: "ACTIVE MEDIA", icon: "play" },
                        { id: "library", label: "QUICK FETCH", icon: "folder" }
                    ]
                    Rectangle {
                        width: 110; height: 32; radius: 16
                        color: root.currentView === modelData.id ? "#1AFFFFFF" : "transparent"
                        border.width: 1; border.color: root.currentView === modelData.id ? "#33FFFFFF" : "transparent"
                        
                        RowLayout {
                            anchors.centerIn: parent; spacing: 6
                            BroadcastIcon { name: modelData.icon; iconSize: 12; opacity: root.currentView === modelData.id ? 1 : 0.5 }
                            Label { 
                                text: modelData.label; color: root.currentView === modelData.id ? "white" : "#6b7280"
                                font.bold: true; font.pixelSize: 10; font.letterSpacing: 0.5
                            }
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.currentView = modelData.id }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Action Buttons
            Row {
                spacing: 8
                // Import Button
                Rectangle {
                    width: 32; height: 32; radius: 8; color: "#10b981"
                    BroadcastIcon { anchors.centerIn: parent; name: "plus"; color: "white"; iconSize: 14 }
                    MouseArea { 
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (MediaFlowBackend) {
                                MediaFlowBackend.importMediaToFileSystem(root.selectedCategory || "General")
                            }
                        }
                    }
                }
            }
        }
    }

    // Secondary Header / Categories (Only in Library View)
    Rectangle {
        id: subHeader
        anchors.top: header.bottom; width: parent.width; height: 40; color: "transparent"
        visible: root.currentView === "library"
        
        ListView {
            id: categoryList
            anchors.fill: parent; anchors.leftMargin: 15; anchors.rightMargin: 15
            orientation: ListView.Horizontal; spacing: 10
            model: (MediaFlowBackend && MediaFlowBackend.mediaLibrary) ? MediaFlowBackend.mediaLibrary.categories() : ["General"]
            delegate: Rectangle {
                width: catLabel.contentWidth + 24; height: 26; radius: 13
                color: root.selectedCategory === modelData ? theme.accentEmerald : (catMa.containsMouse ? "#1AFFFFFF" : "#0DFFFFFF")
                Label {
                    id: catLabel; anchors.centerIn: parent; text: modelData.toUpperCase()
                    color: "white"; font.bold: true; font.pixelSize: 9; font.letterSpacing: 0.5
                }
                MouseArea {
                    id: catMa; anchors.fill: parent; hoverEnabled: true
                    onClicked: root.selectedCategory = modelData
                }
            }
        }
    }

    // Grid
    GridView {
        id: grid
        anchors.top: root.currentView === "library" ? subHeader.bottom : header.bottom
        anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
        anchors.topMargin: 12; anchors.leftMargin: 15; anchors.rightMargin: 15
        clip: true
        cellWidth: width / 2
        cellHeight: cellWidth * 0.7

        model: (MediaFlowBackend && MediaFlowBackend.stagedMediaProxy) ? MediaFlowBackend.stagedMediaProxy : null

        // Sync Proxy Filter
        Binding {
            target: (MediaFlowBackend && MediaFlowBackend.stagedMediaProxy) ? MediaFlowBackend.stagedMediaProxy : null
            property: "filterType"
            value: root.currentView === "segment" ? "segment" : "category"
            when: MediaFlowBackend && MediaFlowBackend.stagedMediaProxy
        }
        Binding {
            target: (MediaFlowBackend && MediaFlowBackend.stagedMediaProxy) ? MediaFlowBackend.stagedMediaProxy : null
            property: "categoryFilter"
            value: root.selectedCategory
            when: MediaFlowBackend && MediaFlowBackend.stagedMediaProxy
        }

        delegate: Item {
            id: delegateRoot
            width: grid.cellWidth; height: grid.cellHeight

            Rectangle {
                id: assetCard
                anchors.fill: parent; anchors.margins: 6; radius: 10; clip: true; color: "#1AFFFFFF"
                border.width: 1; border.color: cardMa.containsMouse ? "#33FFFFFF" : "transparent"

                Image {
                    anchors.fill: parent; fillMode: Image.PreserveAspectCrop; opacity: cardMa.containsMouse ? 0.9 : 0.7
                    asynchronous: true
                    source: model.thumbnailPath || "qrc:/MediaFlow/qml/assets/video_placeholder.png"
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    width: 32; height: 32
                    visible: model.thumbnailPath === "" && model.type !== "image"
                    running: visible
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: "#CC000000" }
                    }
                }

                // Delete/Remove Button
                Rectangle {
                    anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 8
                    width: 26; height: 26; radius: 6; color: "#EF4444"
                    opacity: cardMa.containsMouse ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                    BroadcastIcon { anchors.centerIn: parent; name: "trash"; color: "white"; iconSize: 12 }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.currentView === "segment") {
                                MediaFlowBackend.removeMediaFromSequence(MediaFlowBackend.selectedSegmentId, model.id)
                            } else {
                                MediaFlowBackend.removeMedia(model.id)
                            }
                        }
                    }
                }

                Column {
                    anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.margins: 12; spacing: 2
                    Label {
                        text: model.name.toUpperCase(); color: "white"
                        font.bold: true; font.pixelSize: 11; width: assetCard.width - 24; elide: Text.ElideRight
                    }
                    Row {
                        spacing: 4
                        BroadcastIcon { anchors.verticalCenter: parent.verticalCenter; name: model.type; color: "#6b7280"; iconSize: 10 }
                        Label { text: model.type.toUpperCase(); color: "#6b7280"; font.pixelSize: 8; font.bold: true }
                        Label { text: " • " + model.category.toUpperCase(); color: "#4b5563"; font.pixelSize: 8; font.bold: true }
                    }
                }

                MouseArea {
                    id: cardMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.currentView === "segment") {
                            MediaFlowBackend.stageMedia(model.id)
                        } else {
                            // In Library view, clicking binds to active segment
                            if (MediaFlowBackend.selectedSegmentId) {
                                MediaFlowBackend.bindMediaToSequence(model.id)
                            } else {
                                MediaFlowBackend.stageMedia(model.id) // Just preview
                            }
                        }
                    }
                }
            }
        }
    }

    // Empty State
    Column {
        anchors.centerIn: grid; spacing: 12; visible: grid.count === 0
        BroadcastIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "monitor"; iconSize: 40; opacity: 0.1 }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.currentView === "segment" ? "NO MEDIA FOR THIS SEGMENT" : "CATEGORY IS EMPTY"
            color: "#4b5563"; font.bold: true; font.pixelSize: 12; font.letterSpacing: 1
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.currentView === "segment" ? "Link files from QUICK FETCH or import new ones" : "Import media using the + button above"
            color: "#374151"; font.pixelSize: 10
        }
    }

    Connections {
        target: MediaFlowBackend || {}
        function onSelectedSegmentIdChanged() {
            if (MediaFlowBackend.selectedSegmentId) {
                root.currentView = "segment"
            }
        }
    }
}
