import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    // Helper: format bytes into human-readable string
    function formatBytes(b) {
        if (b >= 1073741824) return (b / 1073741824).toFixed(1) + " GB"
        if (b >= 1048576) return (b / 1048576).toFixed(1) + " MB"
        if (b >= 1024) return (b / 1024).toFixed(1) + " KB"
        return b + " B"
    }

    // Helper: protocol color
    function protoColor(proto) {
        if (proto === "TCP")  return "#4D9DE0"
        if (proto === "UDP")  return "#4DE0A0"
        if (proto === "ICMP") return "#E0D04D"
        return "#cccccc"
    }

    // Draw a line chart on a canvas
    function drawLineChart(canvas, dlData, ulData, label) {
        var ctx = canvas.getContext("2d")
        var w = canvas.width
        var h = canvas.height
        var pad = 8
        var plotW = w - 2 * pad
        var plotH = h - 2 * pad

        ctx.clearRect(0, 0, w, h)

        // Find max value for Y scaling (avoid division by zero)
        var maxVal = 1
        for (var i = 0; i < dlData.length; i++) if (dlData[i] > maxVal) maxVal = dlData[i]
        for (var j = 0; j < ulData.length; j++) if (ulData[j] > maxVal) maxVal = ulData[j]
        maxVal = Math.ceil(maxVal * 1.2)

        // Grid lines
        ctx.strokeStyle = "#333333"
        ctx.lineWidth = 1
        for (var g = 1; g <= 4; g++) {
            var gy = pad + plotH * (1 - g / 4)
            ctx.beginPath()
            ctx.moveTo(pad, gy)
            ctx.lineTo(w - pad, gy)
            ctx.stroke()
        }

        // Y-axis labels
        ctx.fillStyle = "#666666"
        ctx.font = "10px sans-serif"
        ctx.textAlign = "right"
        for (var l = 0; l <= 4; l++) {
            var val = (maxVal * l / 4)
            ctx.fillText(val.toFixed(0), pad - 4, pad + plotH * (1 - l / 4) + 3)
        }

        // Draw a data line
        function drawDataLine(data, color) {
            ctx.strokeStyle = color
            ctx.lineWidth = 2
            ctx.beginPath()
            var n = data.length
            for (var k = 0; k < n; k++) {
                var x = pad + (k / Math.max(n - 1, 1)) * plotW
                var y = pad + plotH * (1 - data[k] / maxVal)
                if (k === 0) ctx.moveTo(x, y)
                else ctx.lineTo(x, y)
            }
            ctx.stroke()
        }

        drawDataLine(dlData, "#4D9DE0")
        drawDataLine(ulData, "#4DE0A0")
    }

    // Draw a pie chart on a canvas
    function drawPieChart(canvas, tcpPct, udpPct, icmpPct) {
        var ctx = canvas.getContext("2d")
        var w = canvas.width
        var h = canvas.height
        var cx = w / 2
        var cy = h / 2
        var r = Math.min(w, h) * 0.38

        ctx.clearRect(0, 0, w, h)

        var total = tcpPct + udpPct + icmpPct
        if (total <= 0) {
            ctx.fillStyle = "#555555"
            ctx.font = "12px sans-serif"
            ctx.textAlign = "center"
            ctx.fillText("No data", cx, cy + 4)
            return
        }

        var slices = [ { pct: tcpPct, color: "#4D9DE0", label: "TCP" },
                       { pct: udpPct, color: "#4DE0A0", label: "UDP" },
                       { pct: icmpPct, color: "#E0D04D", label: "ICMP" } ]

        var startAngle = -Math.PI / 2
        for (var i = 0; i < slices.length; i++) {
            if (slices[i].pct <= 0) continue
            var sweepAngle = (slices[i].pct / 100) * 2 * Math.PI

            ctx.beginPath()
            ctx.moveTo(cx, cy)
            ctx.arc(cx, cy, r, startAngle, startAngle + sweepAngle)
            ctx.closePath()
            ctx.fillStyle = slices[i].color
            ctx.fill()
            ctx.strokeStyle = "#252525"
            ctx.lineWidth = 1
            ctx.stroke()

            startAngle += sweepAngle
        }
    }

    Connections {
        target: netVM
        function onDataChanged() {
            throughputCanvas.requestPaint()
            pieCanvas.requestPaint()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // ==================== Status bar ====================
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            Label {
                text: "Interface:"
                color: "#aaaaaa"
                font.pixelSize: 13
            }
            Label {
                text: ifaceName
                color: "#66ccff"
                font.pixelSize: 13
            }

            Rectangle {
                width: 10; height: 10; radius: 5
                color: netInitDone ? "#33cc66" : "#ff6666"
                Layout.alignment: Qt.AlignVCenter
            }
            Label {
                text: netInitDone ? "Running" : "Offline"
                color: netInitDone ? "#33cc66" : "#ff6666"
                font.pixelSize: 13
            }

            Item { Layout.fillWidth: true }

            Label {
                text: netVM.droppedCount > 0 ? "Dropped: " + netVM.droppedCount : ""
                color: "#ff9900"
                font.pixelSize: 13
                visible: netVM.droppedCount > 0
            }
        }

        // ==================== Packet real-time table ====================
        Label {
            text: "Recent Packets"
            color: "#cccccc"
            font.pixelSize: 14
            font.bold: true
        }

        Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            padding: 2
            background: Rectangle { color: "#1a1a1a"; radius: 4 }

            ListView {
                id: packetList
                anchors.fill: parent
                clip: true
                model: netVM.recentPackets
                spacing: 1

                header: Rectangle {
                    width: parent.width; height: 26; color: "#2a2a2a"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 8
                        Label { text: "Time"; color: "#aaaaaa"; font.pixelSize: 12; Layout.preferredWidth: 90 }
                        Label { text: "Source"; color: "#aaaaaa"; font.pixelSize: 12; Layout.fillWidth: true }
                        Label { text: "Dest"; color: "#aaaaaa"; font.pixelSize: 12; Layout.fillWidth: true }
                        Label { text: "Proto"; color: "#aaaaaa"; font.pixelSize: 12; Layout.preferredWidth: 50 }
                        Label { text: "Length"; color: "#aaaaaa"; font.pixelSize: 12; Layout.preferredWidth: 60 }
                    }
                    Label { anchors.right: parent.right; anchors.rightMargin: 8; text: packetList.count; color: "#666"; font.pixelSize: 11 }
                }

                delegate: Rectangle {
                    width: packetList.width; height: 22; color: index % 2 === 0 ? "#1e1e1e" : "#222222"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 8
                        Label { text: modelData.time; color: "#cccccc"; font.pixelSize: 12; Layout.preferredWidth: 90 }
                        Label { text: modelData.src; color: protoColor(modelData.protocol); font.pixelSize: 12; Layout.fillWidth: true }
                        Label { text: modelData.dst; color: protoColor(modelData.protocol); font.pixelSize: 12; Layout.fillWidth: true }
                        Label { text: modelData.protocol; color: protoColor(modelData.protocol); font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 50 }
                        Label { text: modelData.length; color: "#cccccc"; font.pixelSize: 12; Layout.preferredWidth: 60 }
                    }
                }
                ScrollIndicator.vertical: ScrollIndicator { }
            }
        }

        // ==================== Throughput chart ====================
        Label {
            text: "Throughput (last 60 s)"
            color: "#cccccc"
            font.pixelSize: 14
            font.bold: true
        }

        Canvas {
            id: throughputCanvas
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            onPaint: drawLineChart(throughputCanvas, netVM.downloadHistory, netVM.uploadHistory, "KB/s")
        }

        // ==================== Bottom row: Top IP + Protocol pie ====================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // --- Top IP table ---
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label {
                    text: "Top Talkers"
                    color: "#cccccc"
                    font.pixelSize: 14
                    font.bold: true
                }
                Frame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    padding: 2
                    background: Rectangle { color: "#1a1a1a"; radius: 4 }

                    ListView {
                        id: topIpList
                        anchors.fill: parent
                        clip: true
                        model: netVM.topTalkers
                        spacing: 1

                        header: Rectangle {
                            width: parent.width; height: 24; color: "#2a2a2a"
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8
                                Label { text: "#"; color: "#aaaaaa"; font.pixelSize: 12; Layout.preferredWidth: 24 }
                                Label { text: "IP"; color: "#aaaaaa"; font.pixelSize: 12; Layout.fillWidth: true }
                                Label { text: "Bytes"; color: "#aaaaaa"; font.pixelSize: 12; Layout.preferredWidth: 90 }
                            }
                        }

                        delegate: Rectangle {
                            width: topIpList.width; height: 22; color: index % 2 === 0 ? "#1e1e1e" : "#222222"
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8
                                Label { text: index + 1; color: "#aaaaaa"; font.pixelSize: 12; Layout.preferredWidth: 24 }
                                Label { text: modelData.ip; color: "#4D9DE0"; font.pixelSize: 12; Layout.fillWidth: true }
                                Label { text: formatBytes(modelData.bytes); color: "#cccccc"; font.pixelSize: 12; Layout.preferredWidth: 90 }
                            }
                        }
                        ScrollIndicator.vertical: ScrollIndicator { }
                    }
                }
            }

            // --- Protocol pie chart ---
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label {
                    text: "Protocol Distribution"
                    color: "#cccccc"
                    font.pixelSize: 14
                    font.bold: true
                }
                Canvas {
                    id: pieCanvas
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onPaint: drawPieChart(pieCanvas, netVM.tcpPct, netVM.udpPct, netVM.icmpPct)
                }
            }
        }

        // ==================== Bottom summary ====================
        Label {
            Layout.fillWidth: true
            text: "Packets/s: " + netVM.packetCount
                  + "  |  Download: " + netVM.downloadKbps.toFixed(1) + " KB/s"
                  + "  |  Upload: " + netVM.uploadKbps.toFixed(1) + " KB/s"
                  + "  |  TCP: " + netVM.tcpPct.toFixed(0) + "%"
                  + "  UDP: " + netVM.udpPct.toFixed(0) + "%"
                  + "  ICMP: " + netVM.icmpPct.toFixed(0) + "%"
            color: "#888888"
            font.pixelSize: 12
        }
    }
}
