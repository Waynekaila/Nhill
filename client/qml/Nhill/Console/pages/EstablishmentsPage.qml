import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

FbPage {
    id: page
    title: qsTr("Establishments")
    subtitle: qsTr("Platform records only: the content of invoices, customers and stock is not visible here")
    property var rows: []
    property int total: 0
    property string search: ""
    property string status: ""
    property bool loading: false
    function reload() {
        loading = true
        var q = "/api/v1/admin/establishments?limit=200" + (search ? "&search=" + encodeURIComponent(search) : "") + (status ? "&subscription_status=" + status : "")
        Api.get(q, function(r) { loading = false; if (r.ok) { page.rows = r.data.items; page.total = r.data.total } else ConsoleNav.fail(r) })
    }
    Component.onCompleted: reload()
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        RowLayout {
            SearchField { Layout.preferredWidth: 380; placeholderText: qsTr("Name, e-mail or identifier"); onSearch: function(t) { page.search = t; page.reload() } }
            FbComboBox { Layout.preferredWidth: 220; value: ""; model: [{ value: "", text: qsTr("All subscriptions") }, { value: "trialing", text: Labels.subscription("trialing").text },
                         { value: "active", text: Labels.subscription("active").text }, { value: "past_due", text: Labels.subscription("past_due").text },
                         { value: "suspended", text: Labels.subscription("suspended").text }, { value: "canceled", text: Labels.subscription("canceled").text }]
                         onActivated: { page.status = currentValue; page.reload() } }
            Item { Layout.fillWidth: true }
            Text { text: qsTr("%1 establishment(s)").arg(page.total); color: Theme.textMuted; font.family: Theme.font }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.rows
            columns: [
                { title: qsTr("Name"), width: 2.4, strong: true, value: function(e) { return e.trade_name } },
                { title: qsTr("Type"), width: 1, muted: true, value: function(e) { return e.kind === "individual" ? qsTr("Individual") : qsTr("Company") } },
                { title: qsTr("Country"), width: 0.6, value: function(e) { return e.country_code } },
                { title: qsTr("Users"), width: 0.6, align: "right", value: function(e) { return String(e.members) } },
                { title: qsTr("Plan"), width: 1, muted: true, value: function(e) { return e.plan || qsTr("Trial") } },
                { title: qsTr("Subscription"), width: 1.3, chip: function(e) { return Labels.subscription(e.subscription_status) } },
                { title: qsTr("Until"), width: 1, muted: true, value: function(e) { return Fmt.date(e.period_end) } },
                { title: qsTr("Account"), width: 1, chip: function(e) { return e.status === "active" ? { text: qsTr("Open"), tone: "success" } : { text: qsTr("Suspended"), tone: "danger" } } }
            ]
            emptyIcon: "building"
            emptyTitle: qsTr("No establishment")
            onRowClicked: function(e) { ConsoleNav.push("pages/EstablishmentPage.qml", { establishmentId: e.id }) }
        }
    }
}
