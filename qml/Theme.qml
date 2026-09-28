import QtQuick

pragma Singleton

QtObject {
    // ── Core Background ──
    readonly property color bg: "#050505"
    readonly property color panelBg: "#121214"
    readonly property color panelBgDark: "#0A0A0A"
    readonly property color panelBorder: "#1AFFFFFF"
    readonly property color panelBorderStrong: "#33FFFFFF"
    readonly property color borderColor: "#1f1f23"
    readonly property color surfaceHover: "#1AFFFFFF"
    readonly property color surfaceRaised: "#0DFFFFFF"

    // ── Accent Colors ──
    readonly property color accentBlue: "#3B82F6"
    readonly property color accentEmerald: "#10B981"
    readonly property color accentRed: "#EF4444"
    readonly property color accentAmber: "#F59E0B"

    // ── Text Colors ──
    readonly property color textPrimary: "#FFFFFF"
    readonly property color textSecondary: "#A1A1AA"
    readonly property color textMuted: "#4DA1A1AA"
    readonly property color textDim: "#6b7280"
    readonly property color textFaint: "#4b5563"

    // ── Layout / Radii ──
    readonly property int radiusSm: 6
    readonly property int radius: 10
    readonly property int radiusLg: 12
    readonly property int radiusXl: 16

    // ── Spacing scale ──
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 20
    readonly property int space6: 24

    // ── Type scale ──
    // Deliberately not smaller than 10px anywhere in the redesign — the
    // previous UI used 6-8px labels for real information (not fine print),
    // which is not legible on an operator's monitor across a room.
    readonly property int textXs: 10
    readonly property int textSm: 11
    readonly property int textMd: 13
    readonly property int textLg: 15
    readonly property int textXl: 20
    readonly property int textNumeral: 40 // meeting timer digits

    // ── Motion ──
    readonly property int durationFast: 120
    readonly property int durationBase: 200
    readonly property int durationSlow: 400

    // ── Typography ──
    readonly property string monoFont: "JetBrains Mono"
    readonly property string sansFont: "Inter"

    // ── Meeting-type identity ──
    // Midweek and Weekend meetings previously shared a single accent
    // (accentBlue) everywhere, making it hard to tell at a glance which
    // meeting's content you're looking at. Weekend reuses accentAmber
    // (already defined above, previously unused anywhere in the app) rather
    // than introducing a new color.
    function meetingAccent(meetingType) {
        return meetingType === "weekend" ? accentAmber : accentBlue
    }
}
