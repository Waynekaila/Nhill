import QtQuick

Rectangle {
    id: root
    default property alias content: inner.data
    property int padding: Theme.s4
    color: Theme.surface
    radius: Theme.radiusLg
    border.color: Theme.line
    border.width: 1
    implicitHeight: inner.childrenRect.height + 2 * padding
    Item {
        id: inner
        anchors.fill: parent
        anchors.margins: root.padding
    }
}
