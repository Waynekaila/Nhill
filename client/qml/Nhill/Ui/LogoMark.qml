import QtQuick

// Nhill logo. variant: "full" | "reverse" | "mark" | "icon"; app: "" | "business" | "console" | "pos"
// (app adds the product name next to the wordmark; the icon uses the app colour).
Image {
    property string variant: "full"
    property string app: ""
    readonly property string suffix: app ? "-" + app : ""
    source: variant === "reverse" ? "qrc:/brand/nhill-logo" + suffix + "-reverse.svg"
          : variant === "mark" ? "qrc:/brand/nhill-mark.svg"
          : variant === "icon" ? "qrc:/brand/nhill-icon-" + (app || "business") + ".svg"
          : (Theme.dark ? "qrc:/brand/nhill-logo" + suffix + "-reverse.svg" : "qrc:/brand/nhill-logo" + suffix + ".svg")
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true
    sourceSize.height: height * 2
    Accessible.name: app ? "Nhill " + (app === "pos" ? "POS" : app.charAt(0).toUpperCase() + app.slice(1)) : "Nhill"
}
