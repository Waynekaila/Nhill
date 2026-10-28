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
        case "plan_changed": return p.plan ? qsTr("Your plan has changed (%1). The screens show the features of the new plan.").arg(p.plan)
                                          : qsTr("Your plan has changed. The screens show the features of the new plan.")
        case "plan_change_scheduled": return qsTr("A change of plan is scheduled at the end of the current period.")
        case "subscription_admin_extend": return qsTr("Nhill extended your subscription.")
        case "subscription_admin_suspend": return qsTr("Nhill suspended your subscription: read-only access.") + (p.reason ? " " + p.reason : "")
        case "subscription_admin_reactivate": return qsTr("Nhill reactivated your subscription.")
        case "subscription_admin_cancel": return qsTr("Your subscription was cancelled.")
        case "subscription_admin_set_plan":
        case "subscription_admin_assign_plan": return p.plan ? qsTr("Nhill assigned the plan %1 to your business.").arg(p.plan) : qsTr("Nhill changed the plan of your business.")
        case "subscription_admin_set_dates": return qsTr("Nhill corrected the dates of your subscription.")
        case "pos_cash_variance": return qsTr("Till %1, session %2: cash difference of %3.").arg(p.register || "").arg(p.number || "").arg(p.variance ? Fmt.money(p.variance, p.currency || Session.currency) : "")
        case "ticket_replied": return qsTr("Nhill support answered your request #%1.").arg(p.number || "")
        case "ticket_status": return qsTr("Your request #%1 has a new status.").arg(p.number || "")
        }
        // A notification this version does not know is described generically, never by its code.
        return qsTr("New information about your business.")
    }
    Popup {
        id: pop
        y: root.height + 6
        margins: Theme.s2
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
