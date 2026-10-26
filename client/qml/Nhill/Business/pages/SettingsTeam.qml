import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

ColumnLayout {
    id: root
    spacing: Theme.s4
    property var members: []
    property var invitations: []
    property int seatLimit: 0
    function load() {
        Api.get("/api/v1/members", function(r) {
            if (!r.ok) { Nav.fail(r); return }
            root.members = r.data.members; root.invitations = r.data.invitations; root.seatLimit = r.data.seat_limit
        })
    }
    Component.onCompleted: load()
    RowLayout {
        Layout.fillWidth: true
        Text { Layout.fillWidth: true; color: Theme.textMuted; font.family: Theme.font; wrapMode: Text.Wrap
               text: qsTr("%1 active user(s) and %2 pending invitation(s) out of %3 allowed by your offer.")
                     .arg(root.members.filter(function(m) { return m.status === "active" }).length).arg(root.invitations.length).arg(root.seatLimit) }
        FbButton { text: qsTr("Invite"); iconName: "plus"; onClicked: invite.start() }
    }
    SectionTitle { text: qsTr("Members") }
    DataTable {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(160, Math.min(520, 64 + root.members.length * 52))
        rows: root.members
        titleColumn: 0; subtitleColumn: 1; chipColumn: 2
        columns: [
            { title: qsTr("Name"), width: 1.8, strong: true, value: function(m) { return m.full_name } },
            { title: qsTr("Email"), width: 2, muted: true, value: function(m) { return m.email } },
            { title: qsTr("Role"), width: 1.2, chip: function(m) { return { text: Labels.role(m.role), tone: m.role === "owner" ? "primary" : "neutral" } } },
            { title: qsTr("Status"), width: 1, value: function(m) { return m.status === "active" ? qsTr("Active") : qsTr("Disabled") } },
            { title: qsTr("Last activity"), width: 1.3, muted: true, value: function(m) { return m.last_seen_at ? Fmt.dateTime(m.last_seen_at) : "—" } }
        ]
        onRowClicked: function(m) { if (m.role !== "owner" && m.user_id !== Session.me.user.id) member.show(m) }
    }
    SectionTitle { text: qsTr("Pending invitations"); visible: root.invitations.length > 0 }
    Repeater {
        model: root.invitations
        FbCard {
            Layout.fillWidth: true
            RowLayout {
                width: parent.width
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: modelData.email; color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold }
                    Text { text: qsTr("%1 · expires on %2").arg(Labels.role(modelData.role)).arg(Fmt.dateTime(modelData.expires_at)); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                }
                FbButton { text: qsTr("Revoke"); variant: "ghost"; iconName: "close"
                           onClicked: Api.send("DELETE", "/api/v1/invitations/" + modelData.id, null, function(r) { if (r.ok) root.load(); else Nav.fail(r) }) }
            }
        }
    }
    FbDialog {
        id: invite
        title: qsTr("Invite a colleague")
        subtitle: qsTr("They will need the establishment code, their email and the activation code below.")
        property var result: null
        property string key: ""
        function start() { result = null; inviteEmail.text = ""; key = Api.newIdempotencyKey(); open() }
        ColumnLayout {
            Layout.fillWidth: true
            visible: !invite.result
            spacing: Theme.s3
            Field { label: qsTr("Email"); required: true; FbTextField { id: inviteEmail; inputMethodHints: Qt.ImhEmailCharactersOnly } }
            Field { label: qsTr("Role"); FbComboBox { id: inviteRole; model: Labels.roles().filter(function(r) { return r.value !== "admin" || Session.role === "owner" }); value: "sales" } }
        }
        ColumnLayout {
            Layout.fillWidth: true
            visible: !!invite.result
            spacing: Theme.s3
            Banner { Layout.fillWidth: true; tone: "warning"; text: qsTr("This activation code is shown only once. Give it to the person directly (in person, by phone or SMS).") }
            Text { Layout.alignment: Qt.AlignHCenter; font.family: Theme.mono; font.pixelSize: 30; font.weight: Font.DemiBold; color: Theme.text; text: invite.result ? invite.result.activation_code : "" }
            Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; color: Theme.textMuted; font.family: Theme.font
                   text: invite.result ? qsTr("For %1 · valid until %2").arg(invite.result.email).arg(Fmt.dateTime(invite.result.expires_at)) : "" }
        }
        footer: [
            FbButton { visible: !invite.result; text: qsTr("Create the invitation"); onClicked: {
                    if (!Check.email(inviteEmail.text)) { Nav.toast(qsTr("Enter a valid email address."), "danger"); return }
                    Api.send("POST", "/api/v1/invitations", { email: inviteEmail.text.trim(), role: inviteRole.value }, function(r) {
                        if (r.ok) { invite.result = r.data; root.load() } else Nav.fail(r) }, invite.key) } },
            FbButton { visible: !!invite.result; text: qsTr("Copy the code"); iconName: "copy"; variant: "secondary"; onClicked: Tools.copyText(invite.result.activation_code) },
            FbButton { text: invite.result ? qsTr("Done") : qsTr("Cancel"); variant: invite.result ? "primary" : "secondary"; onClicked: invite.close() }
        ]
    }
    FbDialog {
        id: member
        property var m: null
        title: m ? m.full_name : ""
        subtitle: m ? m.email : ""
        function show(x) { m = x; memberRole.value = x.role; open() }
        Field { label: qsTr("Role"); FbComboBox { id: memberRole; model: Labels.roles().filter(function(r) { return r.value !== "admin" || Session.role === "owner" }) } }
        footer: [
            FbButton { text: qsTr("Save"); onClicked: Api.send("PATCH", "/api/v1/members/" + member.m.id, { role: memberRole.value }, function(r) { if (r.ok) { member.close(); root.load() } else Nav.fail(r) }) },
            FbButton { text: member.m && member.m.status === "active" ? qsTr("Disable access") : qsTr("Re-enable access"); variant: "secondary"
                       onClicked: Api.send("PATCH", "/api/v1/members/" + member.m.id, { status: member.m.status === "active" ? "disabled" : "active" }, function(r) { if (r.ok) { member.close(); root.load() } else Nav.fail(r) }) },
            FbButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: member.close() }
        ]
    }
}
