import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Pos

// Payment of the ticket with one or several methods. Cash may be tendered above the
// amount: the change is computed by the server and shown here.
FbDialog {
    id: dlg
    property var sale: null
    property var payments: []            // [{method, label, kind, amount, tendered}]
    property var result: null            // server answer after the sale
    property string key: ""
    property bool busy: false
    property int rev: 0                  // bumped when an amount changes (the rows are edited in place)
    preferredWidth: 620
    title: result ? qsTr("Sale recorded") : qsTr("Payment")
    subtitle: result ? result.receipt.number : (sale ? Fmt.money(sale.totals.total, sale.cur) : "")
    readonly property string cur: sale ? sale.cur : Session.currency
    // Invoice instead of a receipt: an identified customer and a plan with invoices.
    property bool asInvoice: false
    readonly property bool invoiceAllowed: Session.has("sales.invoices") && !!sale && !!sale.customer
    readonly property string paid: { rev; return Fmt.sum(payments.map(function(p) { return p.amount || "0" }), cur) }
    readonly property string remaining: sale ? Fmt.sum([sale.totals.total, "-" + paid], cur) : "0"
    function start() {
        result = null; busy = false; key = Api.newIdempotencyKey(); asInvoice = false
        var cash = PosNav.config.payment_methods.filter(function(m) { return m.kind === "cash" })[0]
        payments = cash ? [{ method: cash.code, label: cash.label, kind: "cash", amount: sale.totals.total, tendered: "" }] : []
        open()
    }
    function addMethod(m) {
        var p = payments.slice()
        p.push({ method: m.code, label: m.label, kind: m.kind, amount: Fmt.isNegative(remaining) || Fmt.isZero(remaining) ? "" : remaining, tendered: "" })
        payments = p
    }
    // In place: reassigning the array would rebuild the rows while the cashier types.
    function update(i, field, value) { if (i < payments.length && payments[i][field] !== value) { payments[i][field] = value; rev++ } }
    function tourTender() { var c = payments.filter(function(p) { return p.kind === "cash" })[0]; if (c) { c.tendered = Fmt.sum([c.amount, "2140"], cur); rev++; payments = payments.slice() } }
    function removeAt(i) { var p = payments.slice(); p.splice(i, 1); payments = p }
    function submit() {
        busy = true
        var body = { session_id: PosNav.session.id, lines: sale.cart.map(function(it) { return { product_id: it.product.id, quantity: String(it.qty), discount_bp: it.discount_bp } }),
                     payments: payments.filter(function(p) { return p.amount && !Fmt.isZero(p.amount) }).map(function(p) {
                         var x = { method: p.method, amount: p.amount }; if (p.kind === "cash" && p.tendered) x.tendered = p.tendered; return x }) }
        if (sale.customer) body.customer_id = sale.customer.id
        if (dlg.asInvoice && dlg.invoiceAllowed) body.document = "invoice"
        if (sale.promo) body.promo_code = sale.promo
        var done = function(r) {
            dlg.busy = false
            if (r.ok) { dlg.result = r.data; return }
            // Uncertain network failure: the same key is retried, never applied twice.
            if (r.uncertain) PosNav.toast(qsTr("Connection lost: press Validate again, the sale will not be recorded twice."), "warning")
            else PosNav.fail(r)
            // A promo code can change the total: the server is the reference.
            if (r.error && r.error.details && r.error.details.reason === "payments_must_equal_total" && sale.promo)
                PosNav.toast(qsTr("With the promo code the total changes: the receipt shows the final amount. Adjust the payment."), "warning")
        }
        Api.send("POST", "/api/v1/pos/sales", body, function(r) {
            if (!r.ok && r.error && r.error.code === "approval_required") {
                PosNav.window.askApproval(r.error.details.action, PosNav.session.id, function(token) {
                    body.approval_token = token
                    dlg.key = Api.newIdempotencyKey()
                    Api.send("POST", "/api/v1/pos/sales", body, done, dlg.key)
                })
                dlg.busy = false
                return
            }
            done(r)
        }, key)
    }
    // Same templates as Nhill Business: roll ticket, or A4 page (invoices).
    function printReceipt(format) {
        Api.download("/api/v1/pos/sales/" + result.receipt.id + "/receipt.pdf?format=" + format,
                     result.receipt.number + (format === "a4" ? "-A4" : "") + ".pdf", function(r) { if (!r.ok) PosNav.fail(r) })
    }
    function finish() { close(); sale.clear(); sale.loadCatalog() }

    // ---------------------------------------------------------------- before payment
    ColumnLayout {
        visible: !dlg.result
        Layout.fillWidth: true
        spacing: Theme.s2
        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Total"); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg; Layout.fillWidth: true }
            Text { text: dlg.sale ? Fmt.money(dlg.sale.totals.total, dlg.cur) : ""; color: Theme.primary; font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.Bold }
        }
        Repeater {
            model: dlg.payments
            delegate: RowLayout {
                Layout.fillWidth: true
                spacing: Theme.s2
                Text { text: modelData.label; color: Theme.text; font.family: Theme.font; font.weight: Font.Medium; Layout.preferredWidth: 130; elide: Text.ElideRight }
                MoneyField { Layout.fillWidth: true; currency: dlg.cur; Component.onCompleted: setValue(modelData.amount); onValueChanged: dlg.update(index, "amount", value) }
                MoneyField { visible: modelData.kind === "cash"; Layout.preferredWidth: 150; currency: dlg.cur; placeholderText: qsTr("Tendered")
                             Component.onCompleted: setValue(modelData.tendered); onValueChanged: dlg.update(index, "tendered", value) }
                FbIconButton { iconName: "close"; tip: qsTr("Remove"); onClicked: dlg.removeAt(index) }
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: Theme.s2
            Repeater {
                model: PosNav.config ? PosNav.config.payment_methods : []
                delegate: FbButton { text: "+ " + modelData.label; variant: "secondary"; onClicked: dlg.addMethod(modelData) }
            }
        }
        FbSwitch {
            visible: dlg.invoiceAllowed
            Layout.fillWidth: true
            text: qsTr("Issue an invoice for %1 instead of a receipt").arg(dlg.sale && dlg.sale.customer ? dlg.sale.customer.name : "")
            checked: dlg.asInvoice
            onToggled: dlg.asInvoice = checked
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: Fmt.isNegative(dlg.remaining) ? qsTr("Too much") : qsTr("Remaining"); color: Theme.textMuted; font.family: Theme.font; Layout.fillWidth: true }
            Text { text: Fmt.money(dlg.remaining.replace("-", ""), dlg.cur); color: Fmt.isZero(dlg.remaining) ? Theme.success : Theme.danger; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold }
        }
    }
    // ---------------------------------------------------------------- after payment
    ColumnLayout {
        visible: !!dlg.result
        Layout.fillWidth: true
        spacing: Theme.s3
        Banner { Layout.fillWidth: true; tone: "success"
                 text: !dlg.result ? "" : dlg.result.receipt.doc_type === "invoice"
                       ? qsTr("Invoice %1 — %2").arg(dlg.result.receipt.number).arg(Fmt.money(dlg.result.receipt.total, dlg.cur))
                       : qsTr("Receipt %1 — %2").arg(dlg.result.receipt.number).arg(Fmt.money(dlg.result.receipt.total, dlg.cur)) }
        RowLayout {
            Layout.fillWidth: true
            visible: !!dlg.result && !Fmt.isZero(dlg.result.change)
            Text { text: qsTr("Change to give back"); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg; Layout.fillWidth: true }
            Text { text: dlg.result ? Fmt.money(dlg.result.change, dlg.cur) : ""; color: Theme.accent; font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.Bold }
        }
    }
    footer: [
        FbButton { visible: !dlg.result; text: qsTr("Validate"); iconName: "check"; busy: dlg.busy; enabled: Fmt.isZero(dlg.remaining) && !dlg.busy; onClicked: dlg.submit() },
        FbButton { visible: !dlg.result; text: qsTr("Back"); variant: "secondary"; onClicked: dlg.close() },
        FbButton { visible: !!dlg.result; text: dlg.result && dlg.result.receipt.doc_type === "invoice" ? qsTr("Print the invoice (A4)") : qsTr("Print the receipt")
                   iconName: "download"; variant: "secondary"
                   onClicked: dlg.printReceipt(dlg.result.receipt.doc_type === "invoice" ? "a4" : "ticket") },
        FbButton { visible: !!dlg.result && dlg.result.receipt.doc_type === "invoice"; text: qsTr("Ticket"); iconName: "download"; variant: "ghost"
                   onClicked: dlg.printReceipt("ticket") },
        FbButton { visible: !!dlg.result; text: qsTr("New sale"); iconName: "plus"; onClicked: dlg.finish() }
    ]
}
