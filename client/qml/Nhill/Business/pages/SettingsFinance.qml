import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

ColumnLayout {
    id: root
    spacing: Theme.s4
    property var taxes: []
    property var accounts: []
    property var rates: []
    function load() {
        Api.get("/api/v1/tax-rates", function(r) { if (r.ok) root.taxes = r.data.items })
        Api.get("/api/v1/cash-accounts", function(r) { if (r.ok) root.accounts = r.data.items })
        Api.get("/api/v1/exchange-rates", function(r) { if (r.ok) root.rates = r.data.items })
    }
    Component.onCompleted: load()

    RowLayout { SectionTitle { text: qsTr("Tax rates"); Layout.fillWidth: true } FbButton { text: qsTr("New rate"); iconName: "plus"; variant: "secondary"; onClicked: taxDialog.open() } }
    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
           text: qsTr("Rates come from your country profile. A rate cannot be changed once created: issued documents keep the rate they were issued with. To apply a new rate, create a new code and make it the default.") }
    DataTable {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(420, 64 + root.taxes.length * 52)
        rows: root.taxes
        titleColumn: 1; subtitleColumn: 0; trailingColumn: 2; chipColumn: 3
        columns: [
            { title: qsTr("Code"), width: 0.8, mono: true, value: function(t) { return t.code } },
            { title: qsTr("Label"), width: 2.2, strong: true, value: function(t) { return t.label } },
            { title: qsTr("Rate"), width: 0.8, align: "right", value: function(t) { return Fmt.percent(t.rate_bp) } },
            { title: qsTr("Status"), width: 1, align: "right", chip: function(t) { return t.is_default ? { text: qsTr("Default"), tone: "primary" } : t.active ? { text: qsTr("Active"), tone: "neutral" } : { text: qsTr("Inactive"), tone: "neutral" } } }
        ]
        onRowClicked: function(t) { taxEdit.show(t) }
    }

    RowLayout { SectionTitle { text: qsTr("Cash and bank accounts"); Layout.fillWidth: true } FbButton { text: qsTr("New account"); iconName: "plus"; variant: "secondary"; onClicked: accountDialog.open() } }
    DataTable {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(360, 64 + root.accounts.length * 52)
        rows: root.accounts
        titleColumn: 0; subtitleColumn: 1; trailingColumn: 3
        columns: [
            { title: qsTr("Name"), width: 2, strong: true, value: function(a) { return a.name } },
            { title: qsTr("Type"), width: 1.2, muted: true, value: function(a) { return a.kind === "cash" ? qsTr("Cash") : a.kind === "bank" ? qsTr("Bank") : a.kind === "mobile_money" ? qsTr("Mobile Money") : qsTr("Other") } },
            { title: qsTr("Currency"), width: 0.8, value: function(a) { return a.currency } },
            { title: qsTr("Recorded balance"), width: 1.4, align: "right", value: function(a) { return Fmt.money(a.recorded_balance, a.currency) } }
        ]
    }

    RowLayout { SectionTitle { text: qsTr("Exchange rates"); Layout.fillWidth: true } FbButton { text: qsTr("Record a rate"); iconName: "plus"; variant: "secondary"; onClicked: rateDialog.start() } }
    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
           text: qsTr("Nhill does not fetch rates automatically. Enter the rate you apply with its date and source (e.g. Banque Centrale du Congo daily rate). A document keeps the rate recorded when it was issued.") }
    DataTable {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(360, 64 + root.rates.length * 52)
        rows: root.rates
        titleColumn: 1; subtitleColumn: 0; trailingColumn: 2
        columns: [
            { title: qsTr("Date"), width: 1, muted: true, value: function(x) { return Fmt.date(x.rate_date) } },
            { title: qsTr("Pair"), width: 1, strong: true, value: function(x) { return "1 " + x.from + " = … " + x.to } },
            { title: qsTr("Rate"), width: 1.2, align: "right", mono: true, value: function(x) { return x.rate } },
            { title: qsTr("Source"), width: 2, muted: true, value: function(x) { return x.source || "—" } }
        ]
        emptyIcon: "swap"
        emptyTitle: qsTr("No rate recorded")
    }

    FbDialog {
        id: taxDialog
        title: qsTr("New tax rate")
        Field { label: qsTr("Code"); FbTextField { id: taxCode; monospace: true; placeholderText: "TVA16" } }
        Field { label: qsTr("Label"); FbTextField { id: taxLabel } }
        Field { label: qsTr("Rate (%)"); FbTextField { id: taxRate; horizontalAlignment: TextInput.AlignRight } }
        Field { label: qsTr("Nature"); FbComboBox { id: taxKind; value: "standard"; model: [{ value: "standard", text: qsTr("Standard") }, { value: "reduced", text: qsTr("Reduced") }, { value: "zero", text: qsTr("Zero rate") }, { value: "exempt", text: qsTr("Exempt") }] } }
        FbSwitch { id: taxDefault; text: qsTr("Use by default") }
        footer: [
            FbButton { text: qsTr("Create"); onClicked: {
                    var bp = Fmt.parsePercent(taxRate.text)
                    if (bp < 0 || !taxCode.text.trim() || !taxLabel.text.trim()) { Nav.toast(qsTr("Code, label and a valid rate are required."), "danger"); return }
                    Api.send("POST", "/api/v1/tax-rates", { code: taxCode.text.trim(), label: taxLabel.text.trim(), rate_bp: bp, kind: taxKind.value, is_default: taxDefault.checked },
                             function(r) { if (r.ok) { taxDialog.close(); root.load() } else Nav.fail(r) }) } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: taxDialog.close() }
        ]
    }
    FbDialog {
        id: taxEdit
        property var t: null
        title: t ? t.code + " · " + Fmt.percent(t.rate_bp) : ""
        function show(x) { t = x; editLabel.text = x.label; open() }
        Field { label: qsTr("Label"); FbTextField { id: editLabel } }
        footer: [
            FbButton { text: qsTr("Save"); onClicked: Api.send("PATCH", "/api/v1/tax-rates/" + taxEdit.t.id, { label: editLabel.text }, function(r) { if (r.ok) { taxEdit.close(); root.load() } else Nav.fail(r) }) },
            FbButton { visible: !!taxEdit.t && !taxEdit.t.is_default && taxEdit.t.active; text: qsTr("Make default"); variant: "secondary"
                       onClicked: Api.send("PATCH", "/api/v1/tax-rates/" + taxEdit.t.id, { is_default: true }, function(r) { if (r.ok) { taxEdit.close(); root.load() } else Nav.fail(r) }) },
            FbButton { visible: !!taxEdit.t; text: taxEdit.t && taxEdit.t.active ? qsTr("Deactivate") : qsTr("Reactivate"); variant: "ghost"
                       onClicked: Api.send("PATCH", "/api/v1/tax-rates/" + taxEdit.t.id, { active: !taxEdit.t.active }, function(r) { if (r.ok) { taxEdit.close(); root.load() } else Nav.fail(r) }) }
        ]
    }
    FbDialog {
        id: accountDialog
        title: qsTr("New account")
        Field { label: qsTr("Name"); FbTextField { id: accName; placeholderText: qsTr("e.g. M-Pesa, Rawbank USD") } }
        Field { label: qsTr("Type"); FbComboBox { id: accKind; value: "bank"; model: [{ value: "cash", text: qsTr("Cash") }, { value: "bank", text: qsTr("Bank") }, { value: "mobile_money", text: qsTr("Mobile Money") }, { value: "other", text: qsTr("Other") }] } }
        Field { label: qsTr("Currency"); FbComboBox { id: accCur; model: Labels.currencies(); value: Session.currency } }
        footer: [
            FbButton { text: qsTr("Create"); onClicked: Api.send("POST", "/api/v1/cash-accounts", { name: accName.text.trim(), kind: accKind.value, currency: accCur.value },
                                                                 function(r) { if (r.ok) { accountDialog.close(); accName.text = ""; root.load() } else Nav.fail(r) }) },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: accountDialog.close() }
        ]
    }
    FbDialog {
        id: rateDialog
        title: qsTr("Record an exchange rate")
        subtitle: qsTr("1 unit of the first currency = rate × the second currency")
        function start() { rateDate.value = Fmt.today(); rateValue.text = ""; open() }
        RowLayout {
            Layout.fillWidth: true
            Field { label: qsTr("From"); FbComboBox { id: rateFrom; model: Labels.currencies(); value: "USD" } }
            Field { label: qsTr("To"); FbComboBox { id: rateTo; model: Labels.currencies(); value: "CDF" } }
        }
        Field { label: qsTr("Rate"); FbTextField { id: rateValue; horizontalAlignment: TextInput.AlignRight; placeholderText: "2850.50" } }
        Field { label: qsTr("Date"); DateField { id: rateDate } }
        Field { label: qsTr("Source"); FbTextField { id: rateSource; placeholderText: qsTr("e.g. BCC daily rate") } }
        footer: [
            FbButton { text: qsTr("Record"); onClicked: Api.send("POST", "/api/v1/exchange-rates",
                           { from: rateFrom.value, to: rateTo.value, rate: rateValue.text.trim().replace(",", ".").replace(/\s/g, ""), rate_date: rateDate.value, source: rateSource.text },
                           function(r) { if (r.ok) { rateDialog.close(); root.load() } else Nav.fail(r) }) },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: rateDialog.close() }
        ]
    }
}
