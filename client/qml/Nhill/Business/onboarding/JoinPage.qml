import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

// Option A — "My business already exists": establishment code, then the
// person's own credentials. The code alone never opens an account.
OnboardingFrame {
    id: page
    property string ticket: ""
    property string businessName: ""
    property string mode: "login"
    property bool busy: false
    formWidth: 480
    onBackRequested: ticket.length > 0 ? (ticket = "") : ApplicationWindow.window.back()

    function checkCode() {
        if (!Check.establishmentCode(code.text)) { code.invalid = true; codeField.error = qsTr("The code has the form NH-XXXX-XXXX-XXXX."); return }
        busy = true
        Api.send("POST", "/api/v1/join/check-code", { code: code.text }, function(r) {
            busy = false
            if (!r.ok) { codeField.error = Errors.message(r); code.invalid = true; return }
            page.ticket = r.data.join_ticket
            page.businessName = r.data.establishment.trade_name
        })
    }
    function finish(r) {
        busy = false
        if (!r.ok) {
            if (r.error && r.error.code === "join_ticket_expired") page.ticket = ""
            ApplicationWindow.window.fail(r)
            return
        }
        Session.start(r.data, function(ok) { if (ok) ApplicationWindow.window.openShell() })
    }
    function login() {
        busy = true
        Api.send("POST", "/api/v1/join/login", { join_ticket: ticket, email: email.text, password: password.text }, finish)
    }
    function activate() {
        var issue = Check.password(newPassword.text, aEmail.text)
        if (issue.length > 0) { pwField.error = Labels.password(issue); return }
        if (newPassword.text !== confirmPassword.text) { confirmField.error = qsTr("The passwords do not match."); return }
        busy = true
        Api.send("POST", "/api/v1/join/activate", { join_ticket: ticket, email: aEmail.text, activation_code: activation.text,
                                                    full_name: fullName.text, password: newPassword.text }, finish)
    }

    Text { text: qsTr("Join my business"); font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.DemiBold; color: Theme.text }

    // ---- step 1: establishment code
    ColumnLayout {
        visible: page.ticket.length === 0
        Layout.fillWidth: true
        spacing: Theme.s4
        Text {
            text: qsTr("Enter the establishment code given by the owner or the manager of the business.")
            color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_md
            wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        Field {
            id: codeField
            label: qsTr("Establishment code")
            help: qsTr("The code is renewed every 30 days. If it no longer works, ask for the new one.")
            FbTextField {
                id: code
                monospace: true
                font.pixelSize: Theme.fs_lg
                placeholderText: "NH-XXXX-XXXX-XXXX"
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                onTextChanged: { invalid = false; codeField.error = "" }
                onEditingFinished: if (Check.establishmentCode(text)) text = Check.formatCode(text)
                onAccepted: page.checkCode()
            }
        }
        FbButton { text: qsTr("Continue"); Layout.fillWidth: true; busy: page.busy; enabled: code.text.length >= 12; onClicked: page.checkCode() }
    }

    // ---- step 2: personal access
    ColumnLayout {
        visible: page.ticket.length > 0
        Layout.fillWidth: true
        spacing: Theme.s4
        FbCard {
            Layout.fillWidth: true
            RowLayout {
                width: parent.width
                spacing: Theme.s3
                Icon { name: "building"; color: Theme.primary; size: 22 }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text { text: qsTr("Business"); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs }
                    Text { text: page.businessName; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                }
            }
        }
        Banner { tone: "info"; text: qsTr("For your security, the code is not enough: sign in with your own account, or activate the invitation your manager gave you.") }
        TabStrip {
            model: [ { value: "login", text: qsTr("I have an account") }, { value: "activate", text: qsTr("I received an invitation") } ]
            current: page.mode
            onSelected: function(v) { page.mode = v }
        }
        ColumnLayout {
            visible: page.mode === "login"
            Layout.fillWidth: true
            spacing: Theme.s3
            Field { label: qsTr("E-mail"); FbTextField { id: email; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase } }
            Field { label: qsTr("Password"); FbTextField { id: password; echoMode: TextInput.Password; onAccepted: page.login() } }
            FbButton { text: qsTr("Sign in"); Layout.fillWidth: true; busy: page.busy; onClicked: page.login() }
        }
        ColumnLayout {
            visible: page.mode === "activate"
            Layout.fillWidth: true
            spacing: Theme.s3
            Field { label: qsTr("E-mail used for the invitation"); FbTextField { id: aEmail; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase } }
            Field { label: qsTr("Activation code"); help: qsTr("8 digits, for example 1234-5678."); FbTextField { id: activation; monospace: true; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("Full name"); FbTextField { id: fullName } }
            Field { id: pwField; label: qsTr("Choose a password"); help: qsTr("At least 10 characters. A short sentence is easy to remember.")
                    FbTextField { id: newPassword; echoMode: TextInput.Password; onTextChanged: pwField.error = "" } }
            Field { id: confirmField; label: qsTr("Confirm the password"); FbTextField { id: confirmPassword; echoMode: TextInput.Password; onTextChanged: confirmField.error = "" } }
            FbButton { text: qsTr("Activate my access"); Layout.fillWidth: true; busy: page.busy; onClicked: page.activate() }
        }
    }
}
