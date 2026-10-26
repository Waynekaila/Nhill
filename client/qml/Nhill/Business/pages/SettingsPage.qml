import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Settings")
    property string tab: "establishment"
    scrollable: true
    // Tabs are shown according to the role; the server enforces the same rules.
    readonly property var tabs: {
        var t = [{ value: "establishment", text: Session.individual ? qsTr("My business") : qsTr("Establishment") }]
        if (Session.can("code.view") && !Session.individual) t.push({ value: "code", text: qsTr("Establishment code") })
        if (Session.can("users.manage") && !Session.individual) t.push({ value: "team", text: qsTr("Team") })
        // Plan and subscription options: owners and administrators only.
        if (Session.can("subscription.manage")) t.push({ value: "subscription", text: qsTr("Subscription") })
        if (Session.can("settings.manage")) t.push({ value: "finance", text: qsTr("Taxes & accounts") })
        if (Session.can("pos.manage") || Session.can("pos.report")) t.push({ value: "pos", text: qsTr("Point of sale") })
        t.push({ value: "account", text: qsTr("My account") })
        return t
    }
    Component.onCompleted: { var ok = false; tabs.forEach(function(t) { if (t.value === page.tab) ok = true }); if (!ok) tab = "establishment" }
    TabStrip { Layout.fillWidth: true; model: page.tabs; current: page.tab; onSelected: function(v) { page.tab = v } }
    Loader {
        Layout.fillWidth: true
        source: page.tab === "establishment" ? "SettingsEstablishment.qml"
              : page.tab === "code" ? "SettingsCode.qml"
              : page.tab === "team" ? "SettingsTeam.qml"
              : page.tab === "subscription" ? "SettingsSubscription.qml"
              : page.tab === "finance" ? "SettingsFinance.qml"
              : page.tab === "pos" ? "SettingsPos.qml" : "SettingsAccount.qml"
    }
}
