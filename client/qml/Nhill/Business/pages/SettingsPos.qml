import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

// Point of sale (Nhill POS) seen from Nhill Business: registers and their cashiers,
// payment methods, rules and receipt, promo codes, cash sessions and Z reports.
// Configuration requires pos.manage; reading the sessions requires pos.report.
ColumnLayout {
    id: root
    spacing: Theme.s4
    readonly property bool manage: Session.can("pos.manage")
    property var regs: []
    property var methods: []
    property var settings: null
    property var limit: null
    property int activeCount: 0
    property var promos: []
    property var sessions: []
    property var members: []
    property var warehouses: []
    function load() {
        Api.get("/api/v1/pos/registers", function(r) {
            if (!r.ok) { Nav.fail(r); return }
            root.regs = r.data.items; root.methods = r.data.payment_methods; root.settings = r.data.settings
            root.limit = r.data.limit; root.activeCount = r.data.active_count
            if (root.settings) { maxDisc.text = Fmt.percent(root.settings.max_discount_bp).replace(/\s?%/, ""); priceOverride.checked = root.settings.allow_price_override
                                 footerText.text = root.settings.receipt_footer; receiptWidth.value = root.settings.receipt_width_mm }
        })
        Api.get("/api/v1/pos/sessions?limit=50", function(r) { if (r.ok) root.sessions = r.data.items })
        if (manage) {
            Api.get("/api/v1/pos/promo-codes", function(r) { if (r.ok) root.promos = r.data.items })
            Api.get("/api/v1/members", function(r) { if (r.ok) root.members = r.data.members.filter(function(m) { return m.status === "active" }) })
            Api.get("/api/v1/warehouses", function(r) { if (r.ok) root.warehouses = r.data.items })
        }
    }
    Component.onCompleted: load()

    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
           text: qsTr("Cashiers sell with the Nhill POS application (Windows, Linux) using their own Nhill account. Products, prices, taxes and stock are those of Nhill Business: every sale updates the stock of the register's warehouse.") }

    // ---------------------------------------------------------------- registers
    RowLayout {
        Layout.fillWidth: true
        SectionTitle { text: qsTr("Registers"); Layout.fillWidth: true }
        Text { visible: root.limit !== null; color: Theme.textMuted; font.family: Theme.font
               text: qsTr("%1 of %2 active register(s) allowed by your plan").arg(root.activeCount).arg(root.limit) }
        FbButton { visible: root.manage; text: qsTr("New register"); iconName: "plus"; variant: "secondary"
                   enabled: root.limit === null || root.activeCount < root.limit; onClicked: regDialog.start(null) }
    }
    DataTable {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(380, 52 + Math.max(1, root.regs.length) * 44 + (root.regs.length ? 0 : 120))
        rows: root.regs
        titleColumn: 1; subtitleColumn: 2; chipColumn: 4
        columns: [
            { title: qsTr("Code"), width: 0.6, mono: true, strong: true, value: function(x) { return x.code } },
            { title: qsTr("Name"), width: 1.4, strong: true, value: function(x) { return x.name } },
            { title: qsTr("Warehouse"), width: 1.2, muted: true, value: function(x) { return x.warehouse + (x.location ? " · " + x.location : "") } },
            { title: qsTr("Cashiers"), width: 1.8, muted: true, value: function(x) { return x.users.length ? x.users.map(function(u) { return u.full_name }).join(", ") : qsTr("Supervisors only") } },
            { title: qsTr("Status"), width: 1.1, chip: function(x) { return !x.active ? { text: qsTr("Inactive"), tone: "neutral" }
                                                                    : x.open_session ? { text: qsTr("Open · %1").arg(x.open_session.opened_by_name), tone: "warning" }
                                                                    : { text: qsTr("Closed"), tone: "success" } } }
        ]
        emptyIcon: "cart"
        emptyTitle: qsTr("No register yet")
        emptyText: root.manage ? qsTr("Create a register, then assign the cashiers who may open it.") : ""
        onRowClicked: function(x) { if (root.manage) regDialog.start(x) }
    }

    // ---------------------------------------------------------------- payment methods
    RowLayout {
        visible: root.manage
        Layout.fillWidth: true
        SectionTitle { text: qsTr("Payment methods at the register"); Layout.fillWidth: true }
        FbButton { text: qsTr("New method"); iconName: "plus"; variant: "secondary"; onClicked: methodDialog.start(null) }
    }
    DataTable {
        visible: root.manage
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(360, 52 + Math.max(1, root.methods.length) * 44 + (root.methods.length ? 0 : 120))
        rows: root.methods
        titleColumn: 1; subtitleColumn: 2; chipColumn: 4
        columns: [
            { title: qsTr("Code"), width: 0.8, mono: true, value: function(m) { return m.code } },
            { title: qsTr("Label"), width: 1.6, strong: true, value: function(m) { return m.label } },
            { title: qsTr("Kind"), width: 1.2, muted: true, value: function(m) { return root.kindLabel(m.kind) } },
            { title: qsTr("Account"), width: 1.4, muted: true, value: function(m) { return m.account || "—" } },
            { title: qsTr("Status"), width: 0.9, chip: function(m) { return m.active ? { text: qsTr("Active"), tone: "success" } : { text: qsTr("Inactive"), tone: "neutral" } } }
        ]
        onRowClicked: function(m) { methodDialog.start(m) }
    }
    function kindLabel(k) {
        return k === "cash" ? qsTr("Cash") : k === "card" ? qsTr("Card (terminal)") : k === "mobile_money" ? qsTr("Mobile Money")
             : k === "bank_transfer" ? qsTr("Bank transfer") : k === "cheque" ? qsTr("Cheque") : qsTr("Other")
    }

    // ---------------------------------------------------------------- rules and receipt
    SectionTitle { visible: root.manage; text: qsTr("Rules and receipt") }
    FbCard {
        visible: root.manage && !!root.settings
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.s4
                Field { Layout.fillWidth: true; label: qsTr("Maximum discount without approval (%)")
                        help: qsTr("Above it, a cash manager, manager or administrator approves on the register.")
                        FbTextField { id: maxDisc; horizontalAlignment: TextInput.AlignRight } }
                Field { Layout.fillWidth: true; label: qsTr("Receipt width"); FbComboBox { id: receiptWidth; Layout.fillWidth: true
                        model: [{ value: 80, text: qsTr("80 mm roll") }, { value: 58, text: qsTr("58 mm roll") }] } }
            }
            FbSwitch { id: priceOverride; text: qsTr("Allow price changes at the register (always with approval)") }
            Field { Layout.fillWidth: true; label: qsTr("Message at the bottom of the receipt"); FbTextField { id: footerText; placeholderText: qsTr("e.g. Goods sold are exchanged within 7 days with the receipt.") } }
            FbButton { text: qsTr("Save"); Layout.alignment: Qt.AlignRight
                       onClicked: {
                           var bp = Fmt.parsePercent(maxDisc.text)
                           if (bp < 0) { Nav.toast(qsTr("Enter a valid percentage."), "danger"); return }
                           Api.send("PATCH", "/api/v1/pos/settings", { max_discount_bp: bp, allow_price_override: priceOverride.checked,
                                                                       receipt_footer: footerText.text.trim(), receipt_width_mm: receiptWidth.value },
                                    function(r) { if (r.ok) { Nav.toast(qsTr("Point of sale settings saved."), "success"); root.load() } else Nav.fail(r) })
                       } }
        }
    }

    // ---------------------------------------------------------------- promo codes
    RowLayout {
        visible: root.manage
        Layout.fillWidth: true
        SectionTitle { text: qsTr("Promo codes at the register"); Layout.fillWidth: true }
        FbButton { text: qsTr("New promo code"); iconName: "plus"; variant: "secondary"; onClicked: promoDialog.start() }
    }
    DataTable {
        visible: root.manage
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(360, 52 + Math.max(1, root.promos.length) * 44 + (root.promos.length ? 0 : 120))
        rows: root.promos
        titleColumn: 0; subtitleColumn: 1; chipColumn: 5
        columns: [
            { title: qsTr("Code"), width: 1, mono: true, strong: true, value: function(p) { return p.code } },
            { title: qsTr("Description"), width: 1.6, muted: true, value: function(p) { return p.description || "—" } },
            { title: qsTr("Discount"), width: 0.7, align: "right", value: function(p) { return Fmt.percent(p.percent_bp) } },
            { title: qsTr("Validity"), width: 1.4, muted: true, value: function(p) { return Fmt.date(p.valid_from) + " → " + (p.valid_until ? Fmt.date(p.valid_until) : "∞") } },
            { title: qsTr("Uses"), width: 0.7, align: "right", value: function(p) { return p.used + (p.max_redemptions ? " / " + p.max_redemptions : "") } },
            { title: qsTr("Status"), width: 0.9, chip: function(p) { return p.active ? { text: qsTr("Active"), tone: "success" } : { text: qsTr("Inactive"), tone: "neutral" } } }
        ]
        emptyIcon: "receipt"
        emptyTitle: qsTr("No promo code")
        onRowClicked: function(p) {
            Api.send("PATCH", "/api/v1/pos/promo-codes/" + p.id, { active: !p.active }, function(r) {
                if (r.ok) { Nav.toast(p.active ? qsTr("Promo code %1 deactivated").arg(p.code) : qsTr("Promo code %1 reactivated").arg(p.code), "success"); root.load() } else Nav.fail(r) })
        }
    }

    // ---------------------------------------------------------------- sessions
    SectionTitle { text: qsTr("Cash sessions") }
    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
           text: qsTr("Click a session to download its report: X reading while the register is open, Z report once closed. The variance is the counted cash minus the expected cash (opening float + cash takings + cash in − cash out).") }
    DataTable {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(520, 52 + Math.max(1, root.sessions.length) * 44 + (root.sessions.length ? 0 : 120))
        rows: root.sessions
        titleColumn: 0; subtitleColumn: 1; trailingColumn: 4; chipColumn: 6
        columns: [
            { title: qsTr("Session"), width: 0.9, strong: true, mono: true, value: function(s) { return s.register.code + "-" + s.number } },
            { title: qsTr("Opened"), width: 1.3, muted: true, value: function(s) { return Fmt.dateTime(s.opened_at) + " · " + s.opened_by_name } },
            { title: qsTr("Closed"), width: 1.3, muted: true, value: function(s) { return s.closed_at ? Fmt.dateTime(s.closed_at) + " · " + s.closed_by_name : "—" } },
            { title: qsTr("Tickets"), width: 0.6, align: "right", value: function(s) { return String(s.sales_count) } },
            { title: qsTr("Net sales"), width: 1.1, align: "right", value: function(s) { return Fmt.money(s.net_total, s.currency) } },
            { title: qsTr("Variance"), width: 1, align: "right", value: function(s) { return s.cash_variance === undefined ? "•••" : s.cash_variance === null ? "—" : Fmt.money(s.cash_variance, s.currency) } },
            { title: qsTr("Status"), width: 0.9, chip: function(s) {
                if (s.status === "open") return { text: qsTr("Open"), tone: "warning" }
                if (s.cash_variance !== undefined && s.cash_variance !== null && !Fmt.isZero(s.cash_variance)) return { text: qsTr("Variance"), tone: "danger" }
                return { text: qsTr("Closed"), tone: "success" } } }
        ]
        emptyIcon: "wallet"
        emptyTitle: qsTr("No cash session yet")
        onRowClicked: function(s) {
            Api.download("/api/v1/pos/sessions/" + s.id + "/report.pdf", (s.status === "closed" ? "Z-" : "X-") + s.register.code + "-" + s.number + ".pdf",
                         function(r) { if (!r.ok) Nav.fail(r) })
        }
    }

    // ---------------------------------------------------------------- dialogs
    FbDialog {
        id: regDialog
        property var reg: null
        property var chosen: ({})
        preferredWidth: 560
        title: reg ? qsTr("Register %1").arg(reg.code) : qsTr("New register")
        function start(r) {
            reg = r; regCode.text = r ? r.code : ""; regName.text = r ? r.name : ""; regLoc.text = r ? r.location : ""
            regWh.value = r ? r.warehouse_id : (root.warehouses.filter(function(w) { return w.is_default })[0] || {}).id
            var c = {}; if (r) r.users.forEach(function(u) { c[u.id] = true }); chosen = c
            open()
        }
        Field { visible: !regDialog.reg; label: qsTr("Code (printed on receipts)"); FbTextField { id: regCode; monospace: true; placeholderText: "C1" } }
        Field { label: qsTr("Name"); FbTextField { id: regName; placeholderText: qsTr("e.g. Main counter") } }
        Field { label: qsTr("Location"); FbTextField { id: regLoc } }
        Field { label: qsTr("Stock taken from"); FbComboBox { id: regWh; Layout.fillWidth: true; model: root.warehouses.map(function(w) { return { value: w.id, text: w.name } }) } }
        SectionTitle { visible: !!regDialog.reg; text: qsTr("Who may open this register") }
        Text { visible: !!regDialog.reg; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
               text: qsTr("Cash managers, managers and administrators can open every register. Cashiers and sales staff only open the registers ticked here.") }
        Repeater {
            model: regDialog.reg ? root.members.filter(function(m) { return m.role === "cashier" || m.role === "sales" }) : []
            delegate: CheckBox {
                text: modelData.full_name + " · " + Labels.role(modelData.role)
                font.family: Theme.font
                checked: !!regDialog.chosen[modelData.user_id]
                onToggled: { var c = regDialog.chosen; c[modelData.user_id] = checked; regDialog.chosen = c }
            }
        }
        Text { visible: !!regDialog.reg && root.members.filter(function(m) { return m.role === "cashier" || m.role === "sales" }).length === 0
               Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
               text: qsTr("No cashier in the team yet: invite one from Settings → Team with the Cashier role.") }
        footer: [
            FbButton { text: regDialog.reg ? qsTr("Save") : qsTr("Create")
                       onClicked: {
                           if (!regDialog.reg) {
                               Api.send("POST", "/api/v1/pos/registers", { code: regCode.text.trim(), name: regName.text.trim(), location: regLoc.text.trim(), warehouse_id: regWh.value },
                                        function(r) { if (r.ok) { regDialog.close(); root.load() } else Nav.fail(r) })
                               return
                           }
                           var ids = []; for (var k in regDialog.chosen) if (regDialog.chosen[k]) ids.push(k)
                           Api.send("PATCH", "/api/v1/pos/registers/" + regDialog.reg.id, { name: regName.text.trim(), location: regLoc.text.trim(), warehouse_id: regWh.value }, function(r) {
                               if (!r.ok) { Nav.fail(r); return }
                               Api.send("PUT", "/api/v1/pos/registers/" + regDialog.reg.id + "/users", { user_ids: ids }, function(u) {
                                   if (u.ok) { regDialog.close(); Nav.toast(qsTr("Register saved"), "success"); root.load() } else Nav.fail(u) })
                           })
                       } },
            FbButton { visible: !!regDialog.reg; text: regDialog.reg && regDialog.reg.active ? qsTr("Deactivate") : qsTr("Reactivate"); variant: "ghost"
                       onClicked: Api.send("PATCH", "/api/v1/pos/registers/" + regDialog.reg.id, { active: !regDialog.reg.active }, function(r) {
                           if (r.ok) { regDialog.close(); root.load() } else Nav.fail(r) }) },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: regDialog.close() }
        ]
    }
    FbDialog {
        id: methodDialog
        property var m: null
        title: m ? m.label : qsTr("New payment method")
        function start(x) { m = x; mCode.text = x ? x.code : ""; mLabel.text = x ? x.label : ""; mKind.value = x ? x.kind : "mobile_money"; open() }
        Field { visible: !methodDialog.m; label: qsTr("Code"); FbTextField { id: mCode; monospace: true; placeholderText: "mpesa" } }
        Field { label: qsTr("Label shown on the register and the receipt"); FbTextField { id: mLabel; placeholderText: "M-Pesa" } }
        Field { visible: !methodDialog.m; label: qsTr("Kind"); FbComboBox { id: mKind; Layout.fillWidth: true
                model: ["cash", "card", "mobile_money", "bank_transfer", "cheque", "other"].map(function(k) { return { value: k, text: root.kindLabel(k) } }) } }
        Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
               text: qsTr("Nhill records the amount received by each method; it does not connect to card terminals or Mobile Money operators. The cashier checks the payment on the terminal before validating.") }
        footer: [
            FbButton { text: methodDialog.m ? qsTr("Save") : qsTr("Create")
                       onClicked: {
                           var done = function(r) { if (r.ok) { methodDialog.close(); root.load() } else Nav.fail(r) }
                           if (methodDialog.m) Api.send("PATCH", "/api/v1/pos/payment-methods/" + methodDialog.m.id, { label: mLabel.text.trim() }, done)
                           else Api.send("POST", "/api/v1/pos/payment-methods", { code: mCode.text.trim(), label: mLabel.text.trim(), kind: mKind.value }, done)
                       } },
            FbButton { visible: !!methodDialog.m; text: methodDialog.m && methodDialog.m.active ? qsTr("Deactivate") : qsTr("Reactivate"); variant: "ghost"
                       onClicked: Api.send("PATCH", "/api/v1/pos/payment-methods/" + methodDialog.m.id, { active: !methodDialog.m.active }, function(r) {
                           if (r.ok) { methodDialog.close(); root.load() } else Nav.fail(r) }) },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: methodDialog.close() }
        ]
    }
    FbDialog {
        id: promoDialog
        title: qsTr("New promo code")
        subtitle: qsTr("A percentage off the whole ticket, typed by the cashier.")
        function start() { pCode.text = ""; pDesc.text = ""; pPct.text = ""; pMin.setValue(""); pFrom.value = Fmt.today(); pUntil.value = ""; pMax.text = ""; open() }
        Field { label: qsTr("Code"); FbTextField { id: pCode; monospace: true; placeholderText: "FIDELITE" } }
        Field { label: qsTr("Description"); FbTextField { id: pDesc } }
        RowLayout {
            Layout.fillWidth: true
            Field { Layout.fillWidth: true; label: qsTr("Discount (%)"); FbTextField { id: pPct; horizontalAlignment: TextInput.AlignRight } }
            Field { Layout.fillWidth: true; label: qsTr("Minimum ticket (optional)"); MoneyField { id: pMin } }
        }
        RowLayout {
            Layout.fillWidth: true
            Field { Layout.fillWidth: true; label: qsTr("Valid from"); DateField { id: pFrom } }
            Field { Layout.fillWidth: true; label: qsTr("Until (optional)"); DateField { id: pUntil } }
        }
        Field { label: qsTr("Maximum number of uses (optional)"); FbTextField { id: pMax; inputMethodHints: Qt.ImhDigitsOnly } }
        footer: [
            FbButton { text: qsTr("Create")
                       onClicked: {
                           var bp = Fmt.parsePercent(pPct.text)
                           if (bp <= 0) { Nav.toast(qsTr("Enter a valid percentage."), "danger"); return }
                           var b = { code: pCode.text.trim(), description: pDesc.text.trim(), percent_bp: bp, valid_from: pFrom.value }
                           if (pUntil.value) b.valid_until = pUntil.value
                           if (pMin.value) { b.min_amount = pMin.value; b.currency = Session.currency }
                           if (pMax.text.trim()) b.max_redemptions = parseInt(pMax.text.trim())
                           Api.send("POST", "/api/v1/pos/promo-codes", b, function(r) { if (r.ok) { promoDialog.close(); root.load() } else Nav.fail(r) })
                       } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: promoDialog.close() }
        ]
    }
}
