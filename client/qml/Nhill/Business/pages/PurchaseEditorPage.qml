import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    property string purchaseId: ""
    property var purchase: null
    property var suppliers: []
    property var products: []
    property string currency: Session.currency
    property string createKey: Api.newIdempotencyKey()
    property bool busy: false
    readonly property bool editable: !purchase || purchase.status === "draft"
    title: purchase ? qsTr("Purchase") + " · " + purchase.supplier_name : qsTr("New purchase")
    scrollable: true
    ListModel { id: lines }
    function load() {
        Api.get("/api/v1/suppliers?limit=200", function(r) { if (r.ok) page.suppliers = r.data.items })
        Api.get("/api/v1/products?limit=200", function(r) { if (r.ok) page.products = r.data.items })
        if (!purchaseId) { lines.append({ productId: "", description: "", quantity: "1", unitCost: "", taxBp: 0 }); purchaseDate.value = Fmt.today(); return }
        Api.get("/api/v1/purchases/" + purchaseId, function(r) {
            if (!r.ok) { Nav.fail(r); return }
            page.purchase = r.data; page.currency = r.data.currency
            supplierBox.value = r.data.supplier_id; reference.text = r.data.supplier_reference; purchaseDate.value = r.data.purchase_date
            dueDate.value = r.data.due_date || ""; notes.text = r.data.notes
            lines.clear()
            r.data.lines.forEach(function(l) { lines.append({ productId: l.product_id || "", description: l.description, quantity: l.quantity, unitCost: l.unit_cost, taxBp: l.tax_rate_bp }) })
        })
    }
    Component.onCompleted: load()
    function body() {
        var ls = []
        for (var i = 0; i < lines.count; ++i) {
            var l = lines.get(i)
            var o = { description: l.description, quantity: Fmt.parseQuantity(l.quantity), unit_cost: l.unitCost || "0", tax_rate_bp: l.taxBp }
            if (l.productId) o.product_id = l.productId
            ls.push(o)
        }
        return { supplier_id: supplierBox.value, currency: page.currency, supplier_reference: reference.text, purchase_date: purchaseDate.value,
                 due_date: dueDate.value || null, notes: notes.text, lines: ls }
    }
    function save(thenReceive) {
        if (!supplierBox.value) { Nav.toast(qsTr("Choose a supplier."), "danger"); return }
        busy = true
        var after = function(r) {
            if (!r.ok) { busy = false; Nav.fail(r); return }
            page.purchaseId = r.data.id; page.purchase = r.data
            if (!thenReceive) { busy = false; Nav.toast(qsTr("Saved"), "success"); return }
            Api.send("POST", "/api/v1/purchases/" + r.data.id + "/receive", { update_purchase_prices: true }, function(rr) {
                busy = false
                if (rr.ok) { page.purchase = rr.data; Nav.toast(qsTr("Goods received: stock updated"), "success") } else Nav.fail(rr)
            }, Api.newIdempotencyKey())
        }
        if (page.purchase) Api.send("PUT", "/api/v1/purchases/" + page.purchaseId, body(), after)
        else Api.send("POST", "/api/v1/purchases", body(), after, page.createKey)
    }
    actions: [
        FbButton { visible: page.editable; text: qsTr("Save"); variant: "secondary"; busy: page.busy; onClicked: page.save(false) },
        FbButton { visible: page.editable; text: qsTr("Receive the goods"); iconName: "check"; busy: page.busy; onClicked: page.save(true) },
        FbButton { visible: !!page.purchase && page.purchase.status === "received" && !Fmt.isZero(page.purchase.balance) && Session.can("payments.write")
                   text: qsTr("Record payment"); iconName: "payment"
                   onClicked: pay.start("purchase_id", page.purchaseId, page.currency, page.purchase.balance) }
    ]
    Banner { visible: !!page.purchase && page.purchase.status === "received"; tone: "success"
             text: qsTr("Received on %1. A received purchase can no longer be modified.").arg(page.purchase ? Fmt.dateTime(page.purchase.received_at) : "") }
    FbCard {
        Layout.fillWidth: true
        GridLayout {
            width: parent.width
            columns: Theme.compact ? 1 : 3
            columnSpacing: Theme.s3; rowSpacing: Theme.s3
            Field { label: qsTr("Supplier"); required: true; Layout.columnSpan: Theme.compact ? 1 : 2
                    FbComboBox { id: supplierBox; enabled: page.editable && !page.purchase; model: page.suppliers.map(function(s) { return { value: s.id, text: s.name } })
                                 displayText: currentIndex < 0 ? qsTr("Choose a supplier") : currentText } }
            Field { label: qsTr("Currency"); FbComboBox { enabled: !page.purchase; model: Labels.currencies(); value: page.currency; onActivated: page.currency = currentValue } }
            Field { label: qsTr("Supplier reference"); FbTextField { id: reference; enabled: page.editable; monospace: true } }
            Field { label: qsTr("Date"); DateField { id: purchaseDate; enabled: page.editable } }
            Field { label: qsTr("Due date"); DateField { id: dueDate; enabled: page.editable } }
        }
    }
    SectionTitle { text: qsTr("Lines") }
    Repeater {
        model: lines
        delegate: FbCard {
            Layout.fillWidth: true
            GridLayout {
                width: parent.width
                columns: Theme.compact ? 2 : 6
                columnSpacing: Theme.s2; rowSpacing: Theme.s2
                Field { label: qsTr("Product"); Layout.columnSpan: 2
                        FbComboBox { enabled: page.editable; model: [{ value: "", text: qsTr("— Free line (no stock) —") }].concat(page.products.map(function(p) { return { value: p.id, text: p.sku + " · " + p.name } }))
                                     value: model.productId
                                     onActivated: { lines.setProperty(index, "productId", currentValue); page.products.forEach(function(p) { if (p.id === currentValue) lines.setProperty(index, "description", p.name) }) } } }
                Field { label: qsTr("Description"); Layout.columnSpan: Theme.compact ? 2 : 1; FbTextField { enabled: page.editable; text: model.description; onTextEdited: lines.setProperty(index, "description", text) } }
                Field { label: qsTr("Quantity"); FbTextField { enabled: page.editable; text: Fmt.quantity(model.quantity); horizontalAlignment: TextInput.AlignRight
                                                               onTextEdited: lines.setProperty(index, "quantity", Fmt.parseQuantity(text) || text) } }
                Field { label: qsTr("Unit cost"); MoneyField { enabled: page.editable; currency: page.currency; Component.onCompleted: setValue(model.unitCost); onValueChanged: lines.setProperty(index, "unitCost", value) } }
                Field { label: qsTr("Tax %"); FbTextField { enabled: page.editable; text: model.taxBp ? Fmt.percentText(model.taxBp) : ""; horizontalAlignment: TextInput.AlignRight
                                                            onTextEdited: { var bp = Fmt.parsePercent(text); if (bp >= 0) lines.setProperty(index, "taxBp", bp) } } }
                FbButton { visible: page.editable; text: qsTr("Remove"); iconName: "trash"; variant: "ghost"; onClicked: lines.remove(index) }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        FbButton { visible: page.editable; text: qsTr("Add a line"); iconName: "plus"; variant: "secondary"; onClicked: lines.append({ productId: "", description: "", quantity: "1", unitCost: "", taxBp: 0 }) }
        Item { Layout.fillWidth: true }
        Text { visible: !!page.purchase; text: page.purchase ? qsTr("Total: %1 · To pay: %2").arg(Fmt.money(page.purchase.total, page.currency)).arg(Fmt.money(page.purchase.balance, page.currency)) : ""
               color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold }
    }
    Field { label: qsTr("Notes"); FbTextArea { id: notes } }
    PaymentDialog { id: pay; onSaved: page.load() }
}
