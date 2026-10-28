import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

Item {
    id: shell
    property string section: "stats"
    // Visibility follows the server-side role rules (the server enforces them anyway).
    readonly property var sections: [
        { key: "stats", icon: "chart", title: qsTr("Overview"), page: "pages/StatsPage.qml", role: "" },
        { key: "notifications", icon: "bell", title: qsTr("Notifications"), page: "pages/NotificationsPage.qml", role: "" },
        { key: "establishments", icon: "building", title: qsTr("Establishments"), page: "pages/EstablishmentsPage.qml", role: "" },
        { key: "reports", icon: "download", title: qsTr("Reports"), page: "pages/PlatformReportsPage.qml", role: "billing|auditor" },
        { key: "requests", icon: "users", title: qsTr("Attachment requests"), page: "pages/RequestsPage.qml", role: "support" },
        { key: "tickets", icon: "info", title: qsTr("Support tickets"), page: "pages/TicketsPage.qml", role: "support" },
        { key: "orders", icon: "payment", title: qsTr("Transactions"), page: "pages/OrdersPage.qml", role: "billing|auditor" },
        { key: "plans", icon: "receipt", title: qsTr("Plans & prices"), page: "pages/PlansPage.qml", role: "" },
        { key: "rules", icon: "filter", title: qsTr("Assignment rules"), page: "pages/PlanRulesPage.qml", role: "" },
        { key: "promos", icon: "filter", title: qsTr("Promo codes"), page: "pages/PromoCodesPage.qml", role: "billing" },
        { key: "providers", icon: "wallet", title: qsTr("Payment providers"), page: "pages/ProvidersPage.qml", role: "billing" },
        { key: "webhooks", icon: "swap", title: qsTr("Provider events"), page: "pages/WebhooksPage.qml", role: "billing|auditor" },
        { key: "errors", icon: "alert", title: qsTr("Technical errors"), page: "pages/ErrorsPage.qml", role: "support|auditor" },
        { key: "audit", icon: "shield", title: qsTr("Audit log"), page: "pages/AuditPage.qml", role: "auditor|support" },
        { key: "announcements", icon: "bell", title: qsTr("Announcements"), page: "pages/AnnouncementsPage.qml", role: "" },
        { key: "settings", icon: "settings", title: qsTr("Platform settings"), page: "pages/PlatformSettingsPage.qml", role: "" }
    ]
    function allowed(s) { if (!s.role) return true; return s.role.split("|").some(function(r) { return ConsoleNav.can(r) }) }
    function navigate(key) {
        for (var i = 0; i < sections.length; ++i) if (sections[i].key === key) { section = key; pages.replace(null, Qt.resolvedUrl(sections[i].page)); return }
    }
    function push(url, props) { pages.push(Qt.resolvedUrl(url), props || {}) }
    property int unread: 0
    function refreshBadges() { Api.get("/api/v1/admin/notifications", function(r) { if (r.ok) shell.unread = r.data.unread }) }
    Timer { interval: 60000; repeat: true; running: true; onTriggered: shell.refreshBadges() }
    Component.onCompleted: { ConsoleNav.shell = shell; navigate("stats"); refreshBadges() }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 248
            color: Theme.sidebar
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s4
                spacing: Theme.s1
                LogoMark { variant: "reverse"; app: "console"; Layout.preferredHeight: 30; Layout.preferredWidth: 170; horizontalAlignment: Image.AlignLeft }
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentHeight: navCol.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    ColumnLayout {
                        id: navCol
                        width: parent.width
                        spacing: 2
                        Repeater {
                            model: shell.sections.filter(shell.allowed)
                            delegate: ItemDelegate {
                                id: item
                                Layout.fillWidth: true
                                readonly property bool active: shell.section === modelData.key
                                onClicked: shell.navigate(modelData.key)
                                focusPolicy: Qt.StrongFocus
                                Accessible.name: modelData.title
                                ToolTip.visible: hovered && consoleNavLabel.truncated
                                ToolTip.text: modelData.title
                                ToolTip.delay: 400
                                background: Rectangle { radius: Theme.radius; color: item.active ? Theme.sidebarActive : item.hovered ? "#163650" : "transparent"
                                                        border.width: item.visualFocus ? 2 : 0; border.color: Theme.focus }
                                contentItem: RowLayout {
                                    spacing: Theme.s3
                                    Icon { name: modelData.icon; size: 18; color: item.active ? "#FFFFFF" : Theme.sidebarMuted }
                                    Text { id: consoleNavLabel; text: modelData.title; color: item.active ? "#FFFFFF" : Theme.sidebarText; font.family: Theme.font; font.pixelSize: Theme.fs_md; Layout.fillWidth: true; elide: Text.ElideRight }
                                    Rectangle {
                                        visible: modelData.key === "notifications" && shell.unread > 0
                                        implicitWidth: Math.max(20, badge.implicitWidth + 10); implicitHeight: 20; radius: 10; color: Theme.accent
                                        Text { id: badge; anchors.centerIn: parent; text: shell.unread > 99 ? "99+" : String(shell.unread); color: "#FFFFFF"; font.family: Theme.font; font.pixelSize: Theme.fs_xs; font.weight: Font.DemiBold }
                                    }
                                }
                            }
                        }
                    }
                }
                Text { text: ConsoleNav.me.full_name || ""; color: "#FFFFFF"; font.family: Theme.font; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { text: (ConsoleNav.me.email || "") + " · " + (ConsoleNav.me.role || ""); color: Theme.sidebarMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs; elide: Text.ElideRight; Layout.fillWidth: true }
                ItemDelegate {
                    id: signOut
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.s2
                    onClicked: ConsoleNav.window.signOut("")
                    background: Rectangle { radius: Theme.radius; color: signOut.hovered ? "#163650" : "transparent"; border.color: "#24496A" }
                    contentItem: RowLayout {
                        spacing: Theme.s3
                        Icon { name: "logout"; size: 18; color: Theme.sidebarMuted }
                        Text { text: qsTr("Sign out"); color: Theme.sidebarText; font.family: Theme.font; Layout.fillWidth: true }
                    }
                }
            }
        }
        StackView {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
        }
    }
}
