import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core

// Shown after signing in with a temporary password given by an administrator: no session
// is opened until the person chooses their own password. `completed` receives the normal
// sign-in answer (a session, or the list of businesses to choose from).
FbDialog {
    id: root
    title: qsTr("Choose your password")
    subtitle: qsTr("You signed in with a temporary password given by your administrator. Choose your own password to continue.")
    preferredWidth: 480
    property string token: ""
    property string email: ""
    property string establishmentId: ""
    property bool busy: false
    property string error: ""
    signal completed(var data)

    function start(changeToken, accountEmail, establishment) {
        token = changeToken; email = accountEmail || ""; establishmentId = establishment || ""
        error = ""; busy = false; pw1.text = ""; pw2.text = ""; pwBox.error = ""; pw2Box.error = ""
        open()
        pw1.forceActiveFocus()
    }
    function submit() {
        var issue = Check.password(pw1.text, email)
        pwBox.error = issue ? Labels.password(issue) : ""
        pw2Box.error = !issue && pw1.text !== pw2.text ? qsTr("The two passwords differ.") : ""
        if (pwBox.error || pw2Box.error) return
        busy = true; error = ""
        var body = { change_token: token, new_password: pw1.text }
        if (establishmentId) body.establishment_id = establishmentId
        Api.send("POST", "/api/v1/auth/password/change-required", body, function(r) {
            busy = false
            if (!r.ok) {
                if (r.error && r.error.details && r.error.details.field === "new_password") { pwBox.error = Errors.message(r); return }
                error = Errors.message(r)
                return
            }
            pw1.text = ""; pw2.text = ""
            root.close()
            root.completed(r.data)
        })
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.s3
        Field {
            id: pwBox
            label: qsTr("New password")
            help: qsTr("At least 8 characters, different from the temporary password.")
            FbTextField { id: pw1; echoMode: TextInput.Password }
        }
        Field {
            id: pw2Box
            label: qsTr("Confirm the new password")
            FbTextField { id: pw2; echoMode: TextInput.Password; onAccepted: root.submit() }
        }
        Banner { visible: root.error.length > 0; Layout.fillWidth: true; tone: "danger"; text: root.error }
    }
    footer: [
        FbButton { text: qsTr("Save and continue"); busy: root.busy; onClicked: root.submit() },
        FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: root.close() }
    ]
}
