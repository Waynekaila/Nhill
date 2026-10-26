import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Platform-wide report: one line per establishment and currency (aggregates only).
FbPage {
    id: page
    title: qsTr("Reports")
    subtitle: qsTr("Activity of every establishment over a period — aggregates per currency, never converted")
    property var columns: []
    property var rows: []
    property string status: ""
    property bool loading: false
    function query() { return "/api/v1/admin/reports/establishments?from=" + from.value + "&to=" + to.value + (status ? "&subscription_status=" + status : "") }
    function run() {
        loading = true
        Api.get(query(), function(r) {
            loading = false
            if (!r.ok) { ConsoleNav.fail(r); return }
            var cur = -1
            r.data.columns.forEach(function(c, i) { if (c.key === "col.currency") cur = i })
            // On screen: the key columns (exports keep every column).
            var keep = ["col.establishment", "col.plan", "col.subscription", "col.users", "col.currency", "col.invoices_issued",
                        "col.invoiced_total", "col.credited_total", "col.payments_in", "col.expenses"]
            page.columns = r.data.columns.map(function(c, i) {
                return { title: c.label, width: c.key === "col.establishment" ? 2.2 : c.type === "money" ? 1.3 : c.type === "int" ? 0.8 : 1, align: c.type === "text" ? "left" : "right", strong: i === 0,
                         key: c.key,
                         value: function(row) { var v = row[i]; if (c.type === "money" && cur >= 0 && row[cur]) return Fmt.amount(v, row[cur]); return v === null ? "" : String(v) } }
            }).filter(function(c) { return keep.indexOf(c.key) >= 0 })
            page.rows = r.data.rows
        })
    }
    function exportAs(fmt) {
        Api.download(query() + "&format=" + fmt, "nhill-etablissements-" + from.value + "_" + to.value + "." + fmt,
                     function(r) { if (r.ok) ConsoleNav.toast(qsTr("Saved to %1").arg(r.data), "success"); else ConsoleNav.fail(r) })
    }
    Component.onCompleted: { from.value = Fmt.monthStart(); to.value = Fmt.today(); run() }
    actions: [
        FbButton { text: "CSV"; iconName: "download"; variant: "secondary"; onClicked: page.exportAs("csv") },
        FbButton { text: "PDF"; iconName: "download"; variant: "secondary"; onClicked: page.exportAs("pdf") }
    ]
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        FbCard {
            Layout.fillWidth: true
            RowLayout {
                width: parent.width
                spacing: Theme.s3
                Field { label: qsTr("From"); Layout.preferredWidth: 180; Layout.fillWidth: false; DateField { id: from } }
                Field { label: qsTr("To"); Layout.preferredWidth: 180; Layout.fillWidth: false; DateField { id: to } }
                Field { label: qsTr("Subscription"); Layout.preferredWidth: 220; Layout.fillWidth: false
                        FbComboBox { value: ""; model: [{ value: "", text: qsTr("All") }, { value: "trialing", text: Labels.subscription("trialing").text },
                                     { value: "active", text: Labels.subscription("active").text }, { value: "past_due", text: Labels.subscription("past_due").text },
                                     { value: "suspended", text: Labels.subscription("suspended").text }]
                                     onActivated: page.status = currentValue } }
                FbButton { Layout.alignment: Qt.AlignBottom; text: qsTr("Generate"); iconName: "chart"; onClicked: page.run() }
                Item { Layout.fillWidth: true }
            }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.rows
            columns: page.columns
            emptyIcon: "chart"
            emptyTitle: qsTr("No establishment")
        }
    }
}
