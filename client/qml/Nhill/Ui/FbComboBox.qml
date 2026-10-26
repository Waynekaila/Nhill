import QtQuick
import QtQuick.Controls

// model: array of {value, text}
ComboBox {
    id: control
    property var value: undefined
    property bool invalid: false
    textRole: "text"
    valueRole: "value"
    implicitHeight: Theme.controlHeight
    font.family: Theme.font
    font.pixelSize: Theme.fs_md
    Accessible.name: displayText
    onActivated: value = currentValue
    onValueChanged: {
        var i = indexOfValue(value)
        if (i !== currentIndex) currentIndex = i
    }
    onModelChanged: Qt.callLater(function() { var i = control.indexOfValue(control.value); if (i >= 0) control.currentIndex = i })
    Component.onCompleted: { var i = indexOfValue(value); if (i >= 0) currentIndex = i }
    contentItem: Text {
        leftPadding: Theme.s3
        rightPadding: Theme.s6
        text: control.displayText
        font: control.font
        color: control.currentIndex < 0 ? Theme.textFaint : Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Icon {
        x: control.width - width - Theme.s3
        y: (control.height - height) / 2
        name: "chevronDown"; size: 16; color: Theme.textMuted
    }
    background: Rectangle {
        radius: Theme.radius
        color: Theme.surface
        border.width: control.activeFocus ? 2 : 1
        border.color: control.invalid ? Theme.danger : control.activeFocus ? Theme.focus : Theme.inputBorder
    }
    delegate: ItemDelegate {
        width: control.width
        height: Theme.controlHeight
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: modelData[control.textRole] !== undefined ? modelData[control.textRole] : modelData
            color: Theme.text
            font.family: Theme.font
            font.pixelSize: Theme.fs_md
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle { color: highlighted ? Theme.primarySoft : Theme.surface }
    }
    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2, 320)
        padding: 1
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle { color: Theme.surface; border.color: Theme.line; radius: Theme.radius }
    }
}
