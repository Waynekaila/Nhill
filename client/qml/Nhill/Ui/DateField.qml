import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core

// ISO date (yyyy-MM-dd) with a calendar popup.
FbTextField {
    id: root
    property string value: ""
    readonly property bool valid: value.length === 0 || /^\d{4}-\d{2}-\d{2}$/.test(value)
    placeholderText: Fmt.language === "fr" ? "jj/mm/aaaa" : "dd/mm/yyyy"
    text: value ? Fmt.date(value) : ""
    readOnly: true
    rightPadding: 40
    invalid: !valid
    MouseArea { anchors.fill: parent; onClicked: picker.open() }
    Icon { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; name: "calendar"; size: 18; color: Theme.textMuted }
    Popup {
        id: picker
        y: root.height + 4
        margins: Theme.s2          // 2.0.0: kept inside the window (moved up near the bottom edge)
        padding: Theme.s3
        property date shown: root.value ? new Date(root.value + "T12:00:00") : new Date()
        background: Rectangle { color: Theme.surface; border.color: Theme.line; radius: Theme.radius }
        contentItem: ColumnLayout {
            RowLayout {
                FbIconButton { iconName: "chevronLeft"; tip: qsTr("Previous month"); onClicked: picker.shown = new Date(picker.shown.getFullYear(), picker.shown.getMonth() - 1, 1) }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: grid.title
                    color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold
                }
                FbIconButton { iconName: "chevronRight"; tip: qsTr("Next month"); onClicked: picker.shown = new Date(picker.shown.getFullYear(), picker.shown.getMonth() + 1, 1) }
            }
            DayOfWeekRow { locale: grid.locale; Layout.fillWidth: true
                delegate: Text { text: shortName; color: Theme.textFaint; font.pixelSize: Theme.fs_xs; horizontalAlignment: Text.AlignHCenter } }
            MonthGrid {
                id: grid
                month: picker.shown.getMonth()
                year: picker.shown.getFullYear()
                locale: Qt.locale(Fmt.language === "fr" ? "fr_FR" : "en_GB")
                Layout.fillWidth: true
                delegate: Rectangle {
                    implicitWidth: 34; implicitHeight: 30; radius: 6
                    readonly property string iso: Qt.formatDate(model.date, "yyyy-MM-dd")
                    color: iso === root.value ? Theme.primary : ma.containsMouse ? Theme.surfaceAlt : "transparent"
                    opacity: model.month === grid.month ? 1 : 0.35
                    Text { anchors.centerIn: parent; text: model.day; color: iso === root.value ? Theme.textOnPrimary : Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                    MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true; onClicked: { root.value = parent.iso; picker.close() } }
                }
            }
            RowLayout {
                FbButton { text: qsTr("Today"); variant: "ghost"; onClicked: { root.value = Fmt.today(); picker.close() } }
                Item { Layout.fillWidth: true }
                FbButton { text: qsTr("Clear"); variant: "ghost"; onClicked: { root.value = ""; picker.close() } }
            }
        }
    }
}
