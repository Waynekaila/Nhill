import QtQuick
import QtQuick.Layouts

FbCard {
    id: root
    property string label: ""
    property string value: ""
    property string caption: ""
    property string iconName: "chart"
    property string tone: "primary"
    Layout.fillWidth: true
    // Grows when the label or the caption wraps (narrow phone screens).
    implicitHeight: Math.max(112, col.implicitHeight + 2 * padding)
    ColumnLayout {
        id: col
        anchors.fill: parent
        spacing: Theme.s1
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: root.label
                font.family: Theme.font; font.pixelSize: Theme.fs_sm; font.weight: Font.Medium
                color: Theme.textMuted
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                wrapMode: Text.Wrap; maximumLineCount: 2
                elide: Text.ElideRight
            }
            Rectangle {
                width: 30; height: 30; radius: 8
                color: root.tone === "accent" ? Theme.accentSoft : root.tone === "danger" ? Theme.dangerSoft : root.tone === "warning" ? Theme.warningSoft : Theme.primarySoft
                Icon { anchors.centerIn: parent; name: root.iconName; size: 16
                       color: root.tone === "accent" ? Theme.accent : root.tone === "danger" ? Theme.danger : root.tone === "warning" ? Theme.warning : Theme.primary }
            }
        }
        Text {
            text: root.value
            font.family: Theme.font; font.pixelSize: Theme.fs_xl; font.weight: Font.DemiBold
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 12
            // Width comes from the layout, not from the (shrinking) text: avoids a polish loop.
            Layout.preferredWidth: 1
            color: Theme.text
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            text: root.caption
            font.family: Theme.font; font.pixelSize: Theme.fs_xs
            color: Theme.textFaint
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            wrapMode: Text.Wrap; maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}
