import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: Session.has("purchases") || Session.has("expenses") ? qsTr("Customers & suppliers") : qsTr("Customers")
    property string role: "customer"
    property string search: ""
    property var rows: []
    property int total: 0
    property bool loading: false
    function reload() {
        loading = true
        Api.get("/api/v1/" + (role === "customer" ? "customers" : "suppliers") + "?limit=200" + (search ? "&search=" + encodeURIComponent(search) : ""),
                function(r) { loading = false; if (r.ok) { rows = r.data.items; total = r.data.total } else Nav.fail(r) })
    }
    function balanceText(p) {
        var keys = Object.keys(p.balance || {})
        if (keys.length === 0) return "—"
        return keys.map(function(c) { return Fmt.money(p.balance[c], c) }).join(" · ")
    }
    Component.onCompleted: reload()
    actions: [
        FbButton { visible: Session.can("parties.write") && (page.role === "customer" || Session.can("purchases.write") || Session.can("expenses.write"))
                   text: page.role === "customer" ? qsTr("New customer") : qsTr("New supplier"); iconName: "plus"
                   onClicked: dialog.openNew(page.role === "customer", page.role === "supplier") }
    ]
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        RowLayout {
            Layout.fillWidth: true
            // Suppliers exist when the plan includes purchases or expenses.
            TabStrip { visible: Session.has("purchases") || Session.has("expenses")
                       model: [{ value: "customer", text: qsTr("Customers") }, { value: "supplier", text: qsTr("Suppliers") }]
                       current: page.role; onSelected: function(v) { page.role = v; page.reload() } }
            Item { Layout.fillWidth: true }
        }
        SearchField { Layout.fillWidth: true; Layout.maximumWidth: 420; placeholderText: qsTr("Name, phone or e-mail"); onSearch: function(t) { page.search = t; page.reload() } }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.rows
            titleColumn: 0; subtitleColumn: 1; trailingColumn: 3
            columns: [
                { title: qsTr("Name"), width: 2.4, strong: true, value: function(p) { return p.name } },
                { title: qsTr("Phone"), width: 1.4, muted: true, value: function(p) { return p.phone || p.email || "" } },
                { title: qsTr("City"), width: 1, muted: true, value: function(p) { return p.city } },
                { title: page.role === "customer" ? qsTr("Receivable") : qsTr("Payable"), width: 1.6, align: "right", value: page.balanceText }
            ]
            emptyIcon: "users"
            emptyTitle: page.role === "customer" ? qsTr("No customer yet") : qsTr("No supplier yet")
            emptyText: qsTr("Contacts are used on documents. Balances are computed from issued documents.")
            emptyActionText: Session.can("parties.write") ? qsTr("Add") : ""
            onEmptyAction: dialog.openNew(page.role === "customer", page.role === "supplier")
            onRowClicked: function(p) { Nav.push("pages/PartyPage.qml", { partyId: p.id }) }
        }
    }
    PartyDialog { id: dialog; onSaved: page.reload() }
}
