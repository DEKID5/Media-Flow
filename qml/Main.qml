import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MediaFlow 1.0

ApplicationWindow {
    id: operatorRoot
    width: 1400
    height: 900
    // No floor previously existed, so the dashboard's fixed-width toolbar
    // controls and side panels would clip/overflow uncontrollably below
    // ~1400px. OperatorDashboard.qml's header collapses its broadcast
    // buttons to icon-only below ~1950px (compactHeader) and the
    // branding/week/meeting-type/language controls further below ~1100px
    // (veryCompactHeader), which is what makes a floor this low possible --
    // the three-column dock below the header only needs ~880px on its own,
    // so 950 keeps a small margin above that while still fitting under a
    // standard 1920-wide display's half-screen Windows Snap slot (960px).
    minimumWidth: 950
    minimumHeight: 700
    visible: true
    title: qsTr("MediaFlow — Broadcast Suite")
    color: "#050505"
    property bool borderlessFullScreen: false
    property rect windowedGeometry: Qt.rect(0, 0, 1400, 900)
    property int windowedFlags: Qt.Window
    property int windowedVisibility: Window.Windowed

    function toggleBorderlessFullScreen() {
        visible = false
        if (!borderlessFullScreen) {
            windowedGeometry = Qt.rect(x, y, width, height)
            windowedFlags = flags
            windowedVisibility = visibility
            visibility = Window.Windowed
            flags = flags | Qt.FramelessWindowHint
            x = Screen.virtualX
            y = Screen.virtualY
            width = Screen.width
            height = Screen.height - 1
            borderlessFullScreen = true
        } else {
            flags = windowedFlags
            x = windowedGeometry.x
            y = windowedGeometry.y
            width = windowedGeometry.width
            height = windowedGeometry.height
            borderlessFullScreen = false
        }
        visible = true
        if (!borderlessFullScreen)
            visibility = windowedVisibility
        raise()
        requestActivate()
    }

    Shortcut {
        sequence: "F11"
        onActivated: operatorRoot.toggleBorderlessFullScreen()
    }

    Shortcut {
        sequence: "Escape"
        enabled: operatorRoot.borderlessFullScreen && !settingsWindow.visible
        onActivated: operatorRoot.toggleBorderlessFullScreen()
    }

    Rectangle {
        anchors.fill: parent
        color: "#050505"
    }

    OperatorDashboard {
        anchors.fill: parent
        onSettingsRequested: settingsWindow.open()
    }

    SettingsWindow {
        id: settingsWindow
    }

    SplashScreen {
        id: splash
        z: 1000
        onFinished: {
            operatorRoot.raise()
            operatorRoot.requestActivate()
            splash.destroy()
        }
    }
}
