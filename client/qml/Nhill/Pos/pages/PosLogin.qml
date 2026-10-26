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
            if (r.data.choose_establishment) { root.choices = r.data.choose_establishment; return }
            Session.start(r.data, function(ok) {
                if (!ok) return
                password.text = ""
                PosNav.window.loadConfig(function() { PosNav.session ? PosNav.window.openSale() : PosNav.window.openHome() })
            })
        })
    }
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 48)
        spacing: Theme.s4
        LogoMark { app: "pos"; Layout.preferredHeight: 44; Layout.preferredWidth: 260; Layout.alignment: Qt.AlignHCenter }
        Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("Point of sale"); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_lg }
        FbCard {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.s3
                Banner { Layout.fillWidth: true; visible: root.notice.length > 0; text: root.notice; tone: "warning" }
                Field { label: qsTr("E-mail"); FbTextField { id: email; focus: true; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase } }
                Field { label: qsTr("Password"); FbTextField { id: password; echoMode: TextInput.Password; onAccepted: root.submit() } }
                FbButton { Layout.fillWidth: true; text: qsTr("Sign in"); busy: root.busy; enabled: email.text.length > 0 && password.text.length > 0; onClicked: root.submit() }
                Repeater {
                    model: root.choices
                    delegate: FbButton { Layout.fillWidth: true; variant: "secondary"; text: modelData.name + " · " + Labels.role(modelData.role); onClicked: root.submit(modelData.id) }
                }
            }
        }
        Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: Theme.textFaint; font.family: Theme.font; font.pixelSize: Theme.fs_xs
               text: qsTr("Use the account created for you in Nhill Business (cashier or cash manager). Server: %1").arg(Prefs.serverUrl) }
        FbButton { Layout.alignment: Qt.AlignHCenter; text: qsTr("Change server"); variant: "ghost"; iconName: "settings"; onClicked: serverDialog.open() }
    }
    FbDialog {
        id: serverDialog
        title: qsTr("Server")
        Field { label: qsTr("Server address"); FbTextField { id: serverField; text: Prefs.serverUrl; monospace: true } }
        footer: [ FbButton { text: qsTr("Save"); onClicked: { Prefs.serverUrl = serverField.text.trim(); serverDialog.close() } },
                  FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: serverDialog.close() } ]
    }
}
