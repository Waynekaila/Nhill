import QtQuick
import Nhill.Core

// Accepts "1 234,50" or "1234.50"; exposes the exact plain value ("1234.50")
// or "" when invalid. Never converts to floating point.
FbTextField {
    id: root
    property string currency: Session.currency
    property string value: ""
    readonly property bool valid: text.trim().length === 0 || Fmt.parseMoney(text, currency).length > 0
    inputMethodHints: Qt.ImhFormattedNumbersOnly
    horizontalAlignment: TextInput.AlignRight
    invalid: !valid
    onTextChanged: value = Fmt.parseMoney(text, currency)
    function setValue(plain) { text = plain ? Fmt.amount(plain, currency) : "" }
    onEditingFinished: if (valid && value.length > 0) text = Fmt.amount(value, currency)
}
