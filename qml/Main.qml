import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MediaFlow 1.0

ApplicationWindow {
    id: operatorRoot
    width: 1400
    height: 900
    visible: true
    title: qsTr("MediaFlow — Broadcast Suite")
    color: "#050505"

    Rectangle {
        anchors.fill: parent
        color: "#050505"
    }

    OperatorDashboard {
        anchors.fill: parent
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
