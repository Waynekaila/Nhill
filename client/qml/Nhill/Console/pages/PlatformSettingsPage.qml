import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

FbPage {
    id: page
    title: qsTr("Platform settings")
    subtitle: qsTr("Trial, grace period, code rotation, limits. Changes are audited.")
    scrollable: true
    property var items: []
    function reload() { Api.get("/api/v1/admin/settings", function(r) { if (r.ok) page.items = r.data.items; else ConsoleNav.fail(r) }) }
    Component.onCompleted: reload()
    Repeater {
        model: page.items
        FbCard {
            Layout.fillWidth: true
            RowLayout {
                width: parent.width
                spacing: Theme.s3
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { text: modelData.key; font.family: Theme.mono; color: Theme.text; font.weight: Font.Medium }
                    Text { text: modelData.description; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm; wrapMode: Text.Wrap; Layout.fillWidth: true }
                }
                FbTextField {
                    id: val
                    Layout.preferredWidth: typeof modelData.value === "number" ? 120 : 380
                    text: typeof modelData.value === "number" || typeof modelData.value === "string" ? String(modelData.value) : JSON.stringify(modelData.value)
                    enabled: ConsoleNav.can("superadmin")
                    monospace: true
                }
                FbButton {
                    visible: ConsoleNav.can("superadmin")
                    text: qsTr("Save"); variant: "secondary"
                    onClicked: {
                        var v
                        try { v = typeof modelData.value === "number" ? parseInt(val.text) : JSON.parse(val.text) } catch (e) { ConsoleNav.toast(qsTr("Invalid value."), "danger"); return }
                        Api.send("PUT", "/api/v1/admin/settings/" + modelData.key, { value: v }, function(r) { if (r.ok) { ConsoleNav.toast(qsTr("Saved"), "success"); page.reload() } else ConsoleNav.fail(r) })
                    }
                }
            }
        }
    }
    FbCard {
        Layout.fillWidth: true
        visible: ConsoleNav.can("superadmin")
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            SectionTitle { text: qsTr("Invite an administrator") }
            RowLayout {
                spacing: Theme.s3
                Field { label: qsTr("Full name"); FbTextField { id: adminName } }
                Field { label: qsTr("Email"); FbTextField { id: adminEmail } }
                Field { label: qsTr("Role"); Layout.preferredWidth: 200; Layout.fillWidth: false
                        FbComboBox { id: adminRole; value: "support"; model: [{ value: "support", text: qsTr("Support") }, { value: "billing", text: qsTr("Billing") }, { value: "auditor", text: qsTr("Auditor") }, { value: "superadmin", text: qsTr("Super administrator") }] } }
                FbButton { Layout.alignment: Qt.AlignBottom; text: qsTr("Create"); onClicked: Api.send("POST", "/api/v1/admin/admins",
                               { full_name: adminName.text.trim(), email: adminEmail.text.trim(), role: adminRole.value },
                               function(r) { if (r.ok) { page.token = r.data.setup_token; page.tokenExpires = r.data.expires_at } else ConsoleNav.fail(r) }) }
            }
            Banner { Layout.fillWidth: true; visible: page.token !== ""; tone: "warning"
                     text: qsTr("Setup token (shown once, valid until %1): %2 — give it to the person directly; they choose their password and configure their authenticator under « First access ».").arg(Fmt.dateTime(page.tokenExpires)).arg(page.token)
                     actionText: qsTr("Copy"); onAction: Tools.copyText(page.token) }
        }
    }
    property string token: ""
    property string tokenExpires: ""
}
