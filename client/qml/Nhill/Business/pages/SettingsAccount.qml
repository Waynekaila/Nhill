import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

ColumnLayout {
    id: root
    spacing: Theme.s4
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            SectionTitle { text: qsTr("Profile") }
            GridLayout {
                Layout.fillWidth: true
                columns: Theme.compact ? 1 : 2
                columnSpacing: Theme.s3; rowSpacing: Theme.s3
                Field { label: qsTr("Full name"); FbTextField { id: fullName; text: Session.me.user.full_name || "" } }
                Field { label: qsTr("Email"); FbTextField { text: Session.me.user.email || ""; enabled: false } }
                Field { label: qsTr("Phone"); FbTextField { id: myPhone; text: Session.me.user.phone || ""; placeholderText: "+243…" } }
                Field { label: qsTr("Role"); FbTextField { text: Labels.role(Session.role); enabled: false } }
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                FbButton { text: qsTr("Save"); onClicked: {
                        var b = { full_name: fullName.text.trim() }
                        if (myPhone.text.trim()) b.phone = myPhone.text.trim()
                        Api.send("PATCH", "/api/v1/me", b, function(r) { if (r.ok) { Nav.toast(qsTr("Saved"), "success"); Session.reload() } else Nav.fail(r) })
                    } }
            }
        }
    }
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            SectionTitle { text: qsTr("Display") }
            GridLayout {
                Layout.fillWidth: true
                columns: Theme.compact ? 1 : 3
                columnSpacing: Theme.s3; rowSpacing: Theme.s3
                Field { label: qsTr("Language"); FbComboBox { model: [{ value: "fr", text: "Français" }, { value: "en", text: "English" }]; value: Prefs.language; onActivated: Prefs.language = currentValue } }
                Field { label: qsTr("Theme"); FbComboBox { model: [{ value: "system", text: qsTr("Follow the system") }, { value: "light", text: qsTr("Light") }, { value: "dark", text: qsTr("Dark") }]; value: Prefs.theme; onActivated: Prefs.theme = currentValue } }
            }
            Text { color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm; Layout.fillWidth: true; wrapMode: Text.Wrap
                   text: qsTr("Server: %1").arg(Prefs.serverUrl) }
        }
    }
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            SectionTitle { text: qsTr("Password") }
            Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: qsTr("Changing your password signs you out on all your other devices.") }
            GridLayout {
                Layout.fillWidth: true
                columns: Theme.compact ? 1 : 2
                columnSpacing: Theme.s3; rowSpacing: Theme.s3
                Field { label: qsTr("Current password"); FbTextField { id: currentPw; echoMode: TextInput.Password } }
                Field { id: newField; label: qsTr("New password"); help: qsTr("At least 8 characters"); FbTextField { id: newPw; echoMode: TextInput.Password } }
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                FbButton { text: qsTr("Change password"); onClicked: {
                        var issue = Check.password(newPw.text, Session.me.user.email || "")
                        if (issue) { newField.error = Labels.password(issue); return }
                        newField.error = ""
                        Api.send("POST", "/api/v1/me/password", { current_password: currentPw.text, new_password: newPw.text }, function(r) {
                            if (r.ok) { currentPw.text = ""; newPw.text = ""; Nav.toast(qsTr("Password changed. Your other sessions have been closed."), "success") } else Nav.fail(r) })
                    } }
            }
        }
    }
    FbButton { text: qsTr("Sign out"); iconName: "logout"; variant: "secondary"; onClicked: { Session.logout(); Nav.window.openWelcome() } }
}
