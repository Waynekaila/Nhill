pragma Singleton
import QtQuick
import Nhill.Core

// Nhill design tokens. Colours: night blue (primary), summit gold (accent),
// ink and warm paper — see docs/brand/identite.md.
QtObject {
    readonly property bool dark: Prefs.theme === "dark" || (Prefs.theme === "system" && Prefs.systemDark)

    // ---- surfaces
    readonly property color bg:          dark ? "#0C141C" : "#F7F5F0"
    readonly property color surface:     dark ? "#121C26" : "#FFFFFF"
    readonly property color surfaceAlt:  dark ? "#17232F" : "#F2EFE8"
    readonly property color sunken:      dark ? "#09101A" : "#ECE8DF"
    readonly property color line:        dark ? "#22303D" : "#E2DED6"
    readonly property color lineStrong:  dark ? "#33465A" : "#CFC9BD"
    // Borders of fields and secondary buttons: >= 3:1 against their background (WCAG 1.4.11).
    readonly property color inputBorder: dark ? "#5D7389" : "#8A8376"
    readonly property color sidebar:     dark ? "#09101A" : "#0E2236"
    readonly property color sidebarText: "#DCE6EF"
    readonly property color sidebarMuted:"#8FA6BC"
    readonly property color sidebarActive: dark ? "#14304A" : "#1B4467"

    // ---- text
    readonly property color text:        dark ? "#E6ECF1" : "#14181F"
    readonly property color textMuted:   dark ? "#9AA8B6" : "#56606B"
    // >= 4.5:1 on bg, surface, surfaceAlt and sunken (was #8A9097 / #677686: 2.8–3.7:1).
    readonly property color textFaint:   dark ? "#8392A2" : "#62686F"

    // ---- brand & semantic
    readonly property color primary:     dark ? "#7FB4D8" : "#173F5F"
    readonly property color primaryHover:dark ? "#9CC7E4" : "#102F48"
    readonly property color textOnPrimary:   dark ? "#07121C" : "#FFFFFF"
    readonly property color primarySoft: dark ? "#15324A" : "#DEE9F2"
    readonly property color accent:      dark ? "#E0B354" : "#8E6519"
    readonly property color accentSoft:  dark ? "#3A2E14" : "#F6EBD3"
    readonly property color success:     dark ? "#6CC9A0" : "#1A6E4C"
    readonly property color successSoft: dark ? "#16352A" : "#DDF1E7"
    readonly property color warning:     dark ? "#E8B65A" : "#8A5D00"
    readonly property color warningSoft: dark ? "#3A2E14" : "#FBF0D6"
    readonly property color danger:      dark ? "#F2867D" : "#B3261E"
    readonly property color dangerSoft:  dark ? "#3E1E1C" : "#FBE4E2"
    readonly property color info:        dark ? "#7DB3E8" : "#2A5F96"
    readonly property color infoSoft:    dark ? "#182A3D" : "#E1ECF7"
    readonly property color focus:       dark ? "#9BC2DA" : "#2D6A8F"
    readonly property color overlay:     dark ? "#99000000" : "#66101820"
    // Disabled controls: explicit colours instead of a translucent primary (unreadable).
    readonly property color disabledBg:   dark ? "#1B2733" : "#E6E2DA"
    readonly property color disabledText: dark ? "#8392A2" : "#6E747B"

    // ---- typography (IBM Plex, embedded)
    readonly property string font: "IBM Plex Sans"
    readonly property string mono: "IBM Plex Mono"
    // 2.0.0: smallest sizes raised (11 -> 12, 12.5 -> 13) for legibility on 96-dpi
    // laptop screens and for long French labels.
    readonly property real fs_xs: 12
    readonly property real fs_sm: 13
    readonly property real fs_md: 14
    readonly property real fs_lg: 16
    readonly property real fs_xl: 20
    readonly property real fs_xxl: 26

    // ---- geometry
    readonly property int s1: 4
    readonly property int s2: 8
    readonly property int s3: 12
    readonly property int s4: 16
    readonly property int s5: 24
    readonly property int s6: 32
    readonly property int radius: 8
    readonly property int radiusLg: 12

    // Set by the main window: true on narrow / touch screens.
    property bool compact: false
    readonly property int controlHeight: compact ? 44 : 38   // touch targets >= 44 on phones
    readonly property int rowHeight: compact ? 56 : 44
    readonly property int iconButton: compact ? 44 : 36       // square icon buttons

    // Window sizing helpers (all three applications): never larger than the screen.
    function fitWidth(preferred, available) { return Math.max(320, Math.min(preferred, Math.round(available * 0.94))) }
    function fitHeight(preferred, available) { return Math.max(480, Math.min(preferred, Math.round(available * 0.92))) }
}
