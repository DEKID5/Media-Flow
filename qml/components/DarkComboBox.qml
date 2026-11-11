import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MediaFlow 1.0

ComboBox {
    id: control

    property bool bordered: false
    property string placeholderLabel: ""
    property int fontPixelSize: Theme.textXs
    property bool fontBold: true
    property int textLeftPadding: 8
    property int textRightPadding: 24
    property int boxRadius: Theme.radius
    property color boxColor: "#17171D"
    property color boxBorderColor: "#292930"
    property int minPopupWidth: 0

    flat: true
    hoverEnabled: true

    contentItem: Label {
        leftPadding: control.textLeftPadding
        rightPadding: control.textRightPadding
        text: control.displayText || control.placeholderLabel
        color: control.displayText ? Theme.textPrimary : Theme.textDim
        font.family: Theme.sansFont
        font.pixelSize: control.fontPixelSize
        font.bold: control.fontBold
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignLeft
        elide: Text.ElideRight
    }

    indicator: Item {
        x: control.width - width - 8
        y: (control.height - height) / 2
        width: 12
        height: 12

        BroadcastIcon {
            anchors.centerIn: parent
            name: "chevron-down"
            iconSize: 10
            color: control.hovered || control.popup.visible ? Theme.textPrimary : Theme.textSecondary
            rotation: control.popup.visible ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: 140; easing.type: Easing.OutQuad } }
        }
    }

    background: Rectangle {
        color: control.bordered ? control.boxColor : "transparent"
        radius: control.boxRadius
        border.color: control.bordered
            ? (control.popup.visible || control.activeFocus ? Theme.accentBlue : (control.hovered ? "#3E4556" : control.boxBorderColor))
            : "transparent"
        border.width: control.bordered ? 1 : 0
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    delegate: ItemDelegate {
        id: rowItem
        required property int index

        width: popupMenu.width - 8
        implicitHeight: 34
        highlighted: control.highlightedIndex === index

        readonly property string resolvedText: control.textAt(index)
        readonly property bool isCurrent: control.currentIndex === index

        contentItem: RowLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: rowItem.resolvedText
                color: rowItem.isCurrent ? "#93C5FD" : (rowItem.highlighted ? Theme.textPrimary : Theme.textSecondary)
                font.family: Theme.sansFont
                font.pixelSize: Theme.textSm
                font.bold: rowItem.isCurrent
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            BroadcastIcon {
                visible: rowItem.isCurrent
                name: "check"
                iconSize: 10
                color: Theme.accentBlue
            }
        }

        background: Rectangle {
            radius: 6
            color: rowItem.isCurrent
                ? "#193B82F6"
                : (rowItem.highlighted || rowItem.hovered ? "#222736" : "transparent")
        }
    }

    popup: Popup {
        id: popupMenu
        y: control.height + 4
        width: Math.max(control.width, control.minPopupWidth)
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
        padding: 4

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 110; easing.type: Easing.OutQuad }
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollBar.vertical: SlimScrollBar { }
        }

        background: Rectangle {
            radius: 10
            color: "#15171F"
            border.color: "#2E3342"
            border.width: 1
        }
    }
}
