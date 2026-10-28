import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

// Create or edit a customer / supplier.
FbDialog {
    id: dlg
    property var party: null
    property bool saving: false
    signal saved(var party)
    title: party ? qsTr("Edit") + " — " + party.name : (isCustomer.checked && !isSupplier.checked ? qsTr("New customer") : isSupplier.checked && !isCustomer.checked ? qsTr("New supplier") : qsTr("New contact"))
    function openNew(customer, supplier) {
        party = null
        name.text = ""; phone.text = ""; email.text = ""; address.text = ""; city.text = ""; nif.text = ""; notes.text = ""
        terms.text = "0"; kind.value = "company"
        isCustomer.checked = customer; isSupplier.checked = supplier
        open()
    }
    function openEdit(p) {
        party = p
        name.text = p.name; phone.text = p.phone || ""; email.text = p.email || ""; address.text = p.address; city.text = p.city
        nif.text = p.tax_ids && p.tax_ids.nif ? p.tax_ids.nif : ""; notes.text = p.notes; terms.text = String(p.payment_terms_days)
        kind.value = p.kind; isCustomer.checked = p.is_customer; isSupplier.checked = p.is_supplier
        open()
    }
    function save() {
        if (name.text.trim().length === 0) { nameField.error = qsTr("The name is required."); return }
        if (email.text && !Check.email(email.text)) { emailField.error = qsTr("Invalid e-mail address."); return }
        if (phone.text && !Check.phone(phone.text)) { phoneField.error = qsTr("Invalid phone number."); return }
        var b = { name: name.text, kind: kind.value, phone: phone.text, email: email.text, address: address.text, city: city.text,
                  notes: notes.text, payment_terms_days: parseInt(terms.text || "0", 10) || 0,
                  is_customer: isCustomer.checked, is_supplier: isSupplier.checked,
                  tax_ids: nif.text ? { nif: nif.text } : {} }
        saving = true
        var cb = function(r) { saving = false; if (r.ok) { dlg.close(); Nav.toast(qsTr("Saved"), "success"); dlg.saved(r.data) } else Nav.fail(r) }
        party ? Api.send("PATCH", "/api/v1/parties/" + party.id, b, cb) : Api.send("POST", "/api/v1/parties", b, cb)
    }
    RowLayout {
        Layout.fillWidth: true
        FbSwitch { id: isCustomer; text: qsTr("Customer"); checked: true }
        FbSwitch { id: isSupplier; text: qsTr("Supplier"); visible: Session.can("purchases.write") || Session.can("expenses.write") }
    }
    Field { label: qsTr("Type"); FbComboBox { id: kind; model: [{ value: "company", text: qsTr("Company / organisation") }, { value: "individual", text: qsTr("Private person") }]; value: "company" } }
    Field { id: nameField; label: qsTr("Name"); required: true; FbTextField { id: name; onTextChanged: nameField.error = "" } }
    GridLayout {
        Layout.fillWidth: true
        columns: Theme.compact ? 1 : 2
        columnSpacing: Theme.s3; rowSpacing: Theme.s3
        Field { id: phoneField; label: qsTr("Phone"); FbTextField { id: phone; inputMethodHints: Qt.ImhDialableCharactersOnly; onTextChanged: phoneField.error = "" } }
        Field { id: emailField; label: qsTr("E-mail"); FbTextField { id: email; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase; onTextChanged: emailField.error = "" } }
        Field { label: qsTr("Address"); FbTextField { id: address } }
        Field { label: qsTr("City"); FbTextField { id: city } }
        Field { label: qsTr("Tax number (NIF)"); FbTextField { id: nif; monospace: true } }
        Field { label: qsTr("Payment terms (days)"); FbTextField { id: terms; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 0; top: 365 } } }
    }
    Field { label: qsTr("Internal notes"); help: qsTr("Never printed on documents."); FbTextArea { id: notes } }
    footer: [
        FbButton { text: qsTr("Save"); busy: dlg.saving; onClicked: dlg.save() },
        FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: dlg.close() }
    ]
}
