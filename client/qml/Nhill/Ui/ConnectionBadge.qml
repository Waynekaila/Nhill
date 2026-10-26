import QtQuick
import QtQuick.Controls
import Nhill.Core

// Connection indicator: online / offline / server problem.
Rectangle {
    id: root
    readonly property int state: Api.connectionState
    readonly property bool problem: state === 2 || state === 3
    implicitWidth: row.implicitWidth + 2 * Theme.s3
    implicitHeight: 28
    radius: 14
    color: state === 2 ? Theme.dangerSoft : state === 3 ? Theme.warningSoft : Theme.successSoft
    visible: state !== 0
    Accessible.name: label.text
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        Icon { name: root.problem ? "offline" : "online"; size: 15; anchors.verticalCenter: parent.verticalCenter
               color: root.state === 2 ? Theme.danger : root.state === 3 ? Theme.warning : Theme.success }
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            text: root.state === 2 ? qsTr("Offline") : root.state === 3 ? qsTr("Server problem") : qsTr("Online")
            visible: !Theme.compact || root.problem
            color: root.state === 2 ? Theme.danger : root.state === 3 ? Theme.warning : Theme.success
            font.family: Theme.font; font.pixelSize: Theme.fs_xs; font.weight: Font.DemiBold
        }
    }
    MouseArea { anchors.fill: parent; onClicked: Api.checkConnection() }
    ToolTip.visible: hovered && problem
    property bool hovered: false
    HoverHandler { onHoveredChanged: root.hovered = hovered }
    ToolTip.text: qsTr("Operations that need the server are paused. Nothing is lost: retry when the connection is back.")
}
