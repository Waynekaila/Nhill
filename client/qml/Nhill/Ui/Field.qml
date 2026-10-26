import QtQuick
import QtQuick.Layouts

// Label + control + help/error text. Put the control inside.
ColumnLayout {
    id: root
    property string label: ""
    property string help: ""
    property string error: ""
    property bool required: false
    default property alias content: holder.data
    spacing: 4
    Layout.fillWidth: true
    Text {
        visible: root.label.length > 0
        text: root.label + (root.required ? " *" : "")
        font.family: Theme.font
        font.pixelSize: Theme.fs_sm
        font.weight: Font.Medium
        color: Theme.textMuted
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
    Item {
        id: holder
        Layout.fillWidth: true
        implicitHeight: children.length > 0 ? children[0].implicitHeight : 0
        onWidthChanged: if (children.length > 0) children[0].width = width
        Component.onCompleted: if (children.length > 0) children[0].width = width
    }
    Text {
        visible: root.error.length > 0 || root.help.length > 0
        text: root.error.length > 0 ? root.error : root.help
        color: root.error.length > 0 ? Theme.danger : Theme.textFaint
        font.family: Theme.font
        font.pixelSize: Theme.fs_xs
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
