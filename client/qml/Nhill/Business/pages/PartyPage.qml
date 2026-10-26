import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    property string partyId: ""
    property var party: null
    title: party ? party.name : qsTr("Contact")
    scrollable: true
    function reload() { Api.get("/api/v1/parties/" + partyId, function(r) { if (r.ok) page.party = r.data; else Nav.fail(r) }) }
    Component.onCompleted: reload()
    actions: [
        FbButton { visible: !!page.party && page.party.is_customer && Session.can("sales.write"); text: qsTr("New invoice"); iconName: "plus"
                   onClicked: Nav.push("pages/DocumentEditorPage.qml", { docType: "invoice", partyId: page.partyId }) },
        FbButton { visible: Session.can("parties.write"); text: qsTr("Edit"); iconName: "edit"; variant: "secondary"; onClicked: dialog.openEdit(page.party) }
    ]
    FbCard {
        visible: !!page.party
        Layout.fillWidth: true
        GridLayout {
            width: parent.width
            columns: Theme.compact ? 2 : 4
            columnSpacing: Theme.s4; rowSpacing: Theme.s2
            Repeater {
                model: page.party ? [
                    [qsTr("Phone"), page.party.phone || "—"], [qsTr("E-mail"), page.party.email || "—"],
                    [qsTr("Address"), [page.party.address, page.party.city].filter(function(x) { return x }).join(", ") || "—"],
                    [qsTr("Payment terms"), qsTr("%n day(s)", "", page.party.payment_terms_days)],
                    [qsTr("Tax number (NIF)"), page.party.tax_ids && page.party.tax_ids.nif ? page.party.tax_ids.nif : "—"],
                    [qsTr("Receivable"), Object.keys(page.party.balances.receivable || {}).map(function(c) { return Fmt.money(page.party.balances.receivable[c], c) }).join(" · ") || "—"],
                    [qsTr("Payable"), Object.keys(page.party.balances.payable || {}).map(function(c) { return Fmt.money(page.party.balances.payable[c], c) }).join(" · ") || "—"]
                ] : []
                delegate: ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Text { text: modelData[0]; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs }
                    Text { text: modelData[1]; color: Theme.text; font.family: Theme.font; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
        }
    }
    Text { visible: !!page.party && page.party.notes.length > 0; text: page.party ? page.party.notes : ""; color: Theme.textMuted; font.family: Theme.font; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    SectionTitle { visible: !!page.party && page.party.documents.length > 0; text: qsTr("Documents") }
    DataTable {
        visible: !!page.party && page.party.documents.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(520, 44 + (page.party ? page.party.documents.length : 0) * Theme.rowHeight + 2)
        rows: page.party ? page.party.documents : []
        titleColumn: 0; subtitleColumn: 1; trailingColumn: 2; chipColumn: 4
        columns: [
            { title: qsTr("Number"), width: 1.3, mono: true, value: function(d) { return d.number || qsTr("Draft") } },
            { title: qsTr("Type"), width: 1, muted: true, value: function(d) { return Labels.docType(d.doc_type) + " · " + Fmt.date(d.issue_date) } },
            { title: qsTr("Amount"), width: 1.3, align: "right", value: function(d) { return Fmt.money(d.total, d.currency) } },
            { title: qsTr("Balance"), width: 1.3, align: "right", value: function(d) { return Fmt.money(d.balance, d.currency) } },
            { title: qsTr("Status"), width: 1.2, align: "right", chip: function(d) { return Labels.docStatus(d) } }
        ]
        onRowClicked: function(d) { Nav.push("pages/DocumentPage.qml", { docId: d.id }) }
    }
    PartyDialog { id: dialog; onSaved: page.reload() }
}
