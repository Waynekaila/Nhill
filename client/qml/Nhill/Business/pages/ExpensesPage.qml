import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Expenses")
    subtitle: qsTr("Overheads with their receipts")
    property var rows: []
    property var categories: []
    property bool unpaidOnly: false
    property bool loading: false
    property var receiptTarget: null
    function reload() {
        loading = true
        Api.get("/api/v1/expense-categories", function(r) { if (r.ok) page.categories = r.data.items })
        Api.get("/api/v1/expenses?limit=200" + (unpaidOnly ? "&unpaid=1" : ""), function(r) { loading = false; if (r.ok) page.rows = r.data.items; else Nav.fail(r) })
    }
    Component.onCompleted: reload()
    actions: [ FbButton { visible: Session.can("expenses.write"); text: qsTr("New expense"); iconName: "plus"; onClicked: editor.start() } ]
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        RowLayout {
            FbSwitch { text: qsTr("Unpaid only"); checked: page.unpaidOnly; onToggled: { page.unpaidOnly = checked; page.reload() } }
            Item { Layout.fillWidth: true }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.rows
            titleColumn: 1; subtitleColumn: 0; trailingColumn: 3; chipColumn: 4
            columns: [
                { title: qsTr("Date"), width: 1, muted: true, value: function(e) { return Fmt.date(e.expense_date) + (Theme.compact && e.category ? " · " + e.category : "") } },
                { title: qsTr("Description"), width: 2.6, strong: true, value: function(e) { return e.description + (e.supplier ? " — " + e.supplier : "") } },
                { title: qsTr("Category"), width: 1.2, muted: true, value: function(e) { return e.category || "—" } },
                { title: qsTr("Amount"), width: 1.3, align: "right", value: function(e) { return Fmt.money(e.amount, e.currency) } },
                { title: qsTr("Status"), width: 1.1, align: "right", chip: function(e) {
                      return e.status === "paid" ? { text: qsTr("Paid"), tone: "success" } : e.status === "partially_paid" ? { text: qsTr("Partially paid"), tone: "warning" } : { text: qsTr("Unpaid"), tone: "danger" } } },
                { title: qsTr("Receipt"), width: 0.8, align: "right", muted: true, value: function(e) { return e.has_receipt ? qsTr("Attached") : "—" } }
            ]
            emptyIcon: "receipt"
            emptyTitle: qsTr("No expense recorded")
            emptyText: qsTr("Rent, fuel, electricity, salaries… Record them to know your real result.")
            emptyActionText: Session.can("expenses.write") ? qsTr("New expense") : ""
            onEmptyAction: editor.start()
            onRowClicked: function(e) { details.show(e) }
        }
    }
    FbDialog {
        id: editor
        title: qsTr("New expense")
        preferredWidth: 560
        property string key: ""
        property string currency: Session.currency
        function start() {
            key = Api.newIdempotencyKey(); currency = Session.currency; desc.text = ""; amount.setValue(""); tax.setValue("")
            date.value = Fmt.today(); paidNow.checked = true; catBox.value = ""; newCat.text = ""
            Api.get("/api/v1/suppliers?limit=200", function(r) { if (r.ok) supplierBox.model = [{ value: "", text: qsTr("— None —") }].concat(r.data.items.map(function(s) { return { value: s.id, text: s.name } })) })
            Api.get("/api/v1/cash-accounts", function(r) { if (r.ok) editor.accounts = r.data.items.filter(function(a) { return a.active }) })
            open()
        }
        property var accounts: []
        Field { label: qsTr("Description"); required: true; FbTextField { id: desc; placeholderText: qsTr("e.g. Electricity October") } }
        GridLayout {
            Layout.fillWidth: true
            columns: Theme.compact ? 1 : 2
            columnSpacing: Theme.s3; rowSpacing: Theme.s3
            Field { label: qsTr("Amount (tax included)"); required: true; MoneyField { id: amount; currency: editor.currency } }
            Field { label: qsTr("Currency"); FbComboBox { model: Labels.currencyChoices(editor.currency); value: editor.currency; onActivated: editor.currency = currentValue } }
            Field { label: qsTr("Of which deductible tax"); help: qsTr("Optional"); MoneyField { id: tax; currency: editor.currency } }
            Field { label: qsTr("Date"); DateField { id: date } }
            Field { label: qsTr("Category")
                    FbComboBox { id: catBox; model: [{ value: "", text: qsTr("— None —") }].concat(page.categories.map(function(c) { return { value: c.id, text: c.name } })) } }
            Field { label: qsTr("…or new category"); FbTextField { id: newCat; placeholderText: qsTr("e.g. Transport") } }
            Field { label: qsTr("Supplier"); Layout.columnSpan: Theme.compact ? 1 : 2; FbComboBox { id: supplierBox; model: [] } }
        }
        FbSwitch { id: paidNow; text: qsTr("Already paid in full") }
        GridLayout {
            visible: paidNow.checked
            Layout.fillWidth: true
            columns: Theme.compact ? 1 : 2
            columnSpacing: Theme.s3
            Field { label: qsTr("Method"); FbComboBox { id: methodBox; model: Labels.methods(); value: "cash" } }
            Field { label: qsTr("Account"); FbComboBox { id: accountBox
                    model: [{ value: "", text: qsTr("— Not specified —") }].concat(editor.accounts.filter(function(a) { return a.currency === editor.currency }).map(function(a) { return { value: a.id, text: a.name } })) } }
        }
        footer: [
            FbButton { text: qsTr("Save"); onClicked: {
                    if (!desc.text.trim() || !amount.value) { Nav.toast(qsTr("Description and amount are required."), "danger"); return }
                    var create = function(categoryId) {
                        var b = { description: desc.text, amount: amount.value, currency: editor.currency, expense_date: date.value }
                        if (tax.value) b.tax = tax.value
                        if (categoryId) b.category_id = categoryId
                        if (supplierBox.value) b.supplier_id = supplierBox.value
                        if (paidNow.checked) { b.paid = { method: methodBox.value }; if (accountBox.value) b.paid.account_id = accountBox.value }
                        Api.send("POST", "/api/v1/expenses", b, function(r) {
                            if (r.ok) { editor.close(); Nav.toast(qsTr("Expense recorded"), "success"); page.reload() } else Nav.fail(r)
                        }, editor.key)
                    }
                    if (newCat.text.trim())
                        Api.send("POST", "/api/v1/expense-categories", { name: newCat.text.trim() }, function(r) { if (r.ok) create(r.data.id); else Nav.fail(r) })
                    else create(catBox.value)
                } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: editor.close() }
        ]
    }
    FbDialog {
        id: details
        property var e: null
        title: e ? e.description : ""
        subtitle: e ? Fmt.date(e.expense_date) + " · " + Fmt.money(e.amount, e.currency) : ""
        function show(x) { e = x; open() }
        Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font
               text: details.e ? qsTr("Paid: %1 · Tax: %2 · Category: %3").arg(Fmt.money(details.e.paid, details.e.currency)).arg(Fmt.money(details.e.tax, details.e.currency)).arg(details.e.category || "—") : "" }
        footer: [
            FbButton { visible: !!details.e && details.e.status !== "paid" && Session.can("payments.write"); text: qsTr("Record payment"); iconName: "payment"
                       onClicked: { details.close(); pay.start("expense_id", details.e.id, details.e.currency, Fmt.sum([details.e.amount, "-" + details.e.paid], details.e.currency)) } },
            FbButton { visible: !!details.e && Session.can("expenses.write"); text: details.e && details.e.has_receipt ? qsTr("Replace receipt") : qsTr("Attach receipt"); iconName: "upload"; variant: "secondary"
                       onClicked: { page.receiptTarget = details.e; receiptPicker.open() } },
            FbButton { visible: !!details.e && details.e.has_receipt; text: qsTr("View receipt"); iconName: "eye"; variant: "secondary"
                       onClicked: Api.download("/api/v1/expenses/" + details.e.id + "/receipt", "justificatif-" + details.e.expense_date, function(r) { if (!r.ok) Nav.fail(r) }) },
            FbButton { text: qsTr("Close"); variant: "ghost"; onClicked: details.close() }
        ]
    }
    FileDialog {
        id: receiptPicker
        title: qsTr("Choose a receipt (PDF, PNG or JPEG, 5 MB max)")
        nameFilters: [qsTr("Receipts (*.pdf *.png *.jpg *.jpeg)")]
        onAccepted: Api.upload("PUT", "/api/v1/expenses/" + page.receiptTarget.id + "/receipt", selectedFile, function(r) {
            if (r.ok) { details.close(); Nav.toast(qsTr("Receipt attached"), "success"); page.reload() } else Nav.fail(r) })
    }
    PaymentDialog { id: pay; onSaved: page.reload() }
}
