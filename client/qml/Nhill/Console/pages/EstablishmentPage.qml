import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

FbPage {
    id: page
    property string establishmentId: ""
    property var d: null
    readonly property var e: d ? d.establishment : null
    title: e ? e.trade_name : qsTr("Establishment")
    subtitle: e ? e.id : ""
    scrollable: true
    property var stats: null
    function reload() { Api.get("/api/v1/admin/establishments/" + establishmentId, function(r) { if (r.ok) page.d = r.data; else ConsoleNav.fail(r) }) }
    function period() { return "?from=" + statsFrom.value + "&to=" + statsTo.value }
    function loadStats() {
        if (!(ConsoleNav.can("billing") || ConsoleNav.can("auditor"))) return
        Api.get("/api/v1/admin/establishments/" + establishmentId + "/stats" + period(), function(r) { if (r.ok) page.stats = r.data; else ConsoleNav.fail(r) })
    }
    function exportStats(fmt) {
        Api.download("/api/v1/admin/reports/establishments/" + establishmentId + period() + "&format=" + fmt, "nhill-etablissement." + fmt,
                     function(r) { if (r.ok) ConsoleNav.toast(qsTr("Saved to %1").arg(r.data), "success"); else ConsoleNav.fail(r) })
    }
    Component.onCompleted: {
        statsFrom.value = Fmt.addDays(Fmt.today(), -90); statsTo.value = Fmt.today(); reload(); loadStats()
        Api.get("/api/v1/admin/capabilities", function(r) { if (r.ok) page.capabilities = r.data.items })
    }
    property var capabilities: []   // catalogue of the capabilities (labels)
    function capLabel(k) {
        for (var i = 0; i < capabilities.length; ++i) if (capabilities[i].key === k) return Prefs.language === "fr" ? capabilities[i].label_fr : capabilities[i].label_en
        return k
    }
    function accessText(s) { return s === "full" ? qsTr("full") : s === "read_only" ? qsTr("read and export only") : qsTr("none") }
    actions: [
        FbButton { text: qsTr("Back"); variant: "ghost"; iconName: "close"; onClicked: ConsoleNav.shell.navigate("establishments") },
        FbButton { visible: !!page.e && ConsoleNav.can("billing"); text: qsTr("Subscription…"); iconName: "receipt"; variant: "secondary"; onClicked: subDialog.start() },
        FbButton { visible: !!page.e && ConsoleNav.can("support"); text: qsTr("Reissue the code"); iconName: "refresh"; variant: "secondary"; onClicked: reasonDialog.ask("reissue") },
        FbButton { visible: !!page.e && ConsoleNav.can("support"); text: page.e && page.e.status === "suspended" ? qsTr("Reactivate") : qsTr("Suspend")
                   iconName: page.e && page.e.status === "suspended" ? "check" : "alert"; variant: page.e && page.e.status === "suspended" ? "primary" : "danger"
                   onClicked: reasonDialog.ask(page.e.status === "suspended" ? "reactivate" : "suspend") }
    ]
    Banner { Layout.fillWidth: true; visible: !!page.e && page.e.status === "suspended"; tone: "danger"; text: page.e ? qsTr("Suspended: %1").arg(page.e.status_reason || "—") : "" }
    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: Theme.s3; rowSpacing: Theme.s3
        FbCard {
            Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
            ColumnLayout {
                width: parent.width
                spacing: Theme.s2
                Repeater {
                    model: page.e ? [
                        [qsTr("Legal name"), page.e.legal_name || "—"], [qsTr("Type"), page.e.kind === "individual" ? qsTr("Individual") : qsTr("Company")],
                        [qsTr("Country / profile"), page.e.country_code + " · " + page.e.profile], [qsTr("City"), page.e.city || "—"],
                        [qsTr("E-mail"), page.e.email || "—"], [qsTr("Phone"), page.e.phone || "—"],
                        [qsTr("VAT registered"), page.e.vat_registered ? qsTr("Yes") : qsTr("No")], [qsTr("Active users"), String(page.e.members)],
                        [qsTr("Created"), Fmt.dateTime(page.e.created_at)],
                        [qsTr("Documents issued (30 days)"), String(page.d.usage_30_days.documents_issued)],
                        [qsTr("Payments recorded (30 days)"), String(page.d.usage_30_days.payments_recorded)]] : []
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Text { text: modelData[0]; color: Theme.textMuted; font.family: Theme.font; Layout.preferredWidth: 220 }
                        Text { text: modelData[1]; color: Theme.text; font.family: Theme.font; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                }
            }
        }
        FbCard {
            Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
            ColumnLayout {
                width: parent.width
                spacing: Theme.s2
                SectionTitle { text: qsTr("Subscription") }
                RowLayout {
                    Text { text: page.d && page.d.subscription.plan ? page.d.subscription.plan.code : qsTr("Trial"); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold }
                    FbChip { visible: !!page.d; text: page.d ? Labels.subscription(page.d.subscription.status).text : ""; tone: page.d ? Labels.subscription(page.d.subscription.status).tone : "neutral" }
                }
                Text { visible: !!page.d; color: Theme.textMuted; font.family: Theme.font
                       text: page.d ? Fmt.date(page.d.subscription.period_start) + " → " + Fmt.date(page.d.subscription.period_end) + (page.d.subscription.grace_until ? " · " + qsTr("grace until %1").arg(Fmt.date(page.d.subscription.grace_until)) : "") : "" }
                Text { visible: !!page.d && !!page.d.subscription.price; color: Theme.textMuted; font.family: Theme.font
                       text: page.d && page.d.subscription.price ? Fmt.money(page.d.subscription.price.price, page.d.subscription.price.currency) + " / " + Labels.duration(page.d.subscription.price.duration_months) : "" }
                Banner { Layout.fillWidth: true; visible: !!page.d && !!page.d.subscription.pending_change; tone: "info"
                         text: page.d && page.d.subscription.pending_change ? qsTr("Change to %1 on %2 (paid)").arg(page.d.subscription.pending_change.plan_code).arg(Fmt.date(page.d.subscription.pending_change.effective_at)) : "" }
                Banner { Layout.fillWidth: true; visible: !!page.d && page.d.subscription.failed_payments > 0; tone: "warning"
                         text: page.d ? qsTr("%1 failed payment attempt(s) since the last successful payment.").arg(page.d.subscription.failed_payments) : "" }
                // Access computed by the server from the dates (distinct from the status and from payments).
                Text { visible: !!page.d && !!page.d.subscription.access; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                       text: page.d && page.d.subscription.access ? qsTr("Access: %1 (%2)").arg(page.accessText(page.d.subscription.access.state)).arg(page.d.subscription.access.reason) : "" }
                SectionTitle { visible: !!page.d && !!page.d.entitlements; text: qsTr("Rights of the plan"); Layout.topMargin: Theme.s2 }
                Flow {
                    visible: !!page.d && !!page.d.entitlements
                    Layout.fillWidth: true
                    spacing: Theme.s2
                    Repeater {
                        model: page.d && page.d.entitlements ? page.d.entitlements.features : []
                        delegate: FbChip { tone: "success"; text: page.capLabel(modelData) }
                    }
                }
                Repeater {
                    model: page.d && page.d.entitlements ? Object.keys(page.d.entitlements.limits) : []
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        property var lim: page.d.entitlements.limits[modelData]
                        Text { text: page.capLabel(modelData); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm; Layout.fillWidth: true }
                        Text { text: lim.max === null ? qsTr("%1 · unlimited").arg(lim.used) : qsTr("%1 / %2").arg(lim.used).arg(lim.max)
                               color: lim.max !== null && lim.used >= lim.max ? Theme.warning : Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                    }
                }
                SectionTitle { text: qsTr("Subscription history"); Layout.topMargin: Theme.s2 }
                Repeater {
                    model: page.d ? page.d.subscription_events : []
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                           text: Fmt.dateTime(modelData.at) + " · " + Labels.subscriptionEvent(modelData.kind) + (modelData.to ? " → " + Labels.subscription(modelData.to).text : "")
                                 + (modelData.details && modelData.details.plan ? " · " + modelData.details.plan : "")
                                 + " (" + (modelData.actor === "user" ? qsTr("establishment") : modelData.actor === "admin" ? (modelData.admin_name || qsTr("Nhill team")) : qsTr("automatic")) + ")"
                                 + (modelData.details && modelData.details.reason ? " — " + modelData.details.reason : "") }
                }
            }
        }
    }
    // ---- aggregated statistics (billing / auditor only; every consultation is audited)
    FbCard {
        Layout.fillWidth: true
        visible: ConsoleNav.can("billing") || ConsoleNav.can("auditor")
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            RowLayout {
                Layout.fillWidth: true
                SectionTitle { text: qsTr("Activity statistics"); Layout.fillWidth: true }
                DateField { id: statsFrom; Layout.preferredWidth: 170 }
                DateField { id: statsTo; Layout.preferredWidth: 170 }
                FbButton { text: qsTr("Show"); iconName: "chart"; onClicked: page.loadStats() }
                FbButton { text: "CSV"; iconName: "download"; variant: "secondary"; onClicked: page.exportStats("csv") }
                FbButton { text: "PDF"; iconName: "download"; variant: "secondary"; onClicked: page.exportStats("pdf") }
            }
            Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: qsTr("Aggregates computed by the database (counts and sums per currency, never converted). Customers, products and document contents are not accessible. This consultation is recorded and visible to the establishment.") }
            GridLayout {
                Layout.fillWidth: true
                visible: !!page.stats
                columns: 4
                columnSpacing: Theme.s3; rowSpacing: Theme.s3
                KpiTile { Layout.fillWidth: true; label: qsTr("Users active / sign-ins"); iconName: "users"
                          value: page.stats ? page.stats.users.active_users + " / " + page.stats.users.sign_ins : ""
                          caption: page.stats ? qsTr("%1 actions · last activity %2").arg(page.stats.users.actions).arg(page.stats.users.last_activity ? Fmt.dateTime(page.stats.users.last_activity) : "—") : "" }
                KpiTile { Layout.fillWidth: true; label: qsTr("Catalogue"); iconName: "box"
                          value: page.stats ? qsTr("%1 products").arg(page.stats.counts.products) : ""
                          caption: page.stats ? qsTr("%1 customers · %2 suppliers").arg(page.stats.counts.customers).arg(page.stats.counts.suppliers) : "" }
                KpiTile { Layout.fillWidth: true; label: qsTr("Documents (all time)"); iconName: "invoice"; tone: "accent"
                          value: page.stats ? String(page.stats.counts.documents) : ""
                          caption: page.stats ? qsTr("%1 drafts").arg(page.stats.counts.drafts) : "" }
                KpiTile { Layout.fillWidth: true; label: qsTr("Roles"); iconName: "shield"; tone: "warning"
                          value: page.stats ? String(Object.keys(page.stats.users.active_by_role).reduce(function(t, k) { return t + page.stats.users.active_by_role[k] }, 0)) : ""
                          caption: page.stats ? Object.keys(page.stats.users.active_by_role).map(function(k) { return Labels.role(k) + " " + page.stats.users.active_by_role[k] }).join(" · ") : "" }
            }
            DataTable {
                Layout.fillWidth: true
                Layout.preferredHeight: 64 + Math.max(1, page.stats ? page.stats.volumes.length : 1) * 44
                rows: page.stats ? page.stats.volumes : []
                columns: [
                    { title: qsTr("Currency"), width: 0.7, strong: true, value: function(v) { return v.currency } },
                    { title: qsTr("Invoices"), width: 0.8, align: "right", value: function(v) { return String(v.invoices_issued) } },
                    { title: qsTr("Invoiced excl. tax"), width: 1.4, align: "right", value: function(v) { return Fmt.amount(v.invoiced_net, v.currency) } },
                    { title: qsTr("Invoiced incl. tax"), width: 1.4, align: "right", value: function(v) { return Fmt.amount(v.invoiced_total, v.currency) } },
                    { title: qsTr("Credit notes"), width: 1.2, align: "right", value: function(v) { return Fmt.amount(v.credited_total, v.currency) } },
                    { title: qsTr("Received"), width: 1.4, align: "right", value: function(v) { return Fmt.amount(v.payments_in, v.currency) } },
                    { title: qsTr("Expenses"), width: 1.3, align: "right", value: function(v) { return Fmt.amount(v.expenses, v.currency) } },
                    { title: qsTr("Purchases"), width: 1.3, align: "right", value: function(v) { return Fmt.amount(v.purchases, v.currency) } }
                ]
                emptyIcon: "chart"
                emptyTitle: qsTr("No activity in this period")
            }
            SectionTitle { text: qsTr("Per month"); visible: !!page.stats && page.stats.monthly.length > 0 }
            DataTable {
                visible: !!page.stats && page.stats.monthly.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 64 + (page.stats ? page.stats.monthly.length : 0) * 44
                rows: page.stats ? page.stats.monthly : []
                columns: [
                    { title: qsTr("Month"), width: 0.9, mono: true, value: function(m) { return m.month } },
                    { title: qsTr("Currency"), width: 0.7, value: function(m) { return m.currency } },
                    { title: qsTr("Invoices"), width: 0.8, align: "right", value: function(m) { return String(m.invoices_issued) } },
                    { title: qsTr("Invoiced incl. tax"), width: 1.4, align: "right", value: function(m) { return Fmt.amount(m.invoiced_total, m.currency) } },
                    { title: qsTr("Credit notes"), width: 1.2, align: "right", value: function(m) { return Fmt.amount(m.credited_total, m.currency) } },
                    { title: qsTr("Received"), width: 1.4, align: "right", value: function(m) { return Fmt.amount(m.payments_in, m.currency) } },
                    { title: qsTr("Expenses"), width: 1.3, align: "right", value: function(m) { return Fmt.amount(m.expenses, m.currency) } }
                ]
            }
        }
    }
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            SectionTitle { text: qsTr("Establishment codes") }
            Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: qsTr("Codes are never displayed in this console: after a reissue the owner reads the new code in the application.") }
            Repeater {
                model: page.d ? page.d.codes : []
                RowLayout {
                    Layout.fillWidth: true
                    FbChip { text: modelData.status; tone: modelData.status === "active" ? "success" : "neutral" }
                    Text { Layout.fillWidth: true; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                           text: qsTr("Created %1 by %2 · expires %3").arg(Fmt.dateTime(modelData.created_at)).arg(modelData.created_by).arg(Fmt.dateTime(modelData.expires_at)) + (modelData.ended_reason ? " · " + modelData.ended_reason : "") }
                }
            }
        }
    }
    FbDialog {
        id: reasonDialog
        property string action: ""
        title: action === "suspend" ? qsTr("Suspend this establishment") : action === "reactivate" ? qsTr("Reactivate this establishment") : qsTr("Reissue the establishment code")
        subtitle: action === "suspend" ? qsTr("All its users are signed out immediately. Their data is kept.") : action === "reissue" ? qsTr("The current code stops working. Connected users are not affected.") : ""
        function ask(a) { action = a; why.text = ""; open() }
        Field { label: qsTr("Reason (recorded in the audit log)"); required: true; FbTextArea { id: why } }
        footer: [
            FbButton { text: qsTr("Confirm"); variant: reasonDialog.action === "suspend" ? "danger" : "primary"; onClicked: {
                    var path = "/api/v1/admin/establishments/" + page.establishmentId + (reasonDialog.action === "reissue" ? "/codes/reissue" : "/" + reasonDialog.action)
                    Api.send("POST", path, { reason: why.text.trim() }, function(r) { if (r.ok) { reasonDialog.close(); ConsoleNav.toast(qsTr("Done"), "success"); page.reload() } else ConsoleNav.fail(r) })
                } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: reasonDialog.close() }
        ]
    }
    FbDialog {
        id: subDialog
        property var prices: []
        title: qsTr("Subscription actions")
        subtitle: qsTr("Every action needs a reason and is recorded; the establishment is notified.")
        preferredWidth: 600
        property var plans: []
        function start() {
            action.value = "extend"; days.text = "30"; months.text = "0"; reason.text = ""
            assignDays.text = "30"; assignMonths.text = "0"; openEnded.checked = false; assignMode.value = "active"
            newEnd.value = ""; newGrace.value = ""
            Api.get("/api/v1/admin/plans", function(r) {
                if (!r.ok) return
                var out = [], plans = []
                r.data.items.forEach(function(p) {
                    if (!p.archived_at) plans.push({ value: p.id, text: p.code + " · " + (Prefs.language === "fr" ? p.name_fr : p.name_en) + " (" + p.kind + ")", kind: p.kind })
                    p.prices.forEach(function(x) { if (x.active) out.push({ value: x.id, text: p.code + " · " + Labels.duration(x.duration_months) + " · " + Fmt.money(x.price, x.currency) }) })
                })
                subDialog.prices = out
                subDialog.plans = plans
            })
            open()
        }
        function planKind(id) { for (var i = 0; i < plans.length; ++i) if (plans[i].value === id) return plans[i].kind; return "" }
        Field { label: qsTr("Action")
                FbComboBox { id: action; Layout.fillWidth: true
                             model: [{ value: "extend", text: qsTr("Extend (free days / months)") }, { value: "order", text: qsTr("Create a payment request") },
                                     { value: "assign_plan", text: qsTr("Assign a plan (trial, period or no end date)") },
                                     { value: "set_plan", text: qsTr("Change the plan (no charge)") }, { value: "set_dates", text: qsTr("Correct the dates") },
                                     { value: "suspend", text: qsTr("Suspend (read only)") },
                                     { value: "reactivate", text: qsTr("Reactivate") }, { value: "cancel", text: qsTr("Cancel the subscription") }] } }
        ColumnLayout {
            visible: action.value === "assign_plan"
            Layout.fillWidth: true
            spacing: Theme.s2
            Field { label: qsTr("Plan"); FbComboBox { id: assignPlan; Layout.fillWidth: true; model: subDialog.plans } }
            Field { label: qsTr("As"); FbComboBox { id: assignMode; model: [{ value: "active", text: qsTr("Active period") }, { value: "trial", text: qsTr("Trial") }] } }
            FbSwitch { id: openEnded; visible: subDialog.planKind(assignPlan.value) === "free" || subDialog.planKind(assignPlan.value) === "custom"
                       text: qsTr("No end date (free or custom plans only)") }
            RowLayout {
                visible: !openEnded.checked || !openEnded.visible
                Field { label: qsTr("Days"); FbTextField { id: assignDays; inputMethodHints: Qt.ImhDigitsOnly } }
                Field { label: qsTr("Months"); FbTextField { id: assignMonths; inputMethodHints: Qt.ImhDigitsOnly } }
            }
            Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: qsTr("An assigned plan is not a payment: no order is created and pending payment requests are cancelled. The rights of the establishment change at its next request.") }
        }
        RowLayout {
            visible: action.value === "set_dates"
            Field { label: qsTr("End of the period"); DateField { id: newEnd } }
            Field { label: qsTr("End of the grace period (optional)"); DateField { id: newGrace } }
        }
        RowLayout {
            visible: action.value === "extend"
            Field { label: qsTr("Days"); FbTextField { id: days; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("Months"); FbTextField { id: months; inputMethodHints: Qt.ImhDigitsOnly } }
        }
        Field { visible: action.value === "set_plan" || action.value === "order"; label: qsTr("Plan and price")
                FbComboBox { id: price; Layout.fillWidth: true; model: subDialog.prices } }
        Field { visible: action.value === "order"; label: qsTr("Promo code (optional)"); FbTextField { id: promo; monospace: true } }
        Field { visible: action.value !== "order"; label: qsTr("Reason (recorded in the audit log)"); required: true; FbTextArea { id: reason } }
        footer: [
            FbButton { text: qsTr("Apply"); variant: action.value === "suspend" || action.value === "cancel" ? "danger" : "primary"; onClicked: {
                    var done = function(r) { if (r.ok) { subDialog.close(); ConsoleNav.toast(qsTr("Done"), "success"); page.reload() } else ConsoleNav.fail(r) }
                    if (action.value === "order") {
                        var b = { price_id: price.value }
                        if (promo.text.trim()) b.promo_code = promo.text.trim()
                        Api.send("POST", "/api/v1/admin/establishments/" + page.establishmentId + "/orders", b, done)
                        return
                    }
                    var body = { action: action.value, reason: reason.text.trim() }
                    if (action.value === "extend") { body.days = parseInt(days.text || "0"); body.months = parseInt(months.text || "0") }
                    if (action.value === "set_plan") body.price_id = price.value
                    if (action.value === "assign_plan") {
                        body.plan_id = assignPlan.value; body.mode = assignMode.value
                        if (openEnded.visible && openEnded.checked) body.open_ended = true
                        else { body.days = parseInt(assignDays.text || "0"); body.months = parseInt(assignMonths.text || "0") }
                    }
                    if (action.value === "set_dates") { body.period_end = newEnd.value; if (newGrace.value) body.grace_until = newGrace.value }
                    Api.send("POST", "/api/v1/admin/establishments/" + page.establishmentId + "/subscription", body, done)
                } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: subDialog.close() }
        ]
    }
}
