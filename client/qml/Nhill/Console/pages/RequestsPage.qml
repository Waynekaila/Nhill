import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// People who tried to create a business that already exists and asked to be attached to it.
FbPage {
    id: page
    title: qsTr("Attachment requests")
    subtitle: qsTr("Check the identity with the establishment owner before approving. Approving does not create an access by itself: the owner invites the person.")
    property var rows: []
    function reload() { Api.get("/api/v1/admin/attachment-requests", function(r) { if (r.ok) page.rows = r.data.items; else ConsoleNav.fail(r) }) }
    Component.onCompleted: reload()
    DataTable {
        anchors.fill: parent
        rows: page.rows
        columns: [
            { title: qsTr("Received"), width: 1.1, muted: true, value: function(a) { return Fmt.dateTime(a.created_at) } },
            { title: qsTr("Establishment"), width: 1.6, strong: true, value: function(a) { return a.establishment } },
            { title: qsTr("Requester"), width: 1.6, value: function(a) { return a.name + " · " + a.email + (a.phone ? " · " + a.phone : "") } },
            { title: qsTr("Message"), width: 2.2, muted: true, value: function(a) { return a.message } },
            { title: qsTr("Status"), width: 0.9, chip: function(a) { return { text: a.status === "open" ? qsTr("Open") : a.status === "approved" ? qsTr("Approved") : qsTr("Rejected"), tone: a.status === "open" ? "warning" : a.status === "approved" ? "success" : "neutral" } } }
        ]
        emptyIcon: "users"
        emptyTitle: qsTr("No request")
        onRowClicked: function(a) { if (a.status === "open") resolve.show(a) }
    }
    FbDialog {
        id: resolve
        property var req: null
        title: req ? req.establishment : ""
        subtitle: req ? req.name + " · " + req.email : ""
        function show(x) { req = x; note.text = ""; open() }
        Field { label: qsTr("Note (recorded in the audit log)"); required: true; FbTextArea { id: note } }
        footer: [
            FbButton { text: qsTr("Approve"); onClicked: page.decide("approved") },
            FbButton { text: qsTr("Reject"); variant: "danger"; onClicked: page.decide("rejected") },
            FbButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: resolve.close() }
        ]
    }
    function decide(status) {
        Api.send("POST", "/api/v1/admin/attachment-requests/" + resolve.req.id + "/resolve", { status: status, reason: note.text.trim() },
                 function(r) { if (r.ok) { resolve.close(); reload() } else ConsoleNav.fail(r) })
    }
}
