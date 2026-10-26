import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Technical error journal: server faults (HTTP 5xx) and errors reported by the
// applications, grouped by fingerprint.
FbPage {
    id: page
    title: qsTr("Technical errors")
    subtitle: qsTr("Server faults and application errors, grouped — for diagnosis only")
    property var rows: []
    property string source: ""
    property bool resolved: false
    function reload() {
        var q = "?limit=200" + (source ? "&source=" + source : "") + (resolved ? "&include_resolved=1" : "")
        Api.get("/api/v1/admin/errors" + q, function(r) { if (r.ok) page.rows = r.data.items; else ConsoleNav.fail(r) })
    }
    Component.onCompleted: reload()
    actions: [ FbButton { text: qsTr("Refresh"); variant: "secondary"; iconName: "refresh"; onClicked: page.reload() } ]
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        RowLayout {
            TabStrip { model: [{ value: "", text: qsTr("All") }, { value: "server", text: qsTr("Server") }, { value: "business", text: "Business" },
                               { value: "pos", text: "POS" }, { value: "console", text: "Console" }]
                       current: page.source; onSelected: function(v) { page.source = v; page.reload() } }
            Item { Layout.fillWidth: true }
            FbSwitch { text: qsTr("Include resolved"); checked: page.resolved; onToggled: { page.resolved = checked; page.reload() } }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            rows: page.rows
            columns: [
                { title: qsTr("Last seen"), width: 1.1, muted: true, value: function(e) { return Fmt.dateTime(e.last_seen) } },
                { title: qsTr("Source"), width: 0.7, value: function(e) { return e.source } },
                { title: qsTr("Code"), width: 1.6, mono: true, value: function(e) { return e.code } },
                { title: qsTr("Message"), width: 2.6, value: function(e) { return e.message } },
                { title: qsTr("Count"), width: 0.6, align: "right", value: function(e) { return String(e.occurrences) } },
                { title: qsTr("Version"), width: 0.8, muted: true, value: function(e) { return e.app_version || "" } },
                { title: qsTr("Status"), width: 0.8, chip: function(e) { return e.resolved_at ? { text: qsTr("Resolved"), tone: "success" } : { text: e.level, tone: e.level === "warning" ? "warning" : "danger" } } }
            ]
            emptyIcon: "check"
            emptyTitle: qsTr("No error")
            onRowClicked: function(e) { detail.show(e) }
        }
    }
    FbDialog {
        id: detail
        property var e: null
        preferredWidth: 680
        title: e ? e.code : ""
        subtitle: e ? qsTr("%1 occurrence(s) · first %2 · last %3").arg(e.occurrences).arg(Fmt.dateTime(e.first_seen)).arg(Fmt.dateTime(e.last_seen)) : ""
        function show(x) { e = x; open() }
        Text { Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; text: detail.e ? detail.e.message : ""; font.family: Theme.mono; font.pixelSize: Theme.fs_sm; color: Theme.text; textFormat: Text.PlainText }
        Text { Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; text: detail.e ? JSON.stringify(detail.e.context, null, 2) : ""; font.family: Theme.mono; font.pixelSize: Theme.fs_xs; color: Theme.textMuted; textFormat: Text.PlainText }
        Text { visible: !!detail.e && !!detail.e.establishment; text: detail.e && detail.e.establishment ? qsTr("Establishment: %1").arg(detail.e.establishment) : ""; color: Theme.textMuted; font.family: Theme.font }
        footer: [
            FbButton { visible: !!detail.e && !detail.e.resolved_at && ConsoleNav.can("support"); text: qsTr("Mark resolved"); iconName: "check"
                       onClicked: Api.send("POST", "/api/v1/admin/errors/" + detail.e.id + "/resolve", {}, function(r) { if (r.ok) { detail.close(); page.reload() } else ConsoleNav.fail(r) }) },
            FbButton { text: qsTr("Copy"); variant: "secondary"; iconName: "copy"; onClicked: { Tools.copyText(detail.e.code + "\n" + detail.e.message + "\n" + JSON.stringify(detail.e.context)); ConsoleNav.toast(qsTr("Copied"), "success") } },
            FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: detail.close() }
        ]
    }
}
