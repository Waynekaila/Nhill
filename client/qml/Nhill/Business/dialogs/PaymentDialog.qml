import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

// Records a payment against an invoice, a credit note (refund), a purchase or an expense.
FbDialog {
    id: dlg
    property string target: "document_id"    // document_id | purchase_id | expense_id
    property string targetId: ""
    property string currency: Session.currency
    property string due: "0"
    property var accounts: []
    property string key: ""
    property bool saving: false
    property bool uncertain: false
    signal saved()
    title: qsTr("Record a payment")
    subtitle: qsTr("Amount still due: %1").arg(Fmt.money(due, currency))
    preferredWidth: 480
    function start(t, id, cur, dueAmount) {
        target = t; targetId = id; currency = cur; due = dueAmount; key = Api.newIdempotencyKey(); uncertain = false
        amount.setValue(dueAmount); reference.text = ""; note.text = ""; paidAt.value = Fmt.today()
        Api.get("/api/v1/cash-accounts", function(r) { if (r.ok) { dlg.accounts = r.data.items.filter(function(a) { return a.currency === dlg.currency && a.active }); account.value = dlg.accounts.length === 1 ? dlg.accounts[0].id : "" } })
        open()
    }
    function save() {
        if (!amount.value || Fmt.isZero(amount.value)) { amountField.error = qsTr("Enter the amount received."); return }
        saving = true
        var b = { amount: amount.value, method: method.value, reference: reference.text, note: note.text, paid_at: paidAt.value }
        b[target] = targetId
        if (account.value) b.account_id = account.value
        Api.send("POST", "/api/v1/payments", b, function(r) {
            saving = false
            if (r.ok) { dlg.uncertain = false; dlg.close(); Nav.toast(qsTr("Payment recorded"), "success"); dlg.saved() }
            else { dlg.uncertain = r.uncertain; Nav.fail(r) }
        }, key)
    }
    Banner { visible: dlg.uncertain; tone: "warning"; text: qsTr("No answer from the server. Retry: the payment will not be recorded twice."); actionText: qsTr("Retry"); onAction: dlg.save() }
    Field { id: amountField; label: qsTr("Amount (%1)").arg(dlg.currency); required: true
            MoneyField { id: amount; currency: dlg.currency; onTextChanged: amountField.error = "" } }
    Field { label: qsTr("Method"); FbComboBox { id: method; model: Labels.methods(); value: "cash" } }
    Field { label: qsTr("Cash / account"); visible: dlg.accounts.length > 0
            FbComboBox { id: account; model: [{ value: "", text: qsTr("— Not specified —") }].concat(dlg.accounts.map(function(a) { return { value: a.id, text: a.name } })); value: "" } }
    Field { label: qsTr("Date"); DateField { id: paidAt } }
    Field { label: qsTr("Reference"); help: qsTr("Mobile Money transaction ID, cheque number, bank reference…"); FbTextField { id: reference; monospace: true } }
    Field { label: qsTr("Note"); FbTextField { id: note } }
    footer: [
        FbButton { text: qsTr("Record"); busy: dlg.saving; onClicked: dlg.save() },
        FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: dlg.close() }
    ]
}
