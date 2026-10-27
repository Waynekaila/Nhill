import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core

// "Forgot your password?" (Nhill Business and Nhill POS sign-in screens).
// Members of an establishment ask its administrator for a temporary password; owners
// and administrators receive a one-time code by e-mail. The server answers the same way
// whatever the address, so this dialog never tells whether an account exists.
FbDialog {
    id: root
    title: qsTr("Forgot your password?")
    preferredWidth: 520
    property int step: 0            // 0: who does what + e-mail, 1: code + new password, 2: done
    property bool busy: false
    property string error: ""
    signal finished(string email)

    function start(email) {
        step = 0; error = ""; busy = false
        emailField.text = email || ""
        codeField.text = ""; pw1.text = ""; pw2.text = ""
        pwBox.error = ""; pw2Box.error = ""
        open()
        emailField.forceActiveFocus()
    }
    function sendCode() {
        if (!Check.email(emailField.text.trim())) { error = qsTr("Enter a valid e-mail address (for example name@company.cd)."); return }
        busy = true; error = ""
        Api.send("POST", "/api/v1/auth/password/forgot", { email: emailField.text.trim() }, function(r) {
            busy = false
            if (!r.ok) { error = Errors.message(r); return }
            step = 1
            codeField.forceActiveFocus()
        })
    }
    function reset() {
        var digits = codeField.text.replace(/[^0-9]/g, "")
        if (digits.length !== 8) { error = qsTr("The code has 8 digits (for example 1234-5678)."); return }
        var issue = Check.password(pw1.text, emailField.text.trim())
        pwBox.error = issue ? Labels.password(issue) : ""
        pw2Box.error = !issue && pw1.text !== pw2.text ? qsTr("The two passwords differ.") : ""
        if (pwBox.error || pw2Box.error) return
        busy = true; error = ""
        Api.send("POST", "/api/v1/auth/password/reset", { email: emailField.text.trim(), code: digits, new_password: pw1.text }, function(r) {
            busy = false
            if (!r.ok) { error = Errors.message(r); return }
            pw1.text = ""; pw2.text = ""; codeField.text = ""
            step = 2
        })
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.s3

        // ---- step 0
        ColumnLayout {
            visible: root.step === 0
            Layout.fillWidth: true
            spacing: Theme.s3
            FbCard {
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s1
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold
                           text: qsTr("You work in a business that uses Nhill") }
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                           text: qsTr("Ask the owner or an administrator of your business. In Nhill Business (Settings › Team) they give you a temporary password; you choose your own when you sign in.") }
                }
            }
            FbCard {
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s1
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold
                           text: qsTr("You are the owner or an administrator") }
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                           text: qsTr("Enter the e-mail address of your account: we send it a one-time code, valid for a few minutes.") }
                }
            }
            Field {
                label: qsTr("E-mail")
                FbTextField { id: emailField; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase; onAccepted: root.sendCode() }
            }
        }

        // ---- step 1
        ColumnLayout {
            visible: root.step === 1
            Layout.fillWidth: true
            spacing: Theme.s3
            Banner {
                Layout.fillWidth: true
                tone: "info"
                text: qsTr("If an account uses %1, an e-mail has just been sent to it: a code for owners and administrators, otherwise the person to contact. Check the spam folder too.").arg(emailField.text.trim())
            }
            Field {
                label: qsTr("Code received by e-mail")
                FbTextField { id: codeField; monospace: true; placeholderText: "1234-5678"; inputMethodHints: Qt.ImhDigitsOnly; maximumLength: 12 }
            }
            Field {
                id: pwBox
                label: qsTr("New password")
                help: qsTr("At least 8 characters. A short sentence is easy to remember.")
                FbTextField { id: pw1; echoMode: TextInput.Password }
            }
            Field {
                id: pw2Box
                label: qsTr("Confirm the new password")
                FbTextField { id: pw2; echoMode: TextInput.Password; onAccepted: root.reset() }
            }
        }

        // ---- step 2
        Banner {
            visible: root.step === 2
            Layout.fillWidth: true
            tone: "success"
            text: qsTr("Password changed. For your security, every open session of this account was closed. Sign in with your new password.")
        }

        Banner { visible: root.error.length > 0; Layout.fillWidth: true; tone: "danger"; text: root.error }
    }

    footer: [
        FbButton { visible: root.step === 0; text: qsTr("Send the code"); busy: root.busy; enabled: emailField.text.trim().length > 0; onClicked: root.sendCode() },
        FbButton { visible: root.step === 1; text: qsTr("Change the password"); busy: root.busy; onClicked: root.reset() },
        FbButton { visible: root.step === 1; text: qsTr("Send a new code"); variant: "ghost"; enabled: !root.busy; onClicked: { root.error = ""; root.step = 0 } },
        FbButton { visible: root.step === 2; text: qsTr("Sign in"); onClicked: { root.close(); root.finished(emailField.text.trim()) } },
        FbButton { visible: root.step < 2; text: qsTr("Cancel"); variant: "secondary"; onClicked: root.close() }
    ]
}
