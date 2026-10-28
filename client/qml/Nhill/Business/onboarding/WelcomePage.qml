import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

OnboardingFrame {
    id: page
    showBack: false
    formWidth: 560
    // Action chosen before the server address was entered (join, create or sign in).
    property string pending: ""
    function go(action) {
        // The server is configured when the business is created (first step of the
        // creation); joining or signing in needs it first.
        if (action !== "create" && !Prefs.serverConfigured) { page.pending = action; serverDialog.open(); return }
        if (action === "join") ApplicationWindow.window.openJoin()
        else if (action === "create") ApplicationWindow.window.openCreate()
        else ApplicationWindow.window.openLogin()
    }
    Text {
        text: qsTr("Welcome")
        font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.DemiBold
        color: Theme.text
    }
    Text {
        text: qsTr("How do you want to start?")
        font.family: Theme.font; font.pixelSize: Theme.fs_lg
        color: Theme.textMuted
        Layout.fillWidth: true; wrapMode: Text.WordWrap
    }
    Repeater {
        model: [
            { key: "join", icon: "key", title: qsTr("My business already exists"),
              text: qsTr("I join a business already registered on Nhill with its establishment code and my personal access.") },
            { key: "create", icon: "building", title: qsTr("Create my business"),
              text: qsTr("I register my company or my individual activity, without payment details.") }
        ]
        delegate: AbstractButton {
            id: card
            Layout.fillWidth: true
            implicitHeight: content.implicitHeight + 2 * Theme.s4
            padding: Theme.s4
            focusPolicy: Qt.StrongFocus
            Accessible.name: modelData.title
            onClicked: page.go(modelData.key)
            background: Rectangle {
                radius: Theme.radiusLg
                color: card.hovered ? Theme.surfaceAlt : Theme.surface
                border.width: card.visualFocus ? 2 : 1
                border.color: card.visualFocus ? Theme.focus : card.hovered ? Theme.primary : Theme.line
            }
            contentItem: RowLayout {
                id: content
                spacing: Theme.s4
                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    width: 44; height: 44; radius: 10
                    color: Theme.primarySoft
                    Icon { anchors.centerIn: parent; name: modelData.icon; color: Theme.primary; size: 22 }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: modelData.title; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Text { text: modelData.text; font.family: Theme.font; font.pixelSize: Theme.fs_md; color: Theme.textMuted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
                Icon { name: "chevronRight"; color: Theme.textMuted; size: 20 }
            }
        }
    }
    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line; Layout.topMargin: Theme.s2 }
    RowLayout {
        Layout.fillWidth: true
        Text { text: qsTr("Already have an account?"); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_md }
        FbButton { text: qsTr("Sign in"); variant: "ghost"; onClicked: page.go("login") }
        Item { Layout.fillWidth: true }
        FbIconButton { iconName: "settings"; tip: qsTr("Server settings"); onClicked: serverDialog.open() }
    }
    Text {
        visible: Prefs.serverConfigured
        Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
        text: qsTr("Server: %1").arg(Prefs.serverUrl)
        color: Theme.textFaint; font.family: Theme.mono; font.pixelSize: Theme.fs_xs
    }
    FbDialog {
        id: serverDialog
        title: qsTr("Nhill server")
        subtitle: qsTr("Enter the address of the Nhill server you use. It is checked, then kept on this device; you can change it later.")
        onClosed: if (!Prefs.serverConfigured) page.pending = ""
        ServerAddressForm {
            Layout.fillWidth: true
            onSaved: function(url) {
                serverDialog.close()
                var next = page.pending
                page.pending = ""
                if (next) page.go(next)
            }
        }
        footer: [ FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: serverDialog.close() } ]
    }
}
