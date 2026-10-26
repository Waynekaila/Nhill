import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Reports")
    subtitle: qsTr("Figures computed by the server from the recorded documents")
    property string kind: "sales"
    property var columns: []
    property var rows: []
    property bool loading: false
    readonly property var kinds: {
        var k = baseKinds.slice()
        if (Session.can("audit.read") && !Session.individual) k.splice(1, 0, { value: "staff", text: qsTr("Activity per employee"), period: true })
        return k
    }
    readonly property var baseKinds: [
        { value: "sales", text: qsTr("Sales journal"), period: true },
        { value: "payments", text: qsTr("Payments"), period: true },
        { value: "expenses", text: qsTr("Expenses"), period: true },
        { value: "products", text: qsTr("Sales by product (estimated margin)"), period: true },
        { value: "stock-movements", text: qsTr("Stock movements"), period: true },
        { value: "receivables", text: qsTr("Customer receivables (today)"), period: false },
        { value: "payables", text: qsTr("Amounts to pay (today)"), period: false }
    ]
    readonly property bool needsPeriod: { for (var i = 0; i < kinds.length; ++i) if (kinds[i].value === kind) return kinds[i].period; return true }
    function query() { return "/api/v1/reports/" + kind + "?from=" + from.value + "&to=" + to.value }
    function run() {
        if (!from.valid || !to.valid || !from.value || !to.value) { Nav.toast(qsTr("Choose a valid period."), "danger"); return }
        loading = true
        Api.get(query(), function(r) {
            loading = false
            if (!r.ok) { Nav.fail(r); return }
            var cur = -1
            r.data.columns.forEach(function(c, i) { if (c.key === "col.currency") cur = i })
            page.columns = r.data.columns.map(function(c, i) {
                return { title: c.label, width: c.type === "text" ? 1.5 : c.type === "date" ? 1.15 : c.type === "money" ? 1.25 : 0.9, align: c.type === "money" || c.type === "qty" || c.type === "int" ? "right" : "left",
                         mono: c.key === "col.number" || c.key === "col.sku",
                         value: function(row) {
                             var v = row[i]
                             if (v === null || v === undefined) return ""
                             if (c.type === "money" && cur >= 0 && row[cur]) return Fmt.amount(v, row[cur])
                             if (c.type === "qty") return Fmt.quantity(v)
                             if (c.type === "date" && v) return Fmt.date(String(v))
                             return String(v)
                         } }
            })
            page.rows = r.data.rows
        })
    }
    function exportAs(fmt) {
        Api.download(query() + "&format=" + fmt, "rapport-" + kind + "-" + from.value + "_" + to.value + "." + fmt,
                     function(r) { if (r.ok) Nav.toast(qsTr("Saved to %1").arg(r.data), "success"); else Nav.fail(r) })
    }
    Component.onCompleted: { from.value = Fmt.monthStart(); to.value = Fmt.today(); run() }
    actions: [
        FbButton { text: qsTr("CSV"); iconName: "download"; variant: "secondary"; onClicked: page.exportAs("csv") },
        FbButton { text: qsTr("PDF"); iconName: "download"; variant: "secondary"; onClicked: page.exportAs("pdf") }
    ]
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        FbCard {
            Layout.fillWidth: true
            GridLayout {
                width: parent.width
                columns: Theme.compact ? 1 : 4
                columnSpacing: Theme.s3; rowSpacing: Theme.s2
                Field { label: qsTr("Report"); Layout.fillWidth: true; Layout.minimumWidth: Theme.compact ? 0 : 280
                        FbComboBox { model: page.kinds; value: page.kind; onActivated: { page.kind = currentValue; page.run() } } }
                Field { label: qsTr("From"); enabled: page.needsPeriod; Layout.preferredWidth: Theme.compact ? -1 : 180; DateField { id: from } }
                Field { label: qsTr("To"); enabled: page.needsPeriod; Layout.preferredWidth: Theme.compact ? -1 : 180; DateField { id: to } }
                RowLayout {
                    Layout.alignment: Qt.AlignBottom
                    FbButton { text: qsTr("This month"); variant: "ghost"; onClicked: { from.value = Fmt.monthStart(); to.value = Fmt.today(); page.run() } }
                    FbButton { text: qsTr("Show"); iconName: "chart"; onClicked: page.run() }
                }
            }
        }
        Text {
            visible: page.kind === "products"
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
            text: qsTr("The estimated margin uses the current purchase price of each product, not the historical cost of the units sold.")
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.rows
            columns: page.columns
            titleColumn: 1; subtitleColumn: 0; trailingColumn: page.columns.length - 1
            emptyIcon: "chart"
            emptyTitle: qsTr("No data for this period")
        }
    }
}
