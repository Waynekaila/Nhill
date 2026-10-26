import QtQuick
import QtQuick.Controls

// Text field. 2.0.0: border with >= 3:1 contrast; password fields (echoMode Password)
// get a show / hide button; accessible name falls back to the placeholder (Field sets
// the label as accessible name).
TextField {
    id: control
    property bool invalid: false
    property bool monospace: false
    property bool revealable: false
    property bool revealed: false
    property string accessibleName: ""
    implicitHeight: Theme.controlHeight
    font.family: monospace ? Theme.mono : Theme.font
    font.pixelSize: Theme.fs_md
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.primarySoft
    selectedTextColor: Theme.text
    leftPadding: Theme.s3
    rightPadding: revealable ? Theme.controlHeight : Theme.s3
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    Accessible.name: accessibleName.length > 0 ? accessibleName : placeholderText
    Component.onCompleted: if (echoMode === TextInput.Password) revealable = true
    function toggleReveal() {
        revealed = !revealed
        echoMode = revealed ? TextInput.Normal : TextInput.Password
        forceActiveFocus()
    }
    background: Rectangle {
        radius: Theme.radius
        color: control.enabled ? Theme.surface : Theme.surfaceAlt
        border.width: control.activeFocus || control.invalid ? 2 : 1
        border.color: control.invalid ? Theme.danger : control.activeFocus ? Theme.focus : Theme.inputBorder
    }
    FbIconButton {
        visible: control.revealable
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: control.height
        implicitHeight: control.height
        iconName: control.revealed ? "eyeOff" : "eye"
        iconColor: Theme.textMuted
        tip: control.revealed ? qsTr("Hide password") : qsTr("Show password")
        onClicked: control.toggleReveal()
    }
}
