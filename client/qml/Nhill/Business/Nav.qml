pragma Singleton
import QtQuick

// Navigation and feedback helpers shared by every page.
QtObject {
    property var window: null
    property var shell: null
    function navigate(key, props) { if (shell) shell.navigate(key, props) }
    function push(url, props) { if (shell) shell.push(url, props) }
    function pop() { if (shell) shell.pop() }
    function toast(text, tone) { if (window) window.toast(text, tone) }
    function fail(result) { if (window) window.fail(result) }
}
