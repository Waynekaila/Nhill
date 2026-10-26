import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Bug reports and questions from Nhill Business and Nhill POS.
FbPage {
    id: page
    title: qsTr("Support tickets")
    subtitle: counts ? qsTr("%1 open · %2 in progress · %3 waiting for the customer").arg(counts.open).arg(counts.in_progress).arg(counts.waiting_customer) : ""
    property var rows: []
    property var counts: null
    property string filter: "active"
    function reload() {
        var q = filter === "active" ? "?open=1" : filter ? "?status=" + filter : ""
        Api.get("/api/v1/admin/tickets" + q + (q ? "&" : "?") + "limit=200", function(r) {
            if (r.ok) { page.rows = r.data.items; page.counts = r.data.counts } else ConsoleNav.fail(r)
        })
    }
    Component.onCompleted: reload()
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        TabStrip { model: [{ value: "active", text: qsTr("To handle") }, { value: "waiting_customer", text: qsTr("Waiting for customer") },
                           { value: "resolved", text: qsTr("Resolved") }, { value: "", text: qsTr("All") }]
                   current: page.filter; onSelected: function(v) { page.filter = v; page.reload() } }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            rows: page.rows
            columns: [
                { title: "#", width: 0.5, mono: true, value: function(t) { return String(t.number) } },
                { title: qsTr("Subject"), width: 2.2, strong: true, value: function(t) { return t.subject } },
                { title: qsTr("Establishment"), width: 1.5, value: function(t) { return t.establishment } },
                { title: qsTr("Category"), width: 0.9, muted: true, value: function(t) { return Labels.ticketCategory(t.category) + " · " + t.source } },
                { title: qsTr("Severity"), width: 0.9, chip: function(t) { return Labels.severity(t.severity) } },
                { title: qsTr("Last message"), width: 1.1, muted: true, value: function(t) { return Fmt.dateTime(t.last_message_at) } },
                { title: qsTr("Assigned"), width: 1, muted: true, value: function(t) { return t.assigned_name || "—" } },
                { title: qsTr("Status"), width: 1, chip: function(t) { return Labels.ticketStatus(t.status) } }
            ]
            emptyIcon: "check"
            emptyTitle: qsTr("No ticket")
            onRowClicked: function(t) { ConsoleNav.push("pages/TicketPage.qml", { ticketId: t.id }) }
        }
    }
}
