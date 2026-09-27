import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

// A Dialog with its header/footer actually styled to match the app -- plain
// Dialog only lets `background`/`contentItem` be customized; its title and
// standardButtons footer fall back to QtQuick Controls' default light Basic
// style otherwise, which is what left every confirm/warning dialog in the
// app with a white title bar and unstyled buttons.
Dialog {
    id: control
    modal: true
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.NoButton

    property bool showCancel: true
    property string acceptText: "OK"
    property string cancelText: "CANCEL"
    property color acceptColor: Theme.accentBlue

    background: Rectangle { color: "#1a1a1e"; radius: Theme.radiusLg; border.color: "#333" }

    header: Item {
        implicitHeight: 50
        Label {
            anchors.left: parent.left; anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            text: control.title
            color: Theme.textPrimary
            font.bold: true
            font.pixelSize: Theme.textMd
        }
    }

    footer: Item {
        implicitHeight: 60
        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: Theme.space2
            Item { Layout.fillWidth: true }
            PillButton {
                visible: control.showCancel
                text: control.cancelText
                implicitWidth: 84; implicitHeight: 32; font.pixelSize: 9
                onClicked: control.reject()
            }
            PillButton {
                text: control.acceptText
                primary: true
                accentColor: control.acceptColor
                implicitWidth: 84; implicitHeight: 32; font.pixelSize: 9
                onClicked: control.accept()
            }
        }
    }
}
