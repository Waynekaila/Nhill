import QtQuick
import QtQuick.Controls

// variant: "primary" | "secondary" | "ghost" | "danger"
// 2.0.0: explicit disabled colours (readable), focus ring outside the button (visible on
// every variant), tooltip + accessible name for icon-only buttons (set "tip").
Button {
    id: control
    property string variant: "primary"
    property string iconName: ""
    property bool busy: false
    property string tip: ""
    readonly property bool inactive: !enabled && !busy
    implicitHeight: Theme.controlHeight
    implicitWidth: Math.max(Theme.controlHeight, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: text.length > 0 ? Theme.s4 : Theme.s2
    rightPadding: text.length > 0 ? Theme.s4 : Theme.s2
    font.family: Theme.font
    font.pixelSize: Theme.fs_md
    font.weight: Font.DemiBold
    enabled: !busy
    focusPolicy: Qt.StrongFocus
    Accessible.name: text.length > 0 ? text : tip
    Accessible.description: tip
    ToolTip.visible: tip.length > 0 && hovered
    ToolTip.text: tip
    ToolTip.delay: 600

    readonly property color fg: inactive ? Theme.disabledText
                              : variant === "primary" ? Theme.textOnPrimary
                              : variant === "danger" ? "#FFFFFF"
                              : variant === "ghost" ? Theme.primary : Theme.text
    readonly property color bgColor: inactive ? (variant === "ghost" ? "transparent" : Theme.disabledBg)
                                   : variant === "primary" ? (control.hovered || control.down ? Theme.primaryHover : Theme.primary)
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
    background: Item {
        Rectangle {
            // keyboard focus ring, drawn outside so it shows on dark and light buttons
            anchors.fill: parent
            anchors.margins: -3
            radius: Theme.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.focus
            visible: control.visualFocus
        }
        Rectangle {
            anchors.fill: parent
            radius: Theme.radius
            color: control.bgColor
            border.width: control.variant === "secondary" && !control.inactive ? 1 : 0
            border.color: Theme.inputBorder
        }
    }
}
