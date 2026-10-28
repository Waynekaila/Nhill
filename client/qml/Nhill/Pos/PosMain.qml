import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Pos

// Nhill POS: sign in with a Nhill Business account of the establishment, choose a
// register, open a cash session, sell, close.
ApplicationWindow {
    id: win
    // 2.0.0 window sizing: the first size fits the screen (1366 x 768 laptops, 125 percent
    // scaling, small netbooks), the window is centred, and small screens start maximised.
    // The documentation tour can still force a size (NHILL_TOUR_SIZE).
    readonly property bool tourSized: typeof tourConfig !== "undefined" && !!tourConfig.width
    readonly property int availW: Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : 1280
    readonly property int availH: Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 800
    width: tourSized ? tourConfig.width : Theme.fitWidth(1280, availW)
    height: tourSized ? tourConfig.height : Theme.fitHeight(800, availH)
    minimumWidth: Math.min(860, availW)
    minimumHeight: Math.min(560, availH)
    function placeOnScreen() {
        if (tourSized || Qt.platform.os === "android" || Qt.platform.os === "ios") return
        x = Screen.virtualX + Math.max(0, Math.round((availW - width) / 2))
        y = Screen.virtualY + Math.max(0, Math.round((availH - height) / 2))
        if (availW < 1100 || availH < 700) showMaximized()
    }
    visible: true
    title: Session.loggedIn ? Session.establishment.trade_name + " — Nhill POS" : "Nhill POS"
    color: Theme.bg
    font.family: Theme.font
    Component.onCompleted: { PosNav.window = win; placeOnScreen() }

    function toast(text, tone) { notifier.show(text, tone) }
    function fail(r) { var m = Errors.message(r); if (m.length > 0) notifier.error(m) }
    function openLogin(notice) { PosNav.config = null; PosNav.session = null; stack.replace(null, loginComp, { notice: notice || "" }) }
    function openHome() { stack.replace(null, homeComp) }
    function openSale() { stack.replace(null, saleComp) }
    function openSession(id) { stack.push(sessionComp, { sessionId: id || (PosNav.session ? PosNav.session.id : "") }) }
    function back() { if (stack.depth > 1) stack.pop(); else openHome() }
    function signOut() { Session.logout(); openLogin("") }
    function askApproval(action, sessionId, cb) { approval.ask(action, sessionId, cb) }
    // Loads the configuration and the open session of this user (if any).
    function loadConfig(done) {
        Api.get("/api/v1/pos/config", function(r) {
            if (!r.ok) {
                if (r.error && r.error.code === "feature_not_included") { openLogin(qsTr("The plan of this business does not include the point of sale.")); return }
                fail(r); if (r.status === 403) openLogin(qsTr("This account cannot use the point of sale.")); return
            }
            PosNav.config = r.data
            if (!r.data.my_open_session) { PosNav.session = null; if (done) done(); return }
            Api.get("/api/v1/pos/sessions/" + r.data.my_open_session, function(s) {
                if (s.ok) {
                    PosNav.session = s.data
                    PosNav.register = r.data.registers.filter(function(x) { return x.id === s.data.register.id })[0] || s.data.register
                }
                if (done) done()
            })
        })
    }

    StackView { id: stack; anchors.fill: parent; initialItem: splashComp }
    Component {
        id: splashComp
        Rectangle {
            color: Theme.bg
            LogoMark { app: "pos"; anchors.centerIn: parent; height: 56; width: 300 }
            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.verticalCenter; anchors.topMargin: 60 }
            property string required: ""
            Text { visible: !!parent.required; anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.verticalCenter; anchors.topMargin: 110
                   width: Math.min(parent.width - 48, 480); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: Theme.text; font.family: Theme.font
                   text: qsTr("This version of Nhill POS (%1) is no longer supported by the server. Please install version %2 or later.").arg(Qt.application.version).arg(parent.required) }
            function newer(a, b) {
                var x = String(a).split("."), y = String(b).split(".")
                for (var i = 0; i < 3; ++i) { var d = (parseInt(x[i]) || 0) - (parseInt(y[i]) || 0); if (d !== 0) return d > 0 }
                return false
            }
            // First launch: no server yet, the sign-in screen asks for it.
            Component.onCompleted: if (!Prefs.serverConfigured) win.openLogin(""); else Api.get("/api/v1/meta", function(m) {
                if (m.ok && m.data.min_client_version && newer(m.data.min_client_version, Qt.application.version)) { required = m.data.min_client_version; return }
                Session.restore(function(ok) {
                    if (!ok) { win.openLogin(""); return }
                    win.loadConfig(function() { PosNav.session ? win.openSale() : win.openHome() })
                })
            })
        }
    }
    Component { id: loginComp; PosLogin {} }
    Component { id: homeComp; PosHome {} }
    Component { id: saleComp; PosSale {} }
    Component { id: sessionComp; PosSession {} }
    Notifier { id: notifier }
    PosApprovalDialog { id: approval }
    Connections {
        target: Session
        function onExpired() { win.openLogin(qsTr("Your session has expired. Please sign in again.")) }
        // Rights reloaded (plan or role changed): without the point of sale, the till stops here.
        function onChanged() { if (Session.loggedIn && Session.me.permissions && !Session.has("pos")) win.openLogin(qsTr("The plan of this business does not include the point of sale.")) }
        function onRememberFailed() { win.toast(qsTr("This device could not keep your session (system secure storage unavailable): you will sign in again at the next start."), "warning") }
    }

    // ---- documentation tour (NHILL_TOUR_DIR, NHILL_TOUR_EMAIL, NHILL_TOUR_PASSWORD)
    Item {
        id: tour
        property var cfg: typeof tourConfig !== "undefined" ? tourConfig : ({})
        property int step: 0
        function shot(name) { if (!Tools.grabWindow(win, tour.cfg.dir + "/" + (tour.cfg.tag || "") + name + ".png")) console.warn("tour: capture failed", name) }
        property var plan: [
            function() { shot("p01-connexion") },
            function() { Api.send("POST", "/api/v1/auth/login", { email: tour.cfg.email, password: tour.cfg.password }, function(r) {
                if (r.ok) Session.start(r.data, function(ok) { if (ok) win.loadConfig(function() { PosNav.session ? win.openSale() : win.openHome() }) }) }) },
            function() { if (stack.currentItem && stack.currentItem.tourOpen) { shot("p02-ouverture-caisse"); stack.currentItem.tourOpen() } },
            function() { shot("p02-vente") },
            function() { if (stack.currentItem && stack.currentItem.tourFill) stack.currentItem.tourFill() },
            function() { shot("p03-panier") },
            function() { if (stack.currentItem && stack.currentItem.tourPay) stack.currentItem.tourPay() },
            function() { shot("p04-paiement") },
            function() { if (stack.currentItem && stack.currentItem.tourSubmit) stack.currentItem.tourSubmit() },
            function() { shot("p05-rendu-monnaie") },
            function() { if (stack.currentItem && stack.currentItem.tourClosePay) stack.currentItem.tourClosePay(); win.openSession() },
            function() { shot("p06-session") },
            function() { if (stack.currentItem && stack.currentItem.tourTicket) stack.currentItem.tourTicket() },
            function() { shot("p07-ticket") },
            function() { if (stack.currentItem && stack.currentItem.tourReturn) stack.currentItem.tourReturn() },
            function() { shot("p08-retour") },
            function() { if (stack.currentItem && stack.currentItem.tourClose) stack.currentItem.tourClose() },
            function() { shot("p09-cloture") },
            function() { if (stack.currentItem && stack.currentItem.tourCloseSubmit) stack.currentItem.tourCloseSubmit() },
            function() { shot("p10-session-cloturee") },
            function() { Qt.quit() }
        ]
        Timer { interval: 1800; repeat: true; running: !!tour.cfg.dir && !!tour.cfg.email
                onTriggered: { if (tour.step < tour.plan.length) tour.plan[tour.step++]() } }
    }
}
