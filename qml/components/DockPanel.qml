import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property color accentColor: "#3B82F6"
    property string title: ""
    property alias headerTrailing: trailingSlot.data
    property alias content: bodySlot.data

    Layout.fillWidth: true
    color: "#0f0f12"
    radius: 12
    border.color: "#1f1f23"
    border.width: 1
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 3
                Layout.preferredHeight: 16
                radius: 2
                color: root.accentColor
            }

            Label {
                text: root.title
                color: "#ffffff"
                font.family: "Inter"
                font.pixelSize: 11
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
