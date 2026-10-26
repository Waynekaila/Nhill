import QtQuick
import QtQuick.Controls

ScrollView {
    id: root
    property alias text: area.text
    property alias placeholderText: area.placeholderText
    implicitHeight: 88
    TextArea {
        id: area
        wrapMode: TextEdit.Wrap
        font.family: Theme.font
        font.pixelSize: Theme.fs_md
        color: Theme.text
        placeholderTextColor: Theme.textFaint
        selectByMouse: true
        padding: Theme.s3
        Accessible.name: placeholderText
        // Tab / Shift+Tab move the focus (keyboard users) instead of inserting a tab.
        Keys.onTabPressed: function(event) { var n = area.nextItemInFocusChain(true); if (n) n.forceActiveFocus(Qt.TabFocusReason); event.accepted = true }
        Keys.onBacktabPressed: function(event) { var n = area.nextItemInFocusChain(false); if (n) n.forceActiveFocus(Qt.BacktabFocusReason); event.accepted = true }
        background: Rectangle {
            radius: Theme.radius
            color: Theme.surface
            border.width: area.activeFocus ? 2 : 1
            border.color: area.activeFocus ? Theme.focus : Theme.inputBorder
        }
    }
}
