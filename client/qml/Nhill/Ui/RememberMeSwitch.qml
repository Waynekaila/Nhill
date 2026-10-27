import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core

// "Remember me" on the sign-in screens: when on, the session token is kept in the
// system's secure store (Windows Credential Manager, Android Keystore, Linux keyring)
// and the application reopens signed in. Off: nothing is kept on the device.
ColumnLayout {
    id: root
    Layout.fillWidth: true
    spacing: 2
    property alias checked: toggle.checked
    FbSwitch {
        id: toggle
        Layout.fillWidth: true
        text: qsTr("Remember me on this device")
        enabled: Session.rememberSupported
        checked: Session.rememberSupported && Session.remember
    }
    Text {
        Layout.fillWidth: true
        leftPadding: 52
        wrapMode: Text.Wrap
        color: Theme.textFaint
        font.family: Theme.font
        font.pixelSize: Theme.fs_xs
        text: Session.rememberSupported
              ? qsTr("Only on a device you do not share. Signing out forgets the session.")
              : qsTr("Not available on this device: the session is never stored without the system's secure storage.")
    }
}
