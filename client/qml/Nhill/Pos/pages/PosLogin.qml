import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Pos

Rectangle {
    id: root
    color: Theme.bg
    property string notice: ""
    property var choices: []
    property bool busy: false
    function submit(establishmentId) {
        busy = true
        var body = { email: email.text.trim(), password: password.text }
        if (establishmentId) body.establishment_id = establishmentId
        Api.send("POST", "/api/v1/auth/login", body, function(r) {
            root.busy = false
            if (!r.ok) { PosNav.fail(r); return }
            if (r.data.password_change_required) {
                changeRequired.start(r.data.change_token, email.text.trim(), establishmentId || "")
                return
            }
            root.signedIn(r.data)
        })
    }
    function signedIn(data) {
        if (data.choose_establishment) { root.choices = data.choose_establishment; return }
        password.text = ""
        Session.remember = remember.checked
        Session.start(data, function(ok) {
            if (!ok) return
            PosNav.window.loadConfig(function() { PosNav.session ? PosNav.window.openSale() : PosNav.window.openHome() })
        })
    }
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 48)
        spacing: Theme.s4
        LogoMark { app: "pos"; Layout.preferredHeight: 44; Layout.preferredWidth: 260; Layout.alignment: Qt.AlignHCenter }
        Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("Point of sale"); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_lg }
        // First connection: the address of the Nhill server is entered (never built in).
        FbCard {
            Layout.fillWidth: true
            visible: !Prefs.serverConfigured
            ColumnLayout {
                width: parent.width
                spacing: Theme.s3
                Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold
                       text: qsTr("Connect this till to your Nhill server") }
                ServerAddressForm { Layout.fillWidth: true; initial: ""; saveText: qsTr("Continue") }
            }
        }
        FbCard {
            Layout.fillWidth: true
            visible: Prefs.serverConfigured
            ColumnLayout {
                width: parent.width
                spacing: Theme.s3
                Banner { Layout.fillWidth: true; visible: root.notice.length > 0; text: root.notice; tone: "warning" }
                Field { label: qsTr("E-mail"); FbTextField { id: email; focus: true; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase; onAccepted: password.forceActiveFocus() } }
                Field { label: qsTr("Password"); FbTextField { id: password; echoMode: TextInput.Password; onAccepted: if (email.text.length > 0 && password.text.length > 0) root.submit() } }
                RememberMeSwitch { id: remember }
                FbButton { Layout.fillWidth: true; text: qsTr("Sign in"); busy: root.busy; enabled: email.text.length > 0 && password.text.length > 0; onClicked: root.submit() }
                FbButton { Layout.alignment: Qt.AlignHCenter; text: qsTr("Forgot your password?"); variant: "ghost"; onClicked: recovery.start(email.text.trim()) }
                Repeater {
                    model: root.choices
                    delegate: FbButton { Layout.fillWidth: true; variant: "secondary"; text: modelData.name + " · " + Labels.role(modelData.role); onClicked: root.submit(modelData.id) }
                }
            }
        }
        Text { visible: Prefs.serverConfigured; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WrapAnywhere; color: Theme.textFaint; font.family: Theme.font; font.pixelSize: Theme.fs_xs
               text: qsTr("Use the account created for you in Nhill Business (cashier or cash manager). Server: %1").arg(Prefs.serverUrl) }
        FbButton { visible: Prefs.serverConfigured; Layout.alignment: Qt.AlignHCenter; text: qsTr("Change server"); variant: "ghost"; iconName: "settings"; onClicked: serverDialog.open() }
    }
    FbDialog {
        id: serverDialog
        title: qsTr("Nhill server")
        subtitle: qsTr("Change the address only if your provider asks you to. The current session of this till ends.")
        ServerAddressForm { Layout.fillWidth: true; onSaved: function(url) { serverDialog.close() } }
        footer: [ FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: serverDialog.close() } ]
    }
    PasswordRecoveryDialog {
        id: recovery
        onFinished: function(address) { email.text = address; password.text = ""; password.forceActiveFocus() }
    }
    PasswordChangeRequiredDialog {
        id: changeRequired
        onCompleted: function(data) { root.signedIn(data) }
    }
}
