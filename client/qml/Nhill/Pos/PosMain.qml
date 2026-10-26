import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Pos

// Nhill POS: sign in with a Nhill Business account of the establishment, choose a
// register, open a cash session, sell, close.
ApplicationWindow {
    id: win
    width: typeof tourConfig !== "undefined" && tourConfig.width ? tourConfig.width : 1280
    height: typeof tourConfig !== "undefined" && tourConfig.height ? tourConfig.height : 800
    minimumWidth: 1000
    minimumHeight: 640
    visible: true
    title: Session.loggedIn ? Session.establishment.trade_name + " — Nhill POS" : "Nhill POS"
    color: Theme.bg
    font.family: Theme.font
    Component.onCompleted: PosNav.window = win

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
            if (!r.ok) { fail(r); if (r.status === 403) openLogin(qsTr("This account cannot use the point of sale.")); return }
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
            Component.onCompleted: Session.restore(function(ok) {
                if (!ok) { win.openLogin(""); return }
                win.loadConfig(function() { PosNav.session ? win.openSale() : win.openHome() })
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
    }

    // ---- documentation tour (NHILL_TOUR_DIR, NHILL_TOUR_EMAIL, NHILL_TOUR_PASSWORD)
    Item {
        id: tour
        property var cfg: typeof tourConfig !== "undefined" ? tourConfig : ({})
        property int step: 0
        function shot(name) { if (!Tools.grabWindow(win, tour.cfg.dir + "/" + name + ".png")) console.warn("tour: capture failed", name) }
        property var plan: [
            function() { shot("p01-connexion") },
            function() { Api.send("POST", "/api/v1/auth/login", { email: tour.cfg.email, password: tour.cfg.password }, function(r) {
                if (r.ok) Session.start(r.data, function(ok) { if (ok) win.loadConfig(function() { PosNav.session ? win.openSale() : win.openHome() }) }) }) },
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
