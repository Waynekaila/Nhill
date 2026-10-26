import QtQuick

// Status pill. tone: neutral | success | warning | danger | info | primary | accent
Rectangle {
    id: root
    property string text: ""
    property string tone: "neutral"
    readonly property color fg: tone === "success" ? Theme.success : tone === "warning" ? Theme.warning
                              : tone === "danger" ? Theme.danger : tone === "info" ? Theme.info
                              : tone === "primary" ? Theme.primary : tone === "accent" ? Theme.accent : Theme.textMuted
    readonly property color bgc: tone === "success" ? Theme.successSoft : tone === "warning" ? Theme.warningSoft
                               : tone === "danger" ? Theme.dangerSoft : tone === "info" ? Theme.infoSoft
                               : tone === "primary" ? Theme.primarySoft : tone === "accent" ? Theme.accentSoft : Theme.surfaceAlt
    implicitWidth: label.implicitWidth + 2 * Theme.s3
    implicitHeight: 22
    radius: 11
    color: bgc
    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.fg
        font.family: Theme.font
        font.pixelSize: Theme.fs_xs
        font.weight: Font.DemiBold
    }
}
