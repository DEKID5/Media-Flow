import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MediaFlow 1.0

Item {
    id: root

    // Internal State
    property string currentView: "segment" // "segment" | "library" | "pins"
    property string selectedCategory: ""
    property string selectedPinFolderId: ""
    property var selectedPinMediaIds: []

    function refreshPinMediaIds() {
        if (!MediaFlowBackend || !MediaFlowBackend.pinnedFolders || !selectedPinFolderId) {
            selectedPinMediaIds = []
            return
        }
        selectedPinMediaIds = MediaFlowBackend.pinnedFolders.mediaIdsForFolder(selectedPinFolderId)
    }
    onSelectedPinFolderIdChanged: refreshPinMediaIds()

    Connections {
        target: (MediaFlowBackend || {}).pinnedFolders || null
        function onDataChanged() { root.refreshPinMediaIds() }
        function onModelReset() { root.refreshPinMediaIds() }
    }

    function openPinFolder(folderId) {
        selectedPinFolderId = folderId
        currentView = "pins"
    }

    property string pendingDeletePinId: ""
    function confirmDeletePin(folderId, folderName) {
        pendingDeletePinId = folderId
        deletePinDialog.folderName = folderName
        deletePinDialog.open()
    }

    // Header
    Rectangle {
        id: header
        width: parent.width; height: 60; color: "transparent"
        RowLayout {
            anchors.fill: parent; anchors.margins: 15; spacing: 12

            // View Toggle — pins are a QUICK FETCH sub-view, so that tab stays
            // highlighted while browsing inside a pin folder too.
            Row {
                spacing: 8
                Repeater {
                    model: [
                        { id: "segment", label: "ACTIVE MEDIA", icon: "play" },
                        { id: "library", label: "QUICK FETCH", icon: "folder" }
                    ]
                    Rectangle {
                        readonly property bool isActive: root.currentView === modelData.id || (modelData.id === "library" && root.currentView === "pins")
                        // ACTIVE MEDIA's tab pill picks up the current meeting's
                        // identity color (see Theme.meetingAccent) so the whole
                        // panel visibly ties to Midweek vs. Weekend; QUICK FETCH
                        // stays neutral since pins aren't meeting-specific.
                        readonly property color activeColor: modelData.id === "segment" ? Theme.meetingAccent((MediaFlowBackend || {}).meetingType) : Theme.textPrimary
                        width: 110; height: 32; radius: 16
                        color: isActive ? Qt.rgba(activeColor.r, activeColor.g, activeColor.b, 0.16) : "transparent"
                        border.width: 1; border.color: isActive ? Qt.rgba(activeColor.r, activeColor.g, activeColor.b, 0.4) : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                        RowLayout {
                            anchors.centerIn: parent; spacing: 6
                            BroadcastIcon { name: modelData.icon; iconSize: 12; opacity: parent.parent.isActive ? 1 : 0.5 }
                            Label {
                                text: modelData.label; color: parent.parent.isActive ? parent.parent.activeColor : Theme.textDim
                                font.bold: true; font.pixelSize: 10; font.letterSpacing: 0.5
                            }
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.currentView = modelData.id
                                root.selectedPinFolderId = ""
                            }
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Action Buttons
            Row {
                spacing: 8
                // Import Button — file dialog supports multi-select, so an
                // operator can add several images/videos in one pass.
                Rectangle {
                    width: 32; height: 32; radius: 8; color: Theme.accentEmerald
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

    // ── Pinned Folders strip — always visible, so a card from either view
    //    can be dragged onto a pin, and OS files can be dropped onto one too.
    Rectangle {
        id: pinStrip
        anchors.top: header.bottom
        width: parent.width
        // Pins live only in QUICK FETCH (and while browsing inside one) --
        // collapsed to 0 height (not just invisible) so ACTIVE MEDIA's layout
        // closes the gap instead of leaving an empty strip.
        height: visible ? 64 : 0
        visible: root.currentView === "library" || root.currentView === "pins"
        clip: true
        color: "transparent"

        ListView {
            id: pinList
            anchors.fill: parent
            anchors.leftMargin: 15; anchors.rightMargin: 15; anchors.topMargin: 4
            orientation: ListView.Horizontal
            spacing: 10
            model: (MediaFlowBackend || {}).pinnedFolders || null

            delegate: Rectangle {
                id: pinChip
                width: 116; height: 52; radius: 10
                color: root.selectedPinFolderId === model.id
                    ? "#1A3B82F6"
                    : (pinDropArea.containsDrag ? "#1A10B981" : (pinChipMa.containsMouse ? "#1AFFFFFF" : "#0DFFFFFF"))
                border.width: 1
                border.color: pinDropArea.containsDrag ? Theme.accentEmerald
                    : (root.selectedPinFolderId === model.id ? Theme.accentBlue : "#1AFFFFFF")
                Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 2
                    RowLayout {
                        spacing: 4
                        BroadcastIcon { name: "pin"; iconSize: 11; color: root.selectedPinFolderId === model.id ? Theme.accentBlue : "#9ca3af" }
                        Label {
                            text: model.name.toUpperCase()
                            Layout.fillWidth: true
                            color: "white"; font.bold: true; font.pixelSize: 9; elide: Text.ElideRight
                        }
                    }
                    Label {
                        text: model.count + (model.count === 1 ? " ITEM" : " ITEMS")
                        color: "#6b7280"; font.pixelSize: 8; font.bold: true
                    }
                }

                // Delete pin — always visible (not hover-only) so it's discoverable,
                // brightens further on hover; asks for confirmation before deleting.
                Rectangle {
                    anchors.top: parent.top; anchors.right: parent.right; anchors.margins: -4
                    width: 16; height: 16; radius: 8; color: Theme.accentRed
                    opacity: deletePinMa.containsMouse ? 1.0 : 0.7
                    Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
                    Label { text: "×"; anchors.centerIn: parent; color: "white"; font.pixelSize: 10; font.bold: true }
                    MouseArea {
                        id: deletePinMa
                        anchors.fill: parent; anchors.margins: -3; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: root.confirmDeletePin(model.id, model.name)
                    }
                }

                MouseArea {
                    id: pinChipMa
                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: root.openPinFolder(model.id)
                }

                // Accepts both an in-app dragged card (text/plain = media id)
                // and files dropped from Windows Explorer (urls).
                DropArea {
                    id: pinDropArea
                    anchors.fill: parent
                    onDropped: (drop) => {
                        if (drop.hasUrls) {
                            let paths = []
                            for (let i = 0; i < drop.urls.length; i++) paths.push(drop.urls[i].toString())
                            MediaFlowBackend.importFilesToPinnedFolder(model.id, paths)
                        } else if (drop.hasText && drop.text !== "") {
                            MediaFlowBackend.pinMediaToFolder(model.id, drop.text)
                        }
                    }
                }
            }

            footer: Rectangle {
                width: 96; height: 52; radius: 10
                color: newPinMa.containsMouse ? "#1AFFFFFF" : "#0DFFFFFF"
                border.width: 1; border.color: "#1AFFFFFF"
                ColumnLayout {
                    anchors.centerIn: parent; spacing: 2
                    BroadcastIcon { name: "plus"; iconSize: 14; color: "#9ca3af"; Layout.alignment: Qt.AlignHCenter }
                    Label { text: "NEW PIN"; color: "#9ca3af"; font.pixelSize: 8; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                }
                MouseArea { id: newPinMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: newPinPopup.open() }
            }
        }
    }

    Popup {
        id: newPinPopup
        anchors.centerIn: Overlay.overlay
        modal: true; focus: true
        width: 260; height: 120
        background: Rectangle { color: "#1a1a1e"; radius: 12; border.color: "#333" }
        onOpened: { newPinField.text = ""; newPinField.forceActiveFocus() }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 10
            Label { text: "NEW PIN FOLDER"; color: "white"; font.bold: true; font.pixelSize: 11; font.letterSpacing: 1 }
            TextField {
                id: newPinField
                Layout.fillWidth: true
                placeholderText: "e.g. Baptism Talk"
                color: "white"
                background: Rectangle { color: "#0DFFFFFF"; radius: 8; border.color: "#33FFFFFF" }
                onAccepted: createBtn.clicked()
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                PillButton {
                    id: createBtn
                    text: "CREATE"; primary: true; accentColor: Theme.accentBlue
                    implicitWidth: 90; implicitHeight: 32; font.pixelSize: 9
                    onClicked: {
                        if (newPinField.text.trim() !== "" && MediaFlowBackend) {
                            let id = MediaFlowBackend.createPinnedFolder(newPinField.text.trim())
                            newPinPopup.close()
                            root.openPinFolder(id)
                        }
                    }
                }
            }
        }
    }

    ThemedDialog {
        id: deletePinDialog
        property string folderName: ""
        title: "Delete Pin Folder"
        acceptText: "DELETE"; acceptColor: Theme.accentRed
        contentItem: Label {
            text: "Delete “" + deletePinDialog.folderName + "”? This only removes the pin folder — the media files themselves aren’t deleted."
            color: "white"; font.pixelSize: 12; wrapMode: Text.WordWrap; width: 280
            leftPadding: 20; rightPadding: 20; topPadding: 4; bottomPadding: 12
        }
        onAccepted: {
            if (root.selectedPinFolderId === root.pendingDeletePinId) { root.selectedPinFolderId = ""; root.currentView = "segment" }
            if (MediaFlowBackend) MediaFlowBackend.deletePinnedFolder(root.pendingDeletePinId)
            root.pendingDeletePinId = ""
        }
    }

    // Secondary Header / Categories (Only in Library View)
    Rectangle {
        id: subHeader
        anchors.top: pinStrip.bottom; width: parent.width; height: 40; color: "transparent"
        visible: root.currentView === "library"

        ListView {
            id: categoryList
            anchors.fill: parent; anchors.leftMargin: 15; anchors.rightMargin: 15
            orientation: ListView.Horizontal; spacing: 10
            model: (MediaFlowBackend && MediaFlowBackend.mediaLibrary) ? MediaFlowBackend.mediaLibrary.categories() : ["General"]
            delegate: Rectangle {
                width: catLabel.contentWidth + 24; height: 26; radius: 13
                color: root.selectedCategory === modelData ? Theme.accentEmerald : (catMa.containsMouse ? "#1AFFFFFF" : "#0DFFFFFF")
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

    // Pin folder content header (breadcrumb + add files)
    Rectangle {
        id: pinContentHeader
        anchors.top: pinStrip.bottom; width: parent.width; height: 34; color: "transparent"
        visible: root.currentView === "pins" && root.selectedPinFolderId !== ""

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 15; anchors.rightMargin: 15; spacing: 8
            Rectangle {
                width: 22; height: 22; radius: 6; color: backMa.containsMouse ? "#1AFFFFFF" : "transparent"
                Label { anchors.centerIn: parent; text: "‹"; color: "white"; font.pixelSize: 14; font.bold: true }
                MouseArea { id: backMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.selectedPinFolderId = "" }
            }
            Label {
                text: MediaFlowBackend && MediaFlowBackend.pinnedFolders ? MediaFlowBackend.pinnedFolders.nameForFolder(root.selectedPinFolderId).toUpperCase() : ""
                color: "white"; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1
            }
            Item { Layout.fillWidth: true }
            PillButton {
                text: "ADD FILES"; iconName: "plus"; accentColor: Theme.accentBlue
                implicitHeight: 24; implicitWidth: 96; font.pixelSize: 8
                onClicked: MediaFlowBackend.browseAndAddFilesToPinnedFolder(root.selectedPinFolderId)
            }
            Rectangle {
                width: 24; height: 24; radius: 6
                color: deleteHeaderMa.containsMouse ? "#33EF4444" : "#1AEF4444"
                BroadcastIcon { anchors.centerIn: parent; name: "trash"; color: Theme.accentRed; iconSize: 12 }
                MouseArea {
                    id: deleteHeaderMa
                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: root.confirmDeletePin(root.selectedPinFolderId,
                        MediaFlowBackend && MediaFlowBackend.pinnedFolders ? MediaFlowBackend.pinnedFolders.nameForFolder(root.selectedPinFolderId) : "")
                }
            }
        }
    }

    // Grid
    GridView {
        id: grid
        anchors.top: {
            if (root.currentView === "library") return subHeader.bottom
            if (root.currentView === "pins") return pinContentHeader.bottom
            return pinStrip.bottom
        }
        anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
        anchors.topMargin: 12; anchors.leftMargin: 15; anchors.rightMargin: 15
        clip: true
        cellWidth: width / 2
        cellHeight: cellWidth * 0.7
        visible: root.currentView !== "pins" || root.selectedPinFolderId !== ""

        model: (MediaFlowBackend && MediaFlowBackend.stagedMediaProxy) ? MediaFlowBackend.stagedMediaProxy : null

        // Sync Proxy Filter
        Binding {
            target: (MediaFlowBackend && MediaFlowBackend.stagedMediaProxy) ? MediaFlowBackend.stagedMediaProxy : null
            property: "filterType"
            value: root.currentView === "pins" ? "pinned" : (root.currentView === "segment" ? "segment" : "category")
            when: MediaFlowBackend && MediaFlowBackend.stagedMediaProxy
        }
        Binding {
            target: (MediaFlowBackend && MediaFlowBackend.stagedMediaProxy) ? MediaFlowBackend.stagedMediaProxy : null
            property: "categoryFilter"
            value: root.selectedCategory
            when: MediaFlowBackend && MediaFlowBackend.stagedMediaProxy
        }
        // Only drives stagedIds while browsing a pin folder -- C++ (selectSegment)
        // remains in control of stagedIds the rest of the time.
        Binding {
            target: (MediaFlowBackend && MediaFlowBackend.stagedMediaProxy) ? MediaFlowBackend.stagedMediaProxy : null
            property: "stagedIds"
            value: root.selectedPinMediaIds
            when: MediaFlowBackend && MediaFlowBackend.stagedMediaProxy && root.currentView === "pins"
        }

        delegate: Item {
            id: delegateRoot
            width: grid.cellWidth; height: grid.cellHeight

            // Reorder target -- delegateRoot stays put at its grid cell while
            // assetCard reparents away during drag (see states below), so
            // this is what a dragged card can actually be dropped onto.
            // Only meaningful in the Active Media (segment) view, where
            // StagedMediaProxyModel now sorts by link order (see
            // StagedMediaProxyModel::lessThan) rather than natural order.
            DropArea {
                anchors.fill: parent
                enabled: root.currentView === "segment"
                onDropped: (drop) => {
                    if (drop.hasText && drop.text !== "" && drop.text !== model.id) {
                        MediaFlowBackend.reorderSegmentMedia(drop.text, model.id)
                    }
                }
            }

            Rectangle {
                id: assetCard
                // Fixed size (not anchors.fill) so it keeps its dimensions once
                // detached from layout anchoring during a drag.
                width: delegateRoot.width - 12; height: delegateRoot.height - 12
                radius: Theme.radius; clip: true; color: Theme.surfaceHover
                border.width: 1; border.color: cardMa.containsMouse ? Theme.panelBorderStrong : "transparent"

                anchors.left: !cardMa.drag.active ? parent.left : undefined
                anchors.top: !cardMa.drag.active ? parent.top : undefined
                anchors.leftMargin: 6
                anchors.topMargin: 6

                // Standard Qt Quick drag idiom: while dragging, reparent to the
                // panel root (so it floats above the grid, unclipped) and let
                // MouseArea.drag.target move it freely; on drop (or release
                // outside a target) it snaps back via the reverted anchors.
                states: State {
                    when: cardMa.drag.active
                    ParentChange { target: assetCard; parent: root }
                    AnchorChanges { target: assetCard; anchors.left: undefined; anchors.top: undefined }
                }
                Drag.active: cardMa.drag.active
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2
                Drag.mimeData: { "text/plain": model.id }
                Drag.dragType: Drag.Automatic

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
                    width: 26; height: 26; radius: 6; color: Theme.accentRed
                    opacity: cardMa.containsMouse ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                    BroadcastIcon { anchors.centerIn: parent; name: "trash"; color: "white"; iconSize: 12 }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.currentView === "segment") {
                                MediaFlowBackend.removeMediaFromSequence(MediaFlowBackend.selectedSegmentId, model.id)
                            } else if (root.currentView === "pins") {
                                MediaFlowBackend.unpinMediaFromFolder(root.selectedPinFolderId, model.id)
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
                        BroadcastIcon { anchors.verticalCenter: parent.verticalCenter; name: model.type; color: Theme.textDim; iconSize: 10 }
                        Label { text: model.type.toUpperCase(); color: Theme.textDim; font.pixelSize: 8; font.bold: true }
                        Label { text: " • " + model.category.toUpperCase(); color: Theme.textFaint; font.pixelSize: 8; font.bold: true }
                    }
                }

                // Small drag hint, visible on hover so the gesture is discoverable.
                BroadcastIcon {
                    anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 8
                    name: "pin"; iconSize: 12; color: "white"; opacity: cardMa.containsMouse && !cardMa.drag.active ? 0.6 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                MouseArea {
                    id: cardMa
                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    drag.target: assetCard
                    onClicked: {
                        if (root.currentView === "segment") {
                            MediaFlowBackend.stageMedia(model.id)
                        } else {
                            // Library and Pins both behave the same: clicking links
                            // the item into the selected segment (so it then shows
                            // up under ACTIVE MEDIA), or just previews it if no
                            // segment is selected yet.
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

    // Empty States
    Column {
        anchors.centerIn: grid; spacing: 12
        visible: grid.visible && grid.count === 0
        BroadcastIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "monitor"; iconSize: 40; opacity: 0.1 }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.currentView === "segment" ? "NO MEDIA FOR THIS SEGMENT" : (root.currentView === "pins" ? "THIS PIN IS EMPTY" : "CATEGORY IS EMPTY")
            color: "#4b5563"; font.bold: true; font.pixelSize: 12; font.letterSpacing: 1
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.currentView === "segment" ? "Link files from QUICK FETCH or import new ones"
                : (root.currentView === "pins" ? "Drag files here from Explorer, drag a card onto this pin, or use ADD FILES" : "Import media using the + button above")
            color: "#374151"; font.pixelSize: 10
        }
    }

    // Drop straight onto the empty pin-folder content area too, not just the chip.
    DropArea {
        anchors.fill: grid
        enabled: root.currentView === "pins" && root.selectedPinFolderId !== ""
        onDropped: (drop) => {
            if (drop.hasUrls) {
                let paths = []
                for (let i = 0; i < drop.urls.length; i++) paths.push(drop.urls[i].toString())
                MediaFlowBackend.importFilesToPinnedFolder(root.selectedPinFolderId, paths)
            } else if (drop.hasText && drop.text !== "") {
                MediaFlowBackend.pinMediaToFolder(root.selectedPinFolderId, drop.text)
            }
        }
    }

    Column {
        anchors.centerIn: grid; spacing: 12
        visible: root.currentView === "pins" && root.selectedPinFolderId === ""
        BroadcastIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "pin"; iconSize: 40; opacity: 0.15 }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "SELECT A PIN ABOVE"
            color: "#4b5563"; font.bold: true; font.pixelSize: 12; font.letterSpacing: 1
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Or create one with + NEW PIN"
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
