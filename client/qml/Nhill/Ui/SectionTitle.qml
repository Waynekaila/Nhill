import QtQuick
import QtQuick.Layouts

RowLayout {
    property string text: ""
    property string hint: ""
    Layout.fillWidth: true
    spacing: Theme.s2
    Text {
        text: parent.text
        font.family: Theme.font; font.pixelSize: Theme.fs_md; font.weight: Font.DemiBold
        color: Theme.text
    }
    Text {
        visible: parent.hint.length > 0
        text: parent.hint
        font.family: Theme.font; font.pixelSize: Theme.fs_sm
        color: Theme.textFaint
        Layout.fillWidth: true
        elide: Text.ElideRight
    }
    // Keeps the title stretchable when there is no hint (a layout without a filling
    // child never grows past its implicit width).
    Item { visible: parent.hint.length === 0; Layout.fillWidth: true; implicitHeight: 1 }
}
