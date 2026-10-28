import QtQuick
import QtQuick.Window
import QtQuick.Controls
import Nhill.Core
import Nhill.Ui

// Nhill administration console: platform operations only. The session token
// lives in memory (closing the console signs out); nothing is stored on disk.
ApplicationWindow {
    id: win
    // 2.0.0 window sizing: the first size fits the screen (1366 x 768 laptops, 125 percent
    // scaling, small netbooks), the window is centred, and small screens start maximised.
    readonly property bool tourSized: false
    readonly property int availW: Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : 1320
    readonly property int availH: Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 840
    width: Theme.fitWidth(1320, availW)
    height: Theme.fitHeight(840, availH)
    minimumWidth: Math.min(900, availW)
    minimumHeight: Math.min(560, availH)
    function placeOnScreen() {
        if (tourSized || Qt.platform.os === "android" || Qt.platform.os === "ios") return
        x = Screen.virtualX + Math.max(0, Math.round((availW - width) / 2))
        y = Screen.virtualY + Math.max(0, Math.round((availH - height) / 2))
        if (availW < 1100 || availH < 700) showMaximized()
    }
    visible: true
    title: "Nhill — " + qsTr("Administration")
    color: Theme.bg
    font.family: Theme.font

    function toast(text, tone) { notifier.show(text, tone) }
    function fail(r) {
        if (r && r.status === 401 && ConsoleNav.me.id) { signOut(qsTr("Your administration session has expired.")); return }
        var m = Errors.message(r)
        if (m.length > 0) notifier.error(m)
    }
    function signIn(token) {
        Api.setAdminToken(token)
        Api.get("/api/v1/admin/me", function(r) {
            if (!r.ok) { Api.signOut(); fail(r); return }
            ConsoleNav.me = r.data
            stack.replace(null, shellComp)
        })
    }
    function signOut(message) {
        if (ConsoleNav.me.id) Api.send("POST", "/api/v1/admin/auth/logout", {}, function() {})
        Api.signOut()
        ConsoleNav.me = ({})
        stack.replace(null, loginComp)
        if (message) toast(message, "warning")
    }

    function openSetup() { stack.replace(null, setupComp) }
    function openLogin(email, token) {
        stack.replace(null, loginComp, email ? { mode: "setup", presetEmail: email, presetToken: token } : {})
    }

    StackView { id: stack; anchors.fill: parent; initialItem: loginComp }
    Component { id: loginComp; ConsoleLogin {} }
    Component { id: setupComp; ConsoleSetup { tour: typeof setupTour !== "undefined" ? setupTour : ({}); onFinished: function(email, token) { win.openLogin(email, token) }
                                              onCancelled: win.openLogin() } }
    // A server without database answers /health with status "setup": open the installation wizard.
    Component.onCompleted: {
        ConsoleNav.window = win
        placeOnScreen()
        if (typeof setupTour !== "undefined" && setupTour.dir) { Prefs.serverUrl = setupTour.server; win.openSetup(); return }
        if (!(typeof adminTour !== "undefined" && adminTour.dir) && Prefs.serverConfigured)
            Api.get("/health", function(r) { if (r.ok && r.data && r.data.status === "setup") win.openSetup() })
    }
    Component { id: shellComp; ConsoleShell {} }
    Notifier { id: notifier }

    // ---- documentation tour (NHILL_TOUR_DIR + NHILL_ADMIN_TOUR_TOKEN)
    Item {
        id: tour
        property var cfg: typeof adminTour !== "undefined" ? adminTour : ({})
        property int step: 0
        property var plan: [
            function() { shot("a01-connexion") },
            function() { win.signIn(tour.cfg.token) },
            function() { shot("a02-vue-ensemble") },
            function() { ConsoleNav.shell.navigate("establishments") },
            function() { shot("a03-etablissements") },
            function() { if (tour.cfg.establishment) ConsoleNav.push("pages/EstablishmentPage.qml", { establishmentId: tour.cfg.establishment }) },
            function() { shot("a04-etablissement") },
            function() { ConsoleNav.shell.navigate("tickets") },
            function() { shot("a05-tickets") },
            function() { if (tour.cfg.ticket) ConsoleNav.push("pages/TicketPage.qml", { ticketId: tour.cfg.ticket }) },
            function() { shot("a06-ticket") },
            function() { ConsoleNav.shell.navigate("orders") },
            function() { shot("a07-transactions") },
            function() { ConsoleNav.shell.navigate("plans") },
            function() { shot("a08-offres") },
            function() { ConsoleNav.shell.navigate("promos") },
            function() { shot("a09-codes-promo") },
            function() { ConsoleNav.shell.navigate("providers") },
            function() { shot("a10-prestataires") },
            function() { ConsoleNav.shell.navigate("errors") },
            function() { shot("a11-erreurs") },
            function() { ConsoleNav.shell.navigate("notifications") },
            function() { shot("a12-notifications") },
            function() { ConsoleNav.shell.navigate("audit") },
            function() { shot("a13-audit") },
            function() { Qt.quit() }
        ]
        function shot(name) { if (!Tools.grabWindow(win, tour.cfg.dir + "/" + name + ".png")) console.warn("tour: capture failed", name) }
        Timer { interval: 1600; repeat: true; running: !!tour.cfg.dir && !!tour.cfg.token
                onTriggered: { if (tour.step < tour.plan.length) tour.plan[tour.step++]() } }
    }
}
