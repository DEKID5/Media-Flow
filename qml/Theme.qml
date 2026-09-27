import QtQuick

pragma Singleton

QtObject {
    // ── Core Background ──
    readonly property color bg: "#050505"
    readonly property color panelBg: "#121214"
    readonly property color panelBgDark: "#0A0A0A"
    readonly property color panelBorder: "#1AFFFFFF"
    readonly property color borderColor: "#1f1f23"

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

    // ── Layout ──
    readonly property int radius: 10
    readonly property int radiusLg: 12

    // ── Typography ──
    readonly property string monoFont: "JetBrains Mono"
    readonly property string sansFont: "Inter"
}
