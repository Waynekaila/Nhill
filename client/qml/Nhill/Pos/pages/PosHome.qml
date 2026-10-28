import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Pos

// Register choice and opening of the cash session (opening float).
Rectangle {
    id: root
    color: Theme.bg
    property var selected: null
    readonly property var regs: PosNav.config ? PosNav.config.registers : []
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.s6
        spacing: Theme.s4
        RowLayout {
            Layout.fillWidth: true
            LogoMark { app: "pos"; Layout.preferredHeight: 34; Layout.preferredWidth: 200; horizontalAlignment: Image.AlignLeft }
            Item { Layout.fillWidth: true }
            Text { text: (Session.me.user ? Session.me.user.full_name : "") + " · " + Labels.role(Session.role); color: Theme.textMuted; font.family: Theme.font }
            FbButton { text: qsTr("Sign out"); variant: "ghost"; iconName: "logout"; onClicked: PosNav.window.signOut() }
        }
        Text { text: qsTr("Choose your register"); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.DemiBold }
        Text { text: PosNav.config ? PosNav.config.establishment : ""; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_lg }
        Banner { Layout.fillWidth: true; visible: root.regs.length === 0; tone: "warning"
                 text: qsTr("No register is assigned to you. The owner or an administrator assigns registers in Nhill Business (Settings → Point of sale).") }
        Flow {
            Layout.fillWidth: true
            spacing: Theme.s3
            Repeater {
                model: root.regs
                delegate: Rectangle {
                    id: tile
                    readonly property bool busyByOther: !!modelData.open_session && modelData.open_session.opened_by !== (Session.me.user ? Session.me.user.id : "")
                    width: 260; height: 150; radius: Theme.radiusLg
                    color: root.selected && root.selected.id === modelData.id ? Theme.primarySoft : Theme.surface
                    border.color: root.selected && root.selected.id === modelData.id ? Theme.primary : Theme.line
                    border.width: root.selected && root.selected.id === modelData.id ? 2 : 1
                    opacity: busyByOther ? 0.6 : 1
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: Theme.s4; spacing: 4
                        Text { text: modelData.code; color: Theme.primary; font.family: Theme.mono; font.pixelSize: Theme.fs_xl; font.weight: Font.Bold }
                        Text { text: modelData.name; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold }
                        Text { text: (modelData.location ? modelData.location + " · " : "") + modelData.warehouse; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm; elide: Text.ElideRight; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                        FbChip { text: modelData.open_session ? qsTr("Open by %1").arg(modelData.open_session.opened_by_name) : qsTr("Closed"); tone: modelData.open_session ? "warning" : "neutral" }
                    }
                    MouseArea { anchors.fill: parent; enabled: !tile.busyByOther; cursorShape: Qt.PointingHandCursor; onClicked: root.selected = modelData }
                }
            }
        }
        FbCard {
            visible: !!root.selected
            Layout.preferredWidth: 520
            ColumnLayout {
                width: parent.width
                spacing: Theme.s3
                SectionTitle { text: root.selected ? qsTr("Open register %1").arg(root.selected.code) : "" }
                Field { label: qsTr("Opening float (cash in the drawer)"); help: qsTr("Count the cash before opening; it is part of the expected amount at closing.")
                        MoneyField { id: floatField; currency: PosNav.config ? PosNav.config.currency : Session.currency } }
                FbButton { text: qsTr("Open the cash session"); iconName: "check"; Layout.fillWidth: true
                           onClicked: Api.send("POST", "/api/v1/pos/sessions", { register_id: root.selected.id, opening_float: floatField.value || "0" }, function(r) {
                               if (!r.ok) { PosNav.fail(r); return }
                               PosNav.session = r.data; PosNav.register = root.selected
                               PosNav.window.loadConfig(function() { PosNav.window.openSale() })
                           }, Api.newIdempotencyKey()) }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
