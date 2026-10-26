import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Products & services")
    property string openId: ""
    property string search: ""
    property bool lowOnly: false
    property var rows: []
    property int total: 0
    property bool loading: false
    function reload() {
        loading = true
        var q = "/api/v1/products?limit=200" + (search ? "&search=" + encodeURIComponent(search) : "") + (lowOnly ? "&low_stock=1" : "")
        Api.get(q, function(r) {
            loading = false
            if (!r.ok) { Nav.fail(r); return }
            rows = r.data.items; total = r.data.total
            if (page.openId) { rows.forEach(function(p) { if (p.id === page.openId) dialog.openEdit(p) }); page.openId = "" }
        })
    }
    Component.onCompleted: reload()
    actions: [
        FbButton { visible: Session.can("catalog.write"); text: qsTr("New"); iconName: "plus"; onClicked: dialog.openNew() },
        FbButton { text: qsTr("Export CSV"); iconName: "download"; variant: "secondary"
                   onClicked: Api.download("/api/v1/products/export.csv", "produits.csv", function(r) { r.ok ? Nav.toast(qsTr("Saved in %1").arg(r.data)) : Nav.fail(r) }) },
        FbButton { visible: Session.can("catalog.write"); text: qsTr("Import CSV"); iconName: "upload"; variant: "secondary"; onClicked: importDialog.open() }
    ]
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        RowLayout {
            Layout.fillWidth: true
            SearchField { Layout.fillWidth: true; Layout.maximumWidth: 420; placeholderText: qsTr("Name, reference or barcode"); onSearch: function(t) { page.search = t; page.reload() } }
            FbSwitch { text: qsTr("Low stock only"); checked: page.lowOnly; onToggled: { page.lowOnly = checked; page.reload() } }
            Item { Layout.fillWidth: true }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.rows
            titleColumn: 1; subtitleColumn: 0; trailingColumn: 3; chipColumn: 4
            columns: [
                { title: qsTr("Reference"), width: 1.1, mono: true, muted: true, value: function(p) { return p.sku } },
                { title: qsTr("Name"), width: 2.6, strong: true, value: function(p) { return p.name } },
                { title: qsTr("Category"), width: 1.2, muted: true, value: function(p) { return p.category_name || "" } },
                { title: qsTr("Sale price"), width: 1.3, align: "right", value: function(p) { return Fmt.money(p.sale_price, p.currency) } },
                { title: qsTr("Stock"), width: 1.1, align: "right",
                  chip: function(p) { return p.track_stock ? { text: Fmt.quantity(p.stock_quantity) + " " + Labels.unit(p.unit), tone: p.low_stock ? "warning" : "neutral" } : { text: qsTr("Service"), tone: "info" } } }
            ]
            emptyIcon: "box"
            emptyTitle: qsTr("No product yet")
            emptyText: qsTr("Add your products and services, or import them from a CSV file (sku;name;sale_price…).")
            emptyActionText: Session.can("catalog.write") ? qsTr("Add a product") : ""
            onEmptyAction: dialog.openNew()
            onRowClicked: function(p) { if (Session.can("catalog.write")) dialog.openEdit(p) }
        }
    }
    ProductDialog { id: dialog; onSaved: page.reload() }
    FileDialog {
        id: importDialog
        title: qsTr("Choose a CSV file")
        nameFilters: [qsTr("CSV files (*.csv)")]
        onAccepted: Api.upload("POST", "/api/v1/products/import", selectedFile, function(r) {
            if (r.ok) { Nav.toast(qsTr("%1 created, %2 updated").arg(r.data.created).arg(r.data.updated), "success"); page.reload() }
            else {
                var lines = r.error && r.error.details && r.error.details.errors ? r.error.details.errors.map(function(e) { return e.line }).slice(0, 8).join(", ") : ""
                Nav.toast(Errors.message(r) + (lines ? " " + qsTr("Lines: %1").arg(lines) : ""), "danger")
            }
        })
    }
}
