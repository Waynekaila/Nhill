import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nhill.Core
import Nhill.Ui
import Nhill.Business

FbPage {
    id: page
    title: qsTr("Dashboard")
    subtitle: Session.establishment.trade_name || ""
    scrollable: true
    property string period: "month"
    property var data: null
    property bool loading: false
    readonly property string cur: Session.currency

    function range() {
        if (period === "year") return [Fmt.yearStart(), Fmt.today()]
        if (period === "30") return [Fmt.addDays(Fmt.today(), -29), Fmt.today()]
        return [Fmt.monthStart(), Fmt.today()]
    }
    function reload() {
        loading = true
        var r = range()
        Api.get("/api/v1/reports/dashboard?from=" + r[0] + "&to=" + r[1], function(res) {
            loading = false
            if (res.ok) page.data = res.data; else Nav.fail(res)
        })
    }
    Component.onCompleted: reload()
    function val(obj, key) { return obj && obj[key] ? obj[key] : null }

    actions: [
        FbButton { visible: Session.can("sales.write"); text: qsTr("New invoice"); iconName: "plus"; onClicked: Nav.push("pages/DocumentEditorPage.qml", { docType: "invoice" }) },
        TabStrip {
            model: [ { value: "month", text: qsTr("This month") }, { value: "30", text: qsTr("30 days") }, { value: "year", text: qsTr("This year") } ]
            current: page.period
            onSelected: function(v) { page.period = v; page.reload() }
        }
    ]

    Banner {
        visible: Session.subscription.status === "trialing" && Session.can("subscription.manage")
        tone: "info"
        text: qsTr("Free trial: %n day(s) left. Choose a plan at any time in Settings › Subscription.", "", Session.subscription.days_left || 0)
    }

    GridLayout {
        Layout.fillWidth: true
        columns: Theme.compact ? 2 : (self ? 3 : 4)
        columnSpacing: Theme.s3; rowSpacing: Theme.s3
        property var rev: page.data ? page.val(page.data.revenue, page.cur) : null
        property var recv: page.data ? page.val(page.data.receivables, page.cur) : null
        readonly property bool self: !!page.data && page.data.scope === "self"
        KpiTile {
            label: parent.self ? qsTr("My sales (excl. tax)") : qsTr("Revenue (excl. tax)")
            iconName: "chart"
            value: parent.rev ? Fmt.money(parent.rev.net, page.cur) : Fmt.money("0", page.cur)
            caption: parent.rev ? qsTr("%n invoice(s)", "", parent.rev.invoices) : qsTr("No invoice in the period")
        }
        KpiTile {
            label: parent.self ? qsTr("My unpaid invoices") : qsTr("Unpaid invoices")
            iconName: "invoice"; tone: parent.recv && parent.recv.overdue_count > 0 ? "danger" : "accent"
            value: parent.recv ? Fmt.money(parent.recv.open, page.cur) : Fmt.money("0", page.cur)
            caption: parent.recv ? qsTr("%1 open · %2 overdue").arg(parent.recv.open_count).arg(parent.recv.overdue_count) : qsTr("Nothing to collect")
        }
        KpiTile {
            visible: !parent.self
            label: qsTr("Expenses")
            iconName: "wallet"; tone: "accent"
            value: page.data && page.data.expenses[page.cur] ? Fmt.money(page.data.expenses[page.cur], page.cur) : Fmt.money("0", page.cur)
            caption: page.data && page.data.payables[page.cur] ? qsTr("To pay: %1").arg(Fmt.money(page.data.payables[page.cur], page.cur)) : qsTr("No supplier debt")
        }
        KpiTile {
            label: qsTr("Stock alerts")
            iconName: "stock"; tone: page.data && page.data.stock.low_stock.length > 0 ? "warning" : "primary"
            value: page.data ? String(page.data.stock.low_stock.length) : "0"
            caption: page.data && page.data.stock.estimated_value[page.cur] ? qsTr("Stock value (est.): %1").arg(Fmt.money(page.data.stock.estimated_value[page.cur], page.cur)) : qsTr("%n tracked product(s)", "", page.data ? page.data.stock.tracked_products : 0)
        }
    }

    // other currencies, never mixed or converted implicitly
    Flow {
        Layout.fillWidth: true
        spacing: Theme.s2
        visible: page.data && Object.keys(page.data.revenue).length > 1
        Repeater {
            model: page.data ? Object.keys(page.data.revenue).filter(function(c) { return c !== page.cur }) : []
            delegate: FbChip { tone: "neutral"; text: qsTr("Also: %1 excl. tax").arg(Fmt.money(page.data.revenue[modelData].net, modelData)) }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: Theme.compact ? 1 : 2
        columnSpacing: Theme.s3; rowSpacing: Theme.s3
        FbCard {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            ColumnLayout {
                width: parent.width
                spacing: Theme.s2
                SectionTitle { text: qsTr("Recent sales") }
                Repeater {
                    model: page.data ? page.data.recent_sales : []
                    delegate: ItemDelegate {
                        Layout.fillWidth: true
                        onClicked: Nav.push("pages/DocumentPage.qml", { docId: modelData.id })
                        background: Rectangle { color: parent.hovered ? Theme.surfaceAlt : "transparent"; radius: 6 }
                        contentItem: RowLayout {
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 0
                                Text { text: modelData.party_name; color: Theme.text; font.family: Theme.font; font.pixelSize: Theme.fs_md; elide: Text.ElideRight; Layout.fillWidth: true }
                                Text { text: modelData.number + " · " + Fmt.date(modelData.issue_date); color: Theme.textMuted; font.family: Theme.mono; font.pixelSize: Theme.fs_xs }
                            }
                            ColumnLayout {
                                spacing: 2
                                Text { Layout.alignment: Qt.AlignRight; text: Fmt.money(modelData.total, modelData.currency); color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold }
                                FbChip { Layout.alignment: Qt.AlignRight; property var c: Labels.docStatus({ status: modelData.status, doc_type: "invoice" }); text: c.text; tone: c.tone }
                            }
                        }
                    }
                }
                Text { visible: page.data && page.data.recent_sales.length === 0; text: qsTr("No sale yet. Your first invoice takes less than a minute."); color: Theme.textFaint; font.family: Theme.font; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.s3
            FbCard {
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s2
                    SectionTitle { text: qsTr("Best sellers"); hint: qsTr("in the period") }
                    Repeater {
                        model: page.data ? page.data.top_products : []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Text { text: modelData.name; color: Theme.text; font.family: Theme.font; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: Fmt.quantity(modelData.quantity); color: Theme.textMuted; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                            Text { text: Fmt.money(modelData.net, modelData.currency); color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold; Layout.preferredWidth: 130; horizontalAlignment: Text.AlignRight }
                        }
                    }
                    Text { visible: page.data && page.data.top_products.length === 0; text: qsTr("No data for this period."); color: Theme.textFaint; font.family: Theme.font }
                }
            }
            FbCard {
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s2
                    SectionTitle { text: qsTr("Low stock") }
                    Repeater {
                        model: page.data ? page.data.stock.low_stock : []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Icon { name: "alert"; color: Theme.warning; size: 16 }
                            Text { text: modelData.name; color: Theme.text; font.family: Theme.font; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: qsTr("%1 left (min. %2)").arg(Fmt.quantity(modelData.quantity)).arg(Fmt.quantity(modelData.reorder_level)); color: Theme.warning; font.family: Theme.font; font.pixelSize: Theme.fs_sm }
                        }
                    }
                    Text { visible: page.data && page.data.stock.low_stock.length === 0; text: qsTr("All stock levels are above their alert threshold."); color: Theme.textFaint; font.family: Theme.font; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }
        }
    }
    Text {
        Layout.fillWidth: true
        text: qsTr("Figures are calculated from issued documents only (drafts excluded), per currency, without conversion. Revenue = invoices minus credit notes, excluding tax.")
        color: Theme.textFaint; font.family: Theme.font; font.pixelSize: Theme.fs_xs; wrapMode: Text.WordWrap
    }
}
