import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Stock")
    subtitle: qsTr("Quantities change only through recorded movements")
    property string tab: "levels"
    property string search: ""
    property bool lowOnly: false
    property var levels: []
    property var movements: []
    property var warehouses: []
    property bool multi: false
    property bool loading: false
    function reload() {
        loading = true
        Api.get("/api/v1/warehouses", function(r) { if (r.ok) { page.warehouses = r.data.items; page.multi = r.data.multi_warehouse } })
        if (tab === "levels")
            Api.get("/api/v1/stock?limit=200" + (search ? "&search=" + encodeURIComponent(search) : "") + (lowOnly ? "&low_stock=1" : ""),
                    function(r) { loading = false; if (r.ok) page.levels = r.data.items; else Nav.fail(r) })
        else
            Api.get("/api/v1/stock/movements?limit=200", function(r) { loading = false; if (r.ok) page.movements = r.data.items; else Nav.fail(r) })
    }
    Component.onCompleted: reload()
    actions: [
        FbButton { visible: Session.can("stock.adjust"); text: qsTr("Adjust stock"); iconName: "edit"; onClicked: adjust.start(null) },
        FbButton { visible: Session.can("stock.adjust") && page.multi && page.warehouses.length > 1; text: qsTr("Transfer"); iconName: "swap"; variant: "secondary"; onClicked: transfer.open() }
    ]
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        TabStrip { model: [{ value: "levels", text: qsTr("Levels") }, { value: "movements", text: qsTr("Movements") }]
                   current: page.tab; onSelected: function(v) { page.tab = v; page.reload() } }
        RowLayout {
            visible: page.tab === "levels"
            Layout.fillWidth: true
            SearchField { Layout.fillWidth: true; Layout.maximumWidth: 420; onSearch: function(t) { page.search = t; page.reload() } }
            FbSwitch { text: qsTr("Below threshold only"); checked: page.lowOnly; onToggled: { page.lowOnly = checked; page.reload() } }
            Item { Layout.fillWidth: true }
        }
        DataTable {
            visible: page.tab === "levels"
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.levels
            titleColumn: 1; subtitleColumn: 0; trailingColumn: 2; chipColumn: 2
            columns: [
                { title: qsTr("Reference"), width: 1, mono: true, muted: true, value: function(s) { return s.sku } },
                { title: qsTr("Product"), width: 2.6, strong: true, value: function(s) { return s.name } },
                { title: qsTr("Quantity"), width: 1.2, align: "right",
                  chip: function(s) { return { text: Fmt.quantity(s.quantity) + " " + Labels.unit(s.unit), tone: s.low_stock ? "warning" : "neutral" } } },
                { title: qsTr("Threshold"), width: 1, align: "right", muted: true, value: function(s) { return Fmt.quantity(s.reorder_level) } },
                { title: qsTr("Value (est.)"), width: 1.4, align: "right", value: function(s) { return Fmt.money(s.estimated_value, s.currency) } }
            ]
            emptyIcon: "stock"
            emptyTitle: qsTr("No stocked product")
            emptyText: qsTr("Products of type \"product\" appear here. Record the opening stock with \"Adjust stock\".")
            onRowClicked: function(s) { if (Session.can("stock.adjust")) adjust.start(s) }
        }
        DataTable {
            visible: page.tab === "movements"
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.movements
            titleColumn: 1; subtitleColumn: 0; trailingColumn: 3
            columns: [
                { title: qsTr("Date"), width: 1.2, muted: true, value: function(m) { return Fmt.dateTime(m.created_at) } },
                { title: qsTr("Product"), width: 2.2, value: function(m) { return m.product_name } },
                { title: qsTr("Movement"), width: 1.3, value: function(m) { return Labels.movement(m.kind) + (m.note ? " · " + m.note : "") } },
                { title: qsTr("Quantity"), width: 1, align: "right", strong: true, value: function(m) { return (Fmt.isNegative(m.quantity) ? "" : "+") + Fmt.quantity(m.quantity) } },
                { title: qsTr("By"), width: 1.2, muted: true, value: function(m) { return m.created_by } }
            ]
            emptyIcon: "stock"
            emptyTitle: qsTr("No movement yet")
        }
    }
    FbDialog {
        id: adjust
        property var product: null
        property var products: []
        property string key: ""
        title: qsTr("Adjust stock")
        subtitle: qsTr("Enter a positive quantity to add, negative to remove. Every adjustment is recorded with its reason.")
        preferredWidth: 480
        function start(p) {
            product = p; key = Api.newIdempotencyKey(); qty.text = ""; reason.text = ""; kindBox.value = "adjustment"
            Api.get("/api/v1/products?limit=200", function(r) { if (r.ok) adjust.products = r.data.items.filter(function(x) { return x.track_stock }) })
            open()
        }
        Field { label: qsTr("Product"); FbComboBox { id: productBox; model: adjust.products.map(function(p) { return { value: p.id, text: p.sku + " · " + p.name } })
                                                     value: adjust.product ? adjust.product.product_id : "" } }
        Field { label: qsTr("Nature"); FbComboBox { id: kindBox; model: [{ value: "adjustment", text: qsTr("Adjustment (count, breakage, loss)") }, { value: "opening", text: qsTr("Opening stock") }]; value: "adjustment" } }
        Field { label: qsTr("Quantity (+/−)"); FbTextField { id: qty; horizontalAlignment: TextInput.AlignRight; inputMethodHints: Qt.ImhFormattedNumbersOnly } }
        Field { label: qsTr("Warehouse"); visible: page.warehouses.length > 1
                FbComboBox { id: whBox; model: page.warehouses.map(function(w) { return { value: w.id, text: w.name } }); value: page.warehouses.length ? page.warehouses[0].id : "" } }
        Field { label: qsTr("Reason"); required: true; FbTextField { id: reason; placeholderText: qsTr("e.g. Inventory count of 25/10") } }
        footer: [
            FbButton { text: qsTr("Record"); onClicked: {
                    if (!productBox.value || !reason.text.trim() || !qty.text.trim()) { Nav.toast(qsTr("Product, quantity and reason are required."), "danger"); return }
                    var b = { product_id: productBox.value, quantity: qty.text.replace(",", ".").replace(/\s/g, ""), reason: reason.text, kind: kindBox.value }
                    if (whBox.value) b.warehouse_id = whBox.value
                    Api.send("POST", "/api/v1/stock/adjustments", b, function(r) {
                        if (r.ok) { adjust.close(); Nav.toast(qsTr("Stock updated: %1").arg(Fmt.quantity(r.data.quantity_after)), "success"); page.reload() } else Nav.fail(r)
                    }, adjust.key) } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: adjust.close() }
        ]
    }
    FbDialog {
        id: transfer
        title: qsTr("Transfer between warehouses")
        property string key: Api.newIdempotencyKey()
        Field { label: qsTr("Product"); FbComboBox { id: tProduct; model: page.levels.map(function(s) { return { value: s.product_id, text: s.sku + " · " + s.name } }) } }
        Field { label: qsTr("From"); FbComboBox { id: tFrom; model: page.warehouses.map(function(w) { return { value: w.id, text: w.name } }) } }
        Field { label: qsTr("To"); FbComboBox { id: tTo; model: page.warehouses.map(function(w) { return { value: w.id, text: w.name } }) } }
        Field { label: qsTr("Quantity"); FbTextField { id: tQty; horizontalAlignment: TextInput.AlignRight } }
        footer: [
            FbButton { text: qsTr("Transfer"); onClicked: Api.send("POST", "/api/v1/stock/transfers",
                        { product_id: tProduct.value, from_warehouse_id: tFrom.value, to_warehouse_id: tTo.value, quantity: Fmt.parseQuantity(tQty.text) },
                        function(r) { if (r.ok) { transfer.close(); transfer.key = Api.newIdempotencyKey(); page.reload() } else Nav.fail(r) }, transfer.key) },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: transfer.close() }
        ]
    }
}
