import QtQuick
import QtQuick.Controls

ScrollBar {
    id: control

    implicitWidth: orientation === Qt.Vertical ? 10 : 0
    implicitHeight: orientation === Qt.Horizontal ? 10 : 0
    padding: 2

    background: Rectangle {
        color: "transparent"
    }

    contentItem: Rectangle {
        implicitWidth: control.orientation === Qt.Vertical ? 6 : 24
        implicitHeight: control.orientation === Qt.Horizontal ? 6 : 24
        radius: 3
        color: control.pressed
            ? "#60A5FA"
            : (control.hovered ? "#525B73" : "#33394A")
        opacity: (control.policy === ScrollBar.AlwaysOn || control.active || control.hovered || control.pressed) ? 0.92 : 0.35

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }
}

