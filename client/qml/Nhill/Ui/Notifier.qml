import QtQuick
import QtQuick.Layouts

// Non-intrusive toasts stacked at the bottom. Call show(text, tone).
Item {
    id: root
    anchors.fill: parent
    z: 1000
    ListModel { id: toasts }
    function show(text, tone) {
        toasts.append({ text: text, tone: tone || "neutral" })
        timer.restart()
    }
    function error(text) { show(text, "danger") }
    function success(text) { show(text, "success") }
    Timer { id: timer; interval: 4200; onTriggered: { if (toasts.count > 0) { toasts.remove(0); if (toasts.count > 0) restart() } } }
    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.s5
        spacing: Theme.s2
        width: Math.min(460, root.width - 32)
        Repeater {
            model: toasts
            delegate: Rectangle {
                width: parent.width
                height: msg.implicitHeight + 2 * Theme.s3
                radius: Theme.radius
                color: Theme.dark ? "#22302B" : "#16181D"
                Rectangle { width: 4; height: parent.height; radius: 2; color: model.tone === "danger" ? Theme.danger : model.tone === "success" ? Theme.success : Theme.accent }
                Text {
                    id: msg
                    anchors.verticalCenter: parent.verticalCenter
                    x: Theme.s4
                    width: parent.width - 2 * Theme.s4
                    text: model.text
                    color: "#F2F4F3"
                    wrapMode: Text.WordWrap
                    font.family: Theme.font; font.pixelSize: Theme.fs_sm
                }
                MouseArea { anchors.fill: parent; onClicked: toasts.remove(index) }
            }
        }
    }
}
