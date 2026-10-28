import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    property string docId: ""
    property var doc: null
    property bool loading: false
    title: doc ? (doc.number || qsTr("Draft")) + " · " + Labels.docType(doc.doc_type) : qsTr("Document")
    subtitle: doc ? doc.party_name : ""
    scrollable: true
    readonly property bool isDraft: doc && doc.status === "draft"
    readonly property bool payable: doc && ((doc.doc_type === "invoice" && (doc.status === "issued" || doc.status === "partially_paid"))
                                           || (doc.doc_type === "credit_note" && doc.status === "issued" && !Fmt.isZero(doc.balance)))
    function reload() {
        loading = true
        Api.get("/api/v1/sales-documents/" + docId, function(r) { loading = false; if (r.ok) page.doc = r.data; else Nav.fail(r) })
    }
    Component.onCompleted: reload()
    function act(path, body, ok) {
        Api.send("POST", "/api/v1/sales-documents/" + docId + path, body || {}, function(r) {
            if (!r.ok) { Nav.fail(r); return }
            if (ok) ok(r.data); else reload()
        }, Api.newIdempotencyKey())
    }

    actions: [
        // The same document as in Nhill POS: A4 page or roll ticket (58/80 mm, till setting).
        FbButton { visible: !!page.doc; text: qsTr("PDF A4"); iconName: "download"; variant: "secondary"
                   onClicked: Api.download("/api/v1/sales-documents/" + page.docId + "/pdf?format=a4", (page.doc.number || "brouillon") + ".pdf",
                                           function(r) { r.ok ? Nav.toast(qsTr("Saved in %1").arg(r.data)) : Nav.fail(r) }) },
        FbButton { visible: !!page.doc && !page.isDraft; text: qsTr("Ticket"); iconName: "download"; variant: "ghost"
                   onClicked: Api.download("/api/v1/sales-documents/" + page.docId + "/pdf?format=ticket", (page.doc.number || "brouillon") + "-ticket.pdf",
                                           function(r) { r.ok ? Nav.toast(qsTr("Saved in %1").arg(r.data)) : Nav.fail(r) }) },
        FbButton { visible: page.payable && Session.can("payments.write"); text: page.doc && page.doc.doc_type === "credit_note" ? qsTr("Record refund") : qsTr("Record payment"); iconName: "payment"
                   onClicked: payDialog.start("document_id", page.docId, page.doc.currency, page.doc.balance) },
        FbButton { visible: page.isDraft && Session.can("sales.write"); text: qsTr("Edit"); iconName: "edit"; variant: "secondary"
                   onClicked: Nav.push("pages/DocumentEditorPage.qml", { docId: page.docId, docType: page.doc.doc_type }) },
        FbButton { visible: page.isDraft && Session.can("sales.issue"); text: qsTr("Issue"); iconName: "check"; onClicked: issueConfirm.open() },
        FbButton { visible: !!page.doc && page.doc.doc_type === "quote" && page.doc.status === "sent"; text: qsTr("Accepted"); variant: "secondary"
                   onClicked: page.act("/status", { status: "accepted" }) },
        FbButton { visible: Session.has("sales.invoices") && Session.can("sales.write") && (!!page.doc && (page.doc.doc_type === "quote" && (page.doc.status === "sent" || page.doc.status === "accepted")) || (!!page.doc && page.doc.doc_type === "order" && page.doc.status === "confirmed"))
                   text: qsTr("Convert to invoice"); iconName: "invoice"
                   onClicked: page.act("/convert", { to: "invoice" }, function(d) { Nav.pop(); Nav.push("pages/DocumentEditorPage.qml", { docId: d.id, docType: "invoice" }) }) },
        FbButton { visible: !!page.doc && page.doc.doc_type === "invoice" && ["issued", "partially_paid", "paid"].indexOf(page.doc.status) >= 0 && Session.can("sales.issue")
                   text: qsTr("Credit note"); variant: "secondary"; onClicked: creditDialog.open() },
        FbButton { visible: page.isDraft && Session.can("sales.write"); text: qsTr("Delete"); iconName: "trash"; variant: "ghost"; onClicked: deleteConfirm.open() }
    ]

    BusyIndicator { visible: page.loading && !page.doc; running: visible; Layout.alignment: Qt.AlignHCenter }

    Banner {
        visible: !!page.doc && page.doc.status === "draft"
        tone: "info"
        text: qsTr("Draft: no number and no fiscal value yet. Issue it to make it final.")
    }
    Banner {
        visible: !!page.doc && page.doc.doc_type === "invoice" && page.doc.status !== "draft" && Session.establishment.vat_registered && page.doc.profile === "CD"
        tone: page.doc && page.doc.fiscal_reference ? "success" : "warning"
        text: page.doc && page.doc.fiscal_reference
              ? qsTr("Standardised invoice reference recorded: DEF %1 · code %2").arg(page.doc.fiscal_reference.def_nid).arg(page.doc.fiscal_reference.def_auth_code)
              : qsTr("Remember to issue the standardised invoice with your DEF, then record its reference here.")
        actionText: Session.can("sales.issue") ? qsTr("Record DEF reference") : ""
        onAction: defDialog.open()
    }

    GridLayout {
        visible: !!page.doc
        Layout.fillWidth: true
        columns: Theme.compact ? 1 : 3
        columnSpacing: Theme.s3; rowSpacing: Theme.s3
        FbCard {
            Layout.fillWidth: true
            Layout.columnSpan: Theme.compact ? 1 : 2
            ColumnLayout {
                width: parent.width
                spacing: Theme.s2
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: page.doc ? page.doc.party_name : ""; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                    FbChip { property var c: page.doc ? Labels.docStatus(page.doc) : { text: "", tone: "neutral" }; text: c.text; tone: c.tone }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.s4
                    Repeater {
                        model: page.doc ? [
                            [qsTr("Date"), Fmt.date(page.doc.issue_date)],
                            [qsTr("Due"), page.doc.due_date ? Fmt.date(page.doc.due_date) : "—"],
                            [qsTr("Currency"), page.doc.currency],
                            [qsTr("Exchange rate"), page.doc.fx_rate ? "1 " + page.doc.currency + " = " + page.doc.fx_rate.replace(/0+$/, "").replace(/\.$/, "") + " " + page.doc.main_currency : "—"],
                            [qsTr("Original invoice"), page.doc.related_number || "—"]
                        ] : []
                        delegate: ColumnLayout {
                            spacing: 0
                            visible: modelData[1] !== "—"
                            Text { text: modelData[0]; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs }
                            Text { text: modelData[1]; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_md }
                        }
                    }
                }
            }
        }
        FbCard {
            Layout.fillWidth: true
            GridLayout {
                width: parent.width
                columns: 2
                rowSpacing: 4
                Repeater {
                    model: page.doc ? [
                        [qsTr("Total excl. tax"), page.doc.net, false],
                        [qsTr("Taxes"), page.doc.tax, false],
                        [qsTr("Total"), page.doc.total, true],
                        [qsTr("Paid"), page.doc.paid, false],
                        [qsTr("Credited"), page.doc.credited, false],
                        [qsTr("Balance"), page.doc.balance, true]
                    ].filter(function(x) { return x[2] || !Fmt.isZero(x[1]) }) : []
                    delegate: Item {
                        Layout.columnSpan: 2; Layout.fillWidth: true; implicitHeight: 22
                        Text { text: modelData[0]; color: modelData[2] ? Theme.primary : Theme.textMuted; font.family: Theme.font; font.weight: modelData[2] ? Font.DemiBold : Font.Normal }
                        Text { anchors.right: parent.right; text: Fmt.money(modelData[1], page.doc.currency); color: modelData[2] ? Theme.primary : Theme.text; font.family: Theme.font; font.weight: modelData[2] ? Font.DemiBold : Font.Normal }
                    }
                }
            }
        }
    }

    SectionTitle { visible: !!page.doc; text: qsTr("Lines") }
    DataTable {
        visible: !!page.doc
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(480, 44 + (page.doc ? page.doc.lines.length : 0) * Theme.rowHeight + 2)
        rows: page.doc ? page.doc.lines : []
        titleColumn: 0; subtitleColumn: 1; trailingColumn: 5
        columns: [
            { title: qsTr("Description"), width: 3, value: function(l) { return l.description } },
            { title: qsTr("Qty"), width: 1, align: "right", value: function(l) { return Fmt.quantity(l.quantity) + " " + Labels.unit(l.unit) } },
            { title: qsTr("Unit price"), width: 1.2, align: "right", value: function(l) { return Fmt.amount(l.unit_price, page.doc.currency) } },
            { title: qsTr("Disc."), width: 0.7, align: "right", muted: true, value: function(l) { return l.discount_bp ? Fmt.percent(l.discount_bp) : "" } },
            { title: qsTr("Tax"), width: 0.7, align: "right", muted: true, value: function(l) { return Fmt.percent(l.tax_rate_bp) } },
            { title: qsTr("Total excl. tax"), width: 1.3, align: "right", strong: true, value: function(l) { return Fmt.amount(l.net, page.doc.currency) } }
        ]
    }

    GridLayout {
        visible: !!page.doc
        Layout.fillWidth: true
        columns: Theme.compact ? 1 : 2
        columnSpacing: Theme.s3
        FbCard {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            ColumnLayout {
                width: parent.width
                SectionTitle { text: qsTr("Payments") }
                Repeater {
                    model: page.doc ? page.doc.payments : []
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Icon { name: modelData.direction === "in" ? "arrowDown" : "arrowUp"; color: modelData.direction === "in" ? Theme.success : Theme.accent; size: 16 }
                        Text { text: Fmt.date(modelData.paid_at) + " · " + Labels.method(modelData.method) + (modelData.reference ? " · " + modelData.reference : ""); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: Fmt.money(modelData.amount, page.doc.currency); color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold }
                    }
                }
                Text { visible: page.doc && page.doc.payments.length === 0; text: qsTr("No payment recorded."); color: Theme.textFaint; font.family: Theme.font }
            }
        }
        FbCard {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            ColumnLayout {
                width: parent.width
                SectionTitle { text: qsTr("History") }
                Repeater {
                    model: page.doc ? page.doc.history : []
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Rectangle { width: 8; height: 8; radius: 4; color: Theme.primary }
                        Text { text: Labels.docStatus({ status: modelData.to, doc_type: page.doc.doc_type }).text + (modelData.by ? " · " + modelData.by : ""); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: Fmt.dateTime(modelData.at); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                    }
                }
            }
        }
    }
    Text {
        visible: !!page.doc && !!page.doc.pdf_sha256
        Layout.fillWidth: true
        text: qsTr("PDF fingerprint at issue (SHA-256): %1").arg(page.doc ? page.doc.pdf_sha256 : "")
        color: Theme.textFaint; font.family: Theme.mono; font.pixelSize: Theme.fs_xs; wrapMode: Text.WrapAnywhere
    }

    PaymentDialog { id: payDialog; onSaved: page.reload() }
    FbConfirm {
        id: issueConfirm
        title: qsTr("Issue this document?")
        message: qsTr("It receives its definitive number and can no longer be modified or deleted.")
        confirmText: qsTr("Issue")
        onAccepted: page.act("/issue", {})
    }
    FbConfirm {
        id: deleteConfirm
        title: qsTr("Delete this draft?")
        message: qsTr("The draft will be permanently deleted. Issued documents can never be deleted.")
        destructive: true
        confirmText: qsTr("Delete")
        onAccepted: Api.send("DELETE", "/api/v1/sales-documents/" + page.docId, null, function(r) { if (r.ok) { Nav.toast(qsTr("Draft deleted")); Nav.pop() } else Nav.fail(r) })
    }
    FbDialog {
        id: creditDialog
        title: qsTr("Create a credit note")
        subtitle: qsTr("A credit note cancels all or part of the invoice. A draft is created with all the lines; you can adjust it before issuing.")
        FbSwitch { id: restock; text: qsTr("Return the goods to stock"); checked: true }
        Field { label: qsTr("Reason"); FbTextField { id: creditReason } }
        footer: [
            FbButton { text: qsTr("Create the draft credit note"); onClicked: { creditDialog.close(); page.act("/credit-note", { restock: restock.checked, reason: creditReason.text }, function(d) { Nav.push("pages/DocumentPage.qml", { docId: d.id }) }) } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: creditDialog.close() }
        ]
    }
    FbDialog {
        id: defDialog
        title: qsTr("Standardised invoice (DEF)")
        subtitle: qsTr("Copy the information printed on the standardised invoice issued by your electronic fiscal device. Nhill keeps it for reconciliation; it does not certify anything.")
        Field { label: qsTr("DEF identification number (NID)"); FbTextField { id: defNid; monospace: true } }
        Field { label: qsTr("Authentication code"); FbTextField { id: defCode; monospace: true } }
        Field { label: qsTr("Number of the standardised invoice"); FbTextField { id: defNumber; monospace: true } }
        footer: [
            FbButton { text: qsTr("Save"); onClicked: Api.send("PUT", "/api/v1/sales-documents/" + page.docId + "/fiscal-reference",
                                                               { def_nid: defNid.text, def_auth_code: defCode.text, normalized_invoice_number: defNumber.text },
                                                               function(r) { if (r.ok) { defDialog.close(); page.reload() } else Nav.fail(r) }) },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: defDialog.close() }
        ]
    }
}
