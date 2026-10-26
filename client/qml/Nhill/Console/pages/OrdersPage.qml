import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Subscription transactions. An order becomes "paid" only through a verified provider
// event or, here, a manual reconciliation after checking the bank statement.
FbPage {
    id: page
    title: qsTr("Transactions")
    subtitle: qsTr("Subscription payment requests, provider payments and reconciliation")
    property var rows: []
    property var totals: []
    property string status: "pending"
    function reload() {
        Api.get("/api/v1/admin/transactions?limit=200" + (status ? "&status=" + status : ""), function(r) {
            if (r.ok) { page.rows = r.data.items; page.totals = r.data.paid_totals } else ConsoleNav.fail(r)
        })
    }
    Component.onCompleted: reload()
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        RowLayout {
            Layout.fillWidth: true
            TabStrip { model: [{ value: "pending", text: qsTr("Awaiting payment") }, { value: "paid", text: qsTr("Paid") }, { value: "failed", text: qsTr("Failed") },
                               { value: "expired", text: qsTr("Expired") }, { value: "", text: qsTr("All") }]
                       current: page.status; onSelected: function(v) { page.status = v; page.reload() } }
            Item { Layout.fillWidth: true }
            Repeater {
                model: page.totals
                FbChip { text: qsTr("%1 collected · %2 payments").arg(Fmt.money(modelData.collected, modelData.currency)).arg(modelData.count); tone: "success" }
            }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            rows: page.rows
            columns: [
                { title: qsTr("Created"), width: 1.1, muted: true, value: function(o) { return Fmt.dateTime(o.created_at) } },
                { title: qsTr("Establishment"), width: 1.7, strong: true, value: function(o) { return o.establishment } },
                { title: qsTr("Plan"), width: 1.3, value: function(o) { return o.plan_code + (o.duration_months ? " · " + Labels.duration(o.duration_months) : "") } },
                { title: qsTr("Type"), width: 0.9, muted: true, value: function(o) { return Labels.orderKind(o.kind) } },
                { title: qsTr("Amount"), width: 1.1, align: "right", value: function(o) { return Fmt.money(o.amount, o.currency) } },
                { title: qsTr("Promo"), width: 0.9, mono: true, muted: true, value: function(o) { return o.promo_code || "" } },
                { title: qsTr("Provider"), width: 1, muted: true, value: function(o) { return o.provider_name || o.provider || "—" } },
                { title: qsTr("Status"), width: 1, chip: function(o) { return Labels.orderStatus(o.status) } }
            ]
            emptyIcon: "payment"
            emptyTitle: qsTr("No transaction")
            onRowClicked: function(o) { detail.show(o.id) }
        }
    }
    FbDialog {
        id: detail
        property var o: null
        preferredWidth: 640
        title: o ? o.establishment : ""
        subtitle: o ? qsTr("Order %1").arg(o.id.slice(0, 8).toUpperCase()) : ""
        function show(id) { o = null; ref.text = ""; why.text = ""; Api.get("/api/v1/admin/transactions/" + id, function(r) { if (r.ok) { detail.o = r.data; detail.open() } else ConsoleNav.fail(r) }) }
        readonly property bool confirmable: !!o && (o.status === "pending" || o.status === "failed" || o.status === "expired") && ConsoleNav.can("billing")
        Repeater {
            model: detail.o ? [
                [qsTr("Status"), Labels.orderStatus(detail.o.status).text + (detail.o.failure_reason ? " (" + detail.o.failure_reason + ")" : "")],
                [qsTr("Type"), Labels.orderKind(detail.o.kind)],
                [qsTr("Plan"), detail.o.plan_code + (detail.o.duration_months ? " · " + Labels.duration(detail.o.duration_months) : "")],
                [qsTr("Subtotal"), Fmt.money(detail.o.subtotal, detail.o.currency)],
                [qsTr("Promo discount"), Fmt.money(detail.o.discount, detail.o.currency) + (detail.o.promo_code ? " (" + detail.o.promo_code + ")" : "")],
                [qsTr("Pro-rata credit"), Fmt.money(detail.o.credit, detail.o.currency)],
                [qsTr("Tax"), Fmt.money(detail.o.tax, detail.o.currency)],
                [qsTr("Total"), Fmt.money(detail.o.amount, detail.o.currency)],
                [qsTr("Provider"), (detail.o.provider_name || detail.o.provider || "—") + (detail.o.provider_reference ? " · " + detail.o.provider_reference : "")],
                [qsTr("Created / paid"), Fmt.dateTime(detail.o.created_at) + (detail.o.paid_at ? " / " + Fmt.dateTime(detail.o.paid_at) : "")],
                [qsTr("Expires"), detail.o.expires_at ? Fmt.dateTime(detail.o.expires_at) : "—"]] : []
            delegate: RowLayout {
                Layout.fillWidth: true
                Text { text: modelData[0]; color: Theme.textMuted; font.family: Theme.font; Layout.preferredWidth: 180 }
                Text { text: modelData[1]; color: Theme.text; font.family: Theme.font; Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
        }
        SectionTitle { visible: !!detail.o && detail.o.provider_events.length > 0; text: qsTr("Provider events") }
        Repeater {
            model: detail.o ? detail.o.provider_events : []
            Text { Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: Fmt.dateTime(modelData.received_at) + " · " + modelData.provider + " · " + modelData.event_id + " → " + (modelData.outcome || "") }
        }
        ColumnLayout {
            visible: detail.confirmable
            Layout.fillWidth: true
            spacing: Theme.s2
            SectionTitle { text: qsTr("Confirm a payment received") }
            Banner { Layout.fillWidth: true; tone: "warning"; text: qsTr("Only confirm after seeing the money on the bank or Mobile Money statement. The subscription is updated immediately and the action is audited.") }
            Field { label: qsTr("Bank / Mobile Money transaction reference"); required: true; FbTextField { id: ref; monospace: true } }
            Field { label: qsTr("Reason / verification made"); required: true; FbTextArea { id: why } }
        }
        footer: [
            FbButton { visible: detail.confirmable; text: qsTr("Confirm the payment"); onClicked: Api.send("POST", "/api/v1/admin/orders/" + detail.o.id + "/confirm-manual",
                           { reference: ref.text.trim(), reason: why.text.trim() }, function(r) {
                               if (r.ok) { detail.close(); ConsoleNav.toast(r.data.applied ? qsTr("Subscription updated") : qsTr("Already applied"), "success"); page.reload() } else ConsoleNav.fail(r) }) },
            FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: detail.close() }
        ]
    }
}
