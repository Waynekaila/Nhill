import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

// Draft editor for quotes, orders and invoices (also credit-note drafts).
FbPage {
    id: page
    property string docType: "invoice"
    property string docId: ""          // edit an existing draft
    property string partyId: ""
    property string currency: Session.currency
    property var customers: []
    property var products: []
    property var taxes: []
    property bool saving: false
    property string createKey: Api.newIdempotencyKey()
    property string issueKey: Api.newIdempotencyKey()
    property bool uncertain: false
    property string lastAction: ""
    title: docId ? qsTr("Edit draft") : docType === "quote" ? qsTr("New quote") : docType === "order" ? qsTr("New order") : qsTr("New invoice")
    scrollable: true

    ListModel { id: lines }

    function defaultTax() { for (var i = 0; i < taxes.length; ++i) if (taxes[i].is_default) return taxes[i].id; return taxes.length ? taxes[0].id : "" }
    function taxBp(id) { for (var i = 0; i < taxes.length; ++i) if (taxes[i].id === id) return taxes[i].rate_bp; return 0 }
    function addLine(p) {
        lines.append({ productId: p ? p.id : "", description: p ? p.name : "", quantity: "1",
                       unitPrice: p && p.currency === page.currency ? p.sale_price : "", discountBp: 0,
                       taxId: p && p.tax_rate_id ? p.tax_rate_id : defaultTax(), unit: p ? p.unit : "unit" })
    }
    function totals() {
        // Preview with the shared C++ rules; the server recomputes and is authoritative.
        var ok = true, nets = [], taxesSum = []
        for (var i = 0; i < lines.count; ++i) {
            var l = lines.get(i)
            var t = Fmt.lineTotals(l.quantity, l.unitPrice, l.discountBp, taxBp(l.taxId), page.currency)
            if (!t.valid) { ok = false; continue }
            nets.push(t.net); taxesSum.push(t.tax)
        }
        return { ok: ok, net: Fmt.sum(nets, page.currency), tax: Fmt.sum(taxesSum, page.currency) }
    }
    function body() {
        var ls = []
        for (var i = 0; i < lines.count; ++i) {
            var l = lines.get(i)
            var o = { description: l.description, quantity: Fmt.parseQuantity(l.quantity), unit: l.unit, discount_bp: l.discountBp }
            if (l.productId) o.product_id = l.productId
            if (l.unitPrice) o.unit_price = l.unitPrice
            if (l.taxId) o.tax_rate_id = l.taxId
            ls.push(o)
        }
        var b = { doc_type: docType, party_id: partyId, currency: currency, lines: ls, notes: notes.text, terms: terms.text }
        if (issueDate.value) b.issue_date = issueDate.value
        if (dueDate.value) b.due_date = dueDate.value
        if (validUntil.value) b.valid_until = validUntil.value
        return b
    }
    function validate() {
        if (!partyId) { Nav.toast(qsTr("Choose a customer."), "danger"); return false }
        if (lines.count === 0) { Nav.toast(qsTr("Add at least one line."), "danger"); return false }
        for (var i = 0; i < lines.count; ++i) {
            var l = lines.get(i)
            if (!l.description || !Fmt.parseQuantity(l.quantity) || !l.unitPrice) { Nav.toast(qsTr("Line %1 is incomplete (description, quantity and price).").arg(i + 1), "danger"); return false }
        }
        return true
    }
    // Saves the draft; then optionally issues it. Uncertain outcomes are retried
    // with the same idempotency keys, so nothing is ever created twice.
    function save(thenIssue) {
        if (!validate()) return
        saving = true
        lastAction = thenIssue ? "issue" : "save"
        var done = function(r) {
            if (!r.ok) { saving = false; page.uncertain = r.uncertain; Nav.fail(r); return }
            page.uncertain = false
            page.docId = r.data.id
            if (!thenIssue) { saving = false; Nav.toast(qsTr("Draft saved"), "success"); Nav.pop(); Nav.push("pages/DocumentPage.qml", { docId: r.data.id }); return }
            Api.send("POST", "/api/v1/sales-documents/" + r.data.id + "/issue", {}, function(ir) {
                saving = false
                if (!ir.ok) { page.uncertain = ir.uncertain; Nav.fail(ir); return }
                page.uncertain = false
                Nav.toast(qsTr("%1 issued").arg(ir.data.number), "success")
                Nav.pop(); Nav.push("pages/DocumentPage.qml", { docId: ir.data.id })
            }, page.issueKey)
        }
        if (docId) Api.send("PUT", "/api/v1/sales-documents/" + docId, body(), done)
        else Api.send("POST", "/api/v1/sales-documents", body(), done, page.createKey)
    }

    Component.onCompleted: {
        Api.get("/api/v1/customers?limit=200", function(r) { if (r.ok) page.customers = r.data.items })
        Api.get("/api/v1/products?limit=200", function(r) { if (r.ok) page.products = r.data.items })
        Api.get("/api/v1/tax-rates", function(r) {
            if (!r.ok) return
            page.taxes = r.data.items.filter(function(t) { return t.active })
            if (page.docId) {
                Api.get("/api/v1/sales-documents/" + page.docId, function(d) {
                    if (!d.ok) return
                    var doc = d.data
                    page.docType = doc.doc_type; page.partyId = doc.party_id; page.currency = doc.currency
                    customerBox.value = doc.party_id; currencyBox.value = doc.currency
                    issueDate.value = doc.issue_date; dueDate.value = doc.due_date || ""; validUntil.value = doc.valid_until || ""
                    notes.text = doc.notes; terms.text = doc.terms
                    lines.clear()
                    doc.lines.forEach(function(l) {
                        var tid = ""; page.taxes.forEach(function(t) { if (t.code === l.tax_code) tid = t.id })
                        lines.append({ productId: l.product_id || "", description: l.description, quantity: l.quantity, unitPrice: l.unit_price,
                                       discountBp: l.discount_bp, taxId: tid, unit: l.unit })
                    })
                })
            } else {
                issueDate.value = Fmt.today()
                if (page.docType === "quote") validUntil.value = Fmt.addDays(Fmt.today(), 30)
                page.addLine(null)
            }
        })
    }

    actions: [
        FbButton { text: qsTr("Save draft"); variant: "secondary"; busy: page.saving && page.lastAction === "save"; onClicked: page.save(false) },
        FbButton {
            visible: Session.can("sales.issue")
            text: page.docType === "invoice" ? qsTr("Issue invoice") : page.docType === "quote" ? qsTr("Send quote") : qsTr("Confirm")
            iconName: "check"
            busy: page.saving && page.lastAction === "issue"
            onClicked: confirmIssue.open()
        }
    ]

    Banner {
        visible: page.uncertain
        tone: "warning"
        text: qsTr("The connection was lost before the server answered. Retry: the operation will never be recorded twice.")
        actionText: qsTr("Retry")
        onAction: page.save(page.lastAction === "issue")
    }
    Banner {
        visible: Session.establishment.vat_registered && Session.establishment.profile === "CD" && page.docType === "invoice"
        tone: "info"
        text: qsTr("VAT-registered in the DRC: this document is a management document. The standardised invoice must be issued by your DEF; you can record its reference after issuing.")
    }

    FbCard {
        Layout.fillWidth: true
        GridLayout {
            width: parent.width
            columns: Theme.compact ? 1 : 3
            columnSpacing: Theme.s3; rowSpacing: Theme.s3
            Field {
                label: qsTr("Customer"); required: true
                Layout.columnSpan: Theme.compact ? 1 : 2
                RowLayout {
                    spacing: Theme.s2
                    FbComboBox {
                        id: customerBox
                        Layout.fillWidth: true
                        editable: false
                        model: page.customers.map(function(c) { return { value: c.id, text: c.name } })
                        displayText: currentIndex < 0 ? qsTr("Choose a customer") : currentText
                        onActivated: page.partyId = currentValue
                    }
                    FbIconButton { iconName: "plus"; tip: qsTr("New customer"); visible: Session.can("parties.write"); onClicked: partyDialog.openNew(true, false) }
                }
            }
            Field {
                label: qsTr("Currency")
                Layout.minimumWidth: Theme.compact ? 0 : 230
                FbComboBox { id: currencyBox; model: Labels.currencyChoices(page.currency); value: page.currency; enabled: !page.docId
                             onActivated: page.currency = currentValue }
            }
            Field { label: qsTr("Date"); Layout.preferredWidth: 220; DateField { id: issueDate } }
            Field { visible: page.docType === "invoice"; Layout.preferredWidth: 220; label: qsTr("Due date"); help: qsTr("Empty = customer's payment terms"); DateField { id: dueDate } }
            Field { visible: page.docType === "quote"; Layout.preferredWidth: 220; label: qsTr("Valid until"); DateField { id: validUntil } }
        }
    }
    Banner {
        visible: page.currency !== Session.currency
        tone: "info"
        text: qsTr("Document in %1: an exchange rate to %2 will be required when issuing (recorded with its date and source, never recalculated).").arg(page.currency).arg(Session.currency)
    }

    SectionTitle { text: qsTr("Lines") }
    Repeater {
        model: lines
        delegate: FbCard {
            id: lineCard
            Layout.fillWidth: true
            property var preview: Fmt.lineTotals(model.quantity, model.unitPrice, model.discountBp, page.taxBp(model.taxId), page.currency)
            GridLayout {
                width: parent.width
                columns: Theme.compact ? 2 : 7
                columnSpacing: Theme.s2; rowSpacing: Theme.s2
                Field {
                    label: qsTr("Product / service")
                    Layout.columnSpan: Theme.compact ? 2 : 2
                    FbComboBox {
                        model: [{ value: "", text: qsTr("— Free line —") }].concat(page.products.map(function(p) { return { value: p.id, text: p.sku + " · " + p.name } }))
                        value: model.productId
                        onActivated: {
                            var p = null; page.products.forEach(function(x) { if (x.id === currentValue) p = x })
                            lines.setProperty(index, "productId", currentValue)
                            if (p) {
                                lines.setProperty(index, "description", p.name)
                                lines.setProperty(index, "unit", p.unit)
                                if (p.currency === page.currency) { lines.setProperty(index, "unitPrice", p.sale_price); priceField.setValue(p.sale_price) }
                                if (p.tax_rate_id) lines.setProperty(index, "taxId", p.tax_rate_id)
                            }
                        }
                    }
                }
                Field {
                    label: qsTr("Description")
                    Layout.columnSpan: Theme.compact ? 2 : 2
                    FbTextField { text: model.description; onTextEdited: lines.setProperty(index, "description", text) }
                }
                Field {
                    label: qsTr("Quantity")
                    FbTextField { text: Fmt.quantity(model.quantity); horizontalAlignment: TextInput.AlignRight; inputMethodHints: Qt.ImhFormattedNumbersOnly
                                  invalid: Fmt.parseQuantity(text).length === 0
                                  onTextEdited: lines.setProperty(index, "quantity", Fmt.parseQuantity(text) || text) }
                }
                Field {
                    label: qsTr("Unit price excl. tax")
                    MoneyField { id: priceField; currency: page.currency; Component.onCompleted: setValue(model.unitPrice)
                                 onValueChanged: lines.setProperty(index, "unitPrice", value) }
                }
                Field {
                    label: qsTr("Discount %")
                    FbTextField { text: model.discountBp ? Fmt.percentText(model.discountBp) : ""; horizontalAlignment: TextInput.AlignRight
                                  inputMethodHints: Qt.ImhFormattedNumbersOnly
                                  invalid: Fmt.parsePercent(text) < 0
                                  onTextEdited: { var bp = Fmt.parsePercent(text); if (bp >= 0) lines.setProperty(index, "discountBp", bp) } }
                }
                Field {
                    label: qsTr("Tax")
                    FbComboBox { model: page.taxes.map(function(t) { return { value: t.id, text: t.label } }); value: model.taxId
                                 onActivated: lines.setProperty(index, "taxId", currentValue) }
                }
                ColumnLayout {
                    Layout.columnSpan: Theme.compact ? 2 : 5
                    Layout.alignment: Qt.AlignRight
                    spacing: 0
                    Text { Layout.alignment: Qt.AlignRight; text: lineCard.preview.valid ? qsTr("Line total excl. tax: %1").arg(Fmt.money(lineCard.preview.net, page.currency)) : qsTr("Incomplete line")
                           color: lineCard.preview.valid ? Theme.text : Theme.textFaint; font.family: Theme.font; font.weight: Font.DemiBold }
                }
                FbButton { text: qsTr("Remove"); iconName: "trash"; variant: "ghost"; Layout.alignment: Qt.AlignRight; onClicked: lines.remove(index) }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        FbButton { text: qsTr("Add a line"); iconName: "plus"; variant: "secondary"; onClicked: page.addLine(null) }
        Item { Layout.fillWidth: true }
        FbCard {
            id: totalsCard
            Layout.preferredWidth: Theme.compact ? 220 : 320
            property int rev: 0
            Connections { target: lines; function onDataChanged() { totalsCard.rev++ } }
            property var t: { lines.count; totalsCard.rev; return page.totals() }
            GridLayout {
                width: parent.width
                columns: 2
                Text { text: qsTr("Total excl. tax"); color: Theme.textMuted; font.family: Theme.font }
                Text { Layout.alignment: Qt.AlignRight; text: Fmt.money(totalsCard.t.net, page.currency); color: Theme.text; font.family: Theme.font }
                Text { text: qsTr("Taxes"); color: Theme.textMuted; font.family: Theme.font }
                Text { Layout.alignment: Qt.AlignRight; text: Fmt.money(totalsCard.t.tax, page.currency); color: Theme.text; font.family: Theme.font }
                Text { text: qsTr("Estimated total"); color: Theme.primary; font.family: Theme.font; font.weight: Font.DemiBold }
                Text { Layout.alignment: Qt.AlignRight; text: Fmt.money(Fmt.sum([totalsCard.t.net, totalsCard.t.tax], page.currency), page.currency); color: Theme.primary; font.family: Theme.font; font.weight: Font.DemiBold }
            }
        }
    }
    GridLayout {
        Layout.fillWidth: true
        columns: Theme.compact ? 1 : 2
        columnSpacing: Theme.s3
        Field { label: qsTr("Notes printed on the document"); FbTextArea { id: notes } }
        Field { label: qsTr("Payment terms"); FbTextArea { id: terms; placeholderText: qsTr("e.g. Payable within 30 days by Mobile Money or bank transfer") } }
    }

    FbConfirm {
        id: confirmIssue
        title: page.docType === "invoice" ? qsTr("Issue this invoice?") : qsTr("Finalise this document?")
        message: page.docType === "invoice"
                 ? qsTr("The invoice receives its definitive number and can no longer be modified or deleted. Stock is updated. Corrections are made with a credit note.")
                 : qsTr("The document receives its definitive number and can no longer be modified.")
        confirmText: qsTr("Issue")
        onAccepted: page.save(true)
    }
    PartyDialog {
        id: partyDialog
        onSaved: function(p) { page.customers = page.customers.concat([p]); customerBox.value = p.id; page.partyId = p.id }
    }
}
