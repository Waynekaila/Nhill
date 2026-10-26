import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

ColumnLayout {
    id: root
    spacing: Theme.s4
    property var code: null
    property bool hidden: true
    function load() { Api.get("/api/v1/establishment/code", function(r) { if (r.ok) root.code = r.data; else Nav.fail(r) }) }
    Component.onCompleted: load()
    FbCard {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.s3
            SectionTitle { text: qsTr("Establishment code") }
            Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font
                   text: qsTr("Give this code to your colleagues so they can join the establishment. It identifies the establishment only: each person still needs an invitation and their own password. It is renewed automatically every 30 days; connected users stay connected.") }
            RowLayout {
                spacing: Theme.s3
                Rectangle {
                    Layout.preferredHeight: 64
                    Layout.preferredWidth: codeText.implicitWidth + Theme.s6 * 2
                    radius: Theme.radius
                    color: Theme.surfaceAlt
                    border.color: Theme.line
                    Text { id: codeText; anchors.centerIn: parent; font.family: Theme.mono; font.pixelSize: Theme.compact ? 20 : 26; font.weight: Font.DemiBold; color: Theme.text
                           text: !root.code ? "…" : root.hidden ? "NH-" + root.code.code.slice(3).replace(/[0-9A-Z]/g, "•") : root.code.code }
                }
                FbIconButton { iconName: "eye"; tip: root.hidden ? qsTr("Show") : qsTr("Hide"); onClicked: root.hidden = !root.hidden }
                FbIconButton { iconName: "copy"; tip: qsTr("Copy"); enabled: !!root.code; onClicked: { Tools.copyText(root.code.code); Nav.toast(qsTr("Code copied"), "success") } }
            }
            Text { visible: !!root.code; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                   text: root.code ? qsTr("Created on %1 · Next automatic renewal: %2").arg(Fmt.dateTime(root.code.created_at)).arg(Fmt.dateTime(root.code.expires_at)) : "" }
            RowLayout {
                visible: Session.can("code.rotate")
                FbButton { text: qsTr("Renew now"); iconName: "refresh"; variant: "secondary"; onClicked: confirm.open() }
                Item { Layout.fillWidth: true }
            }
        }
    }
    FbConfirm {
        id: confirm
        title: qsTr("Renew the code?")
        message: qsTr("The current code stops working immediately. People already connected are not affected. Use this if the code has been shared with someone who should not have it.")
        confirmText: qsTr("Renew")
        onAccepted: Api.send("POST", "/api/v1/establishment/code/rotate", { reason: "manual" }, function(r) {
            if (r.ok) { root.code = r.data; root.hidden = false; Nav.toast(qsTr("New code generated"), "success") } else Nav.fail(r) })
    }
}
