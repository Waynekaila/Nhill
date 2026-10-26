import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Modal dialog: full-screen sheet on phones, centred card on desktop.
Popup {
    id: root
    property string title: ""
    property string subtitle: ""
    default property alias content: body.data
    property alias footer: footerRow.data
    property int preferredWidth: 560
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    parent: Overlay.overlay
    readonly property bool sheet: Theme.compact
    width: sheet ? parent.width : Math.min(preferredWidth, parent.width - 48)
    height: sheet ? parent.height : Math.min(implicitHeight, parent.height - 48)
    x: (parent.width - width) / 2
    y: sheet ? 0 : (parent.height - height) / 2
    padding: 0
    Overlay.modal: Rectangle { color: Theme.overlay }
    background: Rectangle {
        color: Theme.surface
        radius: root.sheet ? 0 : Theme.radiusLg
        border.color: Theme.line
    }
    contentItem: ColumnLayout {
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.s4
            Layout.bottomMargin: Theme.s2
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: root.title
                    font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold
                    color: Theme.text
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
                Text {
                    visible: root.subtitle.length > 0
                    text: root.subtitle
                    font.family: Theme.font; font.pixelSize: Theme.fs_sm
                    color: Theme.textMuted
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
            }
            FbIconButton { iconName: "close"; tip: qsTr("Close"); onClicked: root.close() }
        }
        ScrollView {
            id: scroller
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: body.implicitHeight + 2 * Theme.s4
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                id: body
                width: scroller.availableWidth - 2 * Theme.s4
                x: Theme.s4
                y: Theme.s2
                spacing: Theme.s3
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line; visible: footerRow.children.length > 0 }
        RowLayout {
            id: footerRow
            Layout.fillWidth: true
            Layout.margins: Theme.s3
            spacing: Theme.s2
            layoutDirection: Qt.RightToLeft
            visible: children.length > 0
        }
    }
}
