import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

FbPage {
    id: page
    property string ticketId: ""
    property var t: null
    title: t ? "#" + t.number + " · " + t.subject : qsTr("Ticket")
    subtitle: t ? t.establishment + " · " + Labels.ticketCategory(t.category) + " · " + t.source + (t.app_version ? " " + t.app_version : "") + (t.platform ? " · " + t.platform : "") : ""
    scrollable: true
    function reload() { Api.get("/api/v1/admin/tickets/" + ticketId, function(r) { if (r.ok) page.t = r.data; else ConsoleNav.fail(r) }) }
    function update(b) { Api.send("PATCH", "/api/v1/admin/tickets/" + ticketId, b, function(r) { if (r.ok) page.reload(); else ConsoleNav.fail(r) }) }
    Component.onCompleted: reload()
    actions: [
        FbButton { text: qsTr("Back"); variant: "ghost"; iconName: "close"; onClicked: ConsoleNav.shell.navigate("tickets") },
        FbButton { visible: !!page.t && !page.t.assigned_to; text: qsTr("Take it"); variant: "secondary"; iconName: "user"; onClicked: page.update({ assigned_to: ConsoleNav.me.id }) },
        FbButton { visible: !!page.t && page.t.status !== "resolved" && page.t.status !== "closed"; text: qsTr("Mark resolved"); iconName: "check"; onClicked: page.update({ status: "resolved" }) }
    ]
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.s3
        FbChip { visible: !!page.t; text: page.t ? Labels.ticketStatus(page.t.status).text : ""; tone: page.t ? Labels.ticketStatus(page.t.status).tone : "neutral" }
        FbChip { visible: !!page.t; text: page.t ? Labels.severity(page.t.severity).text : ""; tone: page.t ? Labels.severity(page.t.severity).tone : "neutral" }
        Text { text: page.t ? qsTr("Opened %1 by %2").arg(Fmt.dateTime(page.t.created_at)).arg(page.t.created_by_name || "—") : ""; color: Theme.textMuted; font.family: Theme.font }
        Item { Layout.fillWidth: true }
        FbComboBox { id: statusBox; Layout.preferredWidth: 220; model: Labels.ticketStatuses(); value: page.t ? page.t.status : undefined
                     onActivated: if (page.t && value !== page.t.status) page.update({ status: value }) }
        FbComboBox { Layout.preferredWidth: 160; model: Labels.severities(); value: page.t ? page.t.severity : undefined
                     onActivated: if (page.t && value !== page.t.severity) page.update({ severity: value }) }
    }
    Repeater {
        model: page.t ? page.t.messages : []
        delegate: Rectangle {
            Layout.fillWidth: true
            readonly property bool mine: modelData.author_type === "admin"
            readonly property bool system: modelData.author_type === "system"
            implicitHeight: msg.implicitHeight + 2 * Theme.s3
            radius: Theme.radius
            color: modelData.internal ? Theme.warningSoft : system ? "transparent" : mine ? Theme.primarySoft : Theme.surface
            border.color: system ? "transparent" : Theme.line
            ColumnLayout {
                id: msg
                anchors.fill: parent
                anchors.margins: Theme.s3
                spacing: 4
                Text { text: (modelData.internal ? qsTr("Internal note") + " · " : "") + (modelData.author || (parent.parent.system ? qsTr("System") : "")) + " · " + Fmt.dateTime(modelData.created_at)
                       color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs }
                Text { Layout.fillWidth: true; text: modelData.body; wrapMode: Text.Wrap; color: parent.parent.system ? Theme.textFaint : Theme.text
                       font.family: Theme.font; font.pixelSize: parent.parent.system ? Theme.fs_sm : Theme.fs_md; textFormat: Text.PlainText }
            }
        }
    }
    FbCard {
        Layout.fillWidth: true
        visible: !!page.t && page.t.status !== "closed"
        ColumnLayout {
            width: parent.width
            spacing: Theme.s2
            Field { label: qsTr("Reply"); FbTextArea { id: reply; Layout.fillWidth: true } }
            RowLayout {
                FbSwitch { id: internal; text: qsTr("Internal note (not sent to the customer)") }
                Item { Layout.fillWidth: true }
                FbButton { text: internal.checked ? qsTr("Add the note") : qsTr("Send the reply"); enabled: reply.text.trim().length > 0
                           onClicked: Api.send("POST", "/api/v1/admin/tickets/" + page.ticketId + "/messages", { body: reply.text.trim(), internal: internal.checked }, function(r) {
                               if (r.ok) { reply.text = ""; internal.checked = false; page.reload() } else ConsoleNav.fail(r) }) }
            }
        }
    }
}
