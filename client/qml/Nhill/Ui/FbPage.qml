import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Base for application pages: title row with actions + content area.
// Set scrollable: true for form-like pages.
Item {
    id: root
    property string title: ""
    property string subtitle: ""
    property bool scrollable: false
    property alias actions: actionRow.data
    default property alias content: holder.data
    property int maxContentWidth: 1280

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.compact ? Theme.s3 : Theme.s5
        spacing: Theme.s4
        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: root.maxContentWidth
            spacing: Theme.s3
            visible: !Theme.compact || actionRow.children.length > 0
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    visible: !Theme.compact
                    text: root.title
                    font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.DemiBold
                    color: Theme.text
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    visible: root.subtitle.length > 0 && !Theme.compact
                    text: root.subtitle
                    font.family: Theme.font; font.pixelSize: Theme.fs_md
                    color: Theme.textMuted
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
            Flow {
                id: actionRow
                spacing: Theme.s2
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Layout.fillWidth: Theme.compact
                layoutDirection: Theme.compact ? Qt.LeftToRight : Qt.RightToLeft
            }
        }
        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: root.maxContentWidth
            sourceComponent: root.scrollable ? scrollComp : plainComp
        }
    }
    Item { id: holder; visible: false }

    Component {
        id: plainComp
        Item {
            id: plainBox
            Component.onCompleted: { while (holder.children.length > 0) holder.children[0].parent = plainBox }
        }
    }
    Component {
        id: scrollComp
        ScrollView {
            id: sv
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                id: col
                width: sv.availableWidth
                spacing: Theme.s4
                Component.onCompleted: { while (holder.children.length > 0) holder.children[0].parent = col }
            }
        }
    }
}
