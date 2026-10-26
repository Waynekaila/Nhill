import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Sales")
    subtitle: qsTr("Invoices, quotes, orders and credit notes")
    property string type: "invoice"
    property string status: ""
    property string search: ""
    property bool overdue: false
    property var rows: []
    property int total: 0
    property bool loading: false

    function reload() {
        loading = true
        var q = "/api/v1/sales-documents?limit=100&type=" + type
        if (status) q += "&status=" + status
        if (overdue) q += "&overdue=1"
        if (search) q += "&search=" + encodeURIComponent(search)
        Api.get(q, function(r) { loading = false; if (r.ok) { rows = r.data.items; total = r.data.total } else Nav.fail(r) })
    }
    Component.onCompleted: reload()

    actions: [
        FbButton {
            visible: Session.can("sales.write") && page.type !== "credit_note"
            text: page.type === "quote" ? qsTr("New quote") : page.type === "order" ? qsTr("New order") : qsTr("New invoice")
            iconName: "plus"
            onClicked: Nav.push("pages/DocumentEditorPage.qml", { docType: page.type })
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        Flickable {
            Layout.fillWidth: true
            implicitHeight: 38
            contentWidth: tabs.implicitWidth
            clip: true
            TabStrip {
                id: tabs
                model: [ { value: "invoice", text: qsTr("Invoices") }, { value: "quote", text: qsTr("Quotes") },
                         { value: "order", text: qsTr("Orders") }, { value: "credit_note", text: qsTr("Credit notes") } ]
                current: page.type
                onSelected: function(v) { page.type = v; page.status = ""; page.overdue = false; page.reload() }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2
            SearchField { Layout.fillWidth: true; Layout.maximumWidth: 380; placeholderText: qsTr("Number or customer"); onSearch: function(t) { page.search = t; page.reload() } }
            FbComboBox {
                Layout.preferredWidth: 200
                model: page.type === "invoice"
                       ? [ { value: "", text: qsTr("All statuses") }, { value: "draft", text: qsTr("Drafts") }, { value: "issued", text: qsTr("Unpaid") },
                           { value: "partially_paid", text: qsTr("Partly paid") }, { value: "paid", text: qsTr("Paid") }, { value: "overdue", text: qsTr("Overdue") } ]
                       : [ { value: "", text: qsTr("All statuses") }, { value: "draft", text: qsTr("Drafts") } ]
                value: page.overdue ? "overdue" : page.status
                onActivated: { page.overdue = currentValue === "overdue"; page.status = page.overdue ? "" : currentValue; page.reload() }
            }
            Item { Layout.fillWidth: true }
            Text { text: qsTr("%n document(s)", "", page.total); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm; visible: !Theme.compact }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.rows
            titleColumn: 1; subtitleColumn: 0; trailingColumn: 4; chipColumn: 6
            columns: [
                { title: qsTr("Number"), width: 1.3, mono: true, value: function(r) { return r.number || qsTr("Draft") } },
                { title: qsTr("Customer"), width: 2.4, value: function(r) { return r.party_name } },
                { title: qsTr("Date"), width: 1, muted: true, value: function(r) { return Fmt.date(r.issue_date) } },
                { title: qsTr("Due"), width: 1, muted: true, value: function(r) { return r.due_date ? Fmt.date(r.due_date) : "—" } },
                { title: qsTr("Amount"), width: 1.4, align: "right", strong: true, value: function(r) { return Fmt.money(r.total, r.currency) } },
                { title: qsTr("Balance"), width: 1.3, align: "right", value: function(r) { return Fmt.isZero(r.balance) ? "—" : Fmt.money(r.balance, r.currency) } },
                { title: qsTr("Status"), width: 1.3, align: "right", chip: function(r) { return Labels.docStatus(r) } }
            ]
            emptyIcon: "invoice"
            emptyTitle: page.search || page.status ? qsTr("No matching document") : qsTr("No document yet")
            emptyText: qsTr("Create a document, issue it to give it its definitive number, then record the payments.")
            emptyActionText: Session.can("sales.write") && page.type !== "credit_note" ? qsTr("Create") : ""
            onEmptyAction: Nav.push("pages/DocumentEditorPage.qml", { docType: page.type })
            onRowClicked: function(row) { Nav.push("pages/DocumentPage.qml", { docId: row.id }) }
        }
    }
}
