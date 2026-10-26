import QtQuick
import QtQuick.Controls

ToolButton {
    id: control
    property string iconName: "dot"
    property color iconColor: Theme.text
    property string tip: ""
    implicitWidth: Theme.controlHeight
    implicitHeight: Theme.controlHeight
    focusPolicy: Qt.StrongFocus
    Accessible.name: tip
    ToolTip.visible: tip.length > 0 && hovered
    ToolTip.text: tip
    ToolTip.delay: 600
    contentItem: Item {
        Icon { anchors.centerIn: parent; name: control.iconName; color: control.iconColor; size: 20 }
    }
    background: Rectangle {
        radius: Theme.radius
        color: control.down ? Theme.sunken : control.hovered ? Theme.surfaceAlt : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.focus
    }
}
