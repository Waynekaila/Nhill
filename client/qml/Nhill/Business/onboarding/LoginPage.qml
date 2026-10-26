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
        var body = { email: email.text, password: password.text }
        if (establishmentId) body.establishment_id = establishmentId
        Api.send("POST", "/api/v1/auth/login", body, function(r) {
            busy = false
            if (!r.ok) { ApplicationWindow.window.fail(r); return }
            if (r.data.choose_establishment) { page.choices = r.data.choose_establishment; return }
            Session.start(r.data, function(ok) { if (ok) ApplicationWindow.window.openShell() })
        })
    }

    Text { text: qsTr("Sign in"); font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.DemiBold; color: Theme.text }
    Banner { visible: page.notice.length > 0; text: page.notice; tone: "warning" }
    Field {
        label: qsTr("E-mail")
        FbTextField { id: email; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase; onTextChanged: invalid = false; focus: true }
    }
    Field {
        label: qsTr("Password")
        FbTextField { id: password; echoMode: TextInput.Password; onAccepted: page.submit() }
    }
    FbButton { text: qsTr("Sign in"); Layout.fillWidth: true; busy: page.busy; enabled: email.text.length > 0 && password.text.length > 0; onClicked: page.submit() }
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
}
