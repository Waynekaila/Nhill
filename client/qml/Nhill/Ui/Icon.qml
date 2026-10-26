import QtQuick

// Stroke icons drawn from SVG path data on a 24x24 grid (Nhill set).
Item {
    id: root
    property string name: "dot"
    property color color: Theme.text
    property real size: 20
    property real stroke: 1.8
    implicitWidth: size
    implicitHeight: size

    readonly property var paths: ({
        "dashboard": "M3 3h7v9H3z M14 3h7v5h-7z M14 12h7v9h-7z M3 16h7v5H3z",
        "invoice": "M14 3H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z M14 3v6h6 M8 13h8 M8 17h5",
        "users": "M16 20v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2 M9 10a4 4 0 1 0 0-8 4 4 0 0 0 0 8z M22 20v-2a4 4 0 0 0-3-3.9 M16 2.1a4 4 0 0 1 0 7.8",
        "box": "M21 8l-9-5-9 5 9 5 9-5z M3 8v8l9 5 9-5V8 M12 13v8",
        "stock": "M3 21V8l9-5 9 5v13 M7 21v-8h10v8 M7 17h10",
        "cart": "M3 4h2l2.4 11.2a2 2 0 0 0 2 1.6h7.7a2 2 0 0 0 2-1.5L21 8H6 M9 21a1 1 0 1 0 0-2 1 1 0 0 0 0 2z M18 21a1 1 0 1 0 0-2 1 1 0 0 0 0 2z",
        "wallet": "M3 7a2 2 0 0 1 2-2h13v4 M3 7v10a2 2 0 0 0 2 2h16V9H5a2 2 0 0 1-2-2z M16 14h2",
        "payment": "M2 6h20v12H2z M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z M6 9v6 M18 9v6",
        "chart": "M3 3v18h18 M7 15l4-4 3 3 5-6",
        "settings": "M4 6h9 M17 6h3 M15 4v4 M4 12h3 M11 12h9 M9 10v4 M4 18h11 M19 18h1 M17 16v4",
        "search": "M11 19a8 8 0 1 0 0-16 8 8 0 0 0 0 16z M21 21l-4.3-4.3",
        "plus": "M12 5v14 M5 12h14",
        "logout": "M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4 M16 17l5-5-5-5 M21 12H9",
        "bell": "M6 8a6 6 0 1 1 12 0c0 7 3 9 3 9H3s3-2 3-9 M10.3 21a2 2 0 0 0 3.4 0",
        "offline": "M2 2l20 20 M8.5 16.5a5 5 0 0 1 7 0 M5 12.9a10 10 0 0 1 5.2-2.8 M19 12.9a10 10 0 0 0-2.3-1.6 M2 8.8a15 15 0 0 1 4.2-2.7 M22 8.8a15 15 0 0 0-11.3-3.8 M12 20h0.01",
        "online": "M5 12.9a10 10 0 0 1 14 0 M8.5 16.4a5 5 0 0 1 7 0 M2 8.8a15 15 0 0 1 20 0 M12 20h0.01",
        "check": "M20 6L9 17l-5-5",
        "close": "M18 6L6 18 M6 6l12 12",
        "chevronRight": "M9 18l6-6-6-6",
        "chevronLeft": "M15 18l-6-6 6-6",
        "chevronDown": "M6 9l6 6 6-6",
        "download": "M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4 M7 10l5 5 5-5 M12 15V3",
        "upload": "M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4 M17 8l-5-5-5 5 M12 3v12",
        "edit": "M12 20h9 M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4z",
        "trash": "M3 6h18 M8 6V4h8v2 M19 6l-1 14H6L5 6",
        "menu": "M3 6h18 M3 12h18 M3 18h18",
        "key": "M15.5 7.5l3 3L22 7l-3-3 M21 2l-9.6 9.6 M11.4 11.6a5.5 5.5 0 1 1-7.8 7.8 5.5 5.5 0 0 1 7.8-7.8z",
        "building": "M3 21h18 M5 21V5l7-2v18 M19 21V9l-7-2 M9 9h0.01 M9 13h0.01 M9 17h0.01",
        "refresh": "M21 12a9 9 0 1 1-2.6-6.4L21 8 M21 3v5h-5",
        "copy": "M8 8h12v12H8z M16 8V4H4v12h4",
        "alert": "M12 9v4 M12 17h0.01 M10.3 3.9L1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z",
        "info": "M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20z M12 16v-4 M12 8h0.01",
        "moon": "M21 12.8A9 9 0 1 1 11.2 3 7 7 0 0 0 21 12.8z",
        "sun": "M12 17a5 5 0 1 0 0-10 5 5 0 0 0 0 10z M12 1v2 M12 21v2 M4.2 4.2l1.4 1.4 M18.4 18.4l1.4 1.4 M1 12h2 M21 12h2 M4.2 19.8l1.4-1.4 M18.4 5.6l1.4-1.4",
        "globe": "M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20z M2 12h20 M12 2a15 15 0 0 1 0 20 15 15 0 0 1 0-20z",
        "shield": "M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z",
        "receipt": "M4 2v20l3-2 3 2 3-2 3 2 3-2 3 2V2l-3 2-3-2-3 2-3-2-3 2z M8 8h8 M8 12h8 M8 16h5",
        "arrowUp": "M12 19V5 M5 12l7-7 7 7",
        "arrowDown": "M12 5v14 M19 12l-7 7-7-7",
        "swap": "M7 16V4 M3 8l4-4 4 4 M17 8v12 M21 16l-4 4-4-4",
        "filter": "M22 3H2l8 9.5V19l4 2v-8.5z",
        "percent": "M19 5L5 19 M6.5 4a2.5 2.5 0 1 0 0 5a2.5 2.5 0 1 0 0-5z M17.5 15a2.5 2.5 0 1 0 0 5a2.5 2.5 0 1 0 0-5z",
        "eye": "M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z",
        "user": "M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2 M12 11a4 4 0 1 0 0-8 4 4 0 0 0 0 8z",
        "calendar": "M3 5h18v16H3z M3 10h18 M8 3v4 M16 3v4",
        "clock": "M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20z M12 6v6l4 2",
        "more": "M12 13a1 1 0 1 0 0-2 1 1 0 0 0 0 2z M19 13a1 1 0 1 0 0-2 1 1 0 0 0 0 2z M5 13a1 1 0 1 0 0-2 1 1 0 0 0 0 2z",
        "dot": "M12 13a1 1 0 1 0 0-2 1 1 0 0 0 0 2z"
    })

    // Rendered as an SVG image (Qt Svg) rather than QtQuick.Shapes: with Qt 6.4
    // a Shape destroyed during a StackView transition could leave its scene-graph
    // node on screen (observed in the UI tour).
    Image {
        anchors.fill: parent
        sourceSize.width: Math.ceil(root.size * 2)
        sourceSize.height: Math.ceil(root.size * 2)
        smooth: true
        mipmap: true
        source: "data:image/svg+xml;base64," + Qt.btoa(
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="rgb(' +
            Math.round(root.color.r * 255) + ',' + Math.round(root.color.g * 255) + ',' + Math.round(root.color.b * 255) +
            ')" stroke-opacity="' + root.color.a + '" stroke-width="' + root.stroke + '" stroke-linecap="round" stroke-linejoin="round"><path d="' +
            (root.paths[root.name] || root.paths["dot"]) + '"/></svg>')
    }
}
