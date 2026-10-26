import QtQuick
import QtQuick.Controls

Switch {
    id: control
    font.family: Theme.font
    font.pixelSize: Theme.fs_md
    implicitHeight: Theme.controlHeight
    indicator: Rectangle {
        x: control.leftPadding
        y: (control.height - height) / 2
        width: 40; height: 22; radius: 11
        color: control.checked ? Theme.primary : Theme.lineStrong
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.focus
        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3; width: 16; height: 16; radius: 8
            color: "#FFFFFF"
            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }
    contentItem: Text {
        leftPadding: control.indicator.width + Theme.s3
        text: control.text
        font: control.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }
}
