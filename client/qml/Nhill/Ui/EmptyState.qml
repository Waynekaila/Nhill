import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string iconName: "box"
    property string title: ""
    property string text: ""
    property string actionText: ""
    signal action()
    spacing: Theme.s2
    Item { Layout.fillHeight: true; Layout.preferredHeight: Theme.s5 }
    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        width: 56; height: 56; radius: 28
        color: Theme.primarySoft
        Icon { anchors.centerIn: parent; name: root.iconName; color: Theme.primary; size: 26 }
    }
    Text {
        Layout.alignment: Qt.AlignHCenter
        text: root.title
        font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold
        color: Theme.text
    }
    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 420
        horizontalAlignment: Text.AlignHCenter
        text: root.text
        wrapMode: Text.WordWrap
        font.family: Theme.font; font.pixelSize: Theme.fs_md
        color: Theme.textMuted
    }
    FbButton {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Theme.s2
        visible: root.actionText.length > 0
        text: root.actionText
        iconName: "plus"
        onClicked: root.action()
    }
    Item { Layout.fillHeight: true; Layout.preferredHeight: Theme.s5 }
}
