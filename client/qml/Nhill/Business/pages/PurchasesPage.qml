import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Purchases")
    subtitle: qsTr("Supplier invoices and goods received")
    property var rows: []
    property bool loading: false
    function reload() { loading = true; Api.get("/api/v1/purchases?limit=200", function(r) { loading = false; if (r.ok) rows = r.data.items; else Nav.fail(r) }) }
    Component.onCompleted: reload()
    actions: [ FbButton { visible: Session.can("purchases.write"); text: qsTr("New purchase"); iconName: "plus"; onClicked: Nav.push("pages/PurchaseEditorPage.qml") } ]
    DataTable {
        anchors.fill: parent
        loading: page.loading
        rows: page.rows
        titleColumn: 0; subtitleColumn: 1; trailingColumn: 3; chipColumn: 5
        columns: [
            { title: qsTr("Supplier"), width: 2.2, strong: true, value: function(p) { return p.supplier_name } },
            { title: qsTr("Reference"), width: 1.2, mono: true, muted: true, value: function(p) { return (p.supplier_reference || "—") + " · " + Fmt.date(p.purchase_date) } },
            { title: qsTr("Date"), width: 1, muted: true, value: function(p) { return Fmt.date(p.purchase_date) } },
            { title: qsTr("Amount"), width: 1.3, align: "right", value: function(p) { return Fmt.money(p.total, p.currency) } },
            { title: qsTr("To pay"), width: 1.3, align: "right", value: function(p) { return Fmt.isZero(p.balance) ? "—" : Fmt.money(p.balance, p.currency) } },
            { title: qsTr("Status"), width: 1.1, align: "right", chip: function(p) { return p.status === "received" ? { text: Fmt.isZero(p.balance) ? qsTr("Paid") : qsTr("Received"), tone: Fmt.isZero(p.balance) ? "success" : "warning" } : p.status === "draft" ? { text: qsTr("Draft"), tone: "info" } : { text: qsTr("Canceled"), tone: "neutral" } } }
        ]
        emptyIcon: "cart"
        emptyTitle: qsTr("No purchase yet")
        emptyText: qsTr("Record your purchases: when the goods are received, stock and purchase prices are updated.")
        emptyActionText: Session.can("purchases.write") ? qsTr("New purchase") : ""
        onEmptyAction: Nav.push("pages/PurchaseEditorPage.qml")
        onRowClicked: function(p) { Nav.push("pages/PurchaseEditorPage.qml", { purchaseId: p.id }) }
    }
}
