import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

// Option B — create a business (company or individual entrepreneur).
OnboardingFrame {
    id: page
    property int step: 0
    property string kind: ""
    property var profile: null
    property var legalValues: ({})
    property bool busy: false
    property var created: null
    property var session: null
    readonly property int lastStep: 4
    // First step: the Nhill server of the business (its address is never built into the
    // application). It is checked, then kept on this device.
    property bool serverStep: true
    formWidth: 560
    onBackRequested: {
        if (page.serverStep || page.step >= 5) { ApplicationWindow.window.back(); return }
        if (page.step > 0) page.step--
        else page.serverStep = true
    }

    function loadProfile(cc) {
        if (!cc) return
        // Currency of the country by default (ISO 4217), when the platform offers it.
        var cur = Tools.countryCurrency(cc)
        if (cur && Labels.currencies().some(function(c) { return c.value === cur })) currency.value = cur
        Api.get("/api/v1/countries/" + cc + "/profile", function(r) {
            if (r.ok) {
                page.profile = r.data
                if (!currency.value) currency.value = r.data.default_currency
            }
        })
    }
    function serverReady() { page.serverStep = false; loadProfile(country.value) }

    function legalRequired(def) {
        var req = (def.required_for || []).indexOf(page.kind) >= 0
        return req || (vat.checked && def.required_if_vat_registered === true)
    }
    function validateStep() {
        if (step === 1) {
            if (tradeName.text.trim().length === 0) { tradeField.error = qsTr("Trade name is required."); return false }
            if (!country.value) { countryField.error = qsTr("Choose the country of the business."); return false }
            if (page.profile) for (var i = 0; i < page.profile.legal_ids.length; ++i) {
                var d = page.profile.legal_ids[i]
                if (legalRequired(d) && !(legalValues[d.kind] || "").trim()) { ApplicationWindow.window.toast(qsTr("Please fill in: %1").arg(Prefs.language === "fr" ? d.label_fr : d.label_en), "danger"); return false }
            }
        }
        if (step === 2) {
            if (bizPhone.text.length > 0 && !Check.phone(bizPhone.text)) { bizPhoneField.error = qsTr("Invalid phone number."); return false }
            if (bizEmail.text.length > 0 && !Check.email(bizEmail.text)) { bizEmailField.error = qsTr("Invalid e-mail address."); return false }
        }
        if (step === 3) {
            if (ownerName.text.trim().length === 0) { ownerNameField.error = qsTr("Your name is required."); return false }
            if (!Check.email(ownerEmail.text)) { ownerEmailField.error = qsTr("Invalid e-mail address."); return false }
            var issue = Check.password(ownerPassword.text, ownerEmail.text)
            if (issue.length > 0) { ownerPasswordField.error = Labels.password(issue); return false }
            if (ownerPassword.text !== ownerConfirm.text) { ownerConfirmField.error = qsTr("The passwords do not match."); return false }
        }
        return true
    }
    function next() { if (validateStep()) step++ }
    function body() {
        var ids = {}
        for (var k in legalValues) if ((legalValues[k] || "").trim().length > 0) ids[k] = legalValues[k].trim()
        return {
            kind: kind, trade_name: tradeName.text, legal_name: kind === "company" ? legalName.text : "",
            country_code: country.value, city: city.text, address: address.text, sector: sector.text,
            preferred_language: lang.value, main_currency: currency.value, email: bizEmail.text, phone: bizPhone.text,
            vat_registered: vat.checked, legal_ids: ids, timezone: Tools.systemTimeZone(),
            owner: { full_name: ownerName.text, email: ownerEmail.text, phone: ownerPhone.text, password: ownerPassword.text }
        }
    }
    function create() {
        busy = true
        Api.send("POST", "/api/v1/establishments", body(), function(r) {
            busy = false
            if (r.ok) { page.created = r.data; page.session = r.data.session; page.step = 5; return }
            if (r.error && r.error.code === "establishment_exists") { existsDialog.open(); return }
            ApplicationWindow.window.fail(r)
        })
    }

    // ---- step "server"
    Text {
        visible: page.serverStep
        text: qsTr("Your Nhill server")
        font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.DemiBold; color: Theme.text
        Layout.fillWidth: true; wrapMode: Text.WordWrap
    }
    Text {
        visible: page.serverStep
        text: qsTr("Your business and its data are kept on this server. Enter the address given by your provider or your administrator.")
        font.family: Theme.font; font.pixelSize: Theme.fs_md; color: Theme.textMuted
        Layout.fillWidth: true; wrapMode: Text.WordWrap
    }
    ServerAddressForm {
        visible: page.serverStep
        Layout.fillWidth: true
        saveText: qsTr("Continue")
        onSaved: function(url) { page.serverReady() }
    }

    // ---- progress
    RowLayout {
        visible: !page.serverStep && page.step < 5
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: page.lastStep + 1
            delegate: Rectangle { Layout.fillWidth: true; height: 4; radius: 2; color: index <= page.step ? Theme.primary : Theme.line }
        }
    }
    Text {
        visible: !page.serverStep && page.step < 5
        text: [qsTr("Your type of activity"), qsTr("Identity of the business"), qsTr("Contact and preferences"),
               page.kind === "individual" ? qsTr("Your account") : qsTr("Account of the owner"), qsTr("Check and create")][page.step]
        font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.DemiBold; color: Theme.text
        Layout.fillWidth: true; wrapMode: Text.WordWrap
    }

    // ---- step 0: kind
    ColumnLayout {
        visible: !page.serverStep && page.step === 0
        Layout.fillWidth: true
        spacing: Theme.s3
        Repeater {
            model: [
                { k: "individual", icon: "user", t: qsTr("Individual entrepreneur"), d: qsTr("I work alone: shop, independent, craftsman, consultant… A simplified space with a single user.") },
                { k: "company", icon: "building", t: qsTr("Company or organisation"), d: qsTr("Company, SME, NGO or establishment with several people: roles, team and invitations.") }
            ]
            delegate: AbstractButton {
                id: opt
                Layout.fillWidth: true
                implicitHeight: row.implicitHeight + 2 * Theme.s4
                padding: Theme.s4
                Accessible.name: modelData.t
                onClicked: { page.kind = modelData.k; page.step = 1 }
                background: Rectangle { radius: Theme.radiusLg; color: opt.hovered ? Theme.surfaceAlt : Theme.surface
                                        border.width: page.kind === modelData.k || opt.visualFocus ? 2 : 1
                                        border.color: page.kind === modelData.k ? Theme.primary : opt.visualFocus ? Theme.focus : Theme.line }
                contentItem: RowLayout {
                    id: row
                    spacing: Theme.s4
                    Rectangle { width: 40; height: 40; radius: 10; color: Theme.primarySoft; Layout.alignment: Qt.AlignTop
                                Icon { anchors.centerIn: parent; name: modelData.icon; color: Theme.primary } }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: modelData.t; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold; color: Theme.text }
                        Text { text: modelData.d; font.family: Theme.font; font.pixelSize: Theme.fs_md; color: Theme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    // ---- step 1: identity
    ColumnLayout {
        visible: !page.serverStep && page.step === 1
        Layout.fillWidth: true
        spacing: Theme.s3
        Field { id: tradeField; label: page.kind === "individual" ? qsTr("Name of your activity") : qsTr("Trade name"); required: true
                help: qsTr("The name your customers know, printed on your invoices.")
                FbTextField { id: tradeName; onTextChanged: tradeField.error = "" } }
        Field { visible: page.kind === "company"; label: qsTr("Legal name (if different)"); FbTextField { id: legalName } }
        Field { label: qsTr("Business sector"); FbTextField { id: sector; placeholderText: qsTr("e.g. retail, hardware, pharmacy, services") } }
        Field { id: countryField; label: qsTr("Country"); required: true
                FbComboBox { id: country; model: Labels.countries(); value: Tools.systemCountry()
                             onActivated: { countryField.error = ""; page.loadProfile(currentValue) } } }
        FbSwitch { id: vat; text: qsTr("My business is registered for VAT"); Layout.fillWidth: true }
        Banner {
            visible: vat.checked && page.profile && page.profile.document_notices && page.profile.document_notices.vat_registered
            tone: "warning"
            text: page.profile && page.profile.document_notices && page.profile.document_notices.vat_registered
                  ? qsTr("DRC: VAT-registered businesses must issue the standardised invoice through an electronic fiscal device (DEF). Nhill is not yet a homologated system: your Nhill documents will be marked as management documents, and you will be able to record the reference of the standardised invoice.")
                  : ""
        }
        SectionTitle { text: qsTr("Legal identifiers"); hint: qsTr("only those that apply to you") }
        Repeater {
            model: page.profile ? page.profile.legal_ids : []
            delegate: Field {
                label: Prefs.language === "fr" ? modelData.label_fr : modelData.label_en
                required: page.legalRequired(modelData)
                FbTextField { monospace: true; onTextChanged: { var v = page.legalValues; v[modelData.kind] = text; page.legalValues = v } }
            }
        }
    }

    // ---- step 2: contact & preferences
    ColumnLayout {
        visible: !page.serverStep && page.step === 2
        Layout.fillWidth: true
        spacing: Theme.s3
        GridLayout {
            Layout.fillWidth: true
            columns: Theme.compact ? 1 : 2
            columnSpacing: Theme.s3; rowSpacing: Theme.s3
            Field { label: qsTr("City"); FbTextField { id: city } }
            Field { label: qsTr("Address"); FbTextField { id: address } }
            Field { id: bizPhoneField; label: qsTr("Business phone"); FbTextField { id: bizPhone; inputMethodHints: Qt.ImhDialableCharactersOnly; placeholderText: qsTr("+ country code and number"); onTextChanged: bizPhoneField.error = "" } }
            Field { id: bizEmailField; label: qsTr("Business e-mail"); FbTextField { id: bizEmail; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase; onTextChanged: bizEmailField.error = "" } }
            Field { label: qsTr("Main currency"); help: qsTr("Other currencies remain available on each document.")
                    FbComboBox { id: currency; model: Labels.currencies(); value: "" } }
            Field { label: qsTr("Language of documents")
                    FbComboBox { id: lang; model: [{ value: "fr", text: "Français" }, { value: "en", text: "English" }]; value: Prefs.language } }
        }
    }

    // ---- step 3: owner
    ColumnLayout {
        visible: !page.serverStep && page.step === 3
        Layout.fillWidth: true
        spacing: Theme.s3
        Field { id: ownerNameField; label: qsTr("Full name"); required: true; FbTextField { id: ownerName; onTextChanged: ownerNameField.error = "" } }
        Field { id: ownerEmailField; label: qsTr("E-mail (your login)"); required: true
                FbTextField { id: ownerEmail; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase; onTextChanged: ownerEmailField.error = "" } }
        Field { label: qsTr("Mobile phone"); FbTextField { id: ownerPhone; inputMethodHints: Qt.ImhDialableCharactersOnly } }
        Field { id: ownerPasswordField; label: qsTr("Password"); required: true; help: qsTr("At least 8 characters. If you already have a Nhill account, use its password.")
                FbTextField { id: ownerPassword; echoMode: TextInput.Password; onTextChanged: ownerPasswordField.error = "" } }
        Field { id: ownerConfirmField; label: qsTr("Confirm the password"); required: true
                FbTextField { id: ownerConfirm; echoMode: TextInput.Password; onTextChanged: ownerConfirmField.error = "" } }
    }

    // ---- step 4: review
    ColumnLayout {
        visible: !page.serverStep && page.step === 4
        Layout.fillWidth: true
        spacing: Theme.s3
        FbCard {
            Layout.fillWidth: true
            GridLayout {
                width: parent.width
                columns: 2
                columnSpacing: Theme.s4; rowSpacing: Theme.s2
                Repeater {
                    model: [
                        [qsTr("Type"), page.kind === "individual" ? qsTr("Individual entrepreneur") : qsTr("Company or organisation")],
                        [qsTr("Trade name"), tradeName.text],
                        [qsTr("Country"), country.currentText],
                        [qsTr("Currency"), currency.value || ""],
                        [qsTr("VAT"), vat.checked ? qsTr("Registered") : qsTr("Not registered")],
                        [qsTr("Owner"), ownerName.text + " — " + ownerEmail.text]
                    ]
                    delegate: Item {
                        Layout.columnSpan: 2; Layout.fillWidth: true
                        implicitHeight: Math.max(k.implicitHeight, v.implicitHeight)
                        Text { id: k; text: modelData[0]; width: 140; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                        Text { id: v; x: 150; width: parent.width - 150; text: modelData[1]; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_md; wrapMode: Text.WordWrap }
                    }
                }
            }
        }
        Banner { tone: "success"; text: qsTr("No payment details are requested to create your business. No automatic charge will ever be made without your explicit agreement.") }
    }

    // ---- step 5: done
    ColumnLayout {
        visible: !page.serverStep && page.step === 5
        Layout.fillWidth: true
        spacing: Theme.s4
        Rectangle { width: 56; height: 56; radius: 28; color: Theme.successSoft
                    Icon { anchors.centerIn: parent; name: "check"; color: Theme.success; size: 28 } }
        Text { text: qsTr("Your business is ready"); font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.DemiBold; color: Theme.text }
        Text {
            visible: page.created && !page.created.trial_granted
            text: qsTr("A free trial has already been used with these details: choose a plan in Settings › Subscription to start working.")
            color: Theme.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.family: Theme.font; font.pixelSize: Theme.fs_md
        }
        FbCard {
            Layout.fillWidth: true
            visible: page.kind === "company"
            ColumnLayout {
                width: parent.width
                spacing: Theme.s2
                Text { text: qsTr("Establishment code"); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                RowLayout {
                    Text { text: page.created ? page.created.establishment_code : ""; font.family: Theme.mono; font.pixelSize: 24; color: Theme.primary; Layout.fillWidth: true }
                    FbIconButton { iconName: "copy"; tip: qsTr("Copy"); onClicked: { Tools.copyText(page.created.establishment_code); ApplicationWindow.window.toast(qsTr("Code copied")) } }
                }
                Text {
                    text: qsTr("Your collaborators will need it to join your business, together with the invitation you send them. It is renewed automatically every 30 days; you will always find the current code in Settings.")
                    wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                }
            }
        }
        FbButton { text: qsTr("Open my workspace"); Layout.fillWidth: true; iconName: "chevronRight"
                   onClicked: Session.start(page.session, function(ok) { if (ok) ApplicationWindow.window.openShell() }) }
    }

    // ---- navigation
    RowLayout {
        visible: !page.serverStep && page.step >= 1 && page.step <= page.lastStep
        Layout.fillWidth: true
        FbButton { text: qsTr("Previous"); variant: "secondary"; onClicked: page.step-- }
        Item { Layout.fillWidth: true }
        FbButton {
            text: page.step === page.lastStep ? qsTr("Create my business") : qsTr("Next")
            busy: page.busy
            onClicked: page.step === page.lastStep ? page.create() : page.next()
        }
    }

    FbDialog {
        id: existsDialog
        title: qsTr("This business is already registered")
        subtitle: qsTr("For security, we do not show who registered it. Leave your details: the owner or Nhill support will check your identity before giving you access.")
        Field { label: qsTr("Full name"); FbTextField { id: reqName; text: ownerName.text } }
        Field { label: qsTr("E-mail"); FbTextField { id: reqEmail; text: ownerEmail.text } }
        Field { label: qsTr("Phone"); FbTextField { id: reqPhone; text: ownerPhone.text } }
        Field { label: qsTr("Message"); FbTextArea { id: reqMessage } }
        footer: [
            FbButton {
                text: qsTr("Send the request")
                onClicked: {
                    var kinds = Object.keys(page.legalValues).filter(function(k) { return (page.legalValues[k] || "").trim().length > 0 })
                    Api.send("POST", "/api/v1/establishments/attachment-requests", {
                        country_code: country.value, identifier_kind: kinds.length ? kinds[0] : "", identifier_value: kinds.length ? page.legalValues[kinds[0]] : "",
                        full_name: reqName.text, email: reqEmail.text, phone: reqPhone.text, message: reqMessage.text
                    }, function(r) {
                        existsDialog.close()
                        r.ok ? ApplicationWindow.window.toast(qsTr("Request sent. You will be contacted after verification."), "success") : ApplicationWindow.window.fail(r)
                    })
                }
            },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: existsDialog.close() }
        ]
    }
}
