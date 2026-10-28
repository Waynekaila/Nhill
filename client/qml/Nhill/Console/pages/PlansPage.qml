import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Plans, their prices, their conditions and their capabilities (3.0.0). Nothing is
// hard-coded in the product: the features and limits of each plan are chosen here and
// apply to the establishments of the plan at their next request. A price change applies
// to future payment requests only; paid periods never change.
FbPage {
    id: page
    title: qsTr("Plans & prices")
    property var rows: []
    property var catalog: []        // GET /admin/capabilities: features and limits enforced by the server
    function reload() {
        Api.get("/api/v1/admin/plans", function(r) { if (r.ok) page.rows = r.data.items; else ConsoleNav.fail(r) })
        Api.get("/api/v1/admin/capabilities", function(r) { if (r.ok) page.catalog = r.data.items })
    }
    Component.onCompleted: reload()
    function label(c) { return Prefs.language === "fr" ? c.label_fr : c.label_en }
    function kindText(k) {
        return k === "paid" ? qsTr("Paid") : k === "trial" ? qsTr("Trial") : k === "free" ? qsTr("Free") : k === "custom" ? qsTr("Custom") : k
    }
    function featureCount(p) {
        var n = 0
        page.catalog.forEach(function(c) {
            if (c.kind !== "feature") return
            var v = p.entitlements && p.entitlements[c.key] !== undefined ? p.entitlements[c.key] : c.default_enabled
            if (v) n++
        })
        return n
    }
    actions: [ FbButton { visible: ConsoleNav.can("billing"); text: qsTr("New plan"); iconName: "plus"; onClicked: editor.start(null) } ]
    DataTable {
        anchors.fill: parent
        rows: page.rows
        columns: [
            { title: qsTr("Code"), width: 1, mono: true, value: function(p) { return p.code } },
            { title: qsTr("Name"), width: 1.8, strong: true, value: function(p) { return p.name_fr + " / " + p.name_en } },
            { title: qsTr("Kind"), width: 0.8, value: function(p) { return page.kindText(p.kind) + (p.visibility === "private" ? " · " + qsTr("private") : "") } },
            { title: qsTr("Tier"), width: 0.5, align: "right", value: function(p) { return String(p.tier) } },
            { title: qsTr("Prices excl. tax"), width: 2.2, value: function(p) {
                return p.prices.filter(function(x) { return x.active }).map(function(x) { return Fmt.money(x.price, x.currency) + " / " + Labels.duration(x.duration_months) }).join(" · ") } },
            { title: qsTr("Features"), width: 0.8, align: "right", value: function(p) { return String(page.featureCount(p)) } },
            { title: qsTr("Users"), width: 0.6, align: "right", value: function(p) { return p.max_users ? String(p.max_users) : "∞" } },
            { title: qsTr("For"), width: 1, muted: true, value: function(p) { return p.audience } },
            { title: qsTr("Status"), width: 0.9, chip: function(p) { return p.archived_at ? { text: qsTr("Archived"), tone: "neutral" }
                                                                         : p.active ? { text: qsTr("Published"), tone: "success" } : { text: qsTr("Hidden"), tone: "neutral" } } }
        ]
        emptyIcon: "receipt"
        emptyTitle: qsTr("No plan yet")
        emptyText: qsTr("Establishments can only subscribe once at least one plan is published.")
        onRowClicked: function(p) { if (ConsoleNav.can("billing")) editor.start(p) }
    }
    FbDialog {
        id: editor
        property var plan: null
        // Value of every capability for this plan: feature -> bool, limit -> number or null (unlimited).
        property var values: ({})
        title: plan ? qsTr("Edit plan %1").arg(plan.code) : qsTr("New plan")
        subtitle: plan ? qsTr("Code, currency and period cannot be changed once created.") : ""
        preferredWidth: 760
        function start(p) {
            plan = p
            code.text = p ? p.code : ""; nameFr.text = p ? p.name_fr : ""; nameEn.text = p ? p.name_en : ""
            descFr.text = p ? p.description_fr : ""; descEn.text = p ? p.description_en : ""
            period.value = p ? p.billing_period : "monthly"; currency.value = p ? p.currency : "USD"
            price.setValue(p ? p.price : ""); tax.text = p ? Fmt.percentText(p.tax_rate_bp) : "0"
            audience.value = p ? p.audience : "any"
            active.checked = p ? p.active : true; sort.text = p ? String(p.sort_order) : "0"
            tier.text = p ? String(p.tier) : "0"
            kind.value = p ? p.kind : "paid"; visibility.value = p ? p.visibility : "public"
            selfService.checked = p ? p.self_service : true; autoRenew.checked = p ? p.auto_renew_allowed : true
            grace.text = p && p.grace_days !== null ? String(p.grace_days) : ""
            reminders.text = p && p.reminder_days ? p.reminder_days.join(", ") : ""
            trialDays.text = p && p.trial_days !== null ? String(p.trial_days) : ""
            fallback.value = p && p.expiry_fallback_plan_id ? p.expiry_fallback_plan_id : ""
            archived.checked = p ? !!p.archived_at : false
            var v = {}
            page.catalog.forEach(function(c) {
                var has = p && p.entitlements && p.entitlements[c.key] !== undefined
                v[c.key] = has ? p.entitlements[c.key] : (c.kind === "feature" ? c.default_enabled : c.default_limit)
            })
            values = v
            open()
        }
        function setValue(k, x) { var v = Object.assign({}, values); v[k] = x; values = v }
        function limitText(k) { var x = values[k]; return x === null || x === undefined ? "" : String(x) }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(560, page.height * 0.7)
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: Theme.s3
                SectionTitle { text: qsTr("Identity") }
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
                    Field { label: qsTr("Kind"); FbComboBox { id: kind; model: [{ value: "paid", text: qsTr("Paid") }, { value: "trial", text: qsTr("Trial") }, { value: "free", text: qsTr("Free") }, { value: "custom", text: qsTr("Custom (negotiated)") }] } }
                    Field { label: qsTr("Visibility"); help: qsTr("Private plans are assigned from this console only.")
                            FbComboBox { id: visibility; model: [{ value: "public", text: qsTr("Public") }, { value: "private", text: qsTr("Private") }] } }
                }
                SectionTitle { text: qsTr("Price") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.s3; rowSpacing: Theme.s2
                    Field { label: qsTr("Billing period"); FbComboBox { id: period; enabled: !editor.plan; model: [{ value: "monthly", text: qsTr("Monthly") }, { value: "annual", text: qsTr("Annual") }] } }
                    Field { label: qsTr("Currency"); FbComboBox { id: currency; enabled: !editor.plan; model: Labels.currencies() } }
                    Field { label: qsTr("Price excl. tax"); MoneyField { id: price; currency: currency.value || "USD" } }
                    Field { label: qsTr("Tax rate (%)"); FbTextField { id: tax } }
                    Field { label: qsTr("Tier"); help: qsTr("Higher tier = upgrade (immediate, pro rata); lower = downgrade at period end"); FbTextField { id: tier; inputMethodHints: Qt.ImhDigitsOnly } }
                    Field { label: qsTr("Display order"); FbTextField { id: sort; inputMethodHints: Qt.ImhDigitsOnly } }
                }
                SectionTitle { text: qsTr("Conditions") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.s3; rowSpacing: Theme.s2
                    FbSwitch { id: selfService; text: qsTr("Can be chosen by the establishments (self-service)") }
                    FbSwitch { id: autoRenew; text: qsTr("Automatic renewal allowed (with consent)") }
                    Field { label: qsTr("Grace period (days)"); help: qsTr("Empty = platform setting. 0 to 90."); FbTextField { id: grace; inputMethodHints: Qt.ImhDigitsOnly } }
                    Field { label: qsTr("Reminders (days before the end)"); help: qsTr("e.g. 14, 7, 3, 1 — empty = platform setting"); FbTextField { id: reminders } }
                    Field { label: qsTr("Duration when assigned (days)"); help: qsTr("For trial plans and fallback plans. Empty = no fixed duration."); FbTextField { id: trialDays; inputMethodHints: Qt.ImhDigitsOnly } }
                    Field { label: qsTr("At expiry, move to"); help: qsTr("Free or custom plan used when the grace period ends, instead of read-only access.")
                            FbComboBox { id: fallback
                                         model: [{ value: "", text: qsTr("Read-only access (no fallback plan)") }].concat(page.rows.filter(function(p) {
                                             return (p.kind === "free" || p.kind === "custom") && (!editor.plan || p.id !== editor.plan.id) && !p.archived_at
                                         }).map(function(p) { return { value: p.id, text: p.code } })) } }
                }
                SectionTitle { text: qsTr("Features"); hint: qsTr("A feature that is off is invisible in the applications and refused by the server.") }
                Repeater {
                    model: page.catalog.filter(function(c) { return c.kind === "feature" })
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text { text: page.label(modelData); color: Theme.text; font.family: Theme.font; font.weight: Font.Medium }
                            Text { text: Prefs.language === "fr" ? modelData.description_fr : modelData.description_en; color: Theme.textMuted; font.family: Theme.font
                                   font.pixelSize: Theme.fs_sm; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        }
                        FbSwitch { checked: !!editor.values[modelData.key]; onToggled: editor.setValue(modelData.key, checked)
                                   Accessible.name: page.label(modelData) }
                    }
                }
                SectionTitle { text: qsTr("Limits"); hint: qsTr("Empty = unlimited") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.s3; rowSpacing: Theme.s2
                    Repeater {
                        model: page.catalog.filter(function(c) { return c.kind === "limit" })
                        delegate: Field {
                            label: page.label(modelData)
                            help: Prefs.language === "fr" ? modelData.description_fr : modelData.description_en
                            FbTextField {
                                text: editor.limitText(modelData.key)
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 0; top: 1000000 }
                                onEditingFinished: editor.setValue(modelData.key, text.trim() === "" ? null : parseInt(text))
                            }
                        }
                    }
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
                FbSwitch { id: archived; visible: !!editor.plan; text: qsTr("Archived (no new subscription; current ones continue)") }
            }
        }
        footer: [
            FbButton { text: qsTr("Save"); onClicked: {
                    var bp = Fmt.parsePercent(tax.text)
                    if (bp < 0 || !price.value) { ConsoleNav.toast(qsTr("Check the price and the tax rate."), "danger"); return }
                    var rem = reminders.text.trim() ? reminders.text.split(",").map(function(x) { return parseInt(x.trim()) }) : null
                    if (rem && rem.some(function(x) { return isNaN(x) })) { ConsoleNav.toast(qsTr("Reminders: numbers of days separated by commas."), "danger"); return }
                    var b = { name_fr: nameFr.text, name_en: nameEn.text, description_fr: descFr.text, description_en: descEn.text,
                              price: price.value, tax_rate_bp: bp, active: active.checked && !archived.checked, sort_order: parseInt(sort.text || "0"),
                              tier: parseInt(tier.text || "0"), kind: kind.value, visibility: visibility.value,
                              self_service: selfService.checked, auto_renew_allowed: autoRenew.checked,
                              grace_days: grace.text.trim() ? parseInt(grace.text) : null, reminder_days: rem,
                              trial_days: trialDays.text.trim() ? parseInt(trialDays.text) : null,
                              expiry_fallback_plan_id: fallback.value || null,
                              // every capability of the catalogue, so that the plan never depends on defaults
                              entitlements: editor.values }
                    if (editor.plan) b.archived = archived.checked
                    var cb = function(r) { if (r.ok) { editor.close(); page.reload(); ConsoleNav.toast(qsTr("Plan saved. Establishments of this plan get the new rights at their next request."), "success") } else ConsoleNav.fail(r) }
                    if (editor.plan) Api.send("PATCH", "/api/v1/admin/plans/" + editor.plan.id, b, cb)
                    else { b.code = code.text.trim(); b.billing_period = period.value; b.currency = currency.value; b.audience = audience.value
                           Api.send("POST", "/api/v1/admin/plans", b, cb) }
                } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: editor.close() }
        ]
    }
}
