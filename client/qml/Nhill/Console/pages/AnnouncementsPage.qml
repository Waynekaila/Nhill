import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

FbPage {
    id: page
    title: qsTr("Announcements")
    subtitle: qsTr("Messages shown to establishments (maintenance, new versions)")
    property var rows: []
    function reload() { Api.get("/api/v1/admin/announcements", function(r) { if (r.ok) page.rows = r.data.items; else ConsoleNav.fail(r) }) }
    Component.onCompleted: reload()
    actions: [ FbButton { visible: ConsoleNav.can("support"); text: qsTr("New announcement"); iconName: "plus"; onClicked: editor.open() } ]
    DataTable {
        anchors.fill: parent
        rows: page.rows
        columns: [
            { title: qsTr("From"), width: 1.1, muted: true, value: function(a) { return Fmt.dateTime(a.starts_at) } },
            { title: qsTr("Until"), width: 1.1, muted: true, value: function(a) { return a.ends_at ? Fmt.dateTime(a.ends_at) : "—" } },
            { title: qsTr("Type"), width: 0.8, value: function(a) { return a.kind } },
            { title: qsTr("Title"), width: 2.4, strong: true, value: function(a) { return a.title_fr } },
            { title: qsTr("Min. version"), width: 0.8, mono: true, value: function(a) { return a.min_client_version || "" } }
        ]
        emptyIcon: "bell"
        emptyTitle: qsTr("No announcement")
    }
    FbDialog {
        id: editor
        title: qsTr("New announcement")
        preferredWidth: 620
        Field { label: qsTr("Type"); FbComboBox { id: kind; value: "info"; model: [{ value: "info", text: qsTr("Information") }, { value: "maintenance", text: qsTr("Maintenance") }, { value: "release", text: qsTr("New version") }] } }
        Field { label: qsTr("Title (French)"); required: true; FbTextField { id: titleFr } }
        Field { label: qsTr("Title (English)"); required: true; FbTextField { id: titleEn } }
        Field { label: qsTr("Text (French)"); FbTextArea { id: bodyFr } }
        Field { label: qsTr("Text (English)"); FbTextArea { id: bodyEn } }
        footer: [
            FbButton { text: qsTr("Publish"); onClicked: Api.send("POST", "/api/v1/admin/announcements",
                           { kind: kind.value, title_fr: titleFr.text, title_en: titleEn.text, body_fr: bodyFr.text, body_en: bodyEn.text },
                           function(r) { if (r.ok) { editor.close(); page.reload() } else ConsoleNav.fail(r) }) },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: editor.close() }
        ]
    }
}
