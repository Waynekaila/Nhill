import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

// "Who did what": every change made through Nhill is recorded by the server
// with its author. Owners, administrators and accountants see the whole
// establishment; other users see their own actions only (enforced server-side).
FbPage {
    id: page
    readonly property bool all: Session.can("audit.read")
    title: all ? qsTr("Activity log") : qsTr("My activity")
    subtitle: all ? qsTr("Who did what, and when — recorded by the server, cannot be edited") : qsTr("The actions you made in this establishment")
    property var rows: []
    property var members: []
    property int total: 0
    property string userId: ""
    property string action: ""
    property int offset: 0
    readonly property int pageSize: 100
    property bool loading: false
    function query() {
        var q = "?limit=" + pageSize + "&offset=" + offset
        if (userId) q += "&user_id=" + userId
        if (action) q += "&action=" + encodeURIComponent(action)
        if (from.value) q += "&from=" + from.value
        if (to.value) q += "&to=" + to.value
        return q
    }
    function reload() {
        loading = true
        Api.get("/api/v1/activity" + query(), function(r) {
            loading = false
            if (!r.ok) { Nav.fail(r); return }
            page.rows = r.data.items; page.total = r.data.total
            if (r.data.members.length) page.members = r.data.members
        })
    }
    function exportAs(fmt) {
        Api.download("/api/v1/activity" + query() + "&format=" + fmt, "journal-activite." + fmt,
                     function(r) { if (r.ok) Nav.toast(qsTr("Saved to %1").arg(r.data), "success"); else Nav.fail(r) })
    }
    Component.onCompleted: { from.value = Fmt.addDays(Fmt.today(), -30); to.value = Fmt.today(); reload() }
    actions: [
        FbButton { text: qsTr("CSV"); iconName: "download"; variant: "secondary"; onClicked: page.exportAs("csv") },
        FbButton { text: qsTr("PDF"); iconName: "download"; variant: "secondary"; onClicked: page.exportAs("pdf") }
    ]
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        FbCard {
            Layout.fillWidth: true
            GridLayout {
                width: parent.width
                columns: Theme.compact ? 1 : 5
                columnSpacing: Theme.s3; rowSpacing: Theme.s2
                Field {
                    visible: page.all && !Session.individual
                    Layout.minimumWidth: Theme.compact ? 0 : 220
                    label: qsTr("Employee")
                    FbComboBox { model: [{ value: "", text: qsTr("Everyone") }].concat(page.members.map(function(m) { return { value: m.id, text: m.full_name + " · " + Labels.role(m.role) } }))
                                 value: ""; onActivated: { page.userId = currentValue; page.offset = 0; page.reload() } }
                }
                Field {
                    Layout.minimumWidth: Theme.compact ? 0 : 200
                    label: qsTr("Type of action")
                    FbComboBox { value: ""; model: Labels.activityCategories(); onActivated: { page.action = currentValue; page.offset = 0; page.reload() } }
                }
                Field { label: qsTr("From"); Layout.preferredWidth: Theme.compact ? -1 : 170; DateField { id: from } }
                Field { label: qsTr("To"); Layout.preferredWidth: Theme.compact ? -1 : 170; DateField { id: to } }
                FbButton { Layout.alignment: Qt.AlignBottom; text: qsTr("Show"); iconName: "filter"; onClicked: { page.offset = 0; page.reload() } }
            }
        }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            loading: page.loading
            rows: page.rows
            titleColumn: 2; subtitleColumn: 1; trailingColumn: 0
            columns: [
                { title: qsTr("When"), width: 1.2, muted: true, value: function(a) { return Fmt.dateTime(a.created_at) } },
                { title: qsTr("Who"), width: 1.4, strong: true, value: function(a) { return a.actor || "—" } },
                { title: qsTr("Action"), width: 2, value: function(a) { return a.action_label } },
                { title: qsTr("Concerning"), width: 1.6, mono: true, value: function(a) { return a.reference || "" } },
                { title: qsTr("Details"), width: 2.4, muted: true, value: function(a) { return a.summary } }
            ]
            emptyIcon: "clock"
            emptyTitle: qsTr("No activity for these filters")
            onRowClicked: function(a) {
                if (a.entity === "sales_document" && a.action !== "sales_document.draft_deleted") Nav.push("pages/DocumentPage.qml", { docId: a.entity_id })
                else if (a.entity === "party") Nav.push("pages/PartyPage.qml", { partyId: a.entity_id })
                else if (a.entity === "purchase") Nav.push("pages/PurchaseEditorPage.qml", { purchaseId: a.entity_id })
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: page.total ? qsTr("%1–%2 of %3").arg(page.offset + 1).arg(Math.min(page.offset + page.pageSize, page.total)).arg(page.total) : "" }
            FbButton { text: qsTr("Previous"); variant: "ghost"; enabled: page.offset > 0; onClicked: { page.offset = Math.max(0, page.offset - page.pageSize); page.reload() } }
            FbButton { text: qsTr("Next"); variant: "ghost"; enabled: page.offset + page.pageSize < page.total; onClicked: { page.offset += page.pageSize; page.reload() } }
        }
    }
}
