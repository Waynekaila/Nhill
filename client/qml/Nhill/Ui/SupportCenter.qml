import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core

// Support tickets of the signed-in user (owners and administrators see every ticket
// of the establishment). Shared by Nhill Business and Nhill POS.
ColumnLayout {
    id: root
    property string source: "business"           // business | pos
    property var failHandler: function(r) {}
    property var toastHandler: function(text, tone) {}
    property var rows: []
    property var current: null
    spacing: Theme.s3
    function reload() { Api.get("/api/v1/support/tickets", function(r) { if (r.ok) root.rows = r.data.items; else root.failHandler(r) }) }
    function openTicket(id) { Api.get("/api/v1/support/tickets/" + id, function(r) { if (r.ok) { root.current = r.data; detail.open() } else root.failHandler(r) }) }
    Component.onCompleted: reload()

    RowLayout {
        Layout.fillWidth: true
        Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
               text: qsTr("Report a problem or ask a question to the Nhill team. Do not include passwords, establishment codes or card numbers.") }
        FbButton { text: qsTr("New request"); iconName: "plus"; onClicked: creator.start() }
    }
    // Publisher contact, useful when the request cannot be sent from here.
    Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.StyledText
        color: Theme.textMuted; linkColor: Theme.primary
        font.family: Theme.font; font.pixelSize: Theme.fs_sm
        text: qsTr("You can also write to the %1 team: %2").arg(Tools.publisherName)
                  .arg("<a href=\"mailto:" + Tools.supportEmail + "\">" + Tools.supportEmail + "</a>")
        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
        HoverHandler { cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor }
    }
    DataTable {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 360
        rows: root.rows
        titleColumn: 1; subtitleColumn: 3; chipColumn: 4
        columns: [
            { title: "#", width: 0.4, mono: true, value: function(t) { return String(t.number) } },
            { title: qsTr("Subject"), width: 2.2, strong: true, value: function(t) { return t.subject } },
            { title: qsTr("Category"), width: 0.9, muted: true, value: function(t) { return Labels.ticketCategory(t.category) } },
            { title: qsTr("Last message"), width: 1.1, muted: true, value: function(t) { return Fmt.dateTime(t.last_message_at) } },
            { title: qsTr("Status"), width: 1, chip: function(t) { return Labels.ticketStatus(t.status) } }
        ]
        emptyIcon: "info"
        emptyTitle: qsTr("No request")
        emptyText: qsTr("Your requests to the Nhill team will appear here.")
        onRowClicked: function(t) { root.openTicket(t.id) }
    }
    FbDialog {
        id: creator
        title: qsTr("New request to the Nhill team")
        preferredWidth: 600
        function start() { category.value = "bug"; severity.value = "normal"; subject.text = ""; body.text = ""; open() }
        RowLayout {
            Layout.fillWidth: true
            Field { label: qsTr("Type"); FbComboBox { id: category; Layout.fillWidth: true; model: Labels.ticketCategories() } }
            Field { label: qsTr("Urgency"); FbComboBox { id: severity; Layout.fillWidth: true; model: Labels.severities() } }
        }
        Field { label: qsTr("Subject"); required: true; FbTextField { id: subject; maximumLength: 160 } }
        Field { label: qsTr("Description"); required: true; help: qsTr("What did you do, what happened, what did you expect?"); FbTextArea { id: body } }
        footer: [
            FbButton { text: qsTr("Send"); enabled: subject.text.trim().length >= 3 && body.text.trim().length > 0; onClicked: {
                    Api.send("POST", "/api/v1/support/tickets", { category: category.value, severity: severity.value, subject: subject.text.trim(),
                              body: body.text.trim(), source: root.source, app_version: Qt.application.version, platform: Qt.platform.os }, function(r) {
                        if (r.ok) { creator.close(); root.reload(); root.toastHandler(qsTr("Request #%1 sent").arg(r.data.number), "success") } else root.failHandler(r) })
                } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: creator.close() }
        ]
    }
    FbDialog {
        id: detail
        preferredWidth: 640
        title: root.current ? "#" + root.current.number + " · " + root.current.subject : ""
        subtitle: root.current ? Labels.ticketStatus(root.current.status).text : ""
        Repeater {
            model: root.current ? root.current.messages : []
            delegate: Rectangle {
                Layout.fillWidth: true
                readonly property bool system: modelData.author_type === "system"
                readonly property bool nhill: modelData.author_type === "admin"
                implicitHeight: col.implicitHeight + 2 * Theme.s3
                radius: Theme.radius
                color: system ? "transparent" : nhill ? Theme.primarySoft : Theme.surfaceAlt
                ColumnLayout {
                    id: col
                    anchors.fill: parent
                    anchors.margins: Theme.s3
                    spacing: 2
                    Text { text: (parent.parent.system ? qsTr("Update") : modelData.author) + " · " + Fmt.dateTime(modelData.created_at); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs }
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText; text: modelData.body; color: Theme.text; font.family: Theme.font }
                }
            }
        }
        Field { visible: !!root.current && root.current.status !== "closed"; label: qsTr("Your reply"); FbTextArea { id: reply } }
        footer: [
            FbButton { visible: !!root.current && root.current.status !== "closed"; text: qsTr("Send"); enabled: reply.text.trim().length > 0
                       onClicked: Api.send("POST", "/api/v1/support/tickets/" + root.current.id + "/messages", { body: reply.text.trim() }, function(r) {
                           if (r.ok) { reply.text = ""; root.openTicket(root.current.id); root.reload() } else root.failHandler(r) }) },
            FbButton { visible: !!root.current && root.current.status !== "closed"; text: qsTr("Close the request"); variant: "ghost"
                       onClicked: Api.send("POST", "/api/v1/support/tickets/" + root.current.id + "/close", {}, function(r) {
                           if (r.ok) { detail.close(); root.reload() } else root.failHandler(r) }) },
            FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: detail.close() }
        ]
    }
}
