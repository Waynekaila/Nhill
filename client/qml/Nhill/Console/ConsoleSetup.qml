import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui

// First installation of a Nhill server (server in setup mode).
// The database credentials are sent once, over HTTPS (or to this computer), to the
// Nhill server, which creates its own roles, applies the migrations and checks the
// result. This console stores none of them.
Rectangle {
    id: root
    color: Theme.bg
    signal finished(string adminEmail, string adminToken)
    signal cancelled()

    property string step: "server"        // server | database | admin | result
    property string provider: "supabase"
    property var probe: null
    property var report: null
    property bool busy: false
    property string serverMode: ""        // setup | ready | unreachable
    property var tour: ({})               // development capture tour (see console main.cpp)

    function dbPayload() {
        var d = { provider: provider, host: host.text.trim(), port: parseInt(port.text) || 5432,
                  database: database.text.trim() || "postgres", user: user.text.trim(), password: password.text,
                  sslmode: sslmode.value }
        return d
    }
    function checkServer() {
        Prefs.serverUrl = serverField.text.trim()
        root.busy = true
        Api.get("/health", function(r) {
            root.busy = false
            root.serverMode = r.ok && r.data.status === "setup" ? "setup" : r.ok ? "ready" : "unreachable"
            if (!r.ok) ConsoleNav.fail(r)
        })
    }
    function testConnection() {
        root.busy = true
        root.probe = null
        Api.send("POST", "/api/v1/setup/check", { setup_token: token.text.trim(), database: dbPayload() }, function(r) {
            root.busy = false
            if (r.ok) root.probe = r.data
            else showError(r)
        })
    }
    function apply() {
        root.busy = true
        var body = { setup_token: token.text.trim(), database: dbPayload() }
        if (createAdmin.checked) body.first_admin = { email: adminEmail.text.trim(), full_name: adminName.text.trim() }
        Api.sendLong("POST", "/api/v1/setup/apply", body, function(r) {
            root.busy = false
            if (r.data && r.data.verification) { root.report = r.data; root.step = "result"; password.text = "" }
            else showError(r)
        }, 300000)
    }
    function checkLabel(id) {
        var m = { migrations: qsTr("Migrations"), app_role: qsTr("Application role without privileges"),
                  app_owns_nothing: qsTr("Application role owns no object"), row_level_security: qsTr("Row-level security forced"),
                  not_in_public_schema: qsTr("Dedicated schema \"nhill\""), closed_to_api_roles: qsTr("Closed to PUBLIC / anon / authenticated"),
                  functions_search_path: qsTr("Functions with fixed search_path"),
                  isolation_without_tenant: qsTr("No data visible without establishment"), isolation_write: qsTr("No write without establishment"),
                  app_no_ddl: qsTr("No schema change by the application"), owner_connection: qsTr("Owner connection"),
                  app_connection: qsTr("Application connection") }
        return m[id] || id
    }
    function showError(r) {
        var c = r.error ? r.error.code : ""
        // Database errors carry the PostgreSQL message: it is the useful part.
        if ((c === "database_unreachable" || c === "setup_failed") && r.error.message)
            ConsoleNav.toast(Errors.code(c) + "\n" + r.error.message, "danger")
        else ConsoleNav.fail(r)
    }
    function waitForRestart() {
        root.busy = true
        restartPoll.attempts = 0
        restartPoll.start()
    }
    Timer {
        id: restartPoll
        property int attempts: 0
        interval: 1000; repeat: true
        onTriggered: {
            attempts++
            Api.get("/health", function(r) {
                if (r.ok && r.data.status !== "setup") {
                    restartPoll.stop(); root.busy = false
                    var a = root.report && root.report.first_admin ? root.report.first_admin : {}
                    root.finished(a.email || "", a.setup_token || "")
                } else if (restartPoll.attempts > 30) {
                    restartPoll.stop(); root.busy = false
                    ConsoleNav.toast(qsTr("The server has not restarted yet. Restart the Nhill service, then sign in."), "warning")
                }
            })
        }
    }

    Item {
        id: tourDriver
        property int stepIndex: 0
        function shot(name) { if (!Tools.grabWindow(ConsoleNav.window, root.tour.dir + "/" + name + ".png")) console.warn("tour: capture failed", name) }
        property var plan: [
            function() { tourDriver.shot("s01-serveur") },
            function() { root.checkServer() },
            function() { token.text = root.tour.setup_token; tourDriver.shot("s02-serveur-en-attente") },
            function() { root.step = "database"; var d = root.tour.database
                         root.provider = d.provider; sslmode.value = d.sslmode; host.text = d.host; port.text = "" + d.port
                         database.text = d.database; user.text = d.user; password.text = d.password },
            function() { root.testConnection() },
            function() { tourDriver.shot("s03-base-de-donnees") },
            function() { root.step = "admin"; adminEmail.text = root.tour.admin.email; adminName.text = root.tour.admin.full_name },
            function() { tourDriver.shot("s04-administrateur") },
            function() { root.apply() },
            function() { tourDriver.shot("s05-verification") },
            function() { root.waitForRestart() }
        ]
        Timer { interval: 1500; repeat: true; running: !!root.tour.dir
                onTriggered: { if (root.busy) return; if (tourDriver.stepIndex < tourDriver.plan.length) tourDriver.plan[tourDriver.stepIndex++](); else stop() } }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.implicitHeight + 2 * Theme.s6
        clip: true
        ScrollBar.vertical: ScrollBar {}
        ColumnLayout {
            id: col
            x: (parent.width - width) / 2
            y: Theme.s6
            width: Math.min(620, root.width - 48)
            spacing: Theme.s4
            LogoMark { app: "console"; Layout.preferredHeight: 40; Layout.preferredWidth: 280; Layout.alignment: Qt.AlignHCenter }
            Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("Server installation"); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_xl; font.weight: Font.DemiBold }
            TabStrip {
                Layout.alignment: Qt.AlignHCenter
                enabled: false
                current: root.step
                model: [{ value: "server", text: qsTr("1. Server") }, { value: "database", text: qsTr("2. Database") },
                        { value: "admin", text: qsTr("3. Administrator") }, { value: "result", text: qsTr("4. Verification") }]
            }

            // ---------------------------------------------------------------- 1. server
            FbCard {
                Layout.fillWidth: true
                visible: root.step === "server"
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s3
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                           text: qsTr("Start the Nhill server without a database: it enters setup mode and prints a one-time setup token in its log (journalctl -u nhill-server). Only a person with access to the server can read it.") }
                    Field { label: qsTr("Server address"); help: qsTr("HTTPS is required, except for a server on this computer.")
                            FbTextField { id: serverField; text: Prefs.serverUrl; monospace: true; placeholderText: "https://api.example.com"; onAccepted: root.checkServer() } }
                    RowLayout {
                        FbButton { text: qsTr("Check the server"); variant: "secondary"; busy: root.busy; onClicked: root.checkServer() }
                        Item { Layout.fillWidth: true }
                    }
                    Banner { visible: root.serverMode === "ready"; tone: "info"; Layout.fillWidth: true
                             text: qsTr("This server is already installed. Sign in instead.") ; actionText: qsTr("Sign in"); onAction: root.cancelled() }
                    Banner { visible: root.serverMode === "setup"; tone: "success"; Layout.fillWidth: true
                             text: qsTr("The server is waiting for its installation.") }
                    Field { visible: root.serverMode === "setup"; label: qsTr("Setup token"); help: qsTr("Valid 60 minutes; a new one is printed at each restart.")
                            FbTextField { id: token; monospace: true; echoMode: TextInput.Password } }
                    RowLayout {
                        FbButton { text: qsTr("Back to sign in"); variant: "ghost"; onClicked: root.cancelled() }
                        Item { Layout.fillWidth: true }
                        FbButton { text: qsTr("Continue"); enabled: root.serverMode === "setup" && token.text.trim().length > 0; onClicked: root.step = "database" }
                    }
                }
            }

            // ---------------------------------------------------------------- 2. database
            FbCard {
                Layout.fillWidth: true
                visible: root.step === "database"
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s3
                    TabStrip {
                        model: [{ value: "supabase", text: "Supabase" }, { value: "postgres", text: qsTr("PostgreSQL (self-hosted)") }]
                        current: root.provider
                        onSelected: function(v) {
                            root.provider = v; root.probe = null
                            sslmode.value = v === "supabase" ? "require" : "prefer"
                            if (v === "supabase" && port.text === "") port.text = "5432"
                        }
                    }
                    Banner {
                        Layout.fillWidth: true; tone: "info"
                        text: root.provider === "supabase"
                              ? qsTr("In the Supabase dashboard: Connect → \"Session pooler\" (or \"Direct connection\" if this server has IPv6). Copy the host, the port 5432 and the user (postgres.<project-ref>), with the database password of the project. Do not use the transaction pooler (port 6543) nor any API key: Nhill never uses the service_role key.")
                              : qsTr("Use a superuser, or an account allowed to create roles (CREATEROLE). The database must exist; Nhill creates its own schema \"nhill\" in it.")
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: width > 480 ? 4 : 1
                        columnSpacing: Theme.s3
                        Field { Layout.columnSpan: parent.columns > 1 ? 3 : 1; Layout.fillWidth: true; label: qsTr("Host"); required: true
                                FbTextField { id: host; monospace: true; placeholderText: root.provider === "supabase" ? "aws-0-eu-west-1.pooler.supabase.com" : "db.example.org" } }
                        Field { Layout.fillWidth: true; label: qsTr("Port"); FbTextField { id: port; text: "5432"; monospace: true; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 1; top: 65535 } } }
                        Field { Layout.columnSpan: parent.columns > 1 ? 2 : 1; Layout.fillWidth: true; label: qsTr("Database"); FbTextField { id: database; text: "postgres"; monospace: true } }
                        Field { Layout.columnSpan: parent.columns > 1 ? 2 : 1; Layout.fillWidth: true; label: qsTr("Administrator user"); required: true
                                FbTextField { id: user; monospace: true; placeholderText: root.provider === "supabase" ? "postgres.abcdefghijklmnop" : "postgres" } }
                        Field { Layout.columnSpan: parent.columns > 1 ? 2 : 1; Layout.fillWidth: true; label: qsTr("Password"); required: true
                                help: qsTr("Used once by the server to create the Nhill roles; it is not stored.")
                                FbTextField { id: password; echoMode: TextInput.Password } }
                        Field { Layout.columnSpan: parent.columns > 1 ? 2 : 1; Layout.fillWidth: true; label: qsTr("TLS")
                                FbComboBox { id: sslmode; Layout.fillWidth: true; value: "require"
                                             model: root.provider === "supabase"
                                                    ? [{ value: "require", text: qsTr("Required (encrypted)") }, { value: "verify-full", text: qsTr("Required + certificate check") }]
                                                    : [{ value: "prefer", text: qsTr("If available") }, { value: "require", text: qsTr("Required (encrypted)") },
                                                       { value: "verify-full", text: qsTr("Required + certificate check") }, { value: "disable", text: qsTr("Disabled (local only)") }] } }
                    }
                    ColumnLayout {
                        visible: !!root.probe
                        Layout.fillWidth: true
                        spacing: Theme.s2
                        Banner {
                            Layout.fillWidth: true
                            tone: root.probe && root.probe.can_create_roles && root.probe.version_supported ? "success" : "danger"
                            text: !root.probe ? "" :
                                  qsTr("Connected to PostgreSQL %1 as %2.").arg(root.probe.server_version).arg(root.probe.user) + " " +
                                  (!root.probe.version_supported ? qsTr("PostgreSQL 15 or later is required.") :
                                   !root.probe.can_create_roles ? qsTr("This account cannot create roles: use the administrator account.") :
                                   root.probe.applied_migrations > 0 ? qsTr("A Nhill installation already exists (%1 migrations): it will be updated and its roles will get new passwords.").arg(root.probe.applied_migrations)
                                                                     : qsTr("Ready for installation."))
                        }
                    }
                    RowLayout {
                        FbButton { text: qsTr("Back"); variant: "ghost"; onClicked: root.step = "server" }
                        Item { Layout.fillWidth: true }
                        FbButton { text: qsTr("Test the connection"); variant: "secondary"; busy: root.busy
                                   enabled: host.text.trim().length > 0 && user.text.trim().length > 0 && password.text.length > 0
                                   onClicked: root.testConnection() }
                        FbButton { text: qsTr("Continue"); enabled: !!root.probe && root.probe.can_create_roles && root.probe.version_supported
                                   onClicked: root.step = "admin" }
                    }
                }
            }

            // ---------------------------------------------------------------- 3. administrator
            FbCard {
                Layout.fillWidth: true
                visible: root.step === "admin"
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s3
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm
                           text: qsTr("The server will create the roles nhill_owner (schema, migrations) and nhill_app (application, subject to row-level security), apply every migration in the schema \"nhill\", then verify isolation and privileges. No SQL to run by hand.") }
                    RowLayout {
                        FbSwitch { id: createAdmin; checked: !(root.probe && root.probe.applied_migrations > 0) }
                        Text { text: qsTr("Create the first platform administrator"); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_md; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    }
                    Field { visible: createAdmin.checked; label: qsTr("Email"); required: true; FbTextField { id: adminEmail; inputMethodHints: Qt.ImhEmailCharactersOnly } }
                    Field { visible: createAdmin.checked; label: qsTr("Full name"); required: true; FbTextField { id: adminName } }
                    RowLayout {
                        FbButton { text: qsTr("Back"); variant: "ghost"; onClicked: root.step = "database" }
                        Item { Layout.fillWidth: true }
                        FbButton { text: qsTr("Initialize the database"); iconName: "shield"; busy: root.busy
                                   enabled: !createAdmin.checked || (adminEmail.text.indexOf("@") > 0 && adminName.text.trim().length > 0)
                                   onClicked: root.apply() }
                    }
                }
            }

            // ---------------------------------------------------------------- 4. result
            FbCard {
                Layout.fillWidth: true
                visible: root.step === "result" && !!root.report
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s3
                    Banner { Layout.fillWidth: true; tone: root.report && root.report.ok ? "success" : "danger"
                             text: root.report && root.report.ok
                                   ? qsTr("Installation completed and verified. The server restarts in normal mode.")
                                   : qsTr("The verification failed: the configuration was not saved and the server stays in setup mode. Fix the points below and try again.") }
                    Repeater {
                        model: root.report ? root.report.verification.checks : []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.s2
                            Icon { name: modelData.ok ? "check" : "alert"; color: modelData.ok ? Theme.success : Theme.danger; size: 18 }
                            Text { text: root.checkLabel(modelData.id); font.family: Theme.font; font.pixelSize: Theme.fs_sm; font.weight: Font.Medium; color: Theme.text; Layout.preferredWidth: 270; wrapMode: Text.Wrap }
                            Text { text: modelData.detail; font.family: Theme.font; font.pixelSize: Theme.fs_sm; color: Theme.textMuted; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        }
                    }
                    Repeater {
                        model: root.report ? root.report.warnings : []
                        delegate: Banner { Layout.fillWidth: true; tone: "warning"; text: modelData }
                    }
                    ColumnLayout {
                        visible: !!(root.report && root.report.backup_account)
                        Layout.fillWidth: true
                        spacing: Theme.s2
                        SectionTitle { text: qsTr("Backup account (read-only)"); hint: qsTr("Shown only now: keep it in your password manager for the backup scripts.") }
                        Text { font.family: Theme.mono; font.pixelSize: Theme.fs_sm; color: Theme.text; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true
                               text: root.report && root.report.backup_account ? root.report.backup_account.user + "  /  " + root.report.backup_account.password : "" }
                        FbButton { text: qsTr("Copy the password"); iconName: "copy"; variant: "secondary"
                                   onClicked: { Tools.copyText(root.report.backup_account.password); ConsoleNav.toast(qsTr("Copied"), "success") } }
                    }
                    ColumnLayout {
                        visible: !!(root.report && root.report.first_admin)
                        Layout.fillWidth: true
                        spacing: Theme.s2
                        SectionTitle { text: qsTr("First administrator") }
                        Banner { Layout.fillWidth: true; tone: root.report && root.report.first_admin && root.report.first_admin.error ? "warning" : "info"
                                 text: !root.report || !root.report.first_admin ? "" : root.report.first_admin.error
                                       ? qsTr("No administrator created: %1").arg(root.report.first_admin.error)
                                       : qsTr("Setup token for %1 (valid 30 minutes). It is filled in automatically on the next screen.").arg(root.report.first_admin.email) }
                    }
                    RowLayout {
                        FbButton { visible: root.report && !root.report.ok; text: qsTr("Back"); variant: "ghost"; onClicked: root.step = "database" }
                        Item { Layout.fillWidth: true }
                        FbButton { visible: root.report && root.report.ok; text: qsTr("Continue to sign in"); busy: root.busy; onClicked: root.waitForRestart() }
                    }
                }
            }
        }
    }
}
