import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FbDialog {
    id: root
    property string message: ""
    property string confirmText: qsTr("Confirm")
    property bool destructive: false
    signal accepted()
    preferredWidth: 460
    Text {
        text: root.message
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.family: Theme.font; font.pixelSize: Theme.fs_md
        color: Theme.text
    }
    footer: [
        FbButton { text: root.confirmText; variant: root.destructive ? "danger" : "primary"; onClicked: { root.close(); root.accepted() } },
        FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: root.close() }
    ]
}
