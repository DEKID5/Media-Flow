import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

Popup {
    id: musicPopup

    property string activeTab: "library"
    property var backend: MediaFlowBackend || ({})
    property var selectedTrackPaths: []
    property var librarySourceTracks: []
    property var playlistSourceTracks: []
    property var languageOptions: [{ name: "All languages", code: "" }]
    property string languageFilterCode: ""
    property string appliedSearchQuery: ""
    readonly property var libraryTracks: filterTracks(librarySourceTracks, true)
    readonly property var playlistTracks: filterTracks(playlistSourceTracks, false)

    function prepareTracks(tracks) {
        return tracks || []
    }
    function refreshTrackSnapshots() {
        if (activeTab === "library") {
            librarySourceTracks = backend.bgmLibraryTracks || []
        } else {
            playlistSourceTracks = backend.bgmPlaylistTracks || []
        }
    }
    function refreshLanguageOptions() {
        const options = [{ name: "All languages", code: "" }]
        if (backend.getSupportedLanguages) {
            const languages = backend.getSupportedLanguages()
            for (let i = 0; i < languages.length; ++i)
                options.push(languages[i])
        }
        languageOptions = options
        let selectedLanguageAvailable = languageFilterCode === ""
        for (let i = 0; i < options.length; ++i) {
            if (options[i].code === languageFilterCode)
                selectedLanguageAvailable = true
        }
        if (!selectedLanguageAvailable)
            languageFilterCode = ""
    }
    function filterTracks(tracks, includeLanguageFilter) {
        const query = appliedSearchQuery
        const code = includeLanguageFilter ? languageFilterCode.toLowerCase() : ""
        if (!query && !code)
            return tracks
        const marker = code ? "_" + code + "_" : ""
        return tracks.filter(function(track) {
            return (!query || track.searchText.includes(query))
                && (!marker || track.normalizedPath.includes(marker))
        })
    }

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: Math.min(900, (Overlay.overlay ? Overlay.overlay.width : 940) - 32)
    height: Math.min(680, (Overlay.overlay ? Overlay.overlay.height : 720) - 32)
    modal: true
    focus: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0
    onActiveTabChanged: {
        if (visible) {
            refreshTrackSnapshots()
            trackList.currentIndex = trackList.count > 0 ? 0 : -1
            trackList.positionViewAtBeginning()
        }
    }
    onOpened: {
        searchField.text = ""
        searchDebounce.stop()
        appliedSearchQuery = ""
        languageFilterCode = ""
        refreshLanguageOptions()
        refreshTrackSnapshots()
        selectedTrackPaths = []
        Qt.callLater(function() {
            trackList.currentIndex = trackList.count > 0 ? 0 : -1
            trackList.positionViewAtBeginning()
            trackList.forceActiveFocus()
        })
    }

    Timer {
        id: searchDebounce
        interval: 140
        repeat: false
        onTriggered: musicPopup.appliedSearchQuery = searchField.text.trim().toLowerCase()
    }

    Connections {
        target: musicPopup.backend
        ignoreUnknownSignals: true
        function onBgmLibraryChanged() {
            if (musicPopup.visible && musicPopup.activeTab === "library") {
                musicPopup.librarySourceTracks = musicPopup.backend.bgmLibraryTracks || []
            }
        }
        function onBgmPlaylistTracksChanged() {
            if (musicPopup.visible && musicPopup.activeTab === "playlist") {
                musicPopup.playlistSourceTracks = musicPopup.backend.bgmPlaylistTracks || []
            }
        }
        function onLanguagesChanged() {
            musicPopup.refreshLanguageOptions()
        }
    }

    function isTrackSelected(path) {
        return selectedTrackPaths.indexOf(path) !== -1
    }

    function friendlyTrackTitle(rawName) {
        const s = (rawName || "").trim()
        if (!s) return "Unknown track"
        let m = s.match(/^osg_[A-Za-z]+_(\d+)$/i)
        if (m) return "Original Song " + m[1]
        m = s.match(/^sjj[m]?_[A-Za-z]+_(\d+)$/i)
        if (m) return "Kingdom Song " + parseInt(m[1], 10)
        return s
    }

    function friendlySourceMeta(path) {
        const p = (path || "").replace(/\\/g, "/")
        if (!p) return ""
        const parts = p.split("/")
        const file = parts.length > 0 ? parts[parts.length - 1] : p
        const folder = parts.length > 1 ? parts[parts.length - 2] : ""
        return folder ? (folder + "  •  " + file) : file
    }

    function toggleTrackSelection(path, selected) {
        if (selected) {
            if (!isTrackSelected(path))
                selectedTrackPaths = selectedTrackPaths.concat([path])
        } else {
            selectedTrackPaths = selectedTrackPaths.filter(function(item) { return item !== path })
        }
    }

    Overlay.modal: Rectangle { color: "#B8000000" }
    background: Rectangle {
        color: "#101014"
        radius: 18
        border.color: "#292930"
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 22
            Layout.bottomMargin: 16
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Label {
                    text: "MUSIC"
                    color: Theme.textPrimary
                    font.family: "Inter"
                    font.pixelSize: 18
                    font.bold: true
                    font.letterSpacing: 1.1
                }
                Label {
                    text: "Your library and listening queue"
                    color: Theme.textDim
                    font.family: "Inter"
                    font.pixelSize: Theme.textSm
                }
            }

            Rectangle {
                implicitWidth: queueCountLabel.implicitWidth + 18
                implicitHeight: 24
                radius: 12
                color: "#112820"
                border.color: "#1E4B3B"
                border.width: 1

                Label {
                    id: queueCountLabel
                    anchors.centerIn: parent
                    text: (backend.bgmCount || 0) + " IN PLAYLIST"
                    color: Theme.accentEmerald
                    font.family: "Inter"
                    font.pixelSize: Theme.textXs
                    font.bold: true
                    font.letterSpacing: 0.4
                }
            }

            Button {
                text: "×"
                implicitWidth: 34
                implicitHeight: 34
                onClicked: musicPopup.close()
                contentItem: Label {
                    text: parent.text
                    color: Theme.textSecondary
                    font.pixelSize: 22
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 9
                    color: parent.hovered ? "#27272f" : "#17171c"
                    border.color: "#2d2d35"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            Layout.bottomMargin: 14
            Layout.preferredHeight: 84
            radius: 12
            color: "#16161c"
            border.color: (backend.isPlayingBgm || false) ? "#1E3B30" : "#24242c"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                Item {
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64

                    Rectangle {
                        id: vinylDisc
                        anchors.centerIn: parent
                        width: 58
                        height: 58
                        radius: 29
                        visible: (backend.bgmCoverArt || "") === ""
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#27272f" }
                            GradientStop { position: 1; color: "#09090b" }
                        }
                        border.color: "#41414b"
                        RotationAnimation on rotation {
                            from: 0
                            to: 360
                            duration: 7000
                            loops: Animation.Infinite
                            running: musicPopup.visible && (backend.isPlayingBgm || false)
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            radius: 9
                            color: Theme.accentEmerald
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 6
                            height: 6
                            radius: 3
                            color: "#111115"
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: "#22222a"
                        visible: (backend.bgmCoverArt || "") !== ""
                        clip: true
                        Image {
                            anchors.fill: parent
                            source: backend.bgmCoverArt || ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            sourceSize.width: 140
                            sourceSize.height: 140
                        }
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: 2
                        visible: musicPopup.visible && (backend.isPlayingBgm || false)
                        Repeater {
                            model: [14, 22, 17, 26, 13]
                            delegate: Rectangle {
                                required property int index
                                required property int modelData
                                width: 3
                                height: modelData
                                radius: 1.5
                                color: "#E6FFFFFF"
                                SequentialAnimation on height {
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 5; duration: 180 + index * 35; easing.type: Easing.InOutSine }
                                    NumberAnimation { to: modelData; duration: 230 + index * 30; easing.type: Easing.InOutSine }
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Label {
                        text: (backend.isPlayingBgm ? "NOW PLAYING" : "SELECTED TRACK")
                        color: Theme.accentEmerald
                        font.family: "Inter"
                        font.pixelSize: Theme.textXs
                        font.bold: true
                        font.letterSpacing: 1
                    }
                    Label {
                        Layout.fillWidth: true
                        text: musicPopup.friendlyTrackTitle(backend.bgmTrackName || "No track selected")
                        color: Theme.textPrimary
                        font.family: "Inter"
                        font.pixelSize: Theme.textMd
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: backend.bgmPath ? musicPopup.friendlySourceMeta(backend.bgmPath) : "Add audio files from your library"
                        color: Theme.textSecondary
                        font.family: "Inter"
                        font.pixelSize: Theme.textXs
                        elide: Text.ElideMiddle
                        ToolTip.visible: bannerPathMa.containsMouse && Boolean(backend.bgmPath)
                        ToolTip.delay: 400
                        ToolTip.text: backend.bgmPath || ""
                        MouseArea { id: bannerPathMa; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                    }
                }

                Button {
                    implicitWidth: 38
                    implicitHeight: 38
                    text: backend.isPlayingBgm ? "Ⅱ" : "▶"
                    onClicked: backend.toggleBgmPlayback()
                    contentItem: Label {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 15
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 10
                        color: "#2563EB"
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            Layout.bottomMargin: 14
            spacing: 8

            Repeater {
                model: [
                    { title: "LIBRARY", tab: "library" },
                    { title: "PLAYLIST", tab: "playlist" }
                ]
                delegate: Button {
                    required property var modelData
                    implicitWidth: modelData.tab === "playlist" ? 104 : 96
                    implicitHeight: 34
                    onClicked: musicPopup.activeTab = modelData.tab
                    contentItem: Label {
                        text: modelData.title
                        color: musicPopup.activeTab === modelData.tab ? Theme.textPrimary : Theme.textDim
                        font.family: "Inter"
                        font.pixelSize: Theme.textXs
                        font.bold: true
                        font.letterSpacing: 0.7
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 9
                        color: musicPopup.activeTab === modelData.tab ? "#25252d" : "transparent"
                        border.color: musicPopup.activeTab === modelData.tab ? "#393943" : "transparent"
                    }
                }
            }

            TextField {
                id: searchField
                Layout.fillWidth: true
                implicitHeight: 36
                placeholderText: "Search tracks..."
                placeholderTextColor: "#71717A"
                color: Theme.textPrimary
                font.family: "Inter"
                font.pixelSize: Theme.textSm
                leftPadding: 34
                rightPadding: searchField.text.length > 0 ? 30 : 12
                onTextChanged: searchDebounce.restart()
                background: Rectangle {
                    radius: 9
                    color: "#17171d"
                    border.color: searchField.activeFocus ? "#3B82F6" : "#2D313E"
                }

                BroadcastIcon {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    name: "search"
                    iconSize: 13
                    color: searchField.activeFocus ? Theme.accentBlue : "#71717A"
                }

                Rectangle {
                    visible: searchField.text.length > 0
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20; height: 20; radius: 10
                    color: clearSearchMa.containsMouse ? "#282D3B" : "transparent"
                    BroadcastIcon { anchors.centerIn: parent; name: "close"; iconSize: 9; color: Theme.textSecondary }
                    MouseArea {
                        id: clearSearchMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { searchField.clear(); searchField.forceActiveFocus() }
                    }
                }
            }

            DarkComboBox {
                id: languageSelector
                visible: musicPopup.activeTab === "library"
                Layout.preferredWidth: 152
                implicitHeight: 36
                bordered: true
                boxRadius: 9
                boxColor: "#17171d"
                boxBorderColor: "#2D313E"
                textLeftPadding: 10
                fontBold: false
                model: musicPopup.languageOptions
                textRole: "name"
                currentIndex: {
                    for (let i = 0; i < musicPopup.languageOptions.length; ++i) {
                        if (musicPopup.languageOptions[i].code === musicPopup.languageFilterCode)
                            return i
                    }
                    return 0
                }
                onActivated: musicPopup.languageFilterCode = musicPopup.languageOptions[index].code
            }

            DarkComboBox {
                id: playlistSelector
                visible: musicPopup.activeTab === "playlist"
                Layout.preferredWidth: 168
                implicitHeight: 36
                bordered: true
                boxRadius: 9
                boxColor: "#17171d"
                boxBorderColor: "#2D313E"
                textLeftPadding: 10
                fontBold: false
                model: backend.bgmPlaylistNames || []
                currentIndex: Math.max(0, (backend.bgmPlaylistNames || []).indexOf(backend.currentBgmPlaylistName || ""))
                onActivated: backend.selectBgmPlaylist(currentText)
            }

            Button {
                visible: musicPopup.activeTab === "playlist"
                text: "＋ PLAYLIST"
                implicitWidth: 104
                implicitHeight: 36
                onClicked: createPlaylistDialog.open()
                contentItem: Label {
                    text: parent.text
                    color: Theme.accentEmerald
                    font.family: "Inter"
                    font.pixelSize: Theme.textXs
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 9
                    color: parent.hovered ? "#1a3a30" : "#17241f"
                    border.color: "#24533f"
                }
            }

            Button {
                text: "＋ ADD MUSIC"
                implicitWidth: 116
                implicitHeight: 36
                onClicked: backend.browseAndAddBgmFiles()
                contentItem: Label {
                    text: parent.text
                    color: Theme.textPrimary
                    font.family: "Inter"
                    font.pixelSize: Theme.textXs
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 9
                    color: parent.hovered ? "#1a3a30" : "#17241f"
                    border.color: "#24533f"
                }
            }

            Button {
                visible: musicPopup.activeTab === "library" && musicPopup.selectedTrackPaths.length > 0
                text: "ADD " + musicPopup.selectedTrackPaths.length
                implicitWidth: 92
                implicitHeight: 36
                onClicked: {
                    backend.addBgmTracksToPlaylist(musicPopup.selectedTrackPaths)
                    musicPopup.selectedTrackPaths = []
                }
                contentItem: Label {
                    text: parent.text
                    color: "white"
                    font.family: "Inter"
                    font.pixelSize: Theme.textXs
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 9
                    color: parent.hovered ? "#3B82F6" : "#2563EB"
                }
            }

        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            Layout.bottomMargin: 8
            spacing: 8

            Label {
                text: musicPopup.activeTab === "library"
                    ? (musicPopup.selectedTrackPaths.length > 0
                        ? musicPopup.selectedTrackPaths.length + " SELECTED"
                        : "SELECT MUSIC TO ADD")
                    : "UP NEXT · " + (backend.currentBgmPlaylistName || "PLAYLIST")
                color: Theme.textFaint
                font.family: "Inter"
                font.pixelSize: Theme.textXs
                font.bold: true
                font.letterSpacing: 0.8
            }

            // Library batch selection buttons
            Button {
                visible: musicPopup.activeTab === "library" && musicPopup.libraryTracks.length > 0
                text: "SELECT ALL"
                implicitHeight: 24
                implicitWidth: 78
                padding: 0
                contentItem: Label {
                    text: parent.text
                    color: parent.hovered ? Theme.textPrimary : "#A1A1AA"
                    font.family: "Inter"
                    font.pixelSize: Theme.textXs
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? "#24242e" : "#17171d"
                    border.color: "#2e2e38"
                }
                onClicked: {
                    var all = []
                    for (var i = 0; i < musicPopup.libraryTracks.length; ++i) {
                        all.push(musicPopup.libraryTracks[i].path)
                    }
                    musicPopup.selectedTrackPaths = all
                }
            }

            Button {
                visible: musicPopup.activeTab === "library" && musicPopup.selectedTrackPaths.length > 0
                text: "DESELECT"
                implicitHeight: 24
                implicitWidth: 74
                padding: 0
                contentItem: Label {
                    text: parent.text
                    color: "#EF4444"
                    font.family: "Inter"
                    font.pixelSize: Theme.textXs
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? "#2d1b1e" : "#1f1517"
                    border.color: "#3f2025"
                }
                onClicked: musicPopup.selectedTrackPaths = []
            }

            // Playlist clear button
            Button {
                visible: musicPopup.activeTab === "playlist" && musicPopup.playlistTracks.length > 0
                text: "CLEAR PLAYLIST"
                implicitHeight: 24
                implicitWidth: 108
                padding: 0
                contentItem: Label {
                    text: parent.text
                    color: parent.hovered ? "#EF4444" : "#A1A1AA"
                    font.family: "Inter"
                    font.pixelSize: Theme.textXs
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? "#2d1b1e" : "transparent"
                    border.color: parent.hovered ? "#3f2025" : "transparent"
                }
                onClicked: confirmClearDialog.open()
            }

            Item { Layout.fillWidth: true }

            Label {
                text: musicPopup.activeTab === "library"
                    ? musicPopup.libraryTracks.length + " TRACKS"
                    : musicPopup.playlistTracks.length + " TRACKS"
                color: Theme.textDim
                font.family: "Inter"
                font.pixelSize: Theme.textXs
                font.bold: true
            }
        }

        ListView {
            id: trackList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 14
            Layout.bottomMargin: 14
            clip: true
            spacing: 4
            focus: true
            activeFocusOnTab: true
            boundsBehavior: Flickable.StopAtBounds
            flickDeceleration: 4000
            maximumFlickVelocity: 5000
            cacheBuffer: 580
            reuseItems: true
            model: musicPopup.activeTab === "library"
                ? musicPopup.libraryTracks
                : musicPopup.playlistTracks
            ScrollBar.vertical: SlimScrollBar {
                policy: ScrollBar.AsNeeded
            }
            Keys.onPressed: function(event) {
                if (trackList.count === 0)
                    return

                const pageSize = Math.max(1, Math.floor(trackList.height / 60))
                if (event.key === Qt.Key_Up) {
                    trackList.currentIndex = Math.max(0, trackList.currentIndex - 1)
                } else if (event.key === Qt.Key_Down) {
                    trackList.currentIndex = Math.min(trackList.count - 1, trackList.currentIndex + 1)
                } else if (event.key === Qt.Key_PageUp) {
                    trackList.currentIndex = Math.max(0, trackList.currentIndex - pageSize)
                } else if (event.key === Qt.Key_PageDown) {
                    trackList.currentIndex = Math.min(trackList.count - 1, trackList.currentIndex + pageSize)
                } else if (event.key === Qt.Key_Home) {
                    trackList.currentIndex = 0
                } else if (event.key === Qt.Key_End) {
                    trackList.currentIndex = trackList.count - 1
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    const track = musicPopup.activeTab === "library"
                        ? musicPopup.libraryTracks[trackList.currentIndex]
                        : musicPopup.playlistTracks[trackList.currentIndex]
                    if (track) {
                        if (musicPopup.activeTab === "library")
                            backend.playBgmFile(track.path)
                        else
                            backend.playBgmTrack(track.index)
                    }
                } else if (event.key === Qt.Key_Space && musicPopup.activeTab === "library") {
                    const track = musicPopup.libraryTracks[trackList.currentIndex]
                    if (track)
                        musicPopup.toggleTrackSelection(track.path, !musicPopup.isTrackSelected(track.path))
                } else {
                    return
                }

                trackList.positionViewAtIndex(trackList.currentIndex, ListView.Contain)
                event.accepted = true
            }

            delegate: Rectangle {
                required property var modelData
                required property int index
                property string displayName: modelData.name || "Unknown track"
                property string displayThumbnailPath: modelData.thumbnailPath || ""
                readonly property bool isPlaylist: musicPopup.activeTab === "playlist"
                readonly property bool isCurrent: isPlaylist
                    ? (modelData.index === (musicPopup.backend.bgmIndex ?? -1))
                    : (Boolean(modelData.path) && modelData.path === (musicPopup.backend.bgmPath || ""))
                readonly property bool isKeyboardCurrent: trackList.activeFocus && index === trackList.currentIndex
                onModelDataChanged: {
                    displayName = modelData.name || "Unknown track"
                    displayThumbnailPath = modelData.thumbnailPath || ""
                    if (!displayThumbnailPath && modelData.path)
                        backend.requestBgmThumbnail(modelData.path)
                }
                Component.onCompleted: {
                    if (!displayThumbnailPath && modelData.path)
                        backend.requestBgmThumbnail(modelData.path)
                }
                width: trackList.width - 14
                height: 58
                radius: 9
                color: isCurrent ? "#172A24" : isKeyboardCurrent ? "#1E2532" : rowMouse.containsMouse ? "#191B23" : "transparent"
                border.color: isCurrent ? "#265341" : (rowMouse.containsMouse ? "#252936" : "transparent")
                border.width: 1

                Connections {
                    target: musicPopup.backend
                    ignoreUnknownSignals: true
                    function onBgmTrackMetadataUpdated(path, title, thumbnailUrl) {
                        if (path !== modelData.path)
                            return
                        if (title)
                            displayName = title
                        if (thumbnailUrl)
                            displayThumbnailPath = thumbnailUrl
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 12

                    Rectangle {
                        id: artworkTile
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 8
                        gradient: Gradient {
                            GradientStop { position: 0; color: isCurrent ? "#0f766e" : "#253044" }
                            GradientStop { position: 1; color: "#111318" }
                        }
                        clip: true
                        Image {
                            id: trackArtwork
                            anchors.fill: parent
                            source: displayThumbnailPath
                            visible: source !== ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            sourceSize.width: 96
                            sourceSize.height: 96
                        }

                        Item {
                            anchors.fill: parent
                            visible: !trackArtwork.visible
                            Rectangle {
                                anchors.centerIn: parent
                                width: 25
                                height: 25
                                radius: 13
                                color: "#161a20"
                                border.color: isCurrent ? "#10B981" : "#4b5563"
                                border.width: 1
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width: 7
                                height: 7
                                radius: 4
                                color: isCurrent ? Theme.accentEmerald : "#64748b"
                            }
                            Label {
                                anchors.centerIn: parent
                                text: "♫"
                                color: "#e5e7eb"
                                font.pixelSize: 13
                                font.bold: true
                            }
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4
                                spacing: 2
                                visible: isCurrent && (backend.isPlayingBgm || false)
                                Repeater {
                                    model: [6, 10, 7]
                                    delegate: Rectangle {
                                        required property int index
                                        required property int modelData
                                        width: 2
                                        height: modelData
                                        radius: 1
                                        color: Theme.accentEmerald
                                        SequentialAnimation on height {
                                            loops: Animation.Infinite
                                            running: artworkTile.visible && (backend.isPlayingBgm || false)
                                            NumberAnimation { to: 3; duration: 190 + index * 35; easing.type: Easing.InOutSine }
                                            NumberAnimation { to: modelData; duration: 230 + index * 30; easing.type: Easing.InOutSine }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label {
                            Layout.fillWidth: true
                            text: musicPopup.friendlyTrackTitle(displayName)
                            color: isCurrent ? Theme.accentEmerald : Theme.textPrimary
                            font.family: "Inter"
                            font.pixelSize: Theme.textSm
                            font.bold: isCurrent || rowMouse.containsMouse
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: musicPopup.friendlySourceMeta(modelData.path)
                            color: isCurrent ? "#6EE7B7" : Theme.textDim
                            font.family: "Inter"
                            font.pixelSize: Theme.textXs
                            elide: Text.ElideMiddle
                        }
                    }

                    Button {
                        visible: !isPlaylist && (rowMouse.containsMouse || hovered || isCurrent)
                        text: isCurrent && (backend.isPlayingBgm || false) ? "Ⅱ" : "▶"
                        implicitWidth: 30
                        implicitHeight: 30
                        onClicked: isCurrent ? backend.toggleBgmPlayback() : backend.playBgmFile(modelData.path)
                        contentItem: Label {
                            text: parent.text
                            color: isCurrent ? Theme.accentEmerald : Theme.textSecondary
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 7
                            color: parent.hovered ? "#252A38" : "#181B24"
                            border.color: parent.hovered ? Theme.accentBlue : "#282D3B"
                            border.width: 1
                        }
                    }

                    CheckBox {
                        id: trackSelection
                        visible: !isPlaylist
                        checked: musicPopup.isTrackSelected(modelData.path)
                        onToggled: musicPopup.toggleTrackSelection(modelData.path, checked)
                        indicator: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 20
                            x: parent.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 6
                            color: parent.checked ? "#10B981" : "#171922"
                            border.color: parent.checked ? "#10B981" : "#3E4556"
                            border.width: 1.5
                            Label {
                                anchors.centerIn: parent
                                visible: parent.parent.checked
                                text: "✓"
                                color: "#07130f"
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }
                    }

                    Button {
                        visible: isPlaylist
                        text: "▶"
                        implicitWidth: 32
                        implicitHeight: 30
                        onClicked: backend.playBgmTrack(modelData.index)
                        contentItem: Label {
                            text: parent.text
                            color: Theme.textSecondary
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 7
                            color: parent.hovered ? "#292932" : "transparent"
                        }
                    }

                    Button {
                        visible: isPlaylist
                        text: "×"
                        implicitWidth: 30
                        implicitHeight: 30
                        onClicked: backend.removeBgmTrack(modelData.index)
                        contentItem: Label {
                            text: parent.text
                            color: parent.parent.hovered ? Theme.accentRed : Theme.textFaint
                            font.pixelSize: 18
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 7
                            color: parent.hovered ? "#301d20" : "transparent"
                        }
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    z: -1
                    hoverEnabled: true
                    preventStealing: false
                    onDoubleClicked: {
                        if (isPlaylist)
                            backend.playBgmTrack(modelData.index)
                        else
                            backend.playBgmFile(modelData.path)
                    }
                    onClicked: {
                        if (isPlaylist)
                            backend.playBgmTrack(modelData.index)
                        else
                            backend.playBgmFile(modelData.path)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: trackList.count === 0
                width: parent.width - 60
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: musicPopup.activeTab === "library"
                    ? "No audio tracks found in your media library yet. Add music files or folders to get started."
                    : "Your playlist is empty. Add tracks from the library or choose audio files."
                color: Theme.textDim
                font.family: "Inter"
                font.pixelSize: Theme.textSm
            }
        }
    }

    Dialog {
        id: createPlaylistDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(380, Overlay.overlay ? Overlay.overlay.width - 40 : 380)
        modal: true
        focus: true
        title: "Create playlist"
        standardButtons: Dialog.NoButton
        padding: 20

        background: Rectangle {
            color: "#17171d"
            radius: 14
            border.color: "#34343d"
        }

        ColumnLayout {
            width: parent.width
            spacing: 12
            Label {
                text: "Give your playlist a name"
                color: Theme.textSecondary
                font.family: "Inter"
                font.pixelSize: Theme.textSm
            }
            TextField {
                id: playlistNameField
                Layout.fillWidth: true
                placeholderText: "Playlist name"
                selectByMouse: true
                color: Theme.textPrimary
                font.family: "Inter"
                onAccepted: createPlaylistButton.clicked()
                background: Rectangle {
                    radius: 8
                    color: "#101014"
                    border.color: playlistNameField.activeFocus ? "#3B82F6" : "#34343d"
                }
            }
            Label {
                id: playlistNameError
                Layout.fillWidth: true
                visible: text.length > 0
                text: ""
                color: Theme.accentRed
                font.family: "Inter"
                font.pixelSize: Theme.textXs
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "CANCEL"
                    onClicked: createPlaylistDialog.close()
                }
                Button {
                    id: createPlaylistButton
                    text: "CREATE"
                    onClicked: {
                        if (backend.createBgmPlaylist(playlistNameField.text)) {
                            playlistNameField.clear()
                            playlistNameError.text = ""
                            musicPopup.selectedTrackPaths = []
                            createPlaylistDialog.close()
                        } else {
                            playlistNameError.text = playlistNameField.text.trim().length === 0
                                ? "Enter a playlist name."
                                : "A playlist with that name already exists."
                        }
                    }
                }
            }
        }
        onOpened: {
            playlistNameError.text = ""
            playlistNameField.clear()
            playlistNameField.forceActiveFocus()
        }
    }

    Dialog {
        id: confirmClearDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(360, Overlay.overlay ? Overlay.overlay.width - 40 : 360)
        modal: true
        focus: true
        title: "Clear Playlist"
        standardButtons: Dialog.NoButton
        padding: 20

        background: Rectangle {
            color: "#17171d"
            radius: 14
            border.color: "#34343d"
        }

        ColumnLayout {
            width: parent.width
            spacing: 14
            Label {
                text: "Are you sure you want to remove all tracks from this playlist?"
                color: Theme.textPrimary
                font.family: "Inter"
                font.pixelSize: Theme.textSm
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "CANCEL"
                    onClicked: confirmClearDialog.close()
                }
                Button {
                    text: "CLEAR"
                    onClicked: {
                        backend.clearBgmPlaylist()
                        confirmClearDialog.close()
                    }
                    contentItem: Label {
                        text: parent.text
                        color: "white"
                        font.family: "Inter"
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: Theme.accentRed
                    }
                }
            }
        }
    }
}
