pragma Singleton
import QtQuick
import Nhill.Core

// Shared state of Nhill POS: configuration, current register and cash session.
QtObject {
    property var window: null
    property var config: null          // GET /api/v1/pos/config
    property var session: null         // summary of the open cash session
    property var register: null
    function can(p) { return !!config && config.permissions.indexOf(p) >= 0 }
    function toast(text, tone) { if (window) window.toast(text, tone) }
    function fail(r) { if (window) window.fail(r) }
    // Sends a write; when the server asks for a supervisor approval, asks for it and
    // sends the same request again with the one-time approval token.
    function sendApproved(method, path, body, cb) {
        var key = Api.newIdempotencyKey()
        Api.send(method, path, body, function(r) {
            if (!r.ok && r.error && r.error.code === "approval_required" && window) {
                var action = r.error.details && r.error.details.action ? r.error.details.action : ""
                window.askApproval(action, session ? session.id : "", function(token) {
                    var b = JSON.parse(JSON.stringify(body))
                    b.approval_token = token
                    Api.send(method, path, b, cb, Api.newIdempotencyKey())
                })
                return
            }
            cb(r)
        }, key)
    }
}
