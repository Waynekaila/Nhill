import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Pos

// Selling screen: catalogue (search / barcode / categories) on the left, ticket on the
// right. Amounts shown before payment are a preview computed with the same rules as
// the server; the server prices the sale again and is the reference.
Rectangle {
    id: root
    color: Theme.bg
    property var catalog: []
    property string category: ""
    property var cart: []                 // [{product, qty, discount_bp}]
    property var customer: null           // {id, name} or null (walk-in)
    property string promo: ""
    // 2.0.0: the screen adapts down to 860 px wide (small laptops, 1024 x 600 netbooks)
    readonly property bool narrow: width < 1200
    readonly property string cur: PosNav.session ? PosNav.session.currency : (PosNav.config ? PosNav.config.currency : "CDF")
    readonly property var categories: {
        var set = {}
        catalog.forEach(function(p) { if (p.category) set[p.category] = true })
        return Object.keys(set).sort()
    }
    readonly property var visibleProducts: {
        var q = search.text.trim().toLowerCase()
        return catalog.filter(function(p) {
            if (p.currency !== root.cur) return false
            if (root.category && p.category !== root.category) return false
            if (!q) return true
            return p.name.toLowerCase().indexOf(q) >= 0 || p.sku.toLowerCase().indexOf(q) >= 0 || (p.barcode && p.barcode === q)
        }).slice(0, 120)
    }
    function lineOf(item) { return Fmt.lineTotals(String(item.qty), item.product.price, item.discount_bp, item.product.tax_rate_bp, root.cur) }
    readonly property var totals: {
        var nets = [], taxes = [], totalsList = []
        cart.forEach(function(it) { var t = lineOf(it); nets.push(t.net); taxes.push(t.tax); totalsList.push(t.total) })
        return { net: Fmt.sum(nets, cur), tax: Fmt.sum(taxes, cur), total: Fmt.sum(totalsList, cur), count: cart.length }
    }
    function loadCatalog() {
        if (!PosNav.session) return
        Api.get("/api/v1/pos/catalog?register_id=" + PosNav.session.register.id, function(r) { if (r.ok) root.catalog = r.data.items; else PosNav.fail(r) })
    }
    function add(p) {
        if (p.track_stock && parseFloat(p.stock) <= 0) PosNav.toast(qsTr("%1: no stock left in this warehouse.").arg(p.name), "warning")
        var c = cart.slice()
        for (var i = 0; i < c.length; ++i) if (c[i].product.id === p.id && c[i].discount_bp === 0) { c[i] = { product: p, qty: c[i].qty + 1, discount_bp: 0 }; cart = c; return }
        c.push({ product: p, qty: 1, discount_bp: 0 })
        cart = c
    }
    function setQty(i, q) { var c = cart.slice(); if (q <= 0) c.splice(i, 1); else c[i] = { product: c[i].product, qty: q, discount_bp: c[i].discount_bp }; cart = c }
    function setDiscount(i, bp) { var c = cart.slice(); c[i] = { product: c[i].product, qty: c[i].qty, discount_bp: bp }; cart = c }
    function clear() { cart = []; customer = null; promo = ""; search.text = "" }
    function scan(text) {
        var t = text.trim()
        if (!t) return
        var exact = catalog.filter(function(p) { return p.barcode === t || p.sku.toLowerCase() === t.toLowerCase() })
        if (exact.length === 1) { add(exact[0]); search.text = ""; return }
        if (visibleProducts.length === 1) { add(visibleProducts[0]); search.text = "" }
    }
    Component.onCompleted: loadCatalog()
    // Keyboard (2.0.0): F2 = search / scan field, F9 = pay. The barcode scanner keeps
    // typing into the search field, as before.
    Shortcut { sequence: "F2"; onActivated: search.forceActiveFocus() }
    Shortcut { sequence: "F9"; enabled: root.cart.length > 0 && !pay.opened; onActivated: pay.start() }
    // documentation tour hooks
    function tourFill() { var g = catalog.filter(function(p) { return p.currency === cur }); for (var i = 0; i < Math.min(3, g.length); ++i) add(g[i]); if (cart.length) setQty(0, 2) }
    function tourPay() { pay.start(); pay.tourTender() }
    function tourSubmit() { pay.submit() }
    function tourClosePay() { pay.close() }

    // ---------------------------------------------------------------- top bar
    Rectangle {
        id: top
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 60
        color: Theme.sidebar
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s4
            spacing: Theme.s3
            LogoMark { variant: "reverse"; app: "pos"; Layout.preferredHeight: 26; Layout.preferredWidth: 150; horizontalAlignment: Image.AlignLeft }
            Text { color: "#FFFFFF"; font.family: Theme.font; font.weight: Font.DemiBold
                   text: PosNav.session ? PosNav.session.register.code + " · " + PosNav.session.register.name + " · " + qsTr("Session %1").arg(PosNav.session.number) : "" }
            Text { visible: !root.narrow; color: Theme.sidebarMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm; text: Session.me.user ? Session.me.user.full_name + " · " + Labels.role(Session.role) : ""
                   elide: Text.ElideRight; Layout.maximumWidth: 260 }
            Item { Layout.fillWidth: true }
            ConnectionBadge {}
            FbButton { text: qsTr("Tickets & cash"); variant: "secondary"; iconName: "receipt"; onClicked: PosNav.window.openSession() }
            FbButton { text: root.narrow ? "" : qsTr("Help"); tip: qsTr("Help"); variant: "secondary"; iconName: "info"; onClicked: supportDialog.open() }
            FbButton { text: root.narrow ? "" : qsTr("Sign out"); tip: qsTr("Sign out"); variant: "secondary"; iconName: "logout"; onClicked: PosNav.window.signOut() }
        }
    }

    RowLayout {
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; margins: Theme.s3 }
        spacing: Theme.s3
        // ------------------------------------------------------------ catalogue
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: Theme.s2
            SearchField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Scan a barcode or search a product (Enter adds it, F2 returns here)")
                onAccepted: root.scan(text)
                focus: true
            }
            Flickable {
                Layout.fillWidth: true; Layout.preferredHeight: 40
                contentWidth: catRow.implicitWidth; clip: true
                visible: root.categories.length > 0
                Row {
                    id: catRow
                    spacing: Theme.s2
                    FbButton { text: qsTr("All"); variant: root.category === "" ? "primary" : "secondary"; onClicked: root.category = "" }
                    Repeater { model: root.categories
                               FbButton { text: modelData; variant: root.category === modelData ? "primary" : "secondary"; onClicked: root.category = modelData } }
                }
            }
            GridView {
                id: grid
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true
                cellWidth: Math.max(150, width / Math.max(1, Math.floor(width / 170)))
                cellHeight: 110
                model: root.visibleProducts
                ScrollBar.vertical: ScrollBar {}
                delegate: Item {
                    width: grid.cellWidth; height: grid.cellHeight
                    Rectangle {
                        anchors.fill: parent; anchors.margins: 4
                        radius: Theme.radius
                        color: tileMouse.pressed ? Theme.primarySoft : Theme.surface
                        border.color: tileMouse.containsMouse ? Theme.primary : Theme.line
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: Theme.s2; spacing: 2
                            Text { Layout.fillWidth: true; text: modelData.name; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
                                   color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_sm; font.weight: Font.DemiBold }
                            Item { Layout.fillHeight: true }
                            Text { text: Fmt.money(modelData.price, modelData.currency); color: Theme.primary; font.family: Theme.font; font.weight: Font.Bold }
                            Text { visible: modelData.track_stock; text: qsTr("Stock: %1").arg(Fmt.quantity(modelData.stock))
                                   color: parseFloat(modelData.stock) > 0 ? Theme.textMuted : Theme.danger; font.family: Theme.font; font.pixelSize: Theme.fs_xs }
                        }
                        MouseArea { id: tileMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.add(modelData) }
                    }
                }
                EmptyState { anchors.centerIn: parent; visible: grid.count === 0; iconName: "box"; title: qsTr("No product")
                             text: qsTr("Products and prices come from Nhill Business (catalogue in %1).").arg(root.cur) }
            }
        }
        // ------------------------------------------------------------ ticket
        Rectangle {
            Layout.preferredWidth: Math.max(340, Math.min(480, Math.round(root.width * 0.38)))
            Layout.fillHeight: true
            radius: Theme.radiusLg; color: Theme.surface; border.color: Theme.line
            ColumnLayout {
                anchors.fill: parent; anchors.margins: Theme.s3
                spacing: Theme.s2
                RowLayout {
                    Text { text: qsTr("Ticket"); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_lg; font.weight: Font.DemiBold; Layout.fillWidth: true }
                    FbButton { text: root.customer ? root.customer.name : qsTr("Walk-in customer"); variant: "ghost"; iconName: "user"; onClicked: customerDialog.start() }
                }
                ListView {
                    id: lines
                    Layout.fillWidth: true; Layout.fillHeight: true
                    clip: true
                    model: root.cart
                    spacing: 2
                    delegate: Rectangle {
                        id: lineRow
                        width: lines.width; height: Math.max(64, lineContent.implicitHeight + 2 * Theme.s2)
                        color: index % 2 ? Theme.surfaceAlt : "transparent"; radius: Theme.radius
                        readonly property var t: root.lineOf(modelData)
                        RowLayout {
                            id: lineContent
                            anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Theme.s2; spacing: Theme.s1
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 0
                                // 2.0.0: two lines for long product names instead of "Ciment 50 kg …"
                                Text { Layout.fillWidth: true; text: modelData.product.name; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
                                       color: Theme.text; font.family: Theme.font; font.weight: Font.Medium }
                                Text { text: Fmt.money(modelData.product.price, root.cur) + (modelData.discount_bp ? " · −" + Fmt.percent(modelData.discount_bp) : "")
                                       color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs }
                            }
                            FbIconButton { iconName: "minus"; tip: modelData.qty > 1 ? qsTr("Decrease the quantity") : qsTr("Remove the line"); onClicked: root.setQty(index, modelData.qty - 1) }
                            Text { text: String(modelData.qty); font.family: Theme.mono; font.pixelSize: Theme.fs_lg; color: Theme.text; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 30
                                   Accessible.name: qsTr("Quantity %1").arg(modelData.qty) }
                            FbIconButton { iconName: "plus"; tip: qsTr("Increase the quantity"); onClicked: root.setQty(index, modelData.qty + 1) }
                            FbIconButton { iconName: "percent"; tip: qsTr("Discount"); onClicked: discountDialog.start(index) }
                            Text { text: Fmt.amount(lineRow.t.total, root.cur); color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 96 }
                        }
                    }
                    EmptyState { anchors.centerIn: parent; visible: root.cart.length === 0; iconName: "cart"; title: qsTr("Empty ticket"); text: qsTr("Tap a product or scan a barcode.") }
                }
                RowLayout {
                    Layout.fillWidth: true
                    FbTextField { id: promoField; Layout.fillWidth: true; placeholderText: qsTr("Promo code"); monospace: true; text: root.promo; onTextChanged: root.promo = text.trim() }
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
                RowLayout { Layout.fillWidth: true
                    Text { text: qsTr("Subtotal excl. tax"); color: Theme.textMuted; font.family: Theme.font; Layout.fillWidth: true }
                    Text { text: Fmt.money(root.totals.net, root.cur); color: Theme.text; font.family: Theme.font } }
                RowLayout { Layout.fillWidth: true
                    Text { text: qsTr("Taxes"); color: Theme.textMuted; font.family: Theme.font; Layout.fillWidth: true }
                    Text { text: Fmt.money(root.totals.tax, root.cur); color: Theme.text; font.family: Theme.font } }
                RowLayout { Layout.fillWidth: true
                    Text { text: qsTr("Total"); color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_xl; font.weight: Font.Bold; Layout.fillWidth: true }
                    Text { text: Fmt.money(root.totals.total, root.cur); color: Theme.primary; font.family: Theme.font; font.pixelSize: Theme.fs_xxl; font.weight: Font.Bold } }
                Text { visible: root.promo.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_xs
                       text: qsTr("The promo code is checked and applied by the server when paying.") }
                RowLayout {
                    Layout.fillWidth: true
                    FbButton { text: qsTr("Clear"); variant: "secondary"; iconName: "trash"; enabled: root.cart.length > 0; onClicked: root.clear() }
                    FbButton { Layout.fillWidth: true; implicitHeight: 56; text: qsTr("Pay"); tip: qsTr("Shortcut: F9"); iconName: "payment"; enabled: root.cart.length > 0; onClicked: pay.start() }
                }
            }
        }
    }

    // ---------------------------------------------------------------- dialogs
    PosPayDialog { id: pay; sale: root }
    FbDialog {
        id: discountDialog
        property int index: -1
        title: qsTr("Line discount")
        subtitle: index >= 0 && root.cart[index] ? root.cart[index].product.name : ""
        function start(i) { index = i; pct.text = root.cart[i].discount_bp ? Fmt.percentText(root.cart[i].discount_bp) : ""; open() }
        Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.family: Theme.font
               text: PosNav.config ? qsTr("Up to %1 without approval. Above, a supervisor confirms with their password when paying.").arg(Fmt.percent(PosNav.config.settings.max_discount_bp)) : "" }
        Field { label: qsTr("Discount (%)"); FbTextField { id: pct; inputMethodHints: Qt.ImhFormattedNumbersOnly } }
        footer: [
            FbButton { text: qsTr("Apply"); onClicked: { var bp = pct.text.trim() ? Fmt.parsePercent(pct.text) : 0
                       if (bp < 0 || bp > 10000) { PosNav.toast(qsTr("Invalid percentage."), "danger"); return }
                       root.setDiscount(discountDialog.index, bp); discountDialog.close() } },
            FbButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: discountDialog.close() }
        ]
    }
    FbDialog {
        id: customerDialog
        property var results: []
        title: qsTr("Customer")
        function start() { q.text = ""; results = []; open() }
        function find() { Api.get("/api/v1/customers?limit=20&search=" + encodeURIComponent(q.text.trim()), function(r) { if (r.ok) customerDialog.results = r.data.items }) }
        SearchField { id: q; Layout.fillWidth: true; placeholderText: qsTr("Name or phone"); onAccepted: customerDialog.find(); onTextChanged: if (text.length >= 2) customerDialog.find() }
        Repeater {
            model: customerDialog.results
            delegate: FbButton { Layout.fillWidth: true; variant: "secondary"; text: modelData.name + (modelData.phone ? " · " + modelData.phone : "")
                                 onClicked: { root.customer = { id: modelData.id, name: modelData.name }; customerDialog.close() } }
        }
        footer: [ FbButton { text: qsTr("Walk-in customer"); variant: "secondary"; onClicked: { root.customer = null; customerDialog.close() } },
                  FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: customerDialog.close() } ]
    }
    FbDialog {
        id: supportDialog
        title: qsTr("Help & support")
        preferredWidth: 760
        SupportCenter { Layout.fillWidth: true; Layout.preferredHeight: 480; source: "pos"
                        failHandler: function(r) { PosNav.fail(r) }; toastHandler: function(t, tone) { PosNav.toast(t, tone) } }
        footer: [ FbButton { text: qsTr("Close"); variant: "secondary"; onClicked: supportDialog.close() } ]
    }
}
