import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Pos

// Cash session: tickets of the session (reprint, return, void), cash in / out,
// X reading and closing with the counted amounts (Z report).
Rectangle {
    id: root
    color: Theme.bg
    property string sessionId: ""
    property var s: null
    readonly property bool open: !!s && s.status === "open"
    readonly property bool narrow: width < 1180     // 2.0.0: secondary actions become icon buttons
    readonly property string cur: s ? s.currency : Session.currency
    function reload() { Api.get("/api/v1/pos/sessions/" + sessionId, function(r) { if (r.ok) { root.s = r.data; if (r.data.status === "open") PosNav.session = r.data } else PosNav.fail(r) }) }
    function report() {
        Api.download("/api/v1/pos/sessions/" + sessionId + "/report.pdf", (open ? "X-" : "Z-") + s.register.code + "-" + s.number + ".pdf",
                     function(r) { if (!r.ok) PosNav.fail(r) })
    }
    Component.onCompleted: reload()
    // documentation tour hooks
    function tourTicket() { if (s && s.documents.length) docDialog.start(s.documents[0]) }
    function tourReturn() { docDialog.qty = ({ 1: "1" }); reason.text = qsTr("Damaged item (demo)"); docDialog.doReturn() }
    function tourClose() { closeDialog.start() }
    function tourCloseSubmit() {
        if (!s) return
        var cash = PosNav.config.payment_methods.filter(function(m) { return m.kind === "cash" })[0]
        var c = {}; c[cash.code] = Fmt.sum([s.cash_expected, "-500"], cur); closeDialog.counted = c; closeDialog.submit()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.s5
        spacing: Theme.s3
        RowLayout {
            Layout.fillWidth: true
            FbButton { text: qsTr("Back to sale"); variant: "ghost"; iconName: "chevronLeft"; visible: root.open; onClicked: PosNav.window.openSale() }
            Text { text: root.s ? qsTr("Register %1 · session %2").arg(root.s.register.code).arg(root.s.number) : ""; color: Theme.text
                   font.family: Theme.font; font.pixelSize: root.narrow ? Theme.fs_xl : Theme.fs_xxl; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
            FbChip { visible: !!root.s; text: root.open ? qsTr("Open") : qsTr("Closed"); tone: root.open ? "success" : "neutral" }
            FbButton { text: root.narrow ? "" : (root.open ? qsTr("X reading") : qsTr("Z report")); tip: root.open ? qsTr("X reading") : qsTr("Z report")
                       variant: "secondary"; iconName: "download"; enabled: !!root.s; onClicked: root.report() }
            FbButton { visible: root.open; text: root.narrow ? "" : qsTr("Cash in / out"); tip: qsTr("Cash in / out"); variant: "secondary"; iconName: "wallet"; onClicked: cashDialog.start() }
            FbButton { visible: root.open; text: qsTr("Close the register"); iconName: "check"; onClicked: closeDialog.start() }
            FbButton { visible: !root.open; text: qsTr("Done"); onClicked: { PosNav.session = null; PosNav.window.loadConfig(function() { PosNav.window.openHome() }) } }
        }
        Text { visible: !!root.s; color: Theme.textMuted; font.family: Theme.font
               text: root.s ? qsTr("Opened %1 by %2 · float %3").arg(Fmt.dateTime(root.s.opened_at)).arg(root.s.opened_by_name).arg(Fmt.money(root.s.opening_float, root.cur))
                              + (root.s.closed_at ? " · " + qsTr("closed %1 by %2").arg(Fmt.dateTime(root.s.closed_at)).arg(root.s.closed_by_name) : "") : "" }
        GridLayout {
            Layout.fillWidth: true
            columns: root.width < 1000 ? 2 : 4
            columnSpacing: Theme.s3
            KpiTile { Layout.fillWidth: true; label: qsTr("Sales"); iconName: "receipt"; value: root.s ? Fmt.money(root.s.sales_total, root.cur) : "…"
                      caption: root.s ? qsTr("%1 ticket(s)").arg(root.s.sales_count) : "" }
            KpiTile { Layout.fillWidth: true; label: qsTr("Returns / voids"); iconName: "swap"; tone: "warning"; value: root.s ? Fmt.money(root.s.returns_total, root.cur) : "…"
                      caption: root.s ? qsTr("%1 credit note(s)").arg(root.s.returns_count) : "" }
            KpiTile { Layout.fillWidth: true; label: qsTr("Net sales"); iconName: "chart"; tone: "accent"; value: root.s ? Fmt.money(root.s.net_total, root.cur) : "…"
                      caption: root.s ? qsTr("Discounts %1").arg(Fmt.money(root.s.discounts, root.cur)) : "" }
            KpiTile { Layout.fillWidth: true; label: root.s && root.s.cash_expected !== undefined ? qsTr("Cash expected") : qsTr("Cash movements"); iconName: "wallet"
                      value: root.s ? (root.s.cash_expected !== undefined ? Fmt.money(root.s.cash_expected, root.cur) : "+" + Fmt.money(root.s.cash_in, root.cur) + " / −" + Fmt.money(root.s.cash_out, root.cur)) : "…"
                      caption: root.s && root.s.cash_variance !== undefined && root.s.cash_variance !== null ? qsTr("Variance at closing: %1").arg(Fmt.money(root.s.cash_variance, root.cur)) : "" }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: !!root.s && root.s.by_method.length > 0
            spacing: Theme.s2
            Repeater { model: root.s ? root.s.by_method : []
                       FbChip { text: modelData.label + " · " + Fmt.money(modelData.net, root.cur); tone: "neutral" } }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            rows: root.s ? root.s.documents : []
            columns: [
                { title: qsTr("Date / time"), width: 1, minWidth: 130, muted: true, value: function(d) { return Fmt.dateTime(d.issued_at) } },
                { title: qsTr("Number"), width: 1.2, mono: true, strong: true, value: function(d) { return d.number } },
                { title: qsTr("Type"), width: 0.9, value: function(d) { return d.doc_type === "receipt" ? qsTr("Sale") : d.doc_type === "invoice" ? qsTr("Invoice") : qsTr("Return / void") } },
                { title: qsTr("Customer"), width: 1.3, muted: true, value: function(d) { return d.customer } },
                { title: qsTr("Total"), width: 1, align: "right", value: function(d) { return (d.doc_type === "credit_note" ? "−" : "") + Fmt.money(d.total, d.currency) } },
                { title: qsTr("Status"), width: 0.9, chip: function(d) { return d.doc_type === "credit_note" ? { text: qsTr("Refund"), tone: "warning" }
                                                                         : d.status === "credited" ? { text: qsTr("Refunded"), tone: "neutral" } : { text: qsTr("Paid"), tone: "success" } } }
            ]
            emptyIcon: "receipt"
            emptyTitle: qsTr("No ticket yet")
            onRowClicked: function(d) { docDialog.start(d) }
        }
    }

    // ---------------------------------------------------------------- ticket actions
    FbDialog {
        id: docDialog
        property var d: null
        property var full: null
        property var qty: ({})
        preferredWidth: 640
        title: d ? d.number : ""
        subtitle: full ? Fmt.money(full.total, full.currency) + " · " + full.party_name : ""
        function start(x) {
            d = x; full = null; qty = ({}); reason.text = ""
            Api.get("/api/v1/sales-documents/" + x.id, function(r) { if (r.ok) docDialog.full = r.data })
            open()
        }
        function doReturn() {
                           if (!docDialog.full) return   // details still loading (2.0.0: was a TypeError)
                           var lines = [], cur = docDialog.full.currency
                           docDialog.full.lines.forEach(function(l) {
                               var q = (docDialog.qty[l.line_no] || "").replace(",", ".")
                               if (q && parseFloat(q) > 0) lines.push({ line_no: l.line_no, quantity: q })
                           })
                           if (!lines.length) { PosNav.toast(qsTr("Enter the quantities to return."), "warning"); return }
                           // The server prices the credit note and refunds it in cash.
                           PosNav.sendApproved("POST", "/api/v1/pos/sales/" + docDialog.d.id + "/return",
                                               { session_id: root.sessionId, reason: reason.text.trim(), lines: lines, restock: true }, function(r) {
                               if (r.ok) { docDialog.close(); PosNav.toast(qsTr("Credit note %1 — refund %2").arg(r.data.credit_note.number).arg(Fmt.money(r.data.credit_note.total, cur)), "success"); root.reload() }
                               else PosNav.fail(r) })
        }
        readonly property bool returnable: !!full && (full.doc_type === "receipt" || full.doc_type === "invoice") && full.status !== "credited" && root.open
        Repeater {
            model: docDialog.full ? docDialog.full.lines : []
            delegate: RowLayout {
                Layout.fillWidth: true
                Text { text: modelData.description; color: Theme.text; font.family: Theme.font; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: modelData.quantity + " × " + Fmt.money(modelData.unit_price, docDialog.full.currency); color: Theme.textMuted; font.family: Theme.font }
                FbTextField { visible: docDialog.returnable; Layout.preferredWidth: 80; placeholderText: qsTr("Qty to return"); inputMethodHints: Qt.ImhFormattedNumbersOnly
                              onTextChanged: { var q = docDialog.qty; q[modelData.line_no] = text.trim(); docDialog.qty = q } }
            }
        }
        Field { visible: docDialog.returnable; label: qsTr("Reason (required for a return or a void)"); FbTextField { id: reason } }
        Text { visible: docDialog.returnable; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
               text: qsTr("A return creates a credit note and refunds in cash; returned goods go back to stock. A void cancels the whole ticket (same session only). Without the permission, a supervisor approves.") }
        footer: [
            FbButton { text: qsTr("Print"); variant: "secondary"; iconName: "download"; enabled: !!docDialog.d
                       onClicked: Api.download("/api/v1/pos/sales/" + docDialog.d.id + "/receipt.pdf", docDialog.d.number + ".pdf", function(r) { if (!r.ok) PosNav.fail(r) }) },
            FbButton { visible: docDialog.returnable; text: qsTr("Return the quantities"); variant: "secondary"; iconName: "swap"; onClicked: docDialog.doReturn() },
            FbButton { visible: docDialog.returnable && Fmt.isZero(docDialog.full.credited); text: qsTr("Void the ticket"); variant: "danger"
                       onClicked: PosNav.sendApproved("POST", "/api/v1/pos/sales/" + docDialog.d.id + "/void", { session_id: root.sessionId, reason: reason.text.trim() }, function(r) {
                           if (r.ok) { docDialog.close(); PosNav.toast(qsTr("Ticket voided"), "success"); root.reload() } else PosNav.fail(r) }) },
            FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: docDialog.close() }
        ]
    }
    FbDialog {
        id: cashDialog
        title: qsTr("Cash in / cash out")
        function start() { kind.value = "cash_in"; amount.setValue(""); why.text = ""; open() }
        Field { label: qsTr("Movement"); FbComboBox { id: kind; Layout.fillWidth: true
                model: [{ value: "cash_in", text: qsTr("Cash in (change, deposit)") }, { value: "cash_out", text: qsTr("Cash out (bank deposit, expense)") }] } }
        Field { label: qsTr("Amount"); MoneyField { id: amount; currency: root.cur } }
        Field { label: qsTr("Reason"); FbTextField { id: why } }
        footer: [
            FbButton { text: qsTr("Record"); enabled: !!amount.value && why.text.trim().length >= 3
                       onClicked: PosNav.sendApproved("POST", "/api/v1/pos/sessions/" + root.sessionId + "/cash-movements",
                                                      { kind: kind.value, amount: amount.value, reason: why.text.trim() }, function(r) {
                           if (r.ok) { cashDialog.close(); root.reload() } else PosNav.fail(r) }) },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: cashDialog.close() }
        ]
    }
    FbDialog {
        id: closeDialog
        property var counted: ({})
        title: qsTr("Close the register")
        subtitle: qsTr("Count the drawer and the terminal totals, then confirm.")
        preferredWidth: 560
        function start() { counted = ({}); note.text = ""; open() }
        // The server requires the cash count (cash_count_required): say it before sending.
        function cashCounted() {
            var ms = PosNav.config ? PosNav.config.payment_methods : []
            for (var i = 0; i < ms.length; ++i) if (ms[i].kind === "cash" && counted[ms[i].code]) return true
            return false
        }
        function confirmSubmit() {
            if (!cashCounted()) { PosNav.toast(qsTr("Enter the cash counted in the drawer (0 if the drawer is empty)."), "warning"); return }
            submit()
        }
        function submit() {
                           var c = {}
                           for (var k in closeDialog.counted) if (closeDialog.counted[k]) c[k] = closeDialog.counted[k]
                           Api.send("POST", "/api/v1/pos/sessions/" + root.sessionId + "/close", { counted: c, note: note.text.trim() }, function(r) {
                               if (!r.ok) { PosNav.fail(r); return }
                               closeDialog.close(); PosNav.session = null
                               PosNav.toast(qsTr("Register closed (Z %1).").arg(r.data.number), "success")
                               root.reload()
                           }, Api.newIdempotencyKey())
        }
        Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font
               text: root.s && root.s.cash_expected !== undefined ? qsTr("Expected cash (supervisor view): %1").arg(Fmt.money(root.s.cash_expected, root.cur))
                                                                 : qsTr("Count without looking at the expected amount: the variance is computed by the server and reviewed by a supervisor.") }
        Repeater {
            model: PosNav.config ? PosNav.config.payment_methods : []
            delegate: Field {
                Layout.fillWidth: true
                label: modelData.label + (modelData.kind === "cash" ? " — " + qsTr("cash counted in the drawer") : " — " + qsTr("total of the terminal / statement"))
                MoneyField { currency: root.cur; onValueChanged: { var c = closeDialog.counted; c[modelData.code] = value; closeDialog.counted = c } }
            }
        }
        Field { label: qsTr("Note (optional)"); FbTextField { id: note } }
        footer: [
            FbButton { text: qsTr("Close the register"); iconName: "check"; onClicked: closeDialog.confirmSubmit() },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: closeDialog.close() }
        ]
    }
}
