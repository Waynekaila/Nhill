import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

OnboardingFrame {
    showBack: false
    formWidth: 560
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
              text: qsTr("I register my company or my individual activity. The first 30 days are free, without payment details.") }
        ]
        delegate: AbstractButton {
            id: card
            Layout.fillWidth: true
            implicitHeight: content.implicitHeight + 2 * Theme.s4
            padding: Theme.s4
            focusPolicy: Qt.StrongFocus
            Accessible.name: modelData.title
            onClicked: modelData.key === "join" ? ApplicationWindow.window.openJoin() : ApplicationWindow.window.openCreate()
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
        FbButton { text: qsTr("Sign in"); variant: "ghost"; onClicked: ApplicationWindow.window.openLogin() }
        Item { Layout.fillWidth: true }
        FbIconButton { iconName: "settings"; tip: qsTr("Server settings"); onClicked: serverDialog.open() }
    }
    FbDialog {
        id: serverDialog
        title: qsTr("Server")
        subtitle: qsTr("Address of the Nhill service. Change it only if your provider asks you to.")
        Field {
            label: qsTr("Server address")
            FbTextField { id: serverField; text: Prefs.serverUrl; monospace: true; placeholderText: "https://api.example.com" }
        }
        footer: [
            FbButton { text: qsTr("Save"); onClicked: { Prefs.serverUrl = serverField.text; serverDialog.close(); Api.checkConnection() } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: serverDialog.close() }
        ]
    }
}
