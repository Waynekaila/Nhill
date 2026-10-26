import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Professional table on desktop, card list on phones.
// columns: [{ title, width (relative), align: "right", mono: bool,
//             value: function(row) -> string, chip: function(row) -> {text, tone} }]
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

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.line
        clip: true

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
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: ItemDelegate {
                id: rowItem
                readonly property var rowData: modelData
                width: list.width
                height: root.compact ? 68 : Theme.rowHeight
                padding: 0
                onClicked: root.rowClicked(modelData, index)
                Accessible.name: root.columns.length > 0 ? root.cellText(root.columns[root.titleColumn], modelData) : ""
                background: Rectangle {
                    color: rowItem.down ? Theme.sunken : rowItem.hovered ? Theme.surfaceAlt : "transparent"
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

        EmptyState {
            anchors.centerIn: list
            width: Math.min(parent.width - 32, 440)
            visible: !root.loading && root.rows.length === 0
            iconName: root.emptyIcon
            title: root.emptyTitle
            text: root.emptyText
            actionText: root.emptyActionText
            onAction: root.emptyAction()
        }
        BusyIndicator { anchors.centerIn: list; running: root.loading; visible: running }
    }
}
