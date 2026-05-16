import QtQuick

Item {
    id: root
    property string name: "eye"
    property color color: "white"
    property real iconSize: 14
    
    width: iconSize
    height: iconSize
    
    Canvas {
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
                ctx.beginPath(); ctx.arc(w*0.5, h*0.5, w*0.25, 0, 2*Math.PI); ctx.stroke();
                for(var i=0; i<8; i++) {
                    var angle = i * Math.PI / 4;
                    ctx.beginPath();
                    ctx.moveTo(w*0.5 + Math.cos(angle)*w*0.3, h*0.5 + Math.sin(angle)*h*0.3);
                    ctx.lineTo(w*0.5 + Math.cos(angle)*w*0.45, h*0.5 + Math.sin(angle)*h*0.45);
                    ctx.stroke();
                }
            } else if (name === "refresh") {
                ctx.beginPath(); ctx.arc(w*0.5, h*0.5, w*0.35, 0, 1.5*Math.PI); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.5, h*0.05); ctx.lineTo(w*0.85, h*0.15); ctx.lineTo(w*0.75, h*0.5); ctx.stroke();
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
            } else if (name === "screen") {
                ctx.strokeRect(w*0.1, h*0.2, w*0.8, h*0.5);
                ctx.beginPath(); ctx.moveTo(w*0.3, h*0.7); ctx.lineTo(w*0.7, h*0.7); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.5, h*0.7); ctx.lineTo(w*0.5, h*0.85); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(w*0.35, h*0.85); ctx.lineTo(w*0.65, h*0.85); ctx.stroke();
            }
        }
    }
}
