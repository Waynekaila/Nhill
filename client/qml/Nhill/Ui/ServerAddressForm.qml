import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core

// Address of the Nhill server (API), entered by the user (3.0.0): no address is built
// into the applications. The address is tested first (HTTPS required, the server must
// answer as a Nhill server with a compatible version) and saved only after that.
// Changing it ends the current session on this device: no token goes to another server.
ColumnLayout {
    id: root
    spacing: Theme.s3
    property string initial: Prefs.serverUrl
    property string saveText: qsTr("Use this server")
    property bool busy: false
    property string status: ""          // "", "ok" or "error"
    property string message: ""
    property string checkedUrl: ""
    signal saved(string url)

    function newer(a, b) {
        var x = String(a).split("."), y = String(b).split(".")
        for (var i = 0; i < 3; ++i) { var d = (parseInt(x[i]) || 0) - (parseInt(y[i]) || 0); if (d !== 0) return d > 0 }
        return false
    }
    function check() {
        if (field.text.trim().length === 0) { status = "error"; message = qsTr("Enter the address given by your provider."); return }
        busy = true; status = ""; message = ""; checkedUrl = ""
        Api.probeServer(field.text, function(r) {
            busy = false
            if (!r.ok) { status = "error"; message = Errors.message(r); return }
            if (r.data.min_client_version && newer(r.data.min_client_version, Qt.application.version)) {
                status = "error"
                message = qsTr("This server requires version %1 of the application or later. Install the update first.").arg(r.data.min_client_version)
                return
            }
            status = "ok"
            checkedUrl = r.data.url
            field.text = r.data.url
            message = qsTr("Connection verified: Nhill server %1.").arg(r.data.version)
        })
    }
    function save() {
        if (status !== "ok" || !checkedUrl) return
        Prefs.serverUrl = checkedUrl
        root.saved(checkedUrl)
    }

    Field {
        label: qsTr("Server address (API)")
        help: qsTr("Given by your provider or your administrator, for example https://api.example.com. Secure connections (HTTPS) only.")
        FbTextField {
            id: field
            text: root.initial
            monospace: true
            placeholderText: "https://api.example.com"
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            onTextChanged: if (text !== root.checkedUrl) { root.status = ""; root.checkedUrl = "" }
            onAccepted: root.status === "ok" ? root.save() : root.check()
        }
    }
    Banner { visible: root.status !== ""; tone: root.status === "ok" ? "success" : "danger"; text: root.message }
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.s2
        Item { Layout.fillWidth: true }
        FbButton { text: qsTr("Test the connection"); variant: "secondary"; busy: root.busy; onClicked: root.check() }
        FbButton { text: root.saveText; enabled: root.status === "ok"; onClicked: root.save() }
    }
}
