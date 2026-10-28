import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbDialog {
    id: dlg
    property var product: null
    property var categories: []
    property var taxes: []
    property bool saving: false
    signal saved()
    title: product ? qsTr("Edit product") : qsTr("New product or service")
    function load() {
        Api.get("/api/v1/product-categories", function(r) { if (r.ok) dlg.categories = r.data.items })
        Api.get("/api/v1/tax-rates", function(r) { if (r.ok) dlg.taxes = r.data.items.filter(function(t) { return t.active }) })
    }
    function openNew() {
        product = null; load()
        kind.value = "good"; sku.text = ""; name.text = ""; description.text = ""; barcode.text = ""; unit.value = "unit"
        currency.value = Session.currency; sale.setValue(""); purchase.setValue(""); reorder.text = ""; category.value = ""; tax.value = ""
        open()
    }
    function openEdit(p) {
        product = p; load()
        kind.value = p.kind; sku.text = p.sku; name.text = p.name; description.text = p.description; barcode.text = p.barcode || ""
        unit.value = p.unit; currency.value = p.currency; sale.setValue(p.sale_price); purchase.setValue(p.purchase_price)
        reorder.text = Fmt.quantity(p.reorder_level); category.value = p.category_id || ""; tax.value = p.tax_rate_id || ""
        open()
    }
    function save() {
        if (!sku.text.trim()) { skuField.error = qsTr("The reference is required."); return }
        if (!name.text.trim()) { nameField.error = qsTr("The name is required."); return }
        if (!sale.valid || !purchase.valid) return
        var b = { sku: sku.text.trim(), name: name.text.trim(), description: description.text, unit: unit.value,
                  sale_price: sale.value || "0", purchase_price: purchase.value || "0" }
        if (!product) { b.kind = kind.value; b.currency = currency.value }
        if (barcode.text) b.barcode = barcode.text
        if (category.value) b.category_id = category.value
        if (tax.value) b.tax_rate_id = tax.value
        if (kind.value === "good") b.reorder_level = Fmt.parseQuantity(reorder.text) || "0"
        saving = true
        var cb = function(r) { saving = false; if (r.ok) { dlg.close(); Nav.toast(qsTr("Saved"), "success"); dlg.saved() } else Nav.fail(r) }
        product ? Api.send("PATCH", "/api/v1/products/" + product.id, b, cb) : Api.send("POST", "/api/v1/products", b, cb)
    }
    GridLayout {
        Layout.fillWidth: true
        columns: Theme.compact ? 1 : 2
        columnSpacing: Theme.s3; rowSpacing: Theme.s3
        Field { label: qsTr("Type"); FbComboBox { id: kind; enabled: !dlg.product; model: [{ value: "good", text: qsTr("Product (stock tracked)") }, { value: "service", text: qsTr("Service") }]; value: "good" } }
        Field { id: skuField; label: qsTr("Reference (SKU)"); required: true; FbTextField { id: sku; monospace: true; onTextChanged: skuField.error = "" } }
        Field { id: nameField; label: qsTr("Name"); required: true; Layout.columnSpan: Theme.compact ? 1 : 2; FbTextField { id: name; onTextChanged: nameField.error = "" } }
        Field { label: qsTr("Category")
                FbComboBox { id: category; model: [{ value: "", text: qsTr("— None —") }].concat(dlg.categories.map(function(c) { return { value: c.id, text: c.name } })); value: "" } }
        Field { label: qsTr("Unit"); FbComboBox { id: unit; model: Labels.units(); value: "unit" } }
        Field { label: qsTr("Currency of prices"); FbComboBox { id: currency; enabled: !dlg.product; model: Labels.currencyChoices(dlg.product ? dlg.product.currency : ""); value: Session.currency } }
        Field { label: qsTr("Tax"); FbComboBox { id: tax; model: [{ value: "", text: qsTr("— Default rate —") }].concat(dlg.taxes.map(function(t) { return { value: t.id, text: t.label } })); value: "" } }
        Field { label: qsTr("Sale price excl. tax"); MoneyField { id: sale; currency: currency.value || Session.currency } }
        Field { label: qsTr("Purchase price"); help: qsTr("Used for estimated margins and stock value."); MoneyField { id: purchase; currency: currency.value || Session.currency } }
        Field { visible: kind.value === "good"; label: qsTr("Alert threshold"); help: qsTr("Alert when the stock falls to this level.")
                FbTextField { id: reorder; horizontalAlignment: TextInput.AlignRight; inputMethodHints: Qt.ImhFormattedNumbersOnly } }
        Field { label: qsTr("Barcode"); FbTextField { id: barcode; monospace: true } }
    }
    Field { label: qsTr("Description"); FbTextArea { id: description } }
    footer: [
        FbButton { text: qsTr("Save"); busy: dlg.saving; onClicked: dlg.save() },
        FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: dlg.close() }
    ]
}
