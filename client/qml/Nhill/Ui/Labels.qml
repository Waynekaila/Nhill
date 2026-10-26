pragma Singleton
import QtQuick

// Translated labels and tones for codes returned by the API.
QtObject {
    function docType(t) {
        return t === "invoice" ? qsTr("Invoice") : t === "quote" ? qsTr("Quote") : t === "order" ? qsTr("Order")
             : t === "credit_note" ? qsTr("Credit note") : t
    }
    function docStatus(row) {
        var s = row.status
        if (row.doc_type === "invoice" && (s === "issued" || s === "partially_paid") && row.due_date && row.due_date < Qt.formatDate(new Date(), "yyyy-MM-dd"))
            return { text: qsTr("Overdue"), tone: "danger" }
        switch (s) {
        case "draft": return { text: qsTr("Draft"), tone: "neutral" }
        case "issued": return { text: row.doc_type === "credit_note" ? qsTr("Issued") : qsTr("Unpaid"), tone: "info" }
        case "partially_paid": return { text: qsTr("Partly paid"), tone: "warning" }
        case "paid": return { text: qsTr("Paid"), tone: "success" }
        case "credited": return { text: qsTr("Credited"), tone: "neutral" }
        case "sent": return { text: qsTr("Sent"), tone: "info" }
        case "accepted": return { text: qsTr("Accepted"), tone: "success" }
        case "declined": return { text: qsTr("Declined"), tone: "danger" }
        case "converted": return { text: qsTr("Converted"), tone: "primary" }
        case "confirmed": return { text: qsTr("Confirmed"), tone: "info" }
        case "canceled": return { text: qsTr("Cancelled"), tone: "neutral" }
        case "received": return { text: qsTr("Received"), tone: "success" }
        }
        return { text: s, tone: "neutral" }
    }
    function method(m) {
        return m === "cash" ? qsTr("Cash") : m === "mobile_money" ? qsTr("Mobile money") : m === "card" ? qsTr("Card")
             : m === "bank_transfer" ? qsTr("Bank transfer") : m === "cheque" ? qsTr("Cheque") : qsTr("Other")
    }
    function methods() {
        return [ { value: "cash", text: method("cash") }, { value: "mobile_money", text: method("mobile_money") },
                 { value: "bank_transfer", text: method("bank_transfer") }, { value: "card", text: method("card") },
                 { value: "cheque", text: method("cheque") }, { value: "other", text: method("other") } ]
    }
    function role(r) {
        return r === "owner" ? qsTr("Owner") : r === "admin" ? qsTr("Administrator") : r === "manager" ? qsTr("Manager")
             : r === "sales" ? qsTr("Sales") : r === "accountant" ? qsTr("Accountant") : r === "viewer" ? qsTr("Read only")
             : r === "cash_manager" ? qsTr("Cash manager (POS)") : r === "cashier" ? qsTr("Cashier (POS)") : r
    }
    function roles() {
        return ["admin", "manager", "sales", "accountant", "viewer", "cash_manager", "cashier"].map(function(r) { return { value: r, text: role(r) } })
    }
    function movement(k) {
        switch (k) {
        case "opening": return qsTr("Opening stock")
        case "purchase": return qsTr("Purchase")
        case "sale": return qsTr("Sale")
        case "return_in": return qsTr("Customer return")
        case "return_out": return qsTr("Supplier return")
        case "adjustment": return qsTr("Adjustment")
        case "transfer_in": return qsTr("Transfer in")
        case "transfer_out": return qsTr("Transfer out")
        }
        return k
    }
    function subscription(s) {
        switch (s) {
        case "trialing": return { text: qsTr("Free trial"), tone: "primary" }
        case "active": return { text: qsTr("Active"), tone: "success" }
        case "past_due": return { text: qsTr("Grace period"), tone: "warning" }
        case "suspended": return { text: qsTr("Suspended — read only"), tone: "danger" }
        case "canceled": return { text: qsTr("Cancelled"), tone: "neutral" }
        }
        return { text: s || "", tone: "neutral" }
    }
    function orderStatus(s) {
        switch (s) {
        case "pending": return { text: qsTr("Awaiting payment"), tone: "warning" }
        case "paid": return { text: qsTr("Paid"), tone: "success" }
        case "failed": return { text: qsTr("Failed"), tone: "danger" }
        case "canceled": return { text: qsTr("Cancelled"), tone: "neutral" }
        case "expired": return { text: qsTr("Expired"), tone: "neutral" }
        case "refunded": return { text: qsTr("Refunded"), tone: "neutral" }
        }
        return { text: s || "", tone: "neutral" }
    }
    function orderKind(k) {
        switch (k) {
        case "new": return qsTr("Subscription")
        case "renewal": return qsTr("Renewal")
        case "upgrade": return qsTr("Upgrade")
        case "downgrade": return qsTr("Downgrade")
        case "admin": return qsTr("Created by Nhill")
        }
        return k || ""
    }
    function duration(m) {
        if (m === 1) return qsTr("1 month")
        if (m === 12) return qsTr("1 year")
        if (m === 24) return qsTr("2 years")
        if (m === 36) return qsTr("3 years")
        return qsTr("%1 months").arg(m)
    }
    function durations() { return [1, 3, 6, 12, 24, 36].map(function(m) { return { value: m, text: duration(m) } }) }
    function feature(k) {
        switch (k) {
        case "pos": return qsTr("Point of sale (Nhill POS)")
        case "multi_warehouse": return qsTr("Several warehouses")
        case "reports": return qsTr("Advanced reports")
        case "api": return qsTr("API access")
        case "priority_support": return qsTr("Priority support")
        }
        return k
    }
    function subscriptionEvent(k) {
        switch (k) {
        case "activated": return qsTr("Subscription activated")
        case "renewed": return qsTr("Subscription renewed")
        case "upgraded": return qsTr("Upgraded")
        case "downgrade_scheduled": return qsTr("Downgrade scheduled")
        case "plan_changed": return qsTr("Plan changed")
        case "reminder_sent": return qsTr("Renewal reminder")
        case "grace_started": return qsTr("Grace period started")
        case "suspended": return qsTr("Suspended")
        case "admin_extend": return qsTr("Extended by Nhill")
        case "admin_suspend": return qsTr("Suspended by Nhill")
        case "admin_reactivate": return qsTr("Reactivated by Nhill")
        case "admin_cancel": return qsTr("Cancelled by Nhill")
        case "admin_set_plan": return qsTr("Plan changed by Nhill")
        case "trial_started": return qsTr("Free trial started")
        case "trial_refused": return qsTr("Free trial refused (already used)")
        case "subscription_suspended": return qsTr("Suspended")
        }
        return k
    }
    function ticketStatus(s) {
        switch (s) {
        case "open": return { text: qsTr("Open"), tone: "warning" }
        case "in_progress": return { text: qsTr("In progress"), tone: "primary" }
        case "waiting_customer": return { text: qsTr("Waiting for you"), tone: "accent" }
        case "resolved": return { text: qsTr("Resolved"), tone: "success" }
        case "closed": return { text: qsTr("Closed"), tone: "neutral" }
        }
        return { text: s || "", tone: "neutral" }
    }
    function ticketStatuses() { return ["open", "in_progress", "waiting_customer", "resolved", "closed"].map(function(s) { return { value: s, text: ticketStatus(s).text } }) }
    function ticketCategory(c) {
        switch (c) {
        case "bug": return qsTr("Bug")
        case "question": return qsTr("Question")
        case "billing": return qsTr("Billing")
        case "feature": return qsTr("Suggestion")
        }
        return c || ""
    }
    function ticketCategories() { return ["bug", "question", "billing", "feature"].map(function(c) { return { value: c, text: ticketCategory(c) } }) }
    function severity(s) {
        switch (s) {
        case "low": return { text: qsTr("Low"), tone: "neutral" }
        case "normal": return { text: qsTr("Normal"), tone: "primary" }
        case "high": return { text: qsTr("High"), tone: "warning" }
        case "critical": return { text: qsTr("Critical"), tone: "danger" }
        }
        return { text: s || "", tone: "neutral" }
    }
    function severities() { return ["low", "normal", "high", "critical"].map(function(s) { return { value: s, text: severity(s).text } }) }
    function units() {
        return [ { value: "unit", text: qsTr("Unit (pc)") }, { value: "kg", text: "kg" }, { value: "g", text: "g" },
                 { value: "l", text: qsTr("Litre") }, { value: "m", text: qsTr("Metre") }, { value: "m2", text: "m²" },
                 { value: "hour", text: qsTr("Hour") }, { value: "day", text: qsTr("Day") }, { value: "box", text: qsTr("Box") },
                 { value: "bag", text: qsTr("Bag") }, { value: "pack", text: qsTr("Pack") }, { value: "service", text: qsTr("Flat fee") } ]
    }
    function unit(u) { var all = units(); for (var i = 0; i < all.length; ++i) if (all[i].value === u) return all[i].text; return u }
    function currencies() {
        return [ { value: "CDF", text: qsTr("CDF — Congolese franc") }, { value: "USD", text: qsTr("USD — US dollar") },
                 { value: "EUR", text: qsTr("EUR — Euro") }, { value: "XAF", text: qsTr("XAF — CFA franc (CEMAC)") },
                 { value: "XOF", text: qsTr("XOF — CFA franc (WAEMU)") }, { value: "RWF", text: qsTr("RWF — Rwandan franc") },
                 { value: "ZAR", text: qsTr("ZAR — South African rand") }, { value: "KES", text: qsTr("KES — Kenyan shilling") },
                 { value: "GBP", text: qsTr("GBP — Pound sterling") }, { value: "CHF", text: qsTr("CHF — Swiss franc") } ]
    }
    function countries() {
        return [ { value: "CD", text: qsTr("DR Congo") }, { value: "CG", text: qsTr("Republic of the Congo") },
                 { value: "AO", text: qsTr("Angola") }, { value: "RW", text: qsTr("Rwanda") }, { value: "BI", text: qsTr("Burundi") },
                 { value: "UG", text: qsTr("Uganda") }, { value: "ZM", text: qsTr("Zambia") }, { value: "CM", text: qsTr("Cameroon") },
                 { value: "SN", text: qsTr("Senegal") }, { value: "CI", text: qsTr("Côte d'Ivoire") }, { value: "BE", text: qsTr("Belgium") },
                 { value: "FR", text: qsTr("France") }, { value: "US", text: qsTr("United States") }, { value: "ZA", text: qsTr("South Africa") } ]
    }
    function password(code) {
        switch (code) {
        case "password_too_short": return qsTr("At least 8 characters.")
        case "password_too_common": return qsTr("Too common.")
        case "password_same_as_email": return qsTr("Must differ from the e-mail.")
        case "password_repetitive": return qsTr("Too repetitive.")
        }
        return ""
    }
    function activityCategories() {
        return [ { value: "", text: qsTr("All actions") }, { value: "sales_document.", text: qsTr("Sales documents") },
                 { value: "payment.", text: qsTr("Payments") }, { value: "expense.", text: qsTr("Expenses") },
                 { value: "purchase.", text: qsTr("Purchases") }, { value: "stock.", text: qsTr("Stock") },
                 { value: "product.", text: qsTr("Products") }, { value: "party.", text: qsTr("Customers & suppliers") },
                 { value: "auth.", text: qsTr("Sign-ins & passwords") }, { value: "member.", text: qsTr("Team") },
                 { value: "establishment.", text: qsTr("Settings") }, { value: "admin.", text: qsTr("Nhill support") } ]
    }
    function timeZones() {
        return [ { value: "Africa/Kinshasa", text: qsTr("Kinshasa (UTC+1)") }, { value: "Africa/Lubumbashi", text: qsTr("Lubumbashi (UTC+2)") },
                 { value: "Africa/Brazzaville", text: "Brazzaville (UTC+1)" }, { value: "Africa/Kigali", text: "Kigali (UTC+2)" },
                 { value: "Africa/Bujumbura", text: "Bujumbura (UTC+2)" }, { value: "Africa/Luanda", text: "Luanda (UTC+1)" },
                 { value: "Africa/Douala", text: "Douala (UTC+1)" }, { value: "Africa/Dakar", text: "Dakar (UTC+0)" },
                 { value: "Africa/Abidjan", text: "Abidjan (UTC+0)" }, { value: "Africa/Johannesburg", text: "Johannesburg (UTC+2)" },
                 { value: "Africa/Nairobi", text: "Nairobi (UTC+3)" }, { value: "Europe/Brussels", text: qsTr("Brussels") },
                 { value: "Europe/Paris", text: qsTr("Paris") }, { value: "America/New_York", text: "New York" }, { value: "UTC", text: "UTC" } ]
    }
}
