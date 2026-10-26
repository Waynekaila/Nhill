import QtQuick
import Nhill.Core
import Nhill.Business

// Automated UI tour used to produce screenshots (documentation and visual QA).
// Enabled only when NHILL_TOUR_DIR is set; does nothing otherwise.
Item {
    id: tour
    property var cfg: typeof tourConfig !== "undefined" ? tourConfig : ({})
    readonly property bool active: !!cfg.dir && cfg.dir.length > 0
    property int step: 0
    property string docId: ""
    property var plan: [
        function() { shot("01-bienvenue") },
        function() { Nav.window.openCreate() },
        function() { shot("02-creation-etablissement") },
        function() { Nav.window.openJoin() },
        function() { shot("03-rejoindre-code") },
        function() { Nav.window.openLogin() },
        function() { shot("04-connexion") },
        function() {
            Api.send("POST", "/api/v1/auth/login", { email: cfg.email, password: cfg.password }, function(r) {
                if (r.ok) Session.start(r.data, function(ok) { if (ok) Nav.window.openShell() })
                else console.warn("tour login failed", JSON.stringify(r))
            })
        },
        function() { shot("05-tableau-de-bord") },
        function() { Nav.navigate("sales") },
        function() { shot("06-ventes") },
        function() { Api.get("/api/v1/sales-documents?type=invoice&limit=1&status=partially_paid", function(r) { if (r.ok && r.data.items.length) tour.docId = r.data.items[0].id }) },
        function() { if (tour.docId) Nav.push("pages/DocumentPage.qml", { docId: tour.docId }) },
        function() { shot("07-facture") },
        function() { Nav.navigate("sales"); Nav.push("pages/DocumentEditorPage.qml", { docType: "invoice" }) },
        function() { shot("08-nouvelle-facture") },
        function() { Nav.navigate("customers") },
        function() { shot("09-clients") },
        function() { Nav.navigate("products") },
        function() { shot("10-produits") },
        function() { Nav.navigate("stock") },
        function() { shot("11-stock") },
        function() { Nav.navigate("purchases") },
        function() { shot("12-achats") },
        function() { Nav.navigate("expenses") },
        function() { shot("13-depenses") },
        function() { Nav.navigate("payments") },
        function() { shot("14-paiements") },
        function() { Nav.navigate("reports") },
        function() { shot("15-rapports") },
        function() { Nav.navigate("activity") },
        function() { shot("15b-journal-activite") },
        function() { Nav.navigate("settings") },
        function() { shot("16-parametres") },
        function() { Nav.navigate("settings", { tab: "code" }) },
        function() { shot("17-code-etablissement") },
        function() { Nav.navigate("settings", { tab: "subscription" }) },
        function() { shot("18-abonnement") },
        function() { Nav.navigate("settings", { tab: "team" }) },
        function() { shot("19-equipe") },
        function() { Nav.navigate("settings", { tab: "pos" }) },
        function() { shot("19b-point-de-vente") },
        function() { Nav.navigate("support") },
        function() { shot("20-assistance") },
        function() { Qt.quit() }
    ]
    function shot(name) {
        var file = cfg.dir + "/" + (cfg.tag || "") + name + ".png"
        if (!Tools.grabWindow(Nav.window, file)) console.warn("tour: capture failed for", name)
    }
    Timer {
        interval: 1600
        running: tour.active
        repeat: true
        onTriggered: { if (tour.step < tour.plan.length) tour.plan[tour.step++]() }
    }
}
