import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Subscription promo codes. The discount itself is immutable once created; validity,
// limits and activation can change. A code is consumed only by a paid order.
FbPage {
    id: page
    title: qsTr("Promo codes")
    subtitle: qsTr("Discounts on subscriptions — limits, validity and eligibility checked by the server")
    property var rows: []
    property var plans: []
    function reload() {
        Api.get("/api/v1/admin/promo-codes", function(r) { if (r.ok) page.rows = r.data.items; else ConsoleNav.fail(r) })
        Api.get("/api/v1/admin/plans", function(r) { if (r.ok) page.plans = r.data.items })
    }
    function discountText(p) { return p.discount_kind === "percent" ? Fmt.percent(p.percent_bp) : Fmt.money(p.amount, p.currency) }
    function planNames(ids) {
        if (!ids) return qsTr("All plans")
        return ids.map(function(id) { for (var i = 0; i < plans.length; ++i) if (plans[i].id === id) return plans[i].code; return "?" }).join(", ")
    }
    Component.onCompleted: reload()
    actions: [ FbButton { visible: ConsoleNav.can("billing"); text: qsTr("New code"); iconName: "plus"; onClicked: creator.start() } ]
    DataTable {
        anchors.fill: parent
        rows: page.rows
        columns: [
            { title: qsTr("Code"), width: 1.2, mono: true, strong: true, value: function(p) { return p.code } },
            { title: qsTr("Discount"), width: 0.9, align: "right", value: function(p) { return page.discountText(p) } },
            { title: qsTr("Plans"), width: 1.3, muted: true, value: function(p) { return page.planNames(p.plan_ids) } },
            { title: qsTr("Valid"), width: 1.6, muted: true, value: function(p) { return Fmt.date(p.valid_from) + " → " + (p.valid_until ? Fmt.date(p.valid_until) : "∞") } },
            { title: qsTr("Used"), width: 0.9, align: "right", value: function(p) { return p.redemptions_confirmed + (p.max_redemptions ? " / " + p.max_redemptions : "") + (p.redemptions_reserved ? " (+" + p.redemptions_reserved + ")" : "") } },
            { title: qsTr("Status"), width: 0.9, chip: function(p) {
                var expired = p.valid_until && new Date(p.valid_until) < new Date()
                return !p.active ? { text: qsTr("Disabled"), tone: "neutral" } : expired ? { text: qsTr("Expired"), tone: "neutral" } : { text: qsTr("Active"), tone: "success" } } }
        ]
        emptyIcon: "receipt"
        emptyTitle: qsTr("No promo code")
        onRowClicked: function(p) { details.start(p) }
    }
    FbDialog {
        id: creator
        title: qsTr("New promo code")
        subtitle: qsTr("Codes are case-insensitive and stored in capitals.")
        preferredWidth: 640
        property var selectedPlans: []
        function start() {
            code.text = ""; desc.text = ""; kind.value = "percent"; pct.text = "10"; amount.setValue(""); cur.value = "USD"
            from.value = Fmt.today(); until.value = ""; maxTotal.text = ""; maxEst.text = "1"; minMonths.text = ""
            newOnly.checked = false; audience.value = "any"; selectedPlans = []
            open()
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.s3; rowSpacing: Theme.s2
            Field { label: qsTr("Code"); required: true; FbTextField { id: code; monospace: true; placeholderText: "LANCEMENT2026"; validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9_-]{0,32}/ } } }
            Field { label: qsTr("Description (internal)"); FbTextField { id: desc } }
            Field { label: qsTr("Discount type"); FbComboBox { id: kind; Layout.fillWidth: true; model: [{ value: "percent", text: qsTr("Percentage") }, { value: "fixed", text: qsTr("Fixed amount") }] } }
            Field { visible: kind.value === "percent"; label: qsTr("Percentage (%)"); FbTextField { id: pct } }
            Field { visible: kind.value === "fixed"; label: qsTr("Amount"); MoneyField { id: amount; currency: cur.value || "USD" } }
            Field { visible: kind.value === "fixed"; label: qsTr("Currency"); FbComboBox { id: cur; Layout.fillWidth: true; model: Labels.currencies() } }
            Field { label: qsTr("Valid from"); DateField { id: from } }
            Field { label: qsTr("Valid until (excluded)"); help: qsTr("Empty = no end"); DateField { id: until } }
            Field { label: qsTr("Total uses"); help: qsTr("Empty = unlimited"); FbTextField { id: maxTotal; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("Uses per establishment"); FbTextField { id: maxEst; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("Minimum duration (months)"); FbTextField { id: minMonths; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("For"); FbComboBox { id: audience; Layout.fillWidth: true; model: [{ value: "any", text: qsTr("Everyone") }, { value: "individual", text: qsTr("Individual entrepreneurs") }, { value: "company", text: qsTr("Companies") }] } }
        }
        FbSwitch { id: newOnly; text: qsTr("New customers only (never paid a subscription)") }
        SectionTitle { text: qsTr("Eligible plans"); hint: qsTr("None selected = every plan") }
        Flow {
            Layout.fillWidth: true
            spacing: Theme.s2
            Repeater {
                model: page.plans
                delegate: FbButton {
                    readonly property bool on: creator.selectedPlans.indexOf(modelData.id) >= 0
                    text: modelData.code; variant: on ? "primary" : "secondary"
                    onClicked: { var s = creator.selectedPlans.slice(); var i = s.indexOf(modelData.id); if (i >= 0) s.splice(i, 1); else s.push(modelData.id); creator.selectedPlans = s }
                }
            }
        }
        footer: [
            FbButton { text: qsTr("Create"); onClicked: {
                    var b = { code: code.text.trim(), description: desc.text.trim(), discount_kind: kind.value, audience: audience.value,
                              new_customers_only: newOnly.checked, max_per_establishment: parseInt(maxEst.text || "1") }
                    if (kind.value === "percent") { var bp = Fmt.parsePercent(pct.text); if (bp <= 0) { ConsoleNav.toast(qsTr("Check the percentage."), "danger"); return } b.percent_bp = bp }
                    else { b.amount = amount.value; b.currency = cur.value }
                    if (from.value) b.valid_from = from.value
                    if (until.value) b.valid_until = until.value
                    if (maxTotal.text.trim()) b.max_redemptions = parseInt(maxTotal.text)
                    if (minMonths.text.trim()) b.min_duration_months = parseInt(minMonths.text)
                    if (creator.selectedPlans.length) b.plan_ids = creator.selectedPlans
                    Api.send("POST", "/api/v1/admin/promo-codes", b, function(r) { if (r.ok) { creator.close(); page.reload() } else ConsoleNav.fail(r) })
                } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: creator.close() }
        ]
    }
    FbDialog {
        id: details
        property var promo: null
        property var uses: []
        title: promo ? promo.code : ""
        subtitle: promo ? page.discountText(promo) + " · " + page.planNames(promo.plan_ids) : ""
        preferredWidth: 640
        function start(p) {
            promo = p; uses = []; untilEdit.value = p.valid_until ? p.valid_until.slice(0, 10) : ""; maxEdit.text = p.max_redemptions ? String(p.max_redemptions) : ""
            activeEdit.checked = p.active
            Api.get("/api/v1/admin/promo-codes/" + p.id + "/redemptions", function(r) { if (r.ok) details.uses = r.data.items })
            open()
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.s3
            Field { label: qsTr("Valid until (excluded)"); DateField { id: untilEdit } }
            Field { label: qsTr("Total uses"); FbTextField { id: maxEdit; inputMethodHints: Qt.ImhDigitsOnly } }
        }
        FbSwitch { id: activeEdit; text: qsTr("Active") }
        SectionTitle { text: qsTr("Uses") }
        Repeater {
            model: details.uses
            Text { Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: Fmt.dateTime(modelData.created_at) + " · " + modelData.establishment + " · " + Fmt.money(modelData.discount, modelData.currency) + " · " + modelData.status }
        }
        Text { visible: details.uses.length === 0; text: qsTr("Not used yet."); color: Theme.textFaint; font.family: Theme.font }
        footer: [
            FbButton { visible: ConsoleNav.can("billing"); text: qsTr("Save"); onClicked: {
                    var b = { active: activeEdit.checked, valid_until: untilEdit.value ? untilEdit.value : null,
                              max_redemptions: maxEdit.text.trim() ? parseInt(maxEdit.text) : null }
                    Api.send("PATCH", "/api/v1/admin/promo-codes/" + details.promo.id, b, function(r) { if (r.ok) { details.close(); page.reload() } else ConsoleNav.fail(r) })
                } },
            FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: details.close() }
        ]
    }
}
