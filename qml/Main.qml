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
    // Stays hidden until the splash screen signals it's done (see below) --
    // showing this immediately let the operator see an empty dashboard
    // flash while the startup library scan was still running.
    visible: false
    title: qsTr("MediaFlow — Broadcast Suite")
    color: "#050505"

    // Without this, closing this window alone doesn't actually end the app
    // if Extended Feed, the full-screen timer, or Zoom broadcasting left
    // one of their own separate windows open -- Qt won't quit while any
    // top-level window is still visible, so the process (still sending
    // frames to Zoom, still showing content on the audience screen) kept
    // running invisibly in the background. Stop/hide everything first,
    // then quit unconditionally regardless of what else might be open.
    onClosing: (close) => {
        MediaFlowBackend.shutdownAllOutputs()
        Qt.quit()
    }

    SplashScreen {
        id: splash
        onFinished: {
            operatorRoot.visible = true
            operatorRoot.raise()
            operatorRoot.requestActivate()
            splash.destroy()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#050505"
    }

    OperatorDashboard {
        anchors.fill: parent
        onSettingsRequested: {
            // requestActivate() already brings the window to the front on
            // Windows as part of activating it -- the separate raise() was
            // a redundant extra native call every time Settings opened.
            settingsWindow.show()
            settingsWindow.requestActivate()
        }
    }

    SettingsWindow {
        id: settingsWindow
    }

    footer: Rectangle {
        height: 24
        color: "#050505"
        border.color: "#111113"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            Label {
                text: "● LOGGED IN AS OPERATOR"
                color: "#3a86ff"
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 1
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "MEDIAFLOW SUITE V1.0.0"
                color: "#4a4a50"
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 1
            }
        }
    }
}
