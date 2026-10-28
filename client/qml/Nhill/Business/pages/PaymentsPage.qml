import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Payments")
    subtitle: qsTr("Money received and paid out, as recorded in Nhill")
    property string direction: ""
    property var rows: []
    property var accounts: []
    property bool loading: false
    function reload() {
        loading = true
        Api.get("/api/v1/cash-accounts", function(r) { if (r.ok) page.accounts = r.data.items })
        Api.get("/api/v1/payments?limit=200" + (direction ? "&direction=" + direction : ""), function(r) { loading = false; if (r.ok) page.rows = r.data.items; else Nav.fail(r) })
    }
    Component.onCompleted: reload()
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        Flow {
            Layout.fillWidth: true
            spacing: Theme.s3
            visible: page.accounts.length > 0
            Repeater {
                model: page.accounts
                KpiTile { width: Theme.compact ? parent.width : 240; label: modelData.name; value: Fmt.money(modelData.recorded_balance, modelData.currency)
                          caption: qsTr("Recorded balance (not a bank statement)"); iconName: "wallet" }
            }
        }
        TabStrip { model: [{ value: "", text: qsTr("All") }, { value: "in", text: qsTr("Received") }, { value: "out", text: qsTr("Paid out") }]
                   current: page.direction; onSelected: function(v) { page.direction = v; page.reload() } }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.rows
            titleColumn: 1; subtitleColumn: 0; trailingColumn: 5
            columns: [
                { title: qsTr("Date"), width: 0.9, minWidth: 104, muted: true, value: function(p) { return Fmt.date(p.paid_at) } },
                { title: qsTr("Party"), width: 2, strong: true, value: function(p) { return p.party_name || "—" } },
                { title: qsTr("Document"), width: 1.3, minWidth: 150, mono: true, value: function(p) { return p.document_number || (p.purchase_id ? qsTr("Purchase") : p.expense_id ? qsTr("Expense") : "—") } },
                { title: qsTr("Method"), width: 1.1, muted: true, value: function(p) { return Labels.method(p.method) + (p.reference ? " · " + p.reference : "") } },
                { title: qsTr("Account"), width: 1, muted: true, value: function(p) { return p.account || "—" } },
                { title: qsTr("Amount"), width: 1.3, minWidth: 140, align: "right", strong: true, value: function(p) { return (p.direction === "in" ? "+ " : "− ") + Fmt.money(p.amount, p.currency) } }
            ]
            emptyIcon: "payment"
            emptyTitle: qsTr("No payment yet")
            emptyText: qsTr("Payments are recorded from your documents (invoices, purchases, expenses) when your plan includes them.")
            onRowClicked: function(p) { if (p.document_id) Nav.push("pages/DocumentPage.qml", { docId: p.document_id }); else if (p.purchase_id) Nav.push("pages/PurchaseEditorPage.qml", { purchaseId: p.purchase_id }) }
        }
    }
}
