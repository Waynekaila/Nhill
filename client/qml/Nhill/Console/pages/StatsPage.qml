import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

FbPage {
    id: page
    title: qsTr("Overview")
    subtitle: qsTr("Aggregated platform figures — no business data")
    scrollable: true
    property var s: null
    property var o: null
    function reload() {
        Api.get("/api/v1/admin/stats", function(r) { if (r.ok) page.s = r.data; else ConsoleNav.fail(r) })
        Api.get("/api/v1/admin/overview", function(r) { if (r.ok) page.o = r.data })
    }
    Component.onCompleted: reload()
    function count(obj, key) { return obj && obj[key] ? obj[key] : 0 }
    function total(obj) { var t = 0; for (var k in obj) t += obj[k]; return t }
    GridLayout {
        Layout.fillWidth: true
        columns: 4
        columnSpacing: Theme.s3; rowSpacing: Theme.s3
        KpiTile { Layout.fillWidth: true; label: qsTr("Establishments"); iconName: "building"; value: page.s ? String(page.total(page.s.establishments_by_status)) : "…"
                  caption: page.s ? qsTr("%1 new in 30 days · %2 suspended").arg(page.s.new_establishments_30d).arg(page.count(page.s.establishments_by_status, "suspended")) : "" }
        KpiTile { Layout.fillWidth: true; label: qsTr("Subscriptions"); iconName: "receipt"; tone: "accent"
                  value: page.s ? String(page.count(page.s.subscriptions_by_status, "active")) + " " + qsTr("active") : "…"
                  caption: page.s ? qsTr("%1 trial · %2 grace · %3 suspended").arg(page.count(page.s.subscriptions_by_status, "trialing")).arg(page.count(page.s.subscriptions_by_status, "past_due")).arg(page.count(page.s.subscriptions_by_status, "suspended")) : "" }
        KpiTile { Layout.fillWidth: true; label: qsTr("Active users (24 h)"); iconName: "users"; value: page.s ? String(page.s.active_users_24h) : "…"
                  caption: page.s ? qsTr("%1 individual · %2 companies").arg(page.count(page.s.establishments_by_kind, "individual")).arg(page.count(page.s.establishments_by_kind, "company")) : "" }
        KpiTile { Layout.fillWidth: true; label: qsTr("Activity (30 days)"); iconName: "invoice"; tone: "warning"; value: page.s ? String(page.s.documents_issued_30d) + " " + qsTr("documents") : "…"
                  caption: page.s ? qsTr("%1 payments recorded").arg(page.s.payments_recorded_30d) : "" }
    }
    SectionTitle { text: qsTr("Operations") }
    GridLayout {
        Layout.fillWidth: true
        columns: 4
        columnSpacing: Theme.s3; rowSpacing: Theme.s3
        KpiTile { Layout.fillWidth: true; label: qsTr("Support tickets to handle"); iconName: "info"; tone: page.o && page.o.critical_tickets > 0 ? "warning" : "primary"
                  value: page.o ? String(page.o.open_tickets) : "…"; caption: page.o ? qsTr("%1 critical").arg(page.o.critical_tickets) : ""
                  MouseArea { anchors.fill: parent; onClicked: ConsoleNav.shell.navigate("tickets") } }
        KpiTile { Layout.fillWidth: true; label: qsTr("Unresolved technical errors"); iconName: "alert"; tone: page.o && page.o.unresolved_errors > 0 ? "warning" : "primary"
                  value: page.o ? String(page.o.unresolved_errors) : "…"
                  MouseArea { anchors.fill: parent; onClicked: ConsoleNav.shell.navigate("errors") } }
        KpiTile { Layout.fillWidth: true; label: qsTr("Payment requests open"); iconName: "payment"; tone: "accent"
                  value: page.o ? String(page.o.pending_orders) : "…"; caption: page.o ? qsTr("%1 subscriptions with failed payments").arg(page.o.failed_payment_subscriptions) : ""
                  MouseArea { anchors.fill: parent; onClicked: ConsoleNav.shell.navigate("orders") } }
        KpiTile { Layout.fillWidth: true; label: qsTr("This month"); iconName: "wallet"
                  value: page.o && page.o.revenue_this_month.length ? page.o.revenue_this_month.map(function(x) { return Fmt.money(x.amount, x.currency) }).join(" · ") : "—"
                  caption: page.o ? qsTr("%1 provider(s) enabled · %2 active promo code(s)").arg(page.o.enabled_providers).arg(page.o.active_promo_codes) : "" }
    }
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            SectionTitle { text: qsTr("Subscription revenue confirmed (30 days)") }
            Repeater {
                model: page.s ? Object.keys(page.s.subscription_revenue_30d) : []
                Text { text: Fmt.money(page.s.subscription_revenue_30d[modelData], modelData); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold }
            }
            Text { visible: !!page.s && Object.keys(page.s.subscription_revenue_30d).length === 0; text: qsTr("No confirmed payment in the period."); color: Theme.textFaint; font.family: Theme.font }
            Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: qsTr("Only orders confirmed by a verified provider event or a manual, audited reconciliation are counted.") }
        }
    }
}
