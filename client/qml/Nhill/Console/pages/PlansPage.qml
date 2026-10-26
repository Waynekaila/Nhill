import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Offers and prices (nothing is hard-coded in the product). A price change
// applies to future payment requests only; paid periods never change.
FbPage {
    id: page
    title: qsTr("Plans & prices")
    property var rows: []
    function reload() { Api.get("/api/v1/admin/plans", function(r) { if (r.ok) page.rows = r.data.items; else ConsoleNav.fail(r) }) }
    Component.onCompleted: reload()
    actions: [ FbButton { visible: ConsoleNav.can("billing"); text: qsTr("New plan"); iconName: "plus"; onClicked: editor.start(null) } ]
    DataTable {
        anchors.fill: parent
        rows: page.rows
        columns: [
            { title: qsTr("Code"), width: 1, mono: true, value: function(p) { return p.code } },
            { title: qsTr("Name"), width: 1.8, strong: true, value: function(p) { return p.name_fr + " / " + p.name_en } },
            { title: qsTr("Tier"), width: 0.5, align: "right", value: function(p) { return String(p.tier) } },
            { title: qsTr("Prices excl. tax"), width: 2.2, value: function(p) {
                return p.prices.filter(function(x) { return x.active }).map(function(x) { return Fmt.money(x.price, x.currency) + " / " + Labels.duration(x.duration_months) }).join(" · ") } },
            { title: qsTr("Tax"), width: 0.6, align: "right", value: function(p) { return Fmt.percent(p.tax_rate_bp) } },
            { title: qsTr("Users"), width: 0.6, align: "right", value: function(p) { return p.max_users ? String(p.max_users) : "∞" } },
            { title: qsTr("Registers"), width: 0.7, align: "right", value: function(p) { return p.max_registers === null ? "∞" : String(p.max_registers) } },
            { title: qsTr("For"), width: 1, muted: true, value: function(p) { return p.audience } },
            { title: qsTr("Status"), width: 0.9, chip: function(p) { return p.active ? { text: qsTr("Published"), tone: "success" } : { text: qsTr("Hidden"), tone: "neutral" } } }
        ]
        emptyIcon: "receipt"
        emptyTitle: qsTr("No plan yet")
        emptyText: qsTr("Establishments can only subscribe once at least one plan is published.")
        onRowClicked: function(p) { if (ConsoleNav.can("billing")) editor.start(p) }
    }
    FbDialog {
        id: editor
        property var plan: null
        title: plan ? qsTr("Edit plan %1").arg(plan.code) : qsTr("New plan")
        subtitle: plan ? qsTr("Code, currency and period cannot be changed once created.") : ""
        preferredWidth: 640
        function start(p) {
            plan = p
            code.text = p ? p.code : ""; nameFr.text = p ? p.name_fr : ""; nameEn.text = p ? p.name_en : ""
            descFr.text = p ? p.description_fr : ""; descEn.text = p ? p.description_en : ""
            period.value = p ? p.billing_period : "monthly"; currency.value = p ? p.currency : "USD"
            price.setValue(p ? p.price : ""); tax.text = p ? Fmt.percentText(p.tax_rate_bp) : "0"
            users.text = p && p.max_users ? String(p.max_users) : ""; audience.value = p ? p.audience : "any"
            active.checked = p ? p.active : true; sort.text = p ? String(p.sort_order) : "0"
            tier.text = p ? String(p.tier) : "0"; regs.text = p && p.max_registers !== null ? String(p.max_registers) : ""
            whs.text = p && p.max_warehouses !== null ? String(p.max_warehouses) : ""
            features.text = p ? JSON.stringify(p.features) : "{}"
            open()
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.s3; rowSpacing: Theme.s2
            Field { label: qsTr("Code"); FbTextField { id: code; monospace: true; enabled: !editor.plan; placeholderText: "pro-monthly" } }
            Field { label: qsTr("Audience"); FbComboBox { id: audience; model: [{ value: "any", text: qsTr("Everyone") }, { value: "individual", text: qsTr("Individual entrepreneurs") }, { value: "company", text: qsTr("Companies") }] } }
            Field { label: qsTr("Name (French)"); FbTextField { id: nameFr } }
            Field { label: qsTr("Name (English)"); FbTextField { id: nameEn } }
            Field { label: qsTr("Description (French)"); FbTextArea { id: descFr } }
            Field { label: qsTr("Description (English)"); FbTextArea { id: descEn } }
            Field { label: qsTr("Billing period"); FbComboBox { id: period; enabled: !editor.plan; model: [{ value: "monthly", text: qsTr("Monthly") }, { value: "annual", text: qsTr("Annual") }] } }
            Field { label: qsTr("Currency"); FbComboBox { id: currency; enabled: !editor.plan; model: Labels.currencies() } }
            Field { label: qsTr("Price excl. tax"); MoneyField { id: price; currency: currency.value || "USD" } }
            Field { label: qsTr("Tax rate (%)"); FbTextField { id: tax } }
            Field { label: qsTr("Maximum users"); help: qsTr("Empty = unlimited"); FbTextField { id: users; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("Display order"); FbTextField { id: sort; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("Tier"); help: qsTr("Higher tier = upgrade (immediate, pro rata); lower = downgrade at period end"); FbTextField { id: tier; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("POS registers"); help: qsTr("Empty = unlimited, 0 = no POS"); FbTextField { id: regs; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("Warehouses"); help: qsTr("Empty = unlimited"); FbTextField { id: whs; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("Features (JSON)"); help: qsTr("e.g. {\"pos\": true, \"multi_warehouse\": true}"); FbTextField { id: features; monospace: true } }
        }
        ColumnLayout {
            visible: !!editor.plan
            Layout.fillWidth: true
            spacing: Theme.s2
            SectionTitle { text: qsTr("Prices per duration and currency"); hint: qsTr("Changes apply to future payment requests only.") }
            Repeater {
                model: editor.plan ? editor.plan.prices : []
                delegate: RowLayout {
                    Layout.fillWidth: true
                    Text { text: Labels.duration(modelData.duration_months) + " · " + modelData.currency; color: Theme.text; font.family: Theme.font; Layout.preferredWidth: 160 }
                    Text { text: Fmt.money(modelData.price, modelData.currency); color: modelData.active ? Theme.text : Theme.textFaint; font.family: Theme.font; font.weight: Font.DemiBold; Layout.fillWidth: true }
                    FbButton { text: modelData.active ? qsTr("Withdraw") : qsTr("Offer again"); variant: "ghost"
                               onClicked: Api.send("PATCH", "/api/v1/admin/plan-prices/" + modelData.id, { active: !modelData.active }, function(r) { if (r.ok) { editor.close(); page.reload() } else ConsoleNav.fail(r) }) }
                }
            }
            RowLayout {
                FbComboBox { id: newDuration; Layout.preferredWidth: 140; model: Labels.durations(); value: 12 }
                FbComboBox { id: newCurrency; Layout.preferredWidth: 120; model: Labels.currencies(); value: "USD" }
                MoneyField { id: newPrice; currency: newCurrency.value || "USD"; Layout.preferredWidth: 140 }
                FbButton { text: qsTr("Add / replace the price"); variant: "secondary"; iconName: "plus"; enabled: !!newPrice.value
                           onClicked: Api.send("POST", "/api/v1/admin/plans/" + editor.plan.id + "/prices", { duration_months: newDuration.value, currency: newCurrency.value, price: newPrice.value },
                                               function(r) { if (r.ok) { editor.close(); page.reload(); ConsoleNav.toast(qsTr("Price saved"), "success") } else ConsoleNav.fail(r) }) }
            }
        }
        FbSwitch { id: active; text: qsTr("Published (visible to establishments)") }
        footer: [
            FbButton { text: qsTr("Save"); onClicked: {
                    var bp = Fmt.parsePercent(tax.text)
                    if (bp < 0 || !price.value) { ConsoleNav.toast(qsTr("Check the price and the tax rate."), "danger"); return }
                    var b = { name_fr: nameFr.text, name_en: nameEn.text, description_fr: descFr.text, description_en: descEn.text,
                              price: price.value, tax_rate_bp: bp, active: active.checked, sort_order: parseInt(sort.text || "0") }
                    if (users.text.trim()) b.max_users = parseInt(users.text); else if (editor.plan) b.max_users = null
                    b.tier = parseInt(tier.text || "0")
                    b.max_registers = regs.text.trim() ? parseInt(regs.text) : null
                    b.max_warehouses = whs.text.trim() ? parseInt(whs.text) : null
                    try { b.features = JSON.parse(features.text || "{}") } catch (e) { ConsoleNav.toast(qsTr("Features: invalid JSON."), "danger"); return }
                    var cb = function(r) { if (r.ok) { editor.close(); page.reload() } else ConsoleNav.fail(r) }
                    if (editor.plan) Api.send("PATCH", "/api/v1/admin/plans/" + editor.plan.id, b, cb)
                    else { b.code = code.text.trim(); b.billing_period = period.value; b.currency = currency.value; b.audience = audience.value
                           Api.send("POST", "/api/v1/admin/plans", b, cb) }
                } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: editor.close() }
        ]
    }
}
