import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

OnboardingFrame {
    id: page
    property string notice: ""
    property var choices: []
    property bool busy: false
    formWidth: 440
    onBackRequested: ApplicationWindow.window.openWelcome()

    function submit(establishmentId) {
        if (!Check.email(email.text)) { email.invalid = true; return }
        busy = true
        var body = { email: email.text.trim(), password: password.text }
        if (establishmentId) body.establishment_id = establishmentId
        Api.send("POST", "/api/v1/auth/login", body, function(r) {
            busy = false
            if (!r.ok) { ApplicationWindow.window.fail(r); return }
            if (r.data.password_change_required) {
                changeRequired.start(r.data.change_token, email.text.trim(), establishmentId || "")
                return
            }
            signedIn(r.data)
        })
    }
    // Answer of a sign-in (also after choosing a new password): business choice or session.
    function signedIn(data) {
        if (data.choose_establishment) { page.choices = data.choose_establishment; return }
        password.text = ""
        Session.remember = remember.checked
        Session.start(data, function(ok) { if (ok) ApplicationWindow.window.openShell() })
    }

    Text { text: qsTr("Sign in"); font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.DemiBold; color: Theme.text }
    Banner { visible: page.notice.length > 0; text: page.notice; tone: "warning" }
    Field {
        label: qsTr("E-mail")
        error: email.invalid ? qsTr("Enter a valid e-mail address (for example name@company.cd).") : ""
        FbTextField { id: email; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase; onTextChanged: invalid = false; focus: true }
    }
    Field {
        label: qsTr("Password")
        FbTextField { id: password; echoMode: TextInput.Password; onAccepted: if (email.text.length > 0 && password.text.length > 0) page.submit() }
    }
    RememberMeSwitch { id: remember }
    FbButton { text: qsTr("Sign in"); Layout.fillWidth: true; busy: page.busy; enabled: email.text.length > 0 && password.text.length > 0; onClicked: page.submit() }
    FbButton { text: qsTr("Forgot your password?"); variant: "ghost"; Layout.alignment: Qt.AlignHCenter; onClicked: recovery.start(email.text.trim()) }
    ColumnLayout {
        visible: page.choices.length > 0
        Layout.fillWidth: true
        spacing: Theme.s2
        SectionTitle { text: qsTr("Choose the business") }
        Repeater {
            model: page.choices
            delegate: FbButton {
                Layout.fillWidth: true
                variant: "secondary"
                text: modelData.name + " · " + Labels.role(modelData.role)
                onClicked: page.submit(modelData.id)
            }
        }
    }
    FbButton { text: qsTr("I have an establishment code"); variant: "ghost"; onClicked: ApplicationWindow.window.openJoin() }
    PasswordRecoveryDialog {
        id: recovery
        onFinished: function(address) { email.text = address; password.text = ""; password.forceActiveFocus() }
    }
    PasswordChangeRequiredDialog {
        id: changeRequired
        onCompleted: function(data) { page.signedIn(data) }
    }
}
