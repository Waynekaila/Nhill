import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Platform audit log. Details of business actions are not returned by the server.
FbPage {
    id: page
    title: qsTr("Audit log")
    subtitle: qsTr("Platform and security events; details of business actions are withheld")
    property var rows: []
    property string action: ""
    function reload() { Api.get("/api/v1/admin/audit?limit=300" + (action ? "&action=" + encodeURIComponent(action) : ""), function(r) { if (r.ok) page.rows = r.data.items; else ConsoleNav.fail(r) }) }
    Component.onCompleted: reload()
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        RowLayout {
            FbComboBox { Layout.preferredWidth: 280; value: ""
                         model: [{ value: "", text: qsTr("All events") }, { value: "admin.", text: qsTr("Administrators") }, { value: "auth.", text: qsTr("Sign-ins") },
                                 { value: "subscription.", text: qsTr("Subscriptions") }, { value: "establishment.code", text: qsTr("Establishment codes") }, { value: "job.", text: qsTr("Scheduled jobs") }]
                         onActivated: { page.action = currentValue; page.reload() } }
            Item { Layout.fillWidth: true }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            rows: page.rows
            columns: [
                { title: qsTr("When"), width: 1.1, muted: true, value: function(a) { return Fmt.dateTime(a.created_at) } },
                { title: qsTr("Actor"), width: 0.8, value: function(a) { return a.actor_type } },
                { title: qsTr("Action"), width: 1.8, mono: true, strong: true, value: function(a) { return a.action } },
                { title: qsTr("Establishment"), width: 1.4, mono: true, muted: true, value: function(a) { return a.establishment_id ? a.establishment_id.slice(0, 8) : "—" } },
                { title: qsTr("Details"), width: 2.6, muted: true, value: function(a) { return a.details ? JSON.stringify(a.details) : qsTr("(withheld)") } },
                { title: qsTr("IP"), width: 0.9, mono: true, muted: true, value: function(a) { return a.ip } }
            ]
            onRowClicked: function(a) { if (a.establishment_id) ConsoleNav.push("pages/EstablishmentPage.qml", { establishmentId: a.establishment_id }) }
        }
    }
}
