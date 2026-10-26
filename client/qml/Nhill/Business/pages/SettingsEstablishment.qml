import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

ColumnLayout {
    id: root
    spacing: Theme.s4
    property var est: null
    property var profile: null
    property var legal: ({})
    property string logoUrl: ""
    readonly property bool canEdit: Session.can("settings.manage")
    function load() {
        Api.get("/api/v1/establishment", function(r) {
            if (!r.ok) { Nav.fail(r); return }
            root.est = r.data; root.legal = r.data.legal_ids || {}
            tradeName.text = r.data.trade_name; legalName.text = r.data.legal_name || ""; city.text = r.data.city || ""
            address.text = r.data.address || ""; sector.text = r.data.sector || ""; email.text = r.data.email || ""
            phone.text = r.data.phone || ""; website.text = r.data.website || ""; vat.checked = r.data.vat_registered; tz.value = r.data.timezone
            terms.text = String(r.data.settings.default_payment_terms_days); footer.text = r.data.settings.invoice_footer
            payInfo.text = r.data.settings.payment_instructions; negStock.checked = r.data.settings.allow_negative_stock
            docLang.value = r.data.settings.document_language; rounding.value = r.data.settings.tax_rounding
            Api.get("/api/v1/countries/" + r.data.country_code + "/profile", function(p) { if (p.ok) root.profile = p.data })
            if (r.data.has_logo) Api.fetchImage("/api/v1/establishment/logo", function(i) { root.logoUrl = i.ok ? i.data : "" })
            else root.logoUrl = ""
        })
    }
    Component.onCompleted: load()
    Banner {
        Layout.fillWidth: true
        visible: !!root.est && !!root.est.compliance_notice
        tone: "warning"
        text: root.est && root.est.compliance_notice ? (root.est.compliance_notice[Prefs.language] || root.est.compliance_notice.fr || "") : ""
    }
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            SectionTitle { text: qsTr("Identity") }
            GridLayout {
                Layout.fillWidth: true
                columns: Theme.compact ? 1 : 2
                columnSpacing: Theme.s3; rowSpacing: Theme.s3
                Field { label: qsTr("Trade name"); required: true; FbTextField { id: tradeName; enabled: root.canEdit } }
                Field { label: qsTr("Legal name"); visible: !!root.est && root.est.kind === "company"; FbTextField { id: legalName; enabled: root.canEdit } }
                Field { label: qsTr("Sector"); FbTextField { id: sector; enabled: root.canEdit } }
                Field { label: qsTr("City"); FbTextField { id: city; enabled: root.canEdit } }
                Field { label: qsTr("Address"); FbTextField { id: address; enabled: root.canEdit } }
                Field { label: qsTr("Email"); FbTextField { id: email; enabled: root.canEdit; inputMethodHints: Qt.ImhEmailCharactersOnly } }
                Field { label: qsTr("Phone"); FbTextField { id: phone; enabled: root.canEdit; inputMethodHints: Qt.ImhDialableCharactersOnly; placeholderText: "+243…" } }
                Field { label: qsTr("Website"); FbTextField { id: website; enabled: root.canEdit } }
                Field { label: qsTr("Time zone"); help: qsTr("Dates of documents, cash sessions and daily reports follow this time zone.")
                        FbComboBox { id: tz; enabled: root.canEdit
                                     model: { var m = Labels.timeZones(); if (root.est && !m.some(function(z) { return z.value === root.est.timezone })) m.push({ value: root.est.timezone, text: root.est.timezone }); return m } } }
            }
            FbSwitch { id: vat; enabled: root.canEdit; text: qsTr("Registered for VAT") }
            Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: qsTr("Country: %1 · Currency: %2 · Profile: %3").arg(root.est ? root.est.country_code : "").arg(root.est ? root.est.main_currency : "").arg(root.est ? root.est.profile : "") }
            RowLayout {
                visible: root.canEdit
                Item { Layout.fillWidth: true }
                FbButton { text: qsTr("Save"); onClicked: {
                        var b = { trade_name: tradeName.text, city: city.text, address: address.text, sector: sector.text,
                                  website: website.text, vat_registered: vat.checked }
                        if (tz.value) b.timezone = tz.value
                        if (root.est.kind === "company") b.legal_name = legalName.text
                        if (email.text.trim()) b.email = email.text.trim()
                        if (phone.text.trim()) b.phone = phone.text.trim()
                        Api.send("PATCH", "/api/v1/establishment", b, function(r) { if (r.ok) { Nav.toast(qsTr("Saved"), "success"); Session.reload(); root.load() } else Nav.fail(r) })
                    } }
            }
        }
    }
    FbCard {
        Layout.fillWidth: true
        visible: !!root.profile && root.profile.legal_ids.length > 0
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            SectionTitle { text: qsTr("Legal identifiers") }
            Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: qsTr("Printed on your documents. Nhill checks the format only, not the registration with the authorities.") }
            GridLayout {
                Layout.fillWidth: true
                columns: Theme.compact ? 1 : 2
                columnSpacing: Theme.s3; rowSpacing: Theme.s3
                Repeater {
                    model: root.profile ? root.profile.legal_ids : []
                    Field {
                        label: Prefs.language === "fr" ? modelData.label_fr : modelData.label_en
                        FbTextField { enabled: root.canEdit; monospace: true; text: root.legal[modelData.kind] || ""
                                      onTextEdited: { var v = root.legal; v[modelData.kind] = text; root.legal = v } }
                    }
                }
            }
            RowLayout {
                visible: root.canEdit
                Item { Layout.fillWidth: true }
                FbButton { text: qsTr("Save identifiers"); onClicked: {
                        var ids = {}
                        for (var k in root.legal) if ((root.legal[k] || "").trim()) ids[k] = root.legal[k].trim()
                        Api.send("PUT", "/api/v1/establishment/legal-ids", { legal_ids: ids }, function(r) { if (r.ok) { Nav.toast(qsTr("Saved"), "success"); root.load() } else Nav.fail(r) })
                    } }
            }
        }
    }
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            SectionTitle { text: qsTr("Logo") }
            RowLayout {
                spacing: Theme.s4
                Rectangle {
                    Layout.preferredWidth: 220; Layout.preferredHeight: 110
                    radius: Theme.radius; color: "#FFFFFF"; border.color: Theme.line
                    Image { anchors.fill: parent; anchors.margins: Theme.s3; source: root.logoUrl; fillMode: Image.PreserveAspectFit; smooth: true; visible: root.logoUrl !== "" }
                    Text { anchors.centerIn: parent; visible: root.logoUrl === ""; text: qsTr("No logo"); color: "#8A9096"; font.family: Theme.font }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                           text: qsTr("PNG or JPEG, 2 MB max, 4000×4000 px max. The logo is never stretched: its proportions are kept on every document.") }
                    RowLayout {
                        visible: root.canEdit
                        FbButton { text: root.logoUrl ? qsTr("Replace") : qsTr("Upload a logo"); iconName: "upload"; variant: "secondary"; onClicked: logoPicker.open() }
                        FbButton { visible: root.logoUrl !== ""; text: qsTr("Remove"); iconName: "trash"; variant: "ghost"
                                   onClicked: Api.send("DELETE", "/api/v1/establishment/logo", null, function(r) { if (r.ok) root.load(); else Nav.fail(r) }) }
                    }
                }
            }
        }
    }
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            SectionTitle { text: qsTr("Documents") }
            GridLayout {
                Layout.fillWidth: true
                columns: Theme.compact ? 1 : 3
                columnSpacing: Theme.s3; rowSpacing: Theme.s3
                Field { label: qsTr("Language of documents"); FbComboBox { id: docLang; enabled: root.canEdit; model: [{ value: "fr", text: "Français" }, { value: "en", text: "English" }] } }
                Field { label: qsTr("Default payment terms (days)"); FbTextField { id: terms; enabled: root.canEdit; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 0; top: 365 } } }
                Field { label: qsTr("Tax rounding"); FbComboBox { id: rounding; enabled: root.canEdit; model: [{ value: "per_line", text: qsTr("Per line") }, { value: "per_rate", text: qsTr("Per rate (on totals)") }] } }
            }
            Field { label: qsTr("Payment instructions (printed on invoices)"); FbTextArea { id: payInfo; enabled: root.canEdit; placeholderText: qsTr("e.g. Bank, account, Mobile Money number") } }
            Field { label: qsTr("Footer"); FbTextArea { id: footer; enabled: root.canEdit } }
            FbSwitch { id: negStock; enabled: root.canEdit; text: qsTr("Allow selling below zero stock") }
            RowLayout {
                visible: root.canEdit
                Item { Layout.fillWidth: true }
                FbButton { text: qsTr("Save"); onClicked: Api.send("PATCH", "/api/v1/establishment/settings",
                               { document_language: docLang.value, default_payment_terms_days: parseInt(terms.text || "0"), tax_rounding: rounding.value,
                                 payment_instructions: payInfo.text, invoice_footer: footer.text, allow_negative_stock: negStock.checked },
                               function(r) { if (r.ok) Nav.toast(qsTr("Saved"), "success"); else Nav.fail(r) }) }
            }
        }
    }
    FileDialog {
        id: logoPicker
        title: qsTr("Choose a logo")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg)")]
        onAccepted: Api.upload("PUT", "/api/v1/establishment/logo", selectedFile, function(r) {
            if (r.ok) { Nav.toast(qsTr("Logo updated (%1×%2 px)").arg(r.data.width).arg(r.data.height), "success"); root.load() } else Nav.fail(r) })
    }
}
