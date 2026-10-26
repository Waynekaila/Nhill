import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Pos

// A supervisor (cash manager, manager, administrator) authorises one operation with
// their own credentials. The server issues a one-time token valid a few minutes.
FbDialog {
    id: dlg
    property string action: ""
    property string sessionId: ""
    property var callback: null
    property bool busy: false
    title: qsTr("Supervisor approval")
    subtitle: actionText(action)
    function actionText(a) {
        switch (a) {
        case "discount": return qsTr("Discount above the allowed limit")
        case "price_override": return qsTr("Price change")
        case "return": return qsTr("Return / refund")
        case "void": return qsTr("Void a receipt")
        case "cash_out": return qsTr("Cash withdrawal")
        case "close_other": return qsTr("Close another cashier's session")
        }
        return a
    }
    function ask(a, s, cb) { action = a; sessionId = s; callback = cb; email.text = ""; password.text = ""; busy = false; open(); email.forceActiveFocus() }
    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font
           text: qsTr("The supervisor types their own e-mail and password. The approval is recorded in the audit log with their name.") }
    Field { label: qsTr("Supervisor e-mail"); FbTextField { id: email; inputMethodHints: Qt.ImhEmailCharactersOnly } }
    Field { label: qsTr("Password"); FbTextField { id: password; echoMode: TextInput.Password; onAccepted: ok.clicked() } }
    footer: [
        FbButton { id: ok; text: qsTr("Approve"); busy: dlg.busy; enabled: email.text.length > 0 && password.text.length > 0
                   onClicked: {
                       dlg.busy = true
                       var b = { email: email.text.trim(), password: password.text, action: dlg.action }
                       if (dlg.sessionId) b.session_id = dlg.sessionId
                       Api.send("POST", "/api/v1/pos/approvals", b, function(r) {
                           dlg.busy = false; password.text = ""
                           if (!r.ok) { PosNav.toast(r.status === 401 ? qsTr("Approval refused: wrong credentials or not a supervisor.") : Errors.message(r), "danger"); return }
                           dlg.close()
                           PosNav.toast(qsTr("Approved by %1").arg(r.data.approver), "success")
                           if (dlg.callback) dlg.callback(r.data.token)
                       }, Api.newIdempotencyKey())
                   } },
        FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: dlg.close() }
    ]
}
