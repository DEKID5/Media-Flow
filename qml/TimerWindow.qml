import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import MediaFlow 1.0

// Dedicated window for the manually-toggled full-screen timer mode
// (TimerBackend.fullScreenTimer). Kept separate from AudienceWindow so it
// can be placed on its own monitor (BroadcastController::timerScreenIndex),
// independent of wherever Extended Feed's program content is shown.
Window {
    id: timerRoot
    width: 1920; height: 1080; visible: false
    title: qsTr("MediaFlow — Timer")
    color: "black"
    // Pure display output, same reasoning as AudienceWindow.qml: never
    // needs OS input focus, and giving it focus anyway risks the same
    // cross-window render-thread stall on every other top-level window.
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus

    // Tiles fill the screen: width budget covers 4 tiles + colon + the
    // overtime minus slot (always reserved so the digits never resize when
    // overtime starts) minus the row's spacing; height takes most of the
    // window height, leaving room for the header and labels. 100% on the
    // slider = this maximum.
    readonly property real tileW: width * 0.2 * TimerBackend.timerScale
    readonly property real tileH: Math.min(height * 0.62, width * 0.2 * 1.9) * TimerBackend.timerScale
    readonly property color overtimeColor: "#FF1F1F"
    readonly property color normalColor: "#FFFFFF"
    readonly property color runningColor: "#00FF66"
    readonly property color warningColor: "#FFB000"
    // Traffic-light progress: green for the first half of the target
    // duration, yellow past the halfway point, red once time's actually up.
    readonly property real progressFraction: TimerBackend.targetDurationSeconds > 0
        ? TimerBackend.elapsedSeconds / TimerBackend.targetDurationSeconds : 0
    readonly property color digitColor: {
        if (TimerBackend.state === TimerBackend.Overtime) return overtimeColor
        if (TimerBackend.state === TimerBackend.Idle) return normalColor
        return progressFraction < 0.5 ? runningColor : warningColor
    }

    // "-MM:SS" (the leading "-" only present in overtime) -> four digits
    // (m1 m2 s1 s2) plus a negative flag, recomputed whenever the backend's
    // formatted string changes (once a second while running).
    readonly property string _raw: TimerBackend.displayTime
    readonly property bool negative: _raw.charAt(0) === "-"
    readonly property string _digits: negative ? _raw.substring(1).replace(":", "") : _raw.replace(":", "")
    readonly property string m1: _digits.charAt(0)
    readonly property string m2: _digits.charAt(1)
    readonly property string s1: _digits.charAt(2)
    readonly property string s2: _digits.charAt(3)

    Rectangle {
        anchors.fill: parent
        color: "#050505"

        ColumnLayout {
            anchors.centerIn: parent
            spacing: timerRoot.height * 0.035

            Label {
                text: "REMAINING TIME"
                font.pixelSize: Math.max(18, timerRoot.height * 0.026)
                font.bold: true; color: "#71717A"; font.letterSpacing: 6
                Layout.alignment: Qt.AlignHCenter
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: timerRoot.width * 0.012

                // Negative sign gets its own slot once overtime starts, so
                // the digit groups don't jump sideways when it appears.
                Label {
                    text: "−"
                    visible: timerRoot.negative
                    color: timerRoot.overtimeColor
                    font.bold: true
                    font.pixelSize: timerRoot.tileH * 0.35
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: timerRoot.tileW * 0.3
                }

                ColumnLayout {
                    spacing: timerRoot.height * 0.02
                    RowLayout {
                        spacing: timerRoot.width * 0.006
                        Layout.alignment: Qt.AlignHCenter
                        FlipDigit { digit: timerRoot.m1; tileW: timerRoot.tileW; tileH: timerRoot.tileH; color: timerRoot.digitColor }
                        FlipDigit { digit: timerRoot.m2; tileW: timerRoot.tileW; tileH: timerRoot.tileH; color: timerRoot.digitColor }
                    }
                    Label {
                        text: "MINUTES"
                        color: "#71717A"; font.bold: true; font.letterSpacing: 3
                        font.pixelSize: Math.max(12, timerRoot.tileH * 0.09)
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                Label {
                    text: ":"
                    color: "#3F3F46"
                    font.bold: true
                    font.pixelSize: timerRoot.tileH * 0.55
                    Layout.alignment: Qt.AlignVCenter
                    Layout.bottomMargin: timerRoot.height * 0.05
                }

                ColumnLayout {
                    spacing: timerRoot.height * 0.02
                    RowLayout {
                        spacing: timerRoot.width * 0.006
                        Layout.alignment: Qt.AlignHCenter
                        FlipDigit { digit: timerRoot.s1; tileW: timerRoot.tileW; tileH: timerRoot.tileH; color: timerRoot.digitColor }
                        FlipDigit { digit: timerRoot.s2; tileW: timerRoot.tileW; tileH: timerRoot.tileH; color: timerRoot.digitColor }
                    }
                    Label {
                        text: "SECONDS"
                        color: "#71717A"; font.bold: true; font.letterSpacing: 3
                        font.pixelSize: Math.max(12, timerRoot.tileH * 0.09)
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }
    }

    // Every half-tile shows a slice of "the same" full-height, vertically-
    // centered digit -- four Label instances can't literally share state,
    // so each gets identical sizing/alignment (set via these explicit
    // properties, not a component-scoped id, since inline components can
    // only nest at file scope, not inside another component's body) and is
    // positioned so that coordinate (0, fullTileTop) lines up the same way
    // in every container: y:0 in the top containers (whose own origin
    // already IS the full tile's top), y:-(halfH+seamGap) in the bottom
    // containers. That guarantees the glyph is cut at exactly its true
    // middle with no gap or overlap, regardless of font metrics, size, or
    // which digit -- unlike hand-picked pixel offsets, which broke on
    // characters shaped differently from the ones they were eyeballed
    // against (confirmed live: "5" overflowed its tile while "0"/"4"
    // looked fine).
    component DigitFace: Label {
        property real tileW: 100
        property real tileH: 140
        width: tileW; height: tileH
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        font.family: "JetBrains Mono"; font.bold: true
        // Glyph is limited by tile width; any spare tile height is filled by
        // stretching it vertically (cap height ~0.73em) so the digits use the
        // whole tile instead of floating in it.
        font.pixelSize: Math.min(tileH * 0.95, tileW * 1.5)
        transform: Scale {
            origin.y: tileH / 2
            yScale: Math.max(1, Math.min(1.35, 0.78 * tileH / (0.73 * Math.min(tileH * 0.95, tileW * 1.5))))
        }
    }

    // ── Split-flap digit ──────────────────────────────────────────────
    // Real split-flap displays: a leaf hinged at the seam falls forward
    // under gravity (accelerating - Easing.InQuad) revealing the already-
    // static new value underneath, in two staggered half-flips (top leaf
    // falls away first, then the new bottom leaf falls into place). That
    // physical asymmetry -- not a symmetric ease-out card flip -- is what
    // actually reads as "flip clock" rather than a generic transition.
    component FlipDigit: Item {
        id: digitRoot
        property string digit: "0"
        property real tileW: 100
        property real tileH: 140
        property color color: "#F5F5F5"

        property string previousDigit: "0"
        // Must be a one-time imperative assignment, not `property string
        // previousDigit: digit` -- that would stay a *live* binding until
        // the first flip's ScriptAction breaks it, so on the very first
        // digit change both properties would update in lockstep and the
        // guard below would wrongly skip that first flip's animation.
        Component.onCompleted: previousDigit = digit
        onDigitChanged: {
            if (digit === previousDigit) return
            flipAnim.restart()
        }

        implicitWidth: tileW
        implicitHeight: tileH

        readonly property real seamGap: Math.max(2, tileH * 0.012)
        readonly property real halfH: (tileH - seamGap) / 2

        // Static base: the two halves always show the CURRENT (already
        // committed) digit. The animated flap above them shows the OLD
        // digit and rotates away/into place; the base underneath is what's
        // left once the flap has passed 90° in either stage.
        Rectangle {
            id: topBase
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
            height: halfH
            radius: halfH * 0.12
            color: "#1C1C20"
            clip: true
            DigitFace { text: digitRoot.digit; color: digitRoot.color; tileW: digitRoot.tileW; tileH: digitRoot.tileH; y: 0 }
            Rectangle {
                anchors.fill: parent; radius: parent.radius
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#22FFFFFF" }
                    GradientStop { position: 1.0; color: "#00FFFFFF" }
                }
            }
        }

        Rectangle {
            id: bottomBase
            anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
            height: halfH
            radius: halfH * 0.12
            color: "#1C1C20"
            clip: true
            // Must lag on digitRoot.previousDigit, not digitRoot.digit --
            // bottomFlap only covers this half during stage 2, so if this
            // showed the new value immediately (like topBase safely can,
            // since topFlap covers it for the *entire* stage 1), stage 1
            // would show a garbled top-old/bottom-new hybrid digit for
            // 160ms. previousDigit only advances at the stage boundary
            // (see flipAnim's ScriptAction), exactly when bottomFlap starts
            // covering this with the same new value.
            DigitFace { text: digitRoot.previousDigit; color: digitRoot.color; tileW: digitRoot.tileW; tileH: digitRoot.tileH; y: -(halfH + seamGap) }
        }

        // Seam line
        Rectangle {
            anchors.centerIn: parent
            width: parent.width; height: seamGap
            color: "#050505"
        }

        // Falling top flap -- shows the OLD digit's top half, hinged at its
        // own bottom edge (the seam), rotating 0deg -> 90deg away from the
        // viewer over the first half of the sequence.
        Rectangle {
            id: topFlap
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
            height: halfH
            radius: halfH * 0.12
            color: "#232328"
            clip: true
            visible: flapAngleTop.angle > -89.9
            transformOrigin: Item.Bottom
            transform: Rotation { id: flapAngleTop; angle: 0; axis { x: 1; y: 0; z: 0 } origin.x: topFlap.width / 2; origin.y: topFlap.height }
            DigitFace { text: digitRoot.previousDigit; color: digitRoot.color; tileW: digitRoot.tileW; tileH: digitRoot.tileH; y: 0 }
            Rectangle {
                anchors.fill: parent; radius: parent.radius
                color: "black"
                opacity: Math.min(1, Math.abs(flapAngleTop.angle) / 90) * 0.55
            }
        }

        // Falling-into-place bottom flap -- shows the NEW digit's bottom
        // half, hinged at its own top edge, rotating -90deg -> 0deg over
        // the second half, covering bottomBase (still showing the old
        // value) until it lands flush.
        Rectangle {
            id: bottomFlap
            anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
            height: halfH
            radius: halfH * 0.12
            color: "#232328"
            clip: true
            visible: flapAngleBottom.angle < -0.1
            transformOrigin: Item.Top
            transform: Rotation { id: flapAngleBottom; angle: -90; axis { x: 1; y: 0; z: 0 } origin.x: bottomFlap.width / 2; origin.y: 0 }
            DigitFace { text: digitRoot.digit; color: digitRoot.color; tileW: digitRoot.tileW; tileH: digitRoot.tileH; y: -(halfH + seamGap) }
            Rectangle {
                anchors.fill: parent; radius: parent.radius
                color: "black"
                opacity: Math.min(1, Math.abs(flapAngleBottom.angle) / 90) * 0.55
            }
        }

        SequentialAnimation {
            id: flipAnim
            NumberAnimation { target: flapAngleTop; property: "angle"; from: 0; to: -90; duration: 160; easing.type: Easing.InQuad }
            ScriptAction { script: { digitRoot.previousDigit = digitRoot.digit; flapAngleTop.angle = 0; flapAngleBottom.angle = -90 } }
            NumberAnimation { target: flapAngleBottom; property: "angle"; from: -90; to: 0; duration: 160; easing.type: Easing.OutQuad }
        }
    }
}
