pragma Singleton
import QtQuick

// Shared state of the administration console.
QtObject {
    property var window: null
    property var shell: null
    property var me: ({})
    function can(role) { return me.role === "superadmin" || me.role === role }
    function toast(text, tone) { if (window) window.toast(text, tone) }
    function fail(r) { if (window) window.fail(r) }
    function push(url, props) { if (shell) shell.push(url, props) }
}
