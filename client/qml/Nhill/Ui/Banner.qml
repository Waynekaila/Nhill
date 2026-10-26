import QtQuick
import QtQuick.Layouts

// tone: info | warning | danger | success
Rectangle {
    id: root
    property string tone: "info"
    property string text: ""
    property string actionText: ""
    signal action()
    readonly property color fg: tone === "warning" ? Theme.warning : tone === "danger" ? Theme.danger : tone === "success" ? Theme.success : Theme.info
    color: tone === "warning" ? Theme.warningSoft : tone === "danger" ? Theme.dangerSoft : tone === "success" ? Theme.successSoft : Theme.infoSoft
    radius: Theme.radius
    implicitHeight: row.implicitHeight + 2 * Theme.s3
    Layout.fillWidth: true
    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Theme.s3
        spacing: Theme.s3
        Icon { name: root.tone === "success" ? "check" : root.tone === "info" ? "info" : "alert"; color: root.fg; size: 18; Layout.alignment: Qt.AlignTop }
        Text {
            text: root.text
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.text
            font.family: Theme.font; font.pixelSize: Theme.fs_sm
        }
        FbButton {
            visible: root.actionText.length > 0
            text: root.actionText
            variant: "secondary"
            onClicked: root.action()
        }
    }
}
