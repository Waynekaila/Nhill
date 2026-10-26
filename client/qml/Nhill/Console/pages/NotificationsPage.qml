import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Console notifications: tickets, payments, technical errors.
FbPage {
    id: page
    title: qsTr("Notifications")
    property var rows: []
    property int unread: 0
    function reload() { Api.get("/api/v1/admin/notifications", function(r) { if (r.ok) { page.rows = r.data.items; page.unread = r.data.unread } else ConsoleNav.fail(r) }) }
    function text(n) {
        var p = n.params || {}
        switch (n.kind) {
        case "ticket_opened": return qsTr("New ticket #%1 (%2)").arg(p.number).arg(Labels.severity(p.severity).text)
        case "ticket_reply": return qsTr("Customer reply on ticket #%1").arg(p.number)
        case "order_paid": return qsTr("Payment received (%1, %2)").arg(p.plan).arg(Labels.orderKind(p.kind))
        case "payment_failed": return qsTr("Payment failed")
        case "payment_amount_mismatch": return qsTr("Payment amount mismatch (%1) — to check").arg(p.provider)
        case "order_refunded": return qsTr("Refund reported by the provider — manual review")
        case "new_error": return qsTr("New technical error: %1 · %2").arg(p.source).arg(p.code)
        }
        return n.kind
    }
    function open(n) {
        var p = n.params || {}
        if (p.ticket_id) ConsoleNav.push("pages/TicketPage.qml", { ticketId: p.ticket_id })
        else if (n.kind === "new_error") ConsoleNav.shell.navigate("errors")
        else if (p.order_id) ConsoleNav.shell.navigate("orders")
        else if (n.establishment_id) ConsoleNav.push("pages/EstablishmentPage.qml", { establishmentId: n.establishment_id })
    }
    Component.onCompleted: reload()
    actions: [ FbButton { enabled: page.unread > 0; text: qsTr("Mark all as read"); iconName: "check"; variant: "secondary"
                          onClicked: Api.send("POST", "/api/v1/admin/notifications/read", {}, function(r) { if (r.ok) { page.reload(); ConsoleNav.shell.refreshBadges() } }) } ]
    DataTable {
        anchors.fill: parent
        rows: page.rows
        columns: [
            { title: qsTr("Date"), width: 1.1, muted: true, value: function(n) { return Fmt.dateTime(n.created_at) } },
            { title: qsTr("Event"), width: 2.6, strong: true, value: function(n) { return page.text(n) } },
            { title: qsTr("Establishment"), width: 1.6, value: function(n) { return n.establishment || "—" } },
            { title: "", width: 0.6, chip: function(n) { return n.read ? { text: qsTr("Read"), tone: "neutral" } : { text: qsTr("New"), tone: "accent" } } }
        ]
        emptyIcon: "bell"
        emptyTitle: qsTr("No notification")
        onRowClicked: function(n) { page.open(n) }
    }
}
