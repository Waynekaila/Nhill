import QtQuick
import QtQuick.Controls

TextField {
    id: control
    property bool invalid: false
    property bool monospace: false
    implicitHeight: Theme.controlHeight
    font.family: monospace ? Theme.mono : Theme.font
    font.pixelSize: Theme.fs_md
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.primarySoft
    selectedTextColor: Theme.text
    leftPadding: Theme.s3
    rightPadding: Theme.s3
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    Accessible.name: placeholderText
    background: Rectangle {
        radius: Theme.radius
        color: control.enabled ? Theme.surface : Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.invalid ? Theme.danger : control.activeFocus ? Theme.focus : Theme.lineStrong
    }
}
