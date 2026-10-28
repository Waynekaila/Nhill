import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

// Application shell: sidebar (desktop) or drawer (phone), top bar, page stack.
Item {
    id: shell
    property string section: "dashboard"
    // A section appears only when the role allows it (perm) AND the plan of the
    // establishment includes its feature ("a|b": one of them). The server refuses the
    // same requests; this list only follows the rights it sends (Session.me).
    readonly property var sections: [
        { key: "dashboard", icon: "dashboard", title: qsTr("Dashboard"), page: "pages/DashboardPage.qml" },
        { key: "sales", icon: "invoice", title: qsTr("Sales"), page: "pages/SalesPage.qml", feature: "sales.invoices|sales.quotes|pos" },
        { key: "customers", icon: "users", title: Session.has("purchases") || Session.has("expenses") ? qsTr("Customers & suppliers") : qsTr("Customers"), page: "pages/PartiesPage.qml" },
        { key: "products", icon: "box", title: qsTr("Products & services"), page: "pages/ProductsPage.qml" },
        { key: "stock", icon: "stock", title: qsTr("Stock"), page: "pages/StockPage.qml" },
        { key: "purchases", icon: "cart", title: qsTr("Purchases"), page: "pages/PurchasesPage.qml", perm: "purchases.write", feature: "purchases" },
        { key: "expenses", icon: "wallet", title: qsTr("Expenses"), page: "pages/ExpensesPage.qml", feature: "expenses" },
        { key: "payments", icon: "payment", title: qsTr("Payments"), page: "pages/PaymentsPage.qml" },
        { key: "reports", icon: "chart", title: qsTr("Reports"), page: "pages/ReportsPage.qml", perm: "reports.read" },
        { key: "activity", icon: "clock", title: Session.can("audit.read") ? qsTr("Activity log") : qsTr("My activity"), page: "pages/ActivityPage.qml" },
        { key: "settings", icon: "settings", title: qsTr("Settings"), page: "pages/SettingsPage.qml" },
        { key: "support", icon: "info", title: qsTr("Help & support"), page: "pages/SupportPage.qml" }
    ]
    function hasAny(spec) { return spec.split("|").some(function(f) { return Session.has(f) }) }
    function allowed(s) { return (!s.perm || Session.can(s.perm)) && (!s.feature || hasAny(s.feature)) }
    function visibleSections() { return sections.filter(allowed) }
    // The rights changed (plan, role): a section that is no longer allowed is left.
    Connections {
        target: Session
        function onChanged() {
            for (var i = 0; i < shell.sections.length; ++i)
                if (shell.sections[i].key === shell.section && !shell.allowed(shell.sections[i])) { shell.navigate("dashboard"); return }
        }
    }
    function navigate(key, props) {
        var target = sections[0]   // dashboard, always allowed
        for (var i = 0; i < sections.length; ++i)
            if (sections[i].key === key && allowed(sections[i])) target = sections[i]
        section = target.key
        pages.replace(null, Qt.resolvedUrl(target.page), target.key === key ? (props || {}) : {})
        drawer.close()
    }
    function push(url, props) { pages.push(Qt.resolvedUrl(url), props || {}) }
    function pop() { if (pages.depth > 1) pages.pop() }
    function refreshCurrent() { if (pages.currentItem && pages.currentItem.reload) pages.currentItem.reload() }

    Component.onCompleted: { Nav.shell = shell; navigate("dashboard") }

    // ---- keyboard shortcuts (desktop)
    Shortcut { sequence: "Ctrl+K"; onActivated: searchPopup.openSearch() }
    Shortcut { sequence: "Ctrl+N"; enabled: Session.can("sales.write") && Session.has("sales.invoices"); onActivated: shell.push("pages/DocumentEditorPage.qml", { docType: "invoice" }) }
    Shortcut { sequence: "Ctrl+1"; onActivated: shell.navigate("dashboard") }
    Shortcut { sequence: "Ctrl+2"; enabled: Session.has("sales.invoices") || Session.has("sales.quotes") || Session.has("pos"); onActivated: shell.navigate("sales") }
    Shortcut { sequence: "Ctrl+3"; onActivated: shell.navigate("customers") }
    Shortcut { sequence: "Ctrl+4"; onActivated: shell.navigate("products") }
    Shortcut { sequence: "F5"; onActivated: shell.refreshCurrent() }
    Shortcut { sequences: [StandardKey.Back]; onActivated: shell.pop() }

    Component {
        id: navList
        ColumnLayout {
            spacing: 2
            Repeater {
                // Session.me is read so that the menu follows every change of rights.
                model: Session.me ? shell.visibleSections() : []
                delegate: AbstractButton {
                    id: item
                    readonly property bool active: shell.section === modelData.key
                    Layout.fillWidth: true
                    implicitHeight: Theme.compact ? 48 : 40
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: modelData.title
                    ToolTip.visible: hovered && navLabel.truncated
                    ToolTip.text: modelData.title
                    ToolTip.delay: 400
                    onClicked: shell.navigate(modelData.key)
                    background: Rectangle {
                        radius: Theme.radius
                        color: item.active ? Theme.sidebarActive : item.hovered ? "#14FFFFFF" : "transparent"
                        border.width: item.visualFocus ? 2 : 0
                        border.color: Theme.focus
                        Rectangle { visible: item.active; width: 3; height: 18; radius: 2; color: Theme.accent; anchors.verticalCenter: parent.verticalCenter; x: 0 }
                    }
                    contentItem: RowLayout {
                        spacing: Theme.s3
                        Item { width: 4 }
                        Icon { name: modelData.icon; size: 19; color: item.active ? "#FFFFFF" : Theme.sidebarMuted }
                        Text {
                            id: navLabel
                            text: modelData.title
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: item.active ? "#FFFFFF" : Theme.sidebarText
                            font.family: Theme.font; font.pixelSize: Theme.fs_md
                            font.weight: item.active ? Font.DemiBold : Font.Normal
                        }
                    }
                }
            }
        }
    }

    Component {
        id: sideFooter
        ColumnLayout {
            spacing: Theme.s2
            Rectangle { Layout.fillWidth: true; height: 1; color: "#1FFFFFFF" }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.s2
                Rectangle {
                    width: 34; height: 34; radius: 17; color: Theme.accent
                    Text { anchors.centerIn: parent; color: "#FFFFFF"; font.family: Theme.font; font.weight: Font.DemiBold
                           text: (Session.me.user ? Session.me.user.full_name : "?").split(" ").map(function(p) { return p.charAt(0) }).slice(0, 2).join("").toUpperCase() }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    HoverHandler { id: whoHover }
                    ToolTip.visible: whoHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: (Session.establishment.trade_name || "") + "\n" + (Session.me.user ? Session.me.user.full_name : "") + " · " + Labels.role(Session.role)
                    Text { text: Session.establishment.trade_name || ""; color: "#FFFFFF"; font.family: Theme.font; font.pixelSize: Theme.fs_sm; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                    Text { text: (Session.me.user ? Session.me.user.full_name : "") + " · " + Labels.role(Session.role); color: Theme.sidebarMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs; elide: Text.ElideRight; Layout.fillWidth: true }
                }
                FbIconButton { iconName: "logout"; iconColor: Theme.sidebarMuted; tip: qsTr("Sign out")
                               onClicked: { Session.logout(); ApplicationWindow.window.openWelcome() } }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- sidebar (desktop)
        Rectangle {
            visible: !Theme.compact
            Layout.fillHeight: true
            Layout.preferredWidth: 248
            color: Theme.sidebar
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s4
                spacing: Theme.s4
                LogoMark { variant: "reverse"; app: "business"; Layout.preferredHeight: 30; Layout.preferredWidth: 170; horizontalAlignment: Image.AlignLeft; Layout.topMargin: Theme.s2; Layout.bottomMargin: Theme.s2 }
                // 2.0.0: the menu scrolls when the window is short (1366 x 768 laptops,
                // 125 % scaling) instead of overlapping the account footer.
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: sideNav.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollIndicator.vertical: ScrollIndicator {}
                    Loader { id: sideNav; width: parent.width; sourceComponent: navList }
                }
                Loader { sourceComponent: sideFooter; Layout.fillWidth: true }
            }
        }

        // ---- main area
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.compact ? 56 : 64
                color: Theme.surface
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.compact ? Theme.s2 : Theme.s5
                    anchors.rightMargin: Theme.compact ? Theme.s2 : Theme.s5
                    spacing: Theme.s2
                    FbIconButton { visible: Theme.compact && pages.depth <= 1; iconName: "menu"; tip: qsTr("Menu"); onClicked: drawer.open() }
                    FbIconButton { visible: pages.depth > 1; iconName: "chevronLeft"; tip: qsTr("Back"); onClicked: shell.pop() }
                    Text {
                        visible: Theme.compact
                        Layout.fillWidth: true
                        text: pages.currentItem && pages.currentItem.title ? pages.currentItem.title : ""
                        font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold
                        color: Theme.text; elide: Text.ElideRight
                    }
                    AbstractButton {
                        visible: !Theme.compact
                        Layout.preferredWidth: 420
                        Layout.maximumWidth: 420
                        Layout.minimumWidth: 160
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        Accessible.name: qsTr("Search")
                        onClicked: searchPopup.openSearch()
                        background: Rectangle { radius: Theme.radius; color: Theme.surfaceAlt; border.color: Theme.line }
                        contentItem: RowLayout {
                            spacing: Theme.s2
                            Item { width: 2 }
                            Icon { name: "search"; size: 18; color: Theme.textFaint }
                            Text { text: qsTr("Search customers, products, invoices…"); color: Theme.textFaint; font.family: Theme.font; font.pixelSize: Theme.fs_md; Layout.fillWidth: true; elide: Text.ElideRight }
                            Rectangle { width: kbd.implicitWidth + 10; height: 22; radius: 4; color: Theme.surface; border.color: Theme.line
                                        Text { id: kbd; anchors.centerIn: parent; text: "Ctrl K"; font.family: Theme.mono; font.pixelSize: Theme.fs_xs; color: Theme.textMuted } }
                            Item { width: 2 }
                        }
                    }
                    Item { Layout.fillWidth: !Theme.compact }
                    FbIconButton { visible: Theme.compact; iconName: "search"; tip: qsTr("Search"); onClicked: searchPopup.openSearch() }
                    ConnectionBadge {}
                    FbChip {
                        visible: !Theme.compact && Session.subscription.status !== "active" && Session.can("subscription.manage")
                        property var s: Labels.subscription(Session.subscription.status)
                        text: Session.subscription.status === "trialing" ? qsTr("Trial · %n day(s) left", "", Session.subscription.days_left || 0) : s.text
                        tone: s.tone
                        MouseArea { anchors.fill: parent; onClicked: shell.navigate("settings", { tab: "subscription" }) }
                    }
                    NotificationsButton {}
                }
            }

            Banner {
                Layout.fillWidth: true
                Layout.margins: Theme.s3
                Layout.bottomMargin: 0
                visible: Session.subscription.status === "suspended" || (Session.subscription.status === "past_due" && Session.can("subscription.manage"))
                tone: Session.subscription.status === "suspended" ? "danger" : "warning"
                text: Session.subscription.status === "suspended"
                      ? (Session.can("subscription.manage") ? qsTr("Your subscription is suspended: you can consult and export your data, but not create or modify anything.")
                                                            : qsTr("Read-only mode: you can consult and export data, but not create or modify anything. Contact the administrator of the establishment."))
                      : qsTr("Payment not received: grace period until %1. Service continues meanwhile.").arg(Fmt.date(Session.subscription.grace_until || ""))
                actionText: Session.can("subscription.manage") ? qsTr("Renew") : ""
                onAction: shell.navigate("settings", { tab: "subscription" })
            }

            StackView {
                id: pages
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                pushEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 } }
                pushExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 } }
                popEnter: pushEnter
                popExit: pushExit
                replaceEnter: pushEnter
                replaceExit: pushExit
            }
        }
    }

    // ---- drawer (phone)
    Drawer {
        id: drawer
        width: Math.min(300, shell.width * 0.85)
        height: shell.height
        edge: Qt.LeftEdge
        background: Rectangle { color: Theme.sidebar }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.s4
            spacing: Theme.s3
            LogoMark { variant: "reverse"; app: "business"; Layout.preferredHeight: 28; Layout.preferredWidth: 160; horizontalAlignment: Image.AlignLeft }
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentHeight: drawerNav.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollIndicator.vertical: ScrollIndicator {}
                Loader { id: drawerNav; width: parent.width; sourceComponent: navList }
            }
            Loader { sourceComponent: sideFooter; Layout.fillWidth: true }
        }
    }

    GlobalSearch { id: searchPopup; onOpenDocument: function(id) { shell.push("pages/DocumentPage.qml", { docId: id }) }
                   onOpenParty: function(id) { shell.push("pages/PartyPage.qml", { partyId: id }) }
                   onOpenProduct: function(id) { shell.navigate("products", { openId: id }) } }
}
