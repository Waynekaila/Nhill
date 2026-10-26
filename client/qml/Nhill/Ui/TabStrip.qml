import QtQuick
import QtQuick.Controls

// Segmented tabs. model: [{value, text}]
Row {
    id: root
    property var model: []
    property var current: model.length > 0 ? model[0].value : undefined
    signal selected(var value)
    spacing: 0
    Repeater {
        model: root.model
        delegate: AbstractButton {
            id: tab
            readonly property bool active: root.current === modelData.value
            height: 36
            width: Math.max(implicitWidth, 64)
            implicitWidth: label.implicitWidth + 2 * Theme.s4
            focusPolicy: Qt.StrongFocus
            Accessible.name: modelData.text
            onClicked: { root.current = modelData.value; root.selected(modelData.value) }
            contentItem: Text {
                id: label
                text: modelData.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: Theme.font; font.pixelSize: Theme.fs_md
                font.weight: tab.active ? Font.DemiBold : Font.Normal
                color: tab.active ? Theme.primary : Theme.textMuted
            }
            background: Item {
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: tab.active ? 2 : 1; color: tab.active ? Theme.primary : Theme.line }
                Rectangle { anchors.fill: parent; color: Theme.surfaceAlt; visible: tab.hovered && !tab.active; radius: 4 }
            }
        }
    }
}
