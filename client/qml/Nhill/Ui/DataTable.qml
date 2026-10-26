import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Professional table on desktop, card list on phones.
// columns: [{ title, width (relative), minWidth (px, optional), align: "right", mono: bool,
//             value: function(row) -> string, chip: function(row) -> {text, tone} }]
//
// 2.0.0:
//  * every column keeps a minimum width (minWidth, or 72 px per unit of relative width):
//    when the window is too narrow the table scrolls horizontally instead of cutting
//    every cell to "26/10/2…";
//  * a cut cell shows its full text in a tooltip;
//  * keyboard: Tab enters the table, arrows / Page Up / Page Down move, Enter opens.
Item {
    id: root
    property var columns: []
    property var rows: []
    property bool loading: false
    property bool compact: Theme.compact
    // card layout on phones
    property int titleColumn: 0
    property int subtitleColumn: 1
    property int trailingColumn: -1
    property int chipColumn: -1
    property string emptyIcon: "box"
    property string emptyTitle: qsTr("Nothing here yet")
    property string emptyText: ""
    property string emptyActionText: ""
    signal rowClicked(var row, int index)
    signal emptyAction()
    implicitHeight: 400

    function cellText(col, row) { return col.value ? String(col.value(row) ?? "") : "" }
    readonly property real totalWeight: { var t = 0; for (var i = 0; i < columns.length; ++i) t += (columns[i].width || 1); return t }
    function minColumnWidth(c) { return c.minWidth || Math.round(72 * (c.width || 1)) }
    readonly property real minContentWidth: {
        var t = 2 * Theme.s4
        for (var i = 0; i < columns.length; ++i) t += minColumnWidth(columns[i])
        return t
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.line
        clip: true

        Flickable {
            id: hscroll
            anchors.fill: parent
            anchors.margins: 1
            contentWidth: root.compact ? width : Math.max(width, root.minContentWidth)
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentWidth > width + 1
            clip: true
            ScrollBar.horizontal: ScrollBar { policy: hscroll.contentWidth > hscroll.width + 1 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }

            Item {
                id: canvas
                width: hscroll.contentWidth
                height: hscroll.height

                // header (desktop)
                Rectangle {
                    id: header
                    visible: !root.compact
                    width: parent.width; height: visible ? 40 : 0
                    color: Theme.surfaceAlt
                    radius: Theme.radiusLg
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Theme.radiusLg; color: Theme.surfaceAlt }
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s4
                        Repeater {
                            model: root.columns
                            delegate: Text {
                                width: (header.width - 2 * Theme.s4) * (modelData.width || 1) / root.totalWeight
                                height: header.height
                                text: modelData.title
                                horizontalAlignment: modelData.align === "right" ? Text.AlignRight : Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                rightPadding: modelData.align === "right" ? (index < root.columns.length - 1 ? Theme.s4 : 0) : Theme.s2
                                leftPadding: modelData.align === "right" ? Theme.s2 : 0
                                font.family: Theme.font; font.pixelSize: Theme.fs_xs; font.weight: Font.DemiBold
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: 0.4
                                color: Theme.textMuted
                                elide: Text.ElideRight
                                HoverHandler { id: headHover }
                                ToolTip.visible: headHover.hovered && truncated
                                ToolTip.text: modelData.title
                                ToolTip.delay: 500
                            }
                        }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
                }

                ListView {
                    id: list
                    anchors.top: header.bottom
                    anchors.bottom: parent.bottom
                    width: parent.width
                    clip: true
                    model: root.rows
                    boundsBehavior: Flickable.StopAtBounds
                    activeFocusOnTab: true
                    keyNavigationEnabled: true
                    currentIndex: -1
                    highlightMoveDuration: 0
                    Accessible.role: Accessible.List
                    Accessible.name: root.columns.length > 0 ? root.columns[root.titleColumn].title || "" : ""
                    onActiveFocusChanged: if (activeFocus && currentIndex < 0 && count > 0) currentIndex = 0
                    Keys.onReturnPressed: if (currentIndex >= 0) root.rowClicked(root.rows[currentIndex], currentIndex)
                    Keys.onEnterPressed: if (currentIndex >= 0) root.rowClicked(root.rows[currentIndex], currentIndex)
                    Keys.onSpacePressed: if (currentIndex >= 0) root.rowClicked(root.rows[currentIndex], currentIndex)
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    delegate: ItemDelegate {
                        id: rowItem
                        readonly property var rowData: modelData
                        readonly property bool keyboardCurrent: ListView.isCurrentItem && list.activeFocus
                        width: list.width
                        height: root.compact ? 68 : Theme.rowHeight
                        padding: 0
                        focusPolicy: Qt.NoFocus
                        onClicked: { list.currentIndex = index; root.rowClicked(modelData, index) }
                        Accessible.name: root.columns.length > 0 ? root.cellText(root.columns[root.titleColumn], modelData) : ""
                        background: Rectangle {
                            color: rowItem.down ? Theme.sunken : (rowItem.hovered || rowItem.keyboardCurrent) ? Theme.surfaceAlt : "transparent"
                            border.width: rowItem.keyboardCurrent ? 2 : 0
                            border.color: Theme.focus
                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line; opacity: 0.7 }
                        }
                        contentItem: Item {
                            // desktop row
                            Row {
                                visible: !root.compact
                                anchors.fill: parent
                                anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s4
                                Repeater {
                                    model: root.columns
                                    delegate: Item {
                                        width: (list.width - 2 * Theme.s4) * (modelData.width || 1) / root.totalWeight
                                        height: rowItem.height
                                        FbChip {
                                            visible: !!modelData.chip
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.right: modelData.align === "right" ? parent.right : undefined
                                            anchors.rightMargin: index < root.columns.length - 1 ? Theme.s4 : 0
                                            property var c: modelData.chip ? modelData.chip(rowItem.rowData) : null
                                            text: c ? c.text : ""
                                            tone: c ? c.tone : "neutral"
                                        }
                                        Text {
                                            id: cell
                                            visible: !modelData.chip
                                            anchors.fill: parent
                                            text: root.cellText(modelData, rowItem.rowData)
                                            horizontalAlignment: modelData.align === "right" ? Text.AlignRight : Text.AlignLeft
                                            verticalAlignment: Text.AlignVCenter
                                            rightPadding: modelData.align === "right" ? (index < root.columns.length - 1 ? Theme.s4 : 0) : Theme.s2
                                            font.family: modelData.mono ? Theme.mono : Theme.font
                                            font.pixelSize: Theme.fs_md
                                            font.weight: modelData.strong ? Font.DemiBold : Font.Normal
                                            color: modelData.muted ? Theme.textMuted : Theme.text
                                            elide: Text.ElideRight
                                            HoverHandler { id: cellHover }
                                            ToolTip.visible: cellHover.hovered && cell.truncated
                                            ToolTip.text: cell.text
                                            ToolTip.delay: 500
                                        }
                                    }
                                }
                            }
                            // phone card
                            RowLayout {
                                visible: root.compact
                                anchors.fill: parent
                                anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s4
                                spacing: Theme.s3
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: root.columns.length > root.titleColumn ? root.cellText(root.columns[root.titleColumn], modelData) : ""
                                        font.family: Theme.font; font.pixelSize: Theme.fs_md; font.weight: Font.DemiBold
                                        color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true
                                    }
                                    Text {
                                        visible: root.subtitleColumn >= 0
                                        text: root.subtitleColumn >= 0 && root.columns.length > root.subtitleColumn ? root.cellText(root.columns[root.subtitleColumn], modelData) : ""
                                        font.family: Theme.font; font.pixelSize: Theme.fs_sm
                                        color: Theme.textMuted; elide: Text.ElideRight; Layout.fillWidth: true
                                    }
                                }
                                ColumnLayout {
                                    spacing: 4
                                    Text {
                                        visible: root.trailingColumn >= 0
                                        Layout.alignment: Qt.AlignRight
                                        text: root.trailingColumn >= 0 ? root.cellText(root.columns[root.trailingColumn], modelData) : ""
                                        font.family: Theme.font; font.pixelSize: Theme.fs_md; font.weight: Font.DemiBold
                                        color: Theme.text
                                    }
                                    FbChip {
                                        visible: root.chipColumn >= 0
                                        Layout.alignment: Qt.AlignRight
                                        property var c: root.chipColumn >= 0 && root.columns[root.chipColumn].chip ? root.columns[root.chipColumn].chip(modelData) : null
                                        text: c ? c.text : ""
                                        tone: c ? c.tone : "neutral"
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        EmptyState {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: header.height / 2
            width: Math.min(parent.width - 32, 440)
            visible: !root.loading && root.rows.length === 0
            iconName: root.emptyIcon
            title: root.emptyTitle
            text: root.emptyText
            actionText: root.emptyActionText
            onAction: root.emptyAction()
        }
        BusyIndicator { anchors.centerIn: parent; running: root.loading; visible: running }
    }
}
