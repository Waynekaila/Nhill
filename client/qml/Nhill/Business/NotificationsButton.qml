import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

FbIconButton {
    id: root
    iconName: "bell"
    tip: qsTr("Notifications")
    property var items: []
    readonly property int unread: items.filter(function(n) { return !n.read }).length
    function load() { Api.get("/api/v1/notifications", function(r) { if (r.ok) root.items = r.data.items }) }
    Component.onCompleted: load()
    Timer { interval: 120000; running: true; repeat: true; onTriggered: root.load() }
    onClicked: { load(); pop.open() }
    Rectangle {
        visible: root.unread > 0
        width: 16; height: 16; radius: 8; color: Theme.accent
        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 4
        Text { anchors.centerIn: parent; text: root.unread > 9 ? "9+" : root.unread; color: "#FFFFFF"; font.pixelSize: 9; font.weight: Font.Bold }
    }
    function describe(n) {
        var p = n.params || {}
        switch (n.kind) {
        case "code_rotated": return qsTr("A new establishment code is available (Settings › Establishment code).")
        case "subscription_reminder": return p.is_trial ? qsTr("Your free trial ends on %1.").arg(Fmt.date(p.period_end)) : qsTr("Your subscription ends on %1.").arg(Fmt.date(p.period_end))
        case "subscription_grace": return qsTr("Payment not received. Grace period until %1.").arg(Fmt.date(p.grace_until))
        case "subscription_suspended": return qsTr("Subscription suspended: read-only access.")
        case "subscription_activated": return qsTr("Subscription active until %1. Thank you!").arg(Fmt.date(p.period_end))
        case "payment_failed": return qsTr("A subscription payment failed.")
        case "member_joined": return qsTr("%1 joined the business.").arg(p.email || "")
        case "attachment_request": return qsTr("%1 (%2) asks to join the business.").arg(p.name || "").arg(p.email || "")
        case "establishment_suspended": return qsTr("The business was suspended by support: %1").arg(p.reason || "")
        case "establishment_reactivated": return qsTr("The business was reactivated.")
        }
        return n.kind
    }
    Popup {
        id: pop
        y: root.height + 6
        x: Math.min(0, root.parent ? -(380 - root.width) : 0)
        width: Math.min(380, ApplicationWindow.window ? ApplicationWindow.window.width - 16 : 380)
        height: Math.min(460, list.contentHeight + 60)
        padding: 0
        background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.line }
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Text { text: qsTr("Notifications"); Layout.margins: Theme.s3; font.family: Theme.font; font.weight: Font.DemiBold; color: Theme.text }
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.items
                delegate: ItemDelegate {
                    width: list.width
                    onClicked: Api.send("POST", "/api/v1/notifications/" + modelData.id + "/read", {}, function() { root.load() })
                    contentItem: RowLayout {
                        spacing: Theme.s2
                        Rectangle { width: 8; height: 8; radius: 4; color: modelData.read ? "transparent" : Theme.accent; Layout.alignment: Qt.AlignTop; Layout.topMargin: 6 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: root.describe(modelData); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                            Text { text: Fmt.dateTime(modelData.created_at); color: Theme.textFaint; font.family: Theme.font; font.pixelSize: Theme.fs_xs }
                        }
                    }
                    background: Rectangle { color: hovered ? Theme.surfaceAlt : "transparent" }
                }
                Text { anchors.centerIn: parent; visible: root.items.length === 0; text: qsTr("No notification"); color: Theme.textFaint; font.family: Theme.font }
            }
        }
    }
}
