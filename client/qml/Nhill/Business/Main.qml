import QtQuick
import QtQuick.Window
import QtQuick.Controls
import Nhill.Core
import Nhill.Ui

ApplicationWindow {
    id: win
    // 2.0.0 window sizing: the first size fits the screen (1366 x 768 laptops, 125 percent
    // scaling, small netbooks), the window is centred, and small screens start maximised.
    // The documentation tour can still force a size (NHILL_TOUR_SIZE).
    readonly property bool tourSized: typeof tourConfig !== "undefined" && !!tourConfig.width
    readonly property int availW: Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : 1280
    readonly property int availH: Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 820
    width: tourSized ? tourConfig.width : Theme.fitWidth(1280, availW)
    height: tourSized ? tourConfig.height : Theme.fitHeight(820, availH)
    minimumWidth: Math.min(360, availW)
    minimumHeight: Math.min(520, availH)
    function placeOnScreen() {
        if (tourSized || Qt.platform.os === "android" || Qt.platform.os === "ios") return
        x = Screen.virtualX + Math.max(0, Math.round((availW - width) / 2))
        y = Screen.virtualY + Math.max(0, Math.round((availH - height) / 2))
        if (availW < 1024 || availH < 680) showMaximized()
    }
    visible: true
    title: Session.loggedIn ? Session.establishment.trade_name + " — Nhill" : "Nhill"
    color: Theme.bg
    font.family: Theme.font

    Binding { target: Theme; property: "compact"; value: win.width < 820 }
    Component.onCompleted: { Nav.window = win; placeOnScreen() }

    // ---- global helpers used by every page
    function toast(text, tone) { notifier.show(text, tone) }
    function fail(result) {
        var m = Errors.message(result)
        if (m.length > 0) notifier.error(m)
    }
    function openWelcome() { stack.replace(null, welcomeComp) }
    function openLogin(message) { stack.replace(null, loginComp, { notice: message || "" }) }
    function openJoin() { stack.push(joinComp) }
    function openCreate() { stack.push(createComp) }
    function openShell() { stack.replace(null, shellComp) }
    function back() { if (stack.depth > 1) stack.pop() }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: splashComp
    }

    Component {
        id: splashComp
        Rectangle {
            color: Theme.bg
            LogoMark { app: "business"; anchors.centerIn: parent; height: 56; width: 340 }
            id: splash
            property string required: ""
            BusyIndicator { visible: !splash.required; anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.verticalCenter; anchors.topMargin: 60 }
            Column {
                visible: !!splash.required
                anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.verticalCenter; anchors.topMargin: 60
                width: Math.min(parent.width - 48, 480)
                spacing: Theme.s3
                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg
                       text: qsTr("This version of Nhill (%1) is no longer supported. Please install version %2 or later.").arg(Qt.application.version).arg(splash.required) }
            }
            // Refuses to work with a server that requires a newer client (GET /meta).
            function newer(a, b) {
                var x = a.split("."), y = b.split(".")
                for (var i = 0; i < 3; ++i) { var d = (parseInt(x[i]) || 0) - (parseInt(y[i]) || 0); if (d !== 0) return d > 0 }
                return false
            }
            // No server address yet (first launch): the welcome screen asks for it.
            Component.onCompleted: if (!Prefs.serverConfigured) win.openWelcome(); else Api.get("/api/v1/meta", function(r) {
                if (r.ok && r.data.min_client_version && splash.newer(r.data.min_client_version, Qt.application.version)) { splash.required = r.data.min_client_version; return }
                Session.restore(function(ok) { ok ? win.openShell() : win.openWelcome() })
            })
        }
    }
    Component { id: welcomeComp; WelcomePage {} }
    Component { id: loginComp; LoginPage {} }
    Component { id: joinComp; JoinPage {} }
    Component { id: createComp; CreatePage {} }
    Component { id: shellComp; Shell {} }

    Connections {
        target: Session
        function onExpired() { win.openLogin(qsTr("Your session has expired. Please sign in again.")) }
        function onRememberFailed() { win.toast(qsTr("This device could not keep your session (system secure storage unavailable): you will sign in again at the next start."), "warning") }
    }
    Connections {
        target: Api
        function onSubscriptionBlocked(code) { Session.reload() }
    }

    Notifier { id: notifier }
    Tour {}
}
