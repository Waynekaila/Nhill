import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Payment providers. Secrets (API keys, webhook secrets) are write-only: they are
// sent once to the server, encrypted there, and never displayed again.
FbPage {
    id: page
    title: qsTr("Payment providers")
    subtitle: qsTr("Card payments (Stripe), PayPal, aggregators — secrets stored encrypted on the server only")
    property var rows: []
    property var kinds: []
    property var publicBaseUrl: null
    function reload() {
        Api.get("/api/v1/admin/payment-providers", function(r) {
            if (!r.ok) { ConsoleNav.fail(r); return }
            page.rows = r.data.items; page.kinds = r.data.kinds; page.publicBaseUrl = r.data.public_base_url
        })
    }
    function kindOf(k) { for (var i = 0; i < kinds.length; ++i) if (kinds[i].kind === k) return kinds[i]; return null }
    Component.onCompleted: reload()
    actions: [ FbButton { visible: ConsoleNav.can("superadmin"); text: qsTr("Add a provider"); iconName: "plus"; onClicked: editor.start(null) } ]
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        Banner { Layout.fillWidth: true; visible: !page.publicBaseUrl; tone: "warning"
                 text: qsTr("NH_PUBLIC_BASE_URL is not set on the server: webhook addresses cannot be shown and default return pages are unavailable.") }
        Banner { Layout.fillWidth: true; tone: "info"
                 text: qsTr("The Stripe and PayPal adapters follow the providers' public documentation and are tested against simulated responses only. Run a payment in test mode with your own provider account before enabling live mode.") }
        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            rows: page.rows
            columns: [
                { title: qsTr("Name"), width: 1.4, strong: true, value: function(p) { return p.display_name } },
                { title: qsTr("Code"), width: 1, mono: true, value: function(p) { return p.code } },
                { title: qsTr("Type"), width: 1.4, value: function(p) { var k = page.kindOf(p.kind); return k ? k.label : p.kind } },
                { title: qsTr("Mode"), width: 0.8, chip: function(p) { return p.environment === "live" ? { text: qsTr("Live"), tone: "danger" } : { text: qsTr("Test"), tone: "neutral" } } },
                { title: qsTr("Currencies"), width: 1, muted: true, value: function(p) { return p.currencies.length ? p.currencies.join(", ") : qsTr("All") } },
                { title: qsTr("Status"), width: 1, chip: function(p) {
                    if (!p.implemented) return { text: qsTr("Configuration only"), tone: "warning" }
                    return p.enabled ? { text: qsTr("Enabled"), tone: "success" } : { text: qsTr("Disabled"), tone: "neutral" } } }
            ]
            emptyIcon: "payment"
            emptyTitle: qsTr("No payment provider")
            emptyText: qsTr("Without provider, establishments pay by bank transfer and a billing administrator confirms the payment.")
            onRowClicked: function(p) { editor.start(p) }
        }
    }
    FbDialog {
        id: editor
        property var provider: null
        property var spec: null
        property var values: ({})
        preferredWidth: 680
        title: provider ? provider.display_name : qsTr("New payment provider")
        subtitle: provider && provider.webhook_url ? qsTr("Webhook address: %1").arg(provider.webhook_url) : ""
        function start(p) {
            provider = p
            kind.value = p ? p.kind : "stripe"
            spec = page.kindOf(kind.value)
            code.text = p ? p.code : ""; name.text = p ? p.display_name : ""
            env.value = p ? p.environment : "test"; currencies.text = p ? p.currencies.join(", ") : ""
            enabled.checked = p ? p.enabled : false
            var v = {}
            if (p) for (var k in p.settings) v[k] = p.settings[k]
            values = v
            open()
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.s3; rowSpacing: Theme.s2
            Field { label: qsTr("Type")
                    FbComboBox { id: kind; enabled: !editor.provider; Layout.fillWidth: true
                                 model: page.kinds.map(function(k) { return { value: k.kind, text: k.label } })
                                 onActivated: { editor.spec = page.kindOf(kind.value); editor.values = ({}) } } }
            Field { label: qsTr("Code (webhook address)"); FbTextField { id: code; monospace: true; enabled: !editor.provider; placeholderText: "stripe" } }
            Field { label: qsTr("Name shown to customers"); FbTextField { id: name; placeholderText: qsTr("Card payment") } }
            Field { label: qsTr("Mode"); FbComboBox { id: env; Layout.fillWidth: true; model: [{ value: "test", text: qsTr("Test (sandbox)") }, { value: "live", text: qsTr("Live") }] } }
            Field { Layout.columnSpan: 2; Layout.fillWidth: true; label: qsTr("Accepted currencies"); help: qsTr("ISO codes separated by commas; empty = all")
                    FbTextField { id: currencies; monospace: true; placeholderText: "USD, EUR" } }
        }
        Banner { Layout.fillWidth: true; visible: !!editor.spec && !editor.spec.implemented; tone: "warning"
                 text: qsTr("No adapter is available yet for this provider: its settings can be stored but it cannot be enabled. The integration requires the provider's API documentation and test credentials.") }
        SectionTitle { text: qsTr("Settings and secrets"); hint: qsTr("Secret fields are never displayed. Leave empty to keep the current value.") }
        Repeater {
            id: fieldRep
            model: editor.spec ? editor.spec.fields : []
            delegate: Field {
                Layout.fillWidth: true
                required: modelData.required
                label: modelData.name + (modelData.secret ? "  🔒" : "")
                help: modelData.secret && editor.provider && editor.provider.secret_fields_set.indexOf(modelData.name) >= 0
                      ? qsTr("Set — type a new value to replace it") : (modelData.hint || "")
                FbTextField {
                    monospace: true
                    echoMode: modelData.secret ? TextInput.Password : TextInput.Normal
                    text: modelData.secret ? "" : (editor.values[modelData.name] || "")
                    onTextChanged: { var v = editor.values; v[modelData.name] = text; editor.values = v }
                }
            }
        }
        FbSwitch { id: enabled; text: qsTr("Enabled (offered to establishments)"); enabled: !editor.spec || editor.spec.implemented }
        footer: [
            FbButton { text: qsTr("Save"); enabled: ConsoleNav.can("superadmin"); onClicked: {
                    var settings = {}, secrets = {}
                    var fields = editor.spec ? editor.spec.fields : []
                    for (var i = 0; i < fields.length; ++i) {
                        var f = fields[i], v = editor.values[f.name]
                        if (f.secret) { if (v) secrets[f.name] = v }
                        else if (v !== undefined) settings[f.name] = v.trim()
                    }
                    var curr = currencies.text.split(",").map(function(c) { return c.trim().toUpperCase() }).filter(function(c) { return c.length === 3 })
                    var b = { display_name: name.text.trim(), environment: env.value, currencies: curr, settings: settings, secrets: secrets }
                    var cb = function(r) { if (r.ok) { editor.close(); page.reload(); ConsoleNav.toast(qsTr("Saved"), "success") } else ConsoleNav.fail(r) }
                    if (editor.provider) { b.enabled = enabled.checked; Api.send("PATCH", "/api/v1/admin/payment-providers/" + editor.provider.id, b, cb) }
                    else {
                        b.code = code.text.trim(); b.kind = kind.value
                        Api.send("POST", "/api/v1/admin/payment-providers", b, function(r) {
                            if (!r.ok) { ConsoleNav.fail(r); return }
                            if (enabled.checked) Api.send("PATCH", "/api/v1/admin/payment-providers/" + r.data.id, { enabled: true }, cb)
                            else cb(r)
                        })
                    }
                } },
            FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: editor.close() }
        ]
    }
}
