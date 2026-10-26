import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

// Global search across customers, products and documents.
Popup {
    id: root
    signal openDocument(string id)
    signal openParty(string id)
    signal openProduct(string id)
    property var results: []
    modal: true
    parent: Overlay.overlay
    width: Math.min(640, parent.width - 24)
    x: (parent.width - width) / 2
    y: Theme.compact ? 8 : 80
    height: Math.min(520, parent.height - y - 16)
    padding: 0
    Overlay.modal: Rectangle { color: Theme.overlay }
    background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.line }
    function openSearch() { open(); field.text = ""; results = []; field.forceActiveFocus() }
    function run(q) {
        if (q.length < 2) { results = []; return }
        var acc = [], pending = 3
        function done() { if (--pending === 0) root.results = acc }
        var e = encodeURIComponent(q)
        Api.get("/api/v1/customers?limit=5&search=" + e, function(r) { if (r.ok) r.data.items.forEach(function(p) { acc.push({ kind: "party", id: p.id, title: p.name, sub: p.phone || p.email || "" }) }); done() })
        Api.get("/api/v1/products?limit=5&search=" + e, function(r) { if (r.ok) r.data.items.forEach(function(p) { acc.push({ kind: "product", id: p.id, title: p.name, sub: p.sku }) }); done() })
        Api.get("/api/v1/sales-documents?limit=6&search=" + e, function(r) { if (r.ok) r.data.items.forEach(function(d) { acc.push({ kind: "doc", id: d.id, title: (d.number || qsTr("Draft")) + " · " + d.party_name, sub: Labels.docType(d.doc_type) + " · " + Fmt.money(d.total, d.currency) }) }); done() })
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        SearchField { id: field; Layout.fillWidth: true; Layout.margins: Theme.s3; placeholderText: qsTr("Search customers, products, invoices…"); onSearch: function(t) { root.run(t) } }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.results
            section.property: "kind"
            delegate: ItemDelegate {
                width: list.width
                onClicked: { root.close(); modelData.kind === "doc" ? root.openDocument(modelData.id) : modelData.kind === "party" ? root.openParty(modelData.id) : root.openProduct(modelData.id) }
                contentItem: RowLayout {
                    spacing: Theme.s3
                    Icon { name: modelData.kind === "doc" ? "invoice" : modelData.kind === "party" ? "user" : "box"; color: Theme.textMuted; size: 18 }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 0
                        Text { text: modelData.title; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_md; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: modelData.sub; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                }
                background: Rectangle { color: hovered ? Theme.surfaceAlt : "transparent" }
            }
            Text { anchors.centerIn: parent; visible: root.results.length === 0; text: field.text.length < 2 ? qsTr("Type at least 2 characters") : qsTr("No result"); color: Theme.textFaint; font.family: Theme.font }
        }
    }
}
