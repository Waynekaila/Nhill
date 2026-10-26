import QtQuick
import QtQuick.Controls
import Nhill.Core
import Nhill.Ui

ApplicationWindow {
    id: win
    width: typeof tourConfig !== "undefined" && tourConfig.width ? tourConfig.width : 1280
    height: typeof tourConfig !== "undefined" && tourConfig.height ? tourConfig.height : 820
    minimumWidth: 360
    minimumHeight: 560
    visible: true
    title: Session.loggedIn ? Session.establishment.trade_name + " — Nhill" : "Nhill"
    color: Theme.bg
    font.family: Theme.font

    Binding { target: Theme; property: "compact"; value: win.width < 820 }
    Component.onCompleted: Nav.window = win

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
            Component.onCompleted: Api.get("/api/v1/meta", function(r) {
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
    }
    Connections {
        target: Api
        function onSubscriptionBlocked(code) { Session.reload() }
    }

    Notifier { id: notifier }
    Tour {}
}
