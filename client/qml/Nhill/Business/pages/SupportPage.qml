import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Help & support")
    subtitle: qsTr("Questions and bug reports to the Nhill team")
    function reload() { center.reload() }
    SupportCenter {
        id: center
        anchors.fill: parent
        source: "business"
        failHandler: function(r) { Nav.fail(r) }
        toastHandler: function(t, tone) { Nav.toast(t, tone) }
    }
}
