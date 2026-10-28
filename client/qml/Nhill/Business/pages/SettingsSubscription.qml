import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

// Plans and subscription. Every amount shown before paying comes from the server
// quote (pro-rata credit, promo code, tax); nothing is computed here.
ColumnLayout {
    id: root
    spacing: Theme.s4
    property var sub: null
    property var orders: []
    property var history: []
    property var plans: []
    property int graceDays: 7
    property int duration: 1
    property string currency: ""
    readonly property bool canManage: Session.can("subscription.manage")
    readonly property var durations: {
        var set = {}
        plans.forEach(function(p) { p.prices.forEach(function(x) { set[x.duration_months] = true }) })
        return Object.keys(set).map(Number).sort(function(a, b) { return a - b })
    }
    readonly property var currencies: {
        var set = {}
        plans.forEach(function(p) { p.prices.forEach(function(x) { set[x.currency] = true }) })
        return Object.keys(set).sort()
    }
    property var rights: null   // GET /entitlements: features of the plan, limits and usage
    function load() {
        Api.get("/api/v1/entitlements", function(r) { if (r.ok) root.rights = r.data })
        Api.get("/api/v1/subscription", function(r) {
            if (!r.ok) { Nav.fail(r); return }
            root.sub = r.data.subscription; root.orders = r.data.orders; root.history = r.data.history; root.graceDays = r.data.grace_days || 7
        })
        Api.get("/api/v1/plans", function(r) {
            if (!r.ok) return
            root.plans = r.data.items.filter(function(p) { return p.audience === "any" || p.audience === Session.establishment.kind })
            if (!root.currency) root.currency = root.currencies.indexOf(Session.establishment.main_currency) >= 0 ? Session.establishment.main_currency : (root.currencies[0] || "")
            if (root.durations.length && root.durations.indexOf(root.duration) < 0) root.duration = root.durations[0]
        })
    }
    function planName(p) { return Prefs.language === "fr" ? p.name_fr : p.name_en }
    function priceOf(p) {
        for (var i = 0; i < p.prices.length; ++i) if (p.prices[i].duration_months === root.duration && p.prices[i].currency === root.currency) return p.prices[i]
        return null
    }
    function actionText(p) {
        if (!root.sub || !root.sub.plan || root.sub.is_trial || (root.sub.status !== "active" && root.sub.status !== "past_due")) return qsTr("Choose this plan")
        if (p.id === root.sub.plan.id) return qsTr("Renew")
        return p.tier > root.sub.plan.tier ? qsTr("Upgrade") : p.tier < root.sub.plan.tier ? qsTr("Downgrade") : qsTr("Switch")
    }
    Component.onCompleted: load()

    // ---------------------------------------------------------------- current subscription
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.s2
            SectionTitle { text: qsTr("Current subscription") }
            RowLayout {
                spacing: Theme.s3
                Text { text: root.sub && root.sub.plan ? root.planName(root.sub.plan) : (root.sub && root.sub.is_trial ? qsTr("Free trial") : "—")
                       color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_xl; font.weight: Font.DemiBold }
                FbChip { visible: !!root.sub; text: root.sub ? Labels.subscription(root.sub.status).text : ""; tone: root.sub ? Labels.subscription(root.sub.status).tone : "neutral" }
                FbChip { visible: !!root.sub && !!root.sub.price; text: root.sub && root.sub.price ? Fmt.money(root.sub.price.price, root.sub.price.currency) + " / " + Labels.duration(root.sub.price.duration_months) : ""; tone: "neutral" }
            }
            Text { visible: !!root.sub; color: Theme.textMuted; font.family: Theme.font
                   text: !root.sub ? "" : root.sub.open_ended ? qsTr("Since %1 · no end date").arg(Fmt.date(root.sub.period_start))
                         : qsTr("Period: %1 → %2 · %3 day(s) left").arg(Fmt.date(root.sub.period_start)).arg(Fmt.date(root.sub.period_end)).arg(Math.max(0, root.sub.days_left)) }
            Banner { Layout.fillWidth: true; visible: !!root.sub && !!root.sub.grace_until; tone: "warning"
                     text: root.sub && root.sub.grace_until ? qsTr("Payment overdue. Grace period until %1: after that date the establishment switches to read-only (consultation and export remain possible).").arg(Fmt.date(root.sub.grace_until)) : "" }
            Banner { Layout.fillWidth: true; visible: !!root.sub && !!root.sub.pending_change; tone: "info"
                     text: root.sub && root.sub.pending_change ? qsTr("Change to %1 on %2 (already paid).").arg(Prefs.language === "fr" ? root.sub.pending_change.name_fr : root.sub.pending_change.name_en).arg(Fmt.date(root.sub.pending_change.effective_at)) : "" }
            Banner { Layout.fillWidth: true; visible: !!root.sub && root.sub.failed_payments > 0; tone: "danger"
                     text: root.sub ? qsTr("%1 payment attempt(s) failed. Try again with another payment method; the subscription is not interrupted before the end of the grace period (%2 days after the due date).").arg(root.sub.failed_payments).arg(root.graceDays) : "" }
            FbSwitch {
                visible: root.canManage && !!root.sub && !root.sub.is_trial
                text: qsTr("I agree to automatic renewal when a payment provider supporting it is connected")
                checked: !!root.sub && root.sub.auto_renew_consent
                onToggled: Api.send("POST", "/api/v1/subscription/auto-renew", { consent: checked }, function(r) { if (!r.ok) Nav.fail(r); root.load() })
            }
        }
    }

    // ---------------------------------------------------------------- content of the plan
    // What the plan includes, with the usage of its limits (nothing about what it does not include).
    FbCard {
        Layout.fillWidth: true
        visible: !!root.rights
        ColumnLayout {
            width: parent.width
            spacing: Theme.s2
            SectionTitle { text: qsTr("Included in your plan") }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.s2
                Repeater {
                    model: root.rights ? root.rights.features : []
                    delegate: FbChip {
                        tone: "success"
                        property var l: root.rights.labels[modelData] || {}
                        text: (Prefs.language === "fr" ? l.label_fr : l.label_en) || modelData
                    }
                }
            }
            Repeater {
                model: root.rights ? Object.keys(root.rights.limits) : []
                delegate: RowLayout {
                    Layout.fillWidth: true
                    property var lim: root.rights.limits[modelData]
                    property var l: root.rights.labels[modelData] || {}
                    Text { text: (Prefs.language === "fr" ? l.label_fr : l.label_en) || modelData; color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
                    Text {
                        color: lim.max !== null && lim.used >= lim.max ? Theme.warning : Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                        text: lim.max === null ? qsTr("%1 used · unlimited").arg(lim.used) : qsTr("%1 of %2 used").arg(lim.used).arg(lim.max)
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------------- plans
    RowLayout {
        Layout.fillWidth: true
        visible: root.plans.length > 0
        SectionTitle { text: qsTr("Plans"); Layout.fillWidth: true }
        TabStrip { visible: root.durations.length > 1; model: root.durations.map(function(m) { return { value: m, text: Labels.duration(m) } })
                   current: root.duration; onSelected: function(v) { root.duration = v } }
        FbComboBox { visible: root.currencies.length > 1; Layout.preferredWidth: 110; model: root.currencies.map(function(c) { return { value: c, text: c } })
                     value: root.currency; onActivated: root.currency = value }
    }
    Text {
        visible: root.plans.length === 0
        Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font
        text: qsTr("No paid offer has been published by the Nhill operator yet. Prices are set by the operator in Nhill Console.")
    }
    Flow {
        Layout.fillWidth: true
        spacing: Theme.s3
        Repeater {
            model: root.plans
            FbCard {
                id: card
                readonly property var price: root.priceOf(modelData)
                readonly property bool current: !!root.sub && !!root.sub.plan && root.sub.plan.id === modelData.id
                width: Theme.compact ? parent.width : 300
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s2
                    RowLayout {
                        Text { text: root.planName(modelData); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold; Layout.fillWidth: true }
                        FbChip { visible: card.current; text: qsTr("Current"); tone: "success" }
                    }
                    Text { text: card.price ? Fmt.money(card.price.price, card.price.currency) + " / " + Labels.duration(root.duration) : qsTr("Not offered for this duration")
                           color: card.price ? Theme.primary : Theme.textFaint; font.family: Theme.font; font.pixelSize: card.price ? Theme.fs_xl : Theme.fs_sm; font.weight: Font.Bold }
                    Text { visible: modelData.tax_rate_bp > 0; text: qsTr("excl. tax (%1)").arg(Fmt.percent(modelData.tax_rate_bp)); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                           text: Prefs.language === "fr" ? modelData.description_fr : modelData.description_en; visible: text.length > 0 }
                    Repeater {
                        model: [qsTr("Users: %1").arg(modelData.max_users ? modelData.max_users : qsTr("unlimited")),
                                qsTr("POS registers: %1").arg(modelData.max_registers === null ? qsTr("unlimited") : modelData.max_registers),
                                qsTr("Warehouses: %1").arg(modelData.max_warehouses === null ? qsTr("unlimited") : modelData.max_warehouses)]
                                .concat(Object.keys(modelData.features).filter(function(k) { return modelData.features[k] === true }).map(Labels.feature))
                        RowLayout {
                            spacing: Theme.s2
                            Icon { name: "check"; size: 14; color: Theme.success }
                            Text { text: modelData; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_sm; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        }
                    }
                    FbButton { visible: root.canManage; enabled: !!card.price && !(root.sub && root.sub.pending_change); Layout.fillWidth: true
                               variant: card.current ? "secondary" : "primary"; text: root.actionText(modelData)
                               onClicked: quote.start(modelData, card.price) }
                }
            }
        }
    }

    // ---------------------------------------------------------------- orders & history
    SectionTitle { text: qsTr("Payments"); visible: root.orders.length > 0 }
    DataTable {
        visible: root.orders.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(420, 64 + root.orders.length * 52)
        rows: root.orders
        titleColumn: 0; subtitleColumn: 1; trailingColumn: 3; chipColumn: 4
        columns: [
            { title: qsTr("Plan"), width: 1.4, strong: true, value: function(o) { return (Prefs.language === "fr" ? o.plan_name_fr : o.plan_name_en) + (o.duration_months ? " · " + Labels.duration(o.duration_months) : "") } },
            { title: qsTr("Date"), width: 1.2, muted: true, value: function(o) { return Fmt.dateTime(o.created_at) } },
            { title: qsTr("Type"), width: 0.9, muted: true, value: function(o) { return Labels.orderKind(o.kind) + (o.promo_code ? " · " + o.promo_code : "") } },
            { title: qsTr("Amount"), width: 1.1, align: "right", value: function(o) { return Fmt.money(o.amount, o.currency) } },
            { title: qsTr("Status"), width: 1, align: "right", chip: function(o) { return Labels.orderStatus(o.status) } }
        ]
        onRowClicked: function(o) { if (o.status === "pending" && root.canManage) pay.start(o) }
    }
    SectionTitle { text: qsTr("History"); visible: root.history.length > 0 }
    Repeater {
        model: root.history
        Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
               text: Fmt.dateTime(modelData.at) + " — " + Labels.subscriptionEvent(modelData.kind) + (modelData.to && modelData.from !== modelData.to ? " (" + Labels.subscription(modelData.from).text + " → " + Labels.subscription(modelData.to).text + ")" : "") }
    }

    // ---------------------------------------------------------------- quote → order → checkout
    FbDialog {
        id: quote
        property var plan: null
        property var price: null
        property var q: null
        property string provider: ""
        property bool busy: false
        preferredWidth: 560
        title: plan ? root.planName(plan) + " · " + Labels.duration(price.duration_months) : ""
        subtitle: q ? Labels.orderKind(q.kind) : ""
        function start(p, pr) { plan = p; price = pr; promo.text = ""; q = null; provider = ""; refresh(); open() }
        function refresh() {
            busy = true
            var b = { price_id: price.id }
            if (promo.text.trim()) b.promo_code = promo.text.trim()
            Api.send("POST", "/api/v1/subscription/quote", b, function(r) {
                quote.busy = false
                if (r.ok) { quote.q = r.data; if (!quote.provider && r.data.payment_providers.length) quote.provider = r.data.payment_providers[0].code }
                else { Nav.fail(r); if (promo.text.trim() && r.error && r.error.code.indexOf("promo_") === 0) { promo.text = ""; quote.refresh() } }
            })
        }
        Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
               text: !quote.q ? "" : quote.q.kind === "upgrade" ? qsTr("Immediate change. The unused part of your current period is deducted (pro rata) and a new period starts today.")
                    : quote.q.kind === "downgrade" ? qsTr("Paid now, effective at the end of your current period (%1). Your current plan stays until then.").arg(Fmt.date(quote.q.starts_at))
                    : quote.q.kind === "renewal" ? qsTr("The new period starts at the end of the current one (%1): no day is lost.").arg(Fmt.date(quote.q.starts_at))
                    : qsTr("The plan starts as soon as the payment is confirmed%1.").arg(quote.q.starts_at !== "now" ? " " + qsTr("(after your trial, on %1)").arg(Fmt.date(quote.q.starts_at)) : "") }
        Repeater {
            model: quote.q ? [[qsTr("Price"), quote.q.subtotal], [qsTr("Promo code") + (quote.q.promo ? " " + quote.q.promo.code : ""), "-" + quote.q.discount],
                              [qsTr("Credit for unused days"), "-" + quote.q.credit], [qsTr("Tax"), quote.q.tax]].filter(function(x) { return x[1] !== "-0.00" && x[1] !== "-0" })
                           : []
            delegate: RowLayout {
                Layout.fillWidth: true
                Text { text: modelData[0]; color: Theme.textMuted; font.family: Theme.font; Layout.fillWidth: true }
                Text { text: modelData[1].charAt(0) === "-" ? "− " + Fmt.money(modelData[1].slice(1), quote.q.currency) : Fmt.money(modelData[1], quote.q.currency); color: Theme.text; font.family: Theme.font }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: !!quote.q
            Text { text: qsTr("Total to pay"); color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold; Layout.fillWidth: true }
            Text { text: quote.q ? Fmt.money(quote.q.total, quote.q.currency) : ""; color: Theme.primary; font.family: Theme.font; font.pixelSize: Theme.fs_xl; font.weight: Font.Bold }
        }
        RowLayout {
            Layout.fillWidth: true
            FbTextField { id: promo; Layout.fillWidth: true; placeholderText: qsTr("Promo code"); monospace: true; onAccepted: quote.refresh() }
            FbButton { text: qsTr("Apply"); variant: "secondary"; enabled: promo.text.trim().length > 0; busy: quote.busy; onClicked: quote.refresh() }
        }
        SectionTitle { text: qsTr("Payment method"); visible: !!quote.q }
        Repeater {
            model: quote.q ? quote.q.payment_providers.concat([{ code: "", name: qsTr("Bank transfer / Mobile Money (confirmed by the Nhill team)") }]) : []
            delegate: RadioButton {
                text: modelData.name + (modelData.environment === "test" ? " — " + qsTr("test mode") : "")
                checked: quote.provider === modelData.code
                font.family: Theme.font
                onClicked: quote.provider = modelData.code
            }
        }
        footer: [
            FbButton { text: quote.q && quote.q.total === "0.00" ? qsTr("Confirm") : quote.provider ? qsTr("Continue to payment") : qsTr("Create the payment request")
                       enabled: !!quote.q && !quote.busy; busy: quote.busy
                       onClicked: {
                           quote.busy = true
                           var b = { price_id: quote.price.id }
                           if (quote.q.promo) b.promo_code = quote.q.promo.code
                           if (quote.provider) b.provider = quote.provider
                           Api.send("POST", "/api/v1/subscription/orders", b, function(r) {
                               quote.busy = false
                               if (!r.ok) { Nav.fail(r); return }
                               quote.close(); root.load()
                               pay.after(r.data)
                           })
                       } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: quote.close() }
        ]
    }
    FbDialog {
        id: pay
        property var order: null
        property var data: null
        title: order ? (order.status === "paid" ? qsTr("Subscription updated") : qsTr("Payment request")) : ""
        subtitle: order ? Fmt.money(order.amount, order.currency) + " · " + qsTr("reference %1").arg(order.id.slice(0, 8).toUpperCase()) : ""
        function start(o) { order = o; data = null; open() }
        function after(d) {
            order = d.order; data = d
            if (d.checkout && d.checkout.checkout_url) Qt.openUrlExternally(d.checkout.checkout_url)
            open()
        }
        Banner { Layout.fillWidth: true; visible: !!pay.data && !!pay.data.checkout && !!pay.data.checkout.error; tone: "danger"
                 text: qsTr("The payment page could not be opened (%1). The request is kept: try again or choose another method.").arg(pay.data && pay.data.checkout ? Errors.code(pay.data.checkout.error) : "") }
        Text {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.text; font.family: Theme.font
            text: !pay.order ? "" : pay.order.status === "paid" ? qsTr("Nothing to pay: the change is applied.")
                 : pay.order.checkout_url ? qsTr("The payment page has opened in your browser. Your subscription is updated as soon as the payment provider confirms the payment (this window can be closed).")
                 : qsTr("Nothing has been charged. Your subscription is updated once the payment is confirmed by the Nhill billing team. Mention the reference above with your transfer.")
        }
        Text {
            visible: !!pay.data && !!pay.data.payment_instructions && pay.order && !pay.order.checkout_url && pay.order.status !== "paid"
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font
            text: pay.data && pay.data.payment_instructions ? String(Prefs.language === "fr" ? (pay.data.payment_instructions.fr || "") : (pay.data.payment_instructions.en || "")) : ""
        }
        Text { visible: !!pay.order && !!pay.order.expires_at && pay.order.status === "pending"; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
               text: pay.order && pay.order.expires_at ? qsTr("Valid until %1.").arg(Fmt.dateTime(pay.order.expires_at)) : "" }
        footer: [
            FbButton { visible: !!pay.order && pay.order.status === "pending" && !!pay.order.checkout_url; text: qsTr("Open the payment page"); iconName: "globe"
                       onClicked: Qt.openUrlExternally(pay.order.checkout_url) },
            FbButton { visible: !!pay.order && pay.order.status === "pending"; text: qsTr("Cancel this request"); variant: "ghost"
                       onClicked: Api.send("POST", "/api/v1/subscription/orders/" + pay.order.id + "/cancel", {}, function(r) { if (r.ok) { pay.close(); root.load() } else Nav.fail(r) }) },
            FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: pay.close() }
        ]
    }
}
