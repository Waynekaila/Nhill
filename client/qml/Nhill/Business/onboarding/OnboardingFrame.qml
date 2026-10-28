import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

// Shared frame of the first-launch screens: brand panel on desktop.
Rectangle {
    id: root
    color: Theme.bg
    default property alias content: column.data
    property bool showBack: true
    property int formWidth: 520
    signal backRequested()

    RowLayout {
        anchors.fill: parent
        spacing: 0
        // Brand panel (desktop only)
        Rectangle {
            visible: !Theme.compact
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(460, root.width * 0.38)
            color: Theme.sidebar
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 40
                spacing: Theme.s4
                LogoMark { variant: "reverse"; app: "business"; Layout.preferredHeight: 40; Layout.preferredWidth: 220; horizontalAlignment: Image.AlignLeft }
                Item { Layout.fillHeight: true }
                Text {
                    text: qsTr("Sales, invoices, stock and payments in one place.")
                    color: "#FFFFFF"
                    font.family: Theme.font; font.pixelSize: 28; font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Text {
                    text: qsTr("For traders, independent entrepreneurs and SMEs. Your currency, your taxes, in French or English.")
                    color: Theme.sidebarText
                    font.family: Theme.font; font.pixelSize: Theme.fs_lg
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Item { Layout.fillHeight: true }
                Text {
                    text: qsTr("Your data is isolated per business and travels encrypted (HTTPS).")
                    color: Theme.sidebarMuted
                    font.family: Theme.font; font.pixelSize: Theme.fs_sm
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
        // Form
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: Theme.s4
                    FbButton { visible: root.showBack; text: qsTr("Back"); iconName: "chevronLeft"; variant: "ghost"; onClicked: root.backRequested() }
                    LogoMark { app: "business"; visible: Theme.compact; Layout.preferredHeight: 28; Layout.preferredWidth: 150 }
                    Item { Layout.fillWidth: true }
                    FbButton {
                        text: Prefs.language === "fr" ? "English" : "Français"
                        iconName: "globe"; variant: "ghost"
                        Accessible.name: qsTr("Change language")
                        onClicked: Prefs.language = (Prefs.language === "fr" ? "en" : "fr")
                    }
                }
                ColumnLayout {
                    id: column
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: false
                    Layout.preferredWidth: Math.min(root.formWidth, scroll.availableWidth - 2 * Theme.s4)
                    Layout.topMargin: Theme.compact ? Theme.s2 : 48
                    Layout.bottomMargin: Theme.s6
                    spacing: Theme.s4
                }
            }
        }
    }
}
