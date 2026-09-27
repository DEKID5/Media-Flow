import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

Rectangle {
    id: root

    property color accentColor: Theme.accentBlue
    property string title: ""
    property alias headerTrailing: trailingSlot.data
    property alias content: bodySlot.data

    Layout.fillWidth: true
    color: Theme.panelBgDark
    radius: Theme.radiusLg
    border.color: Theme.panelBorder
    border.width: 1
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            spacing: Theme.space2 + 2

            Rectangle {
                Layout.preferredWidth: 3
                Layout.preferredHeight: 16
                radius: 2
                color: root.accentColor
            }

            Label {
                text: root.title
                color: Theme.textPrimary
                font.family: "Inter"
                font.pixelSize: Theme.textSm
                font.bold: true
                font.letterSpacing: 1.5
                verticalAlignment: Text.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            Item {
                id: trailingSlot
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: childrenRect.width
                implicitHeight: childrenRect.height
            }
        }

        Item {
            id: bodySlot
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
