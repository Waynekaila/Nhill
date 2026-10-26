import QtQuick
import QtQuick.Controls

// variant: "primary" | "secondary" | "ghost" | "danger"
Button {
    id: control
    property string variant: "primary"
    property string iconName: ""
    property bool busy: false
    implicitHeight: Theme.controlHeight
    implicitWidth: Math.max(Theme.controlHeight, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: text.length > 0 ? Theme.s4 : Theme.s2
    rightPadding: text.length > 0 ? Theme.s4 : Theme.s2
    font.family: Theme.font
    font.pixelSize: Theme.fs_md
    font.weight: Font.DemiBold
    enabled: !busy
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    readonly property color fg: variant === "primary" ? Theme.textOnPrimary
                              : variant === "danger" ? "#FFFFFF"
                              : variant === "ghost" ? Theme.primary : Theme.text
    readonly property color bgColor: variant === "primary" ? (control.hovered ? Theme.primaryHover : Theme.primary)
                                   : variant === "danger" ? Theme.danger
                                   : variant === "ghost" ? (control.hovered ? Theme.primarySoft : "transparent")
                                   : (control.hovered ? Theme.surfaceAlt : Theme.surface)

    contentItem: Row {
        spacing: Theme.s2
        anchors.centerIn: parent
        BusyIndicator {
            visible: control.busy
            running: control.busy
            width: 16; height: 16
            anchors.verticalCenter: parent.verticalCenter
        }
        Icon {
            visible: control.iconName.length > 0 && !control.busy
            name: control.iconName
            color: control.fg
            size: 18
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: control.text.length > 0
            text: control.text
            font: control.font
            color: control.fg
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
        }
    }
    background: Rectangle {
        radius: Theme.radius
        color: control.bgColor
        opacity: control.enabled ? 1 : 0.45
        border.width: control.variant === "secondary" ? 1 : (control.visualFocus ? 2 : 0)
        border.color: control.visualFocus ? Theme.focus : Theme.lineStrong
    }
}
