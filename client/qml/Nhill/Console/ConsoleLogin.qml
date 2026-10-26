import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

Rectangle {
    id: root
    color: Theme.bg
    property string mode: "login"          // login | setup | confirm
    property var setupResult: null
    property bool busy: false
    property string presetEmail: ""
    property string presetToken: ""

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 48)
        spacing: Theme.s4
        LogoMark { app: "console"; Layout.preferredHeight: 44; Layout.preferredWidth: 300; Layout.alignment: Qt.AlignHCenter }
        Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("Administration console"); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_lg }
        FbCard {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.s3
                TabStrip { Layout.fillWidth: true; model: [{ value: "login", text: qsTr("Sign in") }, { value: "setup", text: qsTr("First access") }]
                           current: root.mode === "confirm" ? "setup" : root.mode; onSelected: function(v) { root.mode = v } }
                Field { label: qsTr("Email"); FbTextField { id: email; text: root.presetEmail; inputMethodHints: Qt.ImhEmailCharactersOnly } }
                Field { visible: root.mode !== "login"; label: qsTr("Setup token"); help: qsTr("Given once by the person who created your account (valid 30 minutes).")
                        FbTextField { id: setupToken; text: root.presetToken; monospace: true; enabled: root.mode === "setup" } }
                Field { visible: root.mode !== "confirm"; label: root.mode === "setup" ? qsTr("Choose a password (8 characters minimum)") : qsTr("Password")
                        FbTextField { id: password; echoMode: TextInput.Password } }
                ColumnLayout {
                    visible: root.mode === "confirm" && !!root.setupResult
                    Layout.fillWidth: true
                    spacing: Theme.s2
                    Banner { Layout.fillWidth: true; tone: "warning"; text: qsTr("Add this key to an authenticator app (TOTP), then enter the 6-digit code it shows. The key is displayed only now.") }
                    Text { Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; font.family: Theme.mono; font.pixelSize: Theme.fs_lg; color: Theme.text
                           text: root.setupResult ? root.setupResult.totp_secret.replace(/(.{4})/g, "$1 ") : "" }
                    FbButton { text: qsTr("Copy the setup link (otpauth://)"); iconName: "copy"; variant: "secondary"
                               onClicked: { Tools.copyText(root.setupResult.otpauth_uri); ConsoleNav.toast(qsTr("Copied"), "success") } }
                }
                Field { visible: root.mode !== "setup"; label: qsTr("Authenticator code"); FbTextField { id: totp; monospace: true; maximumLength: 6; inputMethodHints: Qt.ImhDigitsOnly; onAccepted: go.clicked() } }
                FbButton {
                    id: go
                    Layout.fillWidth: true
                    busy: root.busy
                    text: root.mode === "login" ? qsTr("Sign in") : root.mode === "setup" ? qsTr("Continue") : qsTr("Activate my account")
                    onClicked: {
                        root.busy = true
                        var done = function(r) { root.busy = false; if (!r.ok) ConsoleNav.fail(r) }
                        if (root.mode === "login")
                            Api.send("POST", "/api/v1/admin/auth/login", { email: email.text.trim(), password: password.text, totp_code: totp.text.trim() }, function(r) {
                                done(r); if (r.ok) { password.text = ""; totp.text = ""; ConsoleNav.window.signIn(r.data.token) } })
                        else if (root.mode === "setup")
                            Api.send("POST", "/api/v1/admin/auth/setup", { email: email.text.trim(), setup_token: setupToken.text.trim(), password: password.text }, function(r) {
                                done(r); if (r.ok) { root.setupResult = r.data; root.mode = "confirm" } })
                        else
                            Api.send("POST", "/api/v1/admin/auth/setup/confirm", { email: email.text.trim(), setup_token: setupToken.text.trim(), totp_code: totp.text.trim() }, function(r) {
                                done(r); if (r.ok) { root.setupResult = null; root.mode = "login"; totp.text = ""; ConsoleNav.toast(qsTr("Account activated. Sign in with your password and a new code."), "success") } })
                    }
                }
            }
        }
        Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: Theme.textFaint; font.family: Theme.font; font.pixelSize: Theme.fs_xs
               text: qsTr("Server: %1 · Every action in this console is recorded in the audit log.").arg(Prefs.serverUrl) }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            FbButton { text: qsTr("Change server"); variant: "ghost"; iconName: "settings"; onClicked: serverDialog.open() }
            FbButton { text: qsTr("Install a new server"); variant: "ghost"; iconName: "shield"; onClicked: ConsoleNav.window.openSetup() }
        }
    }
    FbDialog {
        id: serverDialog
        title: qsTr("Server")
        subtitle: qsTr("Address of the Nhill server administered by this console.")
        Field {
            label: qsTr("Server address")
            FbTextField { id: serverField; text: Prefs.serverUrl; monospace: true; placeholderText: "https://api.example.com" }
        }
        footer: [
            FbButton { text: qsTr("Save"); onClicked: {
                Prefs.serverUrl = serverField.text.trim(); serverDialog.close()
                Api.get("/health", function(r) { if (r.ok && r.data && r.data.status === "setup") ConsoleNav.window.openSetup() }) } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: serverDialog.close() }
        ]
    }
}
