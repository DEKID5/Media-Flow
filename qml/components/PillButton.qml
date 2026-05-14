import QtQuick
import QtQuick.Controls
import MediaFlow 1.0

Button {
    id: control
    
    property color accentColor: "#3B82F6"
    property bool primary: false
    property string iconName: ""
    property int iconSize: 14
    property real customRadius: 10

    contentItem: Row {
        spacing: 8
        anchors.centerIn: parent
        
        BroadcastIcon {
            visible: control.iconName !== ""
            name: control.iconName
            iconSize: control.iconSize
            color: control.primary ? "white" : (control.down ? control.accentColor : (control.hovered ? "white" : "#A1A1AA"))
            anchors.verticalCenter: parent.verticalCenter
        }
        
        Label {
            text: control.text
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 0.5
            color: control.primary ? "white" : (control.down ? control.accentColor : (control.hovered ? "white" : "#A1A1AA"))
            verticalAlignment: Text.AlignVCenter
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 36
        radius: control.customRadius
        
        color: control.primary ? control.accentColor : (control.hovered ? "#1AFFFFFF" : "#0DFFFFFF")
        border.color: control.primary ? "transparent" : (control.hovered ? control.accentColor : "#1AFFFFFF")
        border.width: 1
        
        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }
        
        // Subtle gradient overlay for primary buttons
        Rectangle {
            anchors.fill: parent; radius: parent.radius
            visible: control.primary
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#26FFFFFF" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
        
        // Inner glow/border for primary
        Rectangle {
            anchors.fill: parent; radius: parent.radius; color: "transparent"
            border.color: "#33FFFFFF"; border.width: 1; visible: control.primary
        }
    }

    scale: control.pressed ? 0.96 : (control.hovered ? 1.02 : 1.0)
    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
}
