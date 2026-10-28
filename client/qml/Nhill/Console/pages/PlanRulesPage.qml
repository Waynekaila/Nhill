import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Console

// Automatic assignment of a plan when an establishment is created (3.0.0): the first
// active rule matching the type of establishment and its country (by priority) gives
// its plan, as a trial or as a granted period (no end date for free or custom plans).
// Without a matching rule, the platform's standard free trial applies.
FbPage {
    id: page
    title: qsTr("Assignment rules")
    subtitle: qsTr("Plan given automatically to a new establishment")
    property var rows: []
    property var plans: []
    function reload() {
        Api.get("/api/v1/admin/plan-rules", function(r) { if (r.ok) page.rows = r.data.items; else ConsoleNav.fail(r) })
        Api.get("/api/v1/admin/plans", function(r) {
            if (r.ok) page.plans = r.data.items.filter(function(p) { return p.active && !p.archived_at })
                                               .map(function(p) { return { value: p.id, text: p.code + " (" + p.kind + ")", kind: p.kind } })
        })
    }
    Component.onCompleted: reload()
    function audienceText(a) { return a === "individual" ? qsTr("Individual entrepreneurs") : a === "company" ? qsTr("Companies") : qsTr("Everyone") }
    actions: [ FbButton { visible: ConsoleNav.can("billing"); text: qsTr("New rule"); iconName: "plus"; onClicked: editor.start(null) } ]
    DataTable {
        anchors.fill: parent
        rows: page.rows
        columns: [
            { title: qsTr("Priority"), width: 0.6, align: "right", value: function(r) { return String(r.priority) } },
            { title: qsTr("For"), width: 1.4, value: function(r) { return page.audienceText(r.audience) + (r.country_code ? " · " + r.country_code : "") } },
            { title: qsTr("Plan"), width: 1.2, mono: true, value: function(r) { return r.plan_code } },
            { title: qsTr("As"), width: 1.4, value: function(r) {
                return (r.mode === "trial" ? qsTr("Trial") : qsTr("Granted period")) + " · " + (r.duration_days ? qsTr("%n day(s)", "", r.duration_days) : qsTr("no end date")) } },
            { title: qsTr("Description"), width: 2, muted: true, value: function(r) { return r.description } },
            { title: qsTr("Status"), width: 0.8, chip: function(r) { return r.active ? { text: qsTr("Active"), tone: "success" } : { text: qsTr("Inactive"), tone: "neutral" } } }
        ]
        emptyIcon: "receipt"
        emptyTitle: qsTr("No rule")
        emptyText: qsTr("New establishments get the standard free trial of the platform settings.")
        onRowClicked: function(r) { if (ConsoleNav.can("billing")) editor.start(r) }
    }
    FbDialog {
        id: editor
        property var rule: null
        title: rule ? qsTr("Edit the rule") : qsTr("New rule")
        subtitle: qsTr("The first active rule that matches, by priority (lowest first), applies at the creation of an establishment.")
        preferredWidth: 600
        function start(r) {
            rule = r
            priority.text = r ? String(r.priority) : "100"
            audience.value = r ? r.audience : "any"
            country.value = r && r.country_code ? r.country_code : ""
            plan.value = r ? r.plan_id : (page.plans.length ? page.plans[0].value : "")
            mode.value = r ? r.mode : "trial"
            days.text = r && r.duration_days ? String(r.duration_days) : (r ? "" : "30")
            description.text = r ? r.description : ""
            active.checked = r ? r.active : true
            open()
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.s3; rowSpacing: Theme.s2
            Field { label: qsTr("Priority"); FbTextField { id: priority; inputMethodHints: Qt.ImhDigitsOnly } }
            Field { label: qsTr("Type of establishment"); FbComboBox { id: audience; model: [{ value: "any", text: qsTr("Everyone") }, { value: "individual", text: qsTr("Individual entrepreneurs") }, { value: "company", text: qsTr("Companies") }] } }
            Field { label: qsTr("Country"); FbComboBox { id: country; model: [{ value: "", text: qsTr("All countries") }].concat(Labels.countries()) } }
            Field { label: qsTr("Plan"); FbComboBox { id: plan; model: page.plans } }
            Field { label: qsTr("As"); FbComboBox { id: mode; model: [{ value: "trial", text: qsTr("Trial") }, { value: "grant", text: qsTr("Granted period") }] } }
            Field { label: qsTr("Duration (days)"); help: qsTr("Empty = no end date (granted free or custom plans only)"); FbTextField { id: days; inputMethodHints: Qt.ImhDigitsOnly } }
        }
        Field { label: qsTr("Description"); FbTextField { id: description } }
        FbSwitch { id: active; text: qsTr("Active") }
        footer: [
            FbButton { text: qsTr("Save"); onClicked: {
                    var b = { priority: parseInt(priority.text || "100"), audience: audience.value, country_code: country.value || null,
                              plan_id: plan.value, mode: mode.value, duration_days: days.text.trim() ? parseInt(days.text) : null,
                              description: description.text.trim(), active: active.checked }
                    var cb = function(r) { if (r.ok) { editor.close(); page.reload() } else ConsoleNav.fail(r) }
                    if (editor.rule) Api.send("PATCH", "/api/v1/admin/plan-rules/" + editor.rule.id, b, cb)
                    else Api.send("POST", "/api/v1/admin/plan-rules", b, cb)
                } },
            FbButton { visible: !!editor.rule; text: qsTr("Delete"); variant: "ghost"; iconName: "trash"
                       onClicked: Api.send("DELETE", "/api/v1/admin/plan-rules/" + editor.rule.id, null, function(r) { if (r.ok) { editor.close(); page.reload() } else ConsoleNav.fail(r) }) },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: editor.close() }
        ]
    }
}
