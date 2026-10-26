import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

FbPage {
    id: page
    title: qsTr("Provider events")
    subtitle: qsTr("Signed notifications received from payment providers (duplicates are ignored)")
    property var rows: []
    Component.onCompleted: Api.get("/api/v1/admin/webhook-events?limit=200", function(r) { if (r.ok) page.rows = r.data.items; else ConsoleNav.fail(r) })
    DataTable {
        anchors.fill: parent
        rows: page.rows
        columns: [
            { title: qsTr("Received"), width: 1.2, muted: true, value: function(e) { return Fmt.dateTime(e.received_at) } },
            { title: qsTr("Provider"), width: 1, strong: true, value: function(e) { return e.provider } },
            { title: qsTr("Event"), width: 2, mono: true, value: function(e) { return e.event_id } },
            { title: qsTr("Processed"), width: 1.2, muted: true, value: function(e) { return e.processed_at ? Fmt.dateTime(e.processed_at) : "—" } },
            { title: qsTr("Outcome"), width: 1.4, value: function(e) { return e.outcome } }
        ]
        emptyIcon: "swap"
        emptyTitle: qsTr("No event received")
        emptyText: qsTr("No payment provider is connected yet: payments are confirmed manually in « Payments ».")
    }
}
