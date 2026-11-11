import QtQuick

Item {
    id: root
    property string name: "eye"
    property color color: "white"
    property real iconSize: 14
    
    width: iconSize
    height: iconSize

    onNameChanged: iconCanvas.requestPaint()
    onColorChanged: iconCanvas.requestPaint()
    onIconSizeChanged: iconCanvas.requestPaint()
    
    Canvas {
        id: iconCanvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.strokeStyle = root.color;
            ctx.fillStyle = root.color;
            ctx.lineWidth = 1.5;
            ctx.lineCap = "round";
            ctx.lineJoin = "round";
            
            var w = width;
            var h = height;
            
            if (name === "eye") {
                ctx.beginPath();
                ctx.ellipse(w*0.1, h*0.3, w*0.8, h*0.4);
                ctx.stroke();
                ctx.beginPath();
                ctx.arc(w*0.5, h*0.5, w*0.15, 0, 2*Math.PI);
                ctx.stroke();
            } else if (name === "trash") {
                ctx.strokeRect(w*0.25, h*0.3, w*0.5, h*0.6);
                ctx.beginPath(); ctx.moveTo(w*0.15, h*0.3); ctx.lineTo(w*0.85, h*0.3); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.4, h*0.15); ctx.lineTo(w*0.6, h*0.15); ctx.stroke();
            } else if (name === "folder") {
                ctx.beginPath();
                ctx.moveTo(w*0.1, h*0.2); ctx.lineTo(w*0.4, h*0.2); ctx.lineTo(w*0.5, h*0.35); ctx.lineTo(w*0.9, h*0.35);
                ctx.lineTo(w*0.9, h*0.85); ctx.lineTo(w*0.1, h*0.85); ctx.closePath();
                ctx.stroke();
            } else if (name === "chart") {
                ctx.fillRect(w*0.1, h*0.6, w*0.2, h*0.3);
                ctx.fillRect(w*0.4, h*0.3, w*0.2, h*0.6);
                ctx.fillRect(w*0.7, h*0.45, w*0.2, h*0.45);
            } else if (name === "globe") {
                ctx.beginPath(); ctx.arc(w*0.5, h*0.5, w*0.4, 0, 2*Math.PI); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.1, h*0.5); ctx.lineTo(w*0.9, h*0.5); ctx.stroke();
                ctx.beginPath(); ctx.ellipse(w*0.35, h*0.1, w*0.3, h*0.8); ctx.stroke();
            } else if (name === "speaker") {
                ctx.beginPath();
                ctx.moveTo(w*0.2, h*0.4); ctx.lineTo(w*0.4, h*0.4); ctx.lineTo(w*0.7, h*0.2); ctx.lineTo(w*0.7, h*0.8); ctx.lineTo(w*0.4, h*0.6); ctx.lineTo(w*0.2, h*0.6); ctx.closePath();
                ctx.fill();
            } else if (name === "mute") {
                // Speaker base
                ctx.beginPath();
                ctx.moveTo(w*0.2, h*0.4); ctx.lineTo(w*0.4, h*0.4); ctx.lineTo(w*0.7, h*0.2); ctx.lineTo(w*0.7, h*0.8); ctx.lineTo(w*0.4, h*0.6); ctx.lineTo(w*0.2, h*0.6); ctx.closePath();
                ctx.fill();
                // X mark
                ctx.strokeStyle = "#EF4444"; ctx.lineWidth = 2;
                ctx.beginPath(); ctx.moveTo(w*0.8, h*0.4); ctx.lineTo(w*1.0, h*0.6); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*1.0, h*0.4); ctx.lineTo(w*0.8, h*0.6); ctx.stroke();
            } else if (name === "settings") {
                // Proper 8-tooth gear using evenodd fill rule to punch a center hole
                var gCx = w*0.5, gCy = h*0.5;
                var gInner = w*0.13;   // center hub hole radius
                var gMid   = w*0.24;   // gear body (valley) radius
                var gOuter = w*0.40;   // tooth tip radius
                var gTeeth = 8;
                var gHalfTooth = Math.PI * 0.055;  // half angular width of each tooth
                var gHalfValley = Math.PI * 0.07;  // half angular width of valley

                ctx.fillRule = "evenodd";
                ctx.beginPath();
                for (var gt = 0; gt < gTeeth; gt++) {
                    var gBase = (gt / gTeeth) * 2 * Math.PI;
                    var ga0 = gBase - gHalfTooth;
                    var ga1 = gBase + gHalfTooth;
                    var ga2 = gBase + Math.PI / gTeeth - gHalfValley;
                    var ga3 = gBase + Math.PI / gTeeth + gHalfValley;
                    if (gt === 0) {
                        ctx.moveTo(gCx + gOuter * Math.cos(ga0), gCy + gOuter * Math.sin(ga0));
                    } else {
                        ctx.lineTo(gCx + gOuter * Math.cos(ga0), gCy + gOuter * Math.sin(ga0));
                    }
                    ctx.lineTo(gCx + gOuter * Math.cos(ga1), gCy + gOuter * Math.sin(ga1));
                    ctx.lineTo(gCx + gMid  * Math.cos(ga2), gCy + gMid  * Math.sin(ga2));
                    ctx.lineTo(gCx + gMid  * Math.cos(ga3), gCy + gMid  * Math.sin(ga3));
                }
                ctx.closePath();
                // Inner circle punched out via evenodd
                ctx.arc(gCx, gCy, gInner, 0, 2 * Math.PI, true);
                ctx.fill();
                ctx.fillRule = "nonzero";
            } else if (name === "refresh") {
                // Document page with folded top-right corner + circular refresh
                // arrow overlay — conveys "sync the weekly meeting schedule"
                ctx.lineWidth = 1.5;
                var dL = w*0.09, dT = h*0.07, dW = w*0.54, dH = h*0.82, dFold = w*0.16;

                // Document outline with folded corner
                ctx.beginPath();
                ctx.moveTo(dL, dT);
                ctx.lineTo(dL + dW - dFold, dT);
                ctx.lineTo(dL + dW, dT + dFold);
                ctx.lineTo(dL + dW, dT + dH);
                ctx.lineTo(dL, dT + dH);
                ctx.closePath();
                ctx.stroke();

                // Fold crease
                ctx.beginPath();
                ctx.moveTo(dL + dW - dFold, dT);
                ctx.lineTo(dL + dW - dFold, dT + dFold);
                ctx.lineTo(dL + dW,         dT + dFold);
                ctx.stroke();

                // Text lines suggesting schedule content
                ctx.lineWidth = 1.0;
                var lx0 = dL + w*0.08, lx1 = dL + dW - w*0.10;
                ctx.beginPath(); ctx.moveTo(lx0, h*0.32); ctx.lineTo(lx1 - dFold*0.5, h*0.32); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(lx0, h*0.45); ctx.lineTo(lx1, h*0.45); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(lx0, h*0.57); ctx.lineTo(lx1, h*0.57); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(lx0, h*0.69); ctx.lineTo(lx1 * 0.82, h*0.69); ctx.stroke();

                // Circular refresh arrow (bottom-right, overlapping document)
                ctx.lineWidth = 1.6;
                var rfCx = w*0.73, rfCy = h*0.74, rfR = w*0.23;
                var rfStart = -Math.PI * 0.65, rfEnd = Math.PI * 0.55;
                ctx.beginPath();
                ctx.arc(rfCx, rfCy, rfR, rfStart, rfEnd, false);
                ctx.stroke();

                // Arrowhead at arc end
                var rfTipX = rfCx + rfR * Math.cos(rfEnd);
                var rfTipY = rfCy + rfR * Math.sin(rfEnd);
                var rfTangent = rfEnd + Math.PI / 2;
                var rfPerp = rfTangent + Math.PI / 2;
                var rfLen = w*0.20, rfWidth = w*0.13;
                var rfBackX = rfTipX - rfLen * Math.cos(rfTangent);
                var rfBackY = rfTipY - rfLen * Math.sin(rfTangent);
                ctx.beginPath();
                ctx.moveTo(rfTipX, rfTipY);
                ctx.lineTo(rfBackX + rfWidth * Math.cos(rfPerp), rfBackY + rfWidth * Math.sin(rfPerp));
                ctx.lineTo(rfBackX - rfWidth * Math.cos(rfPerp), rfBackY - rfWidth * Math.sin(rfPerp));
                ctx.closePath();
                ctx.fill();
            } else if (name === "reset") {
                ctx.lineWidth = 1.7;
                ctx.beginPath();
                ctx.arc(w*0.52, h*0.52, w*0.31, Math.PI * 0.12, Math.PI * 1.72);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.32, h*0.17);
                ctx.lineTo(w*0.53, h*0.18);
                ctx.lineTo(w*0.41, h*0.36);
                ctx.stroke();
            } else if (name === "bolt") {
                ctx.beginPath();
                ctx.moveTo(w*0.6, h*0.1); ctx.lineTo(w*0.2, h*0.6); ctx.lineTo(w*0.5, h*0.6);
                ctx.lineTo(w*0.4, h*0.9); ctx.lineTo(w*0.8, h*0.4); ctx.lineTo(w*0.5, h*0.4);
                ctx.closePath();
                ctx.fill();
            } else if (name === "video") {
                ctx.strokeRect(w*0.1, h*0.3, w*0.5, h*0.4);
                ctx.beginPath(); ctx.moveTo(w*0.6, h*0.4); ctx.lineTo(w*0.9, h*0.25); ctx.lineTo(w*0.9, h*0.75); ctx.lineTo(w*0.6, h*0.6); ctx.closePath(); ctx.fill();
            } else if (name === "image") {
                ctx.strokeRect(w*0.1, h*0.2, w*0.8, h*0.6);
                ctx.beginPath(); ctx.arc(w*0.3, h*0.4, w*0.1, 0, 2*Math.PI); ctx.fill();
                ctx.beginPath(); ctx.moveTo(w*0.1, h*0.8); ctx.lineTo(w*0.4, h*0.5); ctx.lineTo(w*0.6, h*0.7); ctx.lineTo(w*0.8, h*0.4); ctx.lineTo(w*0.9, h*0.8); ctx.closePath(); ctx.fill();
            } else if (name === "music") {
                ctx.beginPath(); ctx.moveTo(w*0.3, h*0.8); ctx.lineTo(w*0.3, h*0.2); ctx.lineTo(w*0.8, h*0.1); ctx.lineTo(w*0.8, h*0.7); ctx.stroke();
                ctx.beginPath(); ctx.arc(w*0.2, h*0.8, w*0.15, 0, 2*Math.PI); ctx.fill();
                ctx.beginPath(); ctx.arc(w*0.7, h*0.7, w*0.15, 0, 2*Math.PI); ctx.fill();
            } else if (name === "chevron-down") {
                ctx.beginPath(); ctx.moveTo(w*0.2, h*0.4); ctx.lineTo(w*0.5, h*0.7); ctx.lineTo(w*0.8, h*0.4); ctx.stroke();
            } else if (name === "chevron-up") {
                ctx.beginPath(); ctx.moveTo(w*0.2, h*0.6); ctx.lineTo(w*0.5, h*0.3); ctx.lineTo(w*0.8, h*0.6); ctx.stroke();
            } else if (name === "skip-back") {
                ctx.beginPath(); ctx.moveTo(w*0.75, h*0.2); ctx.lineTo(w*0.35, h*0.5); ctx.lineTo(w*0.75, h*0.8); ctx.closePath(); ctx.fill();
                ctx.beginPath(); ctx.moveTo(w*0.25, h*0.2); ctx.lineTo(w*0.25, h*0.8); ctx.stroke();
            } else if (name === "skip-forward") {
                ctx.beginPath(); ctx.moveTo(w*0.25, h*0.2); ctx.lineTo(w*0.65, h*0.5); ctx.lineTo(w*0.25, h*0.8); ctx.closePath(); ctx.fill();
                ctx.beginPath(); ctx.moveTo(w*0.75, h*0.2); ctx.lineTo(w*0.75, h*0.8); ctx.stroke();
            } else if (name === "check") {
                ctx.beginPath(); ctx.moveTo(w*0.2, h*0.5); ctx.lineTo(w*0.45, h*0.75); ctx.lineTo(w*0.85, h*0.25); ctx.stroke();
            } else if (name === "broadcast" || name === "live") {
                // Central antenna mast
                ctx.beginPath();
                ctx.moveTo(w*0.5, h*0.42); ctx.lineTo(w*0.5, h*0.86); ctx.stroke();
                // Tripod support legs
                ctx.beginPath();
                ctx.moveTo(w*0.30, h*0.86); ctx.lineTo(w*0.5, h*0.62); ctx.lineTo(w*0.70, h*0.86); ctx.stroke();
                // Transmitter beacon
                ctx.beginPath();
                ctx.arc(w*0.5, h*0.38, w*0.09, 0, 2*Math.PI); ctx.fill();
                // Inner radiating waves (left & right)
                ctx.beginPath();
                ctx.arc(w*0.5, h*0.38, w*0.22, -Math.PI*0.35, Math.PI*0.35); ctx.stroke();
                ctx.beginPath();
                ctx.arc(w*0.5, h*0.38, w*0.22, Math.PI*0.65, Math.PI*1.35); ctx.stroke();
                // Outer radiating waves (left & right)
                ctx.beginPath();
                ctx.arc(w*0.5, h*0.38, w*0.38, -Math.PI*0.35, Math.PI*0.35); ctx.stroke();
                ctx.beginPath();
                ctx.arc(w*0.5, h*0.38, w*0.38, Math.PI*0.65, Math.PI*1.35); ctx.stroke();
            } else if (name === "zoom") {
                // Zoom camera body
                var zx = w*0.08, zy = h*0.24, zw = w*0.54, zh = h*0.52, zr = 2;
                ctx.beginPath();
                ctx.moveTo(zx + zr, zy);
                ctx.lineTo(zx + zw - zr, zy);
                ctx.arcTo(zx + zw, zy, zx + zw, zy + zr, zr);
                ctx.lineTo(zx + zw, zy + zh - zr);
                ctx.arcTo(zx + zw, zy + zh, zx + zw - zr, zy + zh, zr);
                ctx.lineTo(zx + zr, zy + zh);
                ctx.arcTo(zx, zy + zh, zx, zy + zh - zr, zr);
                ctx.lineTo(zx, zy + zr);
                ctx.arcTo(zx, zy, zx + zr, zy, zr);
                ctx.closePath();
                ctx.stroke();
                // Lens cone
                ctx.beginPath();
                ctx.moveTo(zx + zw, h*0.38);
                ctx.lineTo(w*0.92, h*0.24);
                ctx.lineTo(w*0.92, h*0.76);
                ctx.lineTo(zx + zw, h*0.62);
                ctx.closePath();
                ctx.fill();
                // Distinctive Zoom 'Z' mark inside camera
                ctx.lineWidth = 1.8;
                ctx.beginPath();
                ctx.moveTo(w*0.20, h*0.38);
                ctx.lineTo(w*0.44, h*0.38);
                ctx.lineTo(w*0.22, h*0.62);
                ctx.lineTo(w*0.46, h*0.62);
                ctx.stroke();
            } else if (name === "webcam") {
                var wcamX = w*0.5, wcamY = h*0.36, wcamR = w*0.26;
                // Outer lens circle
                ctx.beginPath();
                ctx.arc(wcamX, wcamY, wcamR, 0, 2*Math.PI);
                ctx.stroke();
                // Inner optical lens ring
                ctx.beginPath();
                ctx.arc(wcamX, wcamY, wcamR*0.52, 0, 2*Math.PI);
                ctx.stroke();
                // Center camera sensor pupil
                ctx.beginPath();
                ctx.arc(wcamX, wcamY, wcamR*0.22, 0, 2*Math.PI);
                ctx.fill();
                // Stand neck
                ctx.beginPath();
                ctx.moveTo(wcamX, wcamY + wcamR);
                ctx.lineTo(wcamX, h*0.74);
                ctx.stroke();
                // Desk base mount
                ctx.beginPath();
                ctx.moveTo(w*0.22, h*0.86);
                ctx.lineTo(w*0.78, h*0.86);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.36, h*0.74);
                ctx.lineTo(w*0.22, h*0.86);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.64, h*0.74);
                ctx.lineTo(w*0.78, h*0.86);
                ctx.stroke();
            } else if (name === "webcam-off") {
                var wcamOffX = w*0.5, wcamOffY = h*0.36, wcamOffR = w*0.26;
                // Camera outline
                ctx.beginPath();
                ctx.arc(wcamOffX, wcamOffY, wcamOffR, 0, 2*Math.PI);
                ctx.stroke();
                ctx.beginPath();
                ctx.arc(wcamOffX, wcamOffY, wcamOffR*0.52, 0, 2*Math.PI);
                ctx.stroke();
                ctx.beginPath();
                ctx.arc(wcamOffX, wcamOffY, wcamOffR*0.22, 0, 2*Math.PI);
                ctx.fill();
                // Stand neck & base
                ctx.beginPath();
                ctx.moveTo(wcamOffX, wcamOffY + wcamOffR);
                ctx.lineTo(wcamOffX, h*0.74);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.22, h*0.86);
                ctx.lineTo(w*0.78, h*0.86);
                ctx.stroke();
                // Prominent red diagonal strike-through slash
                ctx.strokeStyle = "#EF4444";
                ctx.lineWidth = 2.0;
                ctx.beginPath();
                ctx.moveTo(w*0.14, h*0.14);
                ctx.lineTo(w*0.86, h*0.86);
                ctx.stroke();
            } else if (name === "projector" || name === "audience") {
                // Wide presentation display
                ctx.strokeRect(w*0.10, h*0.18, w*0.80, h*0.48);
                // Stand neck
                ctx.beginPath();
                ctx.moveTo(w*0.5, h*0.66);
                ctx.lineTo(w*0.5, h*0.84);
                ctx.stroke();
                // Stand base
                ctx.beginPath();
                ctx.moveTo(w*0.28, h*0.84);
                ctx.lineTo(w*0.72, h*0.84);
                ctx.stroke();
                // Projection play triangle inside
                ctx.beginPath();
                ctx.moveTo(w*0.42, h*0.32);
                ctx.lineTo(w*0.60, h*0.42);
                ctx.lineTo(w*0.42, h*0.52);
                ctx.closePath();
                ctx.fill();
            } else if (name === "screen") {
                ctx.strokeRect(w*0.1, h*0.2, w*0.8, h*0.5);
                ctx.beginPath(); ctx.moveTo(w*0.3, h*0.7); ctx.lineTo(w*0.7, h*0.7); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.5, h*0.7); ctx.lineTo(w*0.5, h*0.85); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.35, h*0.85); ctx.lineTo(w*0.65, h*0.85); ctx.stroke();
            } else if (name === "shuffle") {
                ctx.beginPath(); ctx.moveTo(w*0.1, h*0.25); ctx.lineTo(w*0.35, h*0.25); ctx.lineTo(w*0.9, h*0.8); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.68, h*0.8); ctx.lineTo(w*0.9, h*0.8); ctx.lineTo(w*0.9, h*0.58); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.1, h*0.75); ctx.lineTo(w*0.35, h*0.75); ctx.lineTo(w*0.55, h*0.55); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.68, h*0.25); ctx.lineTo(w*0.9, h*0.25); ctx.lineTo(w*0.9, h*0.47); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.58, h*0.42); ctx.lineTo(w*0.9, h*0.25); ctx.stroke();
            } else if (name === "pin") {
                ctx.beginPath(); ctx.arc(w*0.5, h*0.35, w*0.28, 0, 2*Math.PI); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.5, h*0.6); ctx.lineTo(w*0.5, h*0.92); ctx.stroke();
                ctx.beginPath(); ctx.arc(w*0.5, h*0.35, w*0.1, 0, 2*Math.PI); ctx.fill();
            } else if (name === "plus") {
                ctx.beginPath(); ctx.moveTo(w*0.5, h*0.15); ctx.lineTo(w*0.5, h*0.85); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.15, h*0.5); ctx.lineTo(w*0.85, h*0.5); ctx.stroke();
            } else if (name === "close") {
                ctx.beginPath(); ctx.moveTo(w*0.2, h*0.2); ctx.lineTo(w*0.8, h*0.8); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.8, h*0.2); ctx.lineTo(w*0.2, h*0.8); ctx.stroke();
            } else if (name === "expand") {
                // Four corner brackets pointing outward -- "expand to full screen".
                ctx.beginPath(); ctx.moveTo(w*0.35, h*0.15); ctx.lineTo(w*0.15, h*0.15); ctx.lineTo(w*0.15, h*0.35); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.65, h*0.15); ctx.lineTo(w*0.85, h*0.15); ctx.lineTo(w*0.85, h*0.35); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.15, h*0.65); ctx.lineTo(w*0.15, h*0.85); ctx.lineTo(w*0.35, h*0.85); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.85, h*0.65); ctx.lineTo(w*0.85, h*0.85); ctx.lineTo(w*0.65, h*0.85); ctx.stroke();
            } else if (name === "play") {
                ctx.beginPath();
                ctx.moveTo(w*0.30, h*0.20);
                ctx.lineTo(w*0.80, h*0.50);
                ctx.lineTo(w*0.30, h*0.80);
                ctx.closePath();
                ctx.fill();
            } else if (name === "pause") {
                var pw = w * 0.16;
                ctx.fillRect(w * 0.28, h * 0.22, pw, h * 0.56);
                ctx.fillRect(w * 0.56, h * 0.22, pw, h * 0.56);
            } else if (name === "stop") {
                var sw = w * 0.52, sh = h * 0.52;
                var sx = (w - sw) * 0.5, sy = (h - sh) * 0.5, sr = 2;
                ctx.beginPath();
                ctx.moveTo(sx + sr, sy);
                ctx.lineTo(sx + sw - sr, sy);
                ctx.arcTo(sx + sw, sy, sx + sw, sy + sr, sr);
                ctx.lineTo(sx + sw, sy + sh - sr);
                ctx.arcTo(sx + sw, sy + sh, sx + sw - sr, sy + sh, sr);
                ctx.lineTo(sx + sr, sy + sh);
                ctx.arcTo(sx, sy + sh, sx, sy + sh - sr, sr);
                ctx.lineTo(sx, sy + sr);
                ctx.arcTo(sx, sy, sx + sr, sy, sr);
                ctx.closePath();
                ctx.fill();
            } else if (name === "fade") {
                // Speaker cone
                ctx.beginPath();
                ctx.moveTo(w*0.12, h*0.38); ctx.lineTo(w*0.28, h*0.38); ctx.lineTo(w*0.48, h*0.22); ctx.lineTo(w*0.48, h*0.78); ctx.lineTo(w*0.28, h*0.62); ctx.lineTo(w*0.12, h*0.62);
                ctx.closePath();
                ctx.fill();
                // Downward fade ramp / arrow
                ctx.lineWidth = 1.6;
                ctx.beginPath();
                ctx.moveTo(w*0.60, h*0.32);
                ctx.lineTo(w*0.86, h*0.68);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.72, h*0.68);
                ctx.lineTo(w*0.86, h*0.68);
                ctx.lineTo(w*0.86, h*0.54);
                ctx.stroke();
            } else if (name === "repeat") {
                ctx.lineWidth = 1.6;
                ctx.beginPath();
                ctx.moveTo(w*0.30, h*0.30);
                ctx.lineTo(w*0.72, h*0.30);
                ctx.arcTo(w*0.86, h*0.30, w*0.86, h*0.44, w*0.14);
                ctx.lineTo(w*0.86, h*0.50);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.64, h*0.18);
                ctx.lineTo(w*0.76, h*0.30);
                ctx.lineTo(w*0.64, h*0.42);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.70, h*0.70);
                ctx.lineTo(w*0.28, h*0.70);
                ctx.arcTo(w*0.14, h*0.70, w*0.14, h*0.56, w*0.14);
                ctx.lineTo(w*0.14, h*0.50);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.36, h*0.58);
                ctx.lineTo(w*0.24, h*0.70);
                ctx.lineTo(w*0.36, h*0.82);
                ctx.stroke();
            } else if (name === "repeat-1") {
                ctx.lineWidth = 1.6;
                ctx.beginPath();
                ctx.moveTo(w*0.30, h*0.30);
                ctx.lineTo(w*0.72, h*0.30);
                ctx.arcTo(w*0.86, h*0.30, w*0.86, h*0.44, w*0.14);
                ctx.lineTo(w*0.86, h*0.50);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.64, h*0.18);
                ctx.lineTo(w*0.76, h*0.30);
                ctx.lineTo(w*0.64, h*0.42);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.70, h*0.70);
                ctx.lineTo(w*0.28, h*0.70);
                ctx.arcTo(w*0.14, h*0.70, w*0.14, h*0.56, w*0.14);
                ctx.lineTo(w*0.14, h*0.50);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w*0.36, h*0.58);
                ctx.lineTo(w*0.24, h*0.70);
                ctx.lineTo(w*0.36, h*0.82);
                ctx.stroke();
                ctx.font = "bold " + Math.round(w * 0.40) + "px sans-serif";
                ctx.textAlign = "center";
                ctx.textBaseline = "middle";
                ctx.fillText("1", w * 0.50, h * 0.51);
            } else if (name === "search") {
                ctx.lineWidth = 1.6;
                ctx.beginPath();
                ctx.arc(w * 0.44, h * 0.44, w * 0.27, 0, 2 * Math.PI);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w * 0.64, h * 0.64);
                ctx.lineTo(w * 0.86, h * 0.86);
                ctx.stroke();
            } else if (name === "book") {
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                ctx.moveTo(w * 0.50, h * 0.26);
                ctx.lineTo(w * 0.14, h * 0.18);
                ctx.lineTo(w * 0.14, h * 0.78);
                ctx.lineTo(w * 0.50, h * 0.86);
                ctx.lineTo(w * 0.86, h * 0.78);
                ctx.lineTo(w * 0.86, h * 0.18);
                ctx.closePath();
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(w * 0.50, h * 0.26);
                ctx.lineTo(w * 0.50, h * 0.86);
                ctx.stroke();
            }
        }
    }
}
