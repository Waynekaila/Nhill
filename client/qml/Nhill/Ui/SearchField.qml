import QtQuick
import QtQuick.Controls

// Debounced search box: emits search(text) 300 ms after typing stops.
FbTextField {
    id: root
    signal search(string text)
    leftPadding: 36
    placeholderText: qsTr("Search")
    onTextChanged: debounce.restart()
    Timer { id: debounce; interval: 300; onTriggered: root.search(root.text.trim()) }
    Icon { x: 10; anchors.verticalCenter: parent.verticalCenter; name: "search"; size: 18; color: Theme.textFaint }
}
