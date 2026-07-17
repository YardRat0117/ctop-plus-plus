import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    function drawLineChart(canvas, data, color) {
        var ctx = canvas.getContext("2d")
        var w = canvas.width
        var h = canvas.height
        var pad = 8
        var plotW = w - 2 * pad
        var plotH = h - 2 * pad

        ctx.clearRect(0, 0, w, h)

        if (data.length < 2) {
            ctx.fillStyle = "#555555"
            ctx.font = "12px sans-serif"
            ctx.textAlign = "center"
            ctx.fillText("Waiting for data...", w / 2, h / 2)
            return
        }

        // Grid lines
        ctx.strokeStyle = "#333333"
        ctx.lineWidth = 1
        for (var g = 0; g <= 4; g++) {
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
            ctx.fillText((l * 25) + "%", pad - 4, pad + plotH * (1 - l / 4) + 3)
        }

        // Data line
        ctx.strokeStyle = color
        ctx.lineWidth = 2
        ctx.beginPath()
        var n = data.length
        for (var i = 0; i < n; i++) {
            var x = pad + (i / Math.max(n - 1, 1)) * plotW
            var y = pad + plotH * (1 - Math.min(data[i], 100) / 100)
            if (i === 0) ctx.moveTo(x, y)
            else ctx.lineTo(x, y)
        }
        ctx.stroke()
    }

    Connections {
        target: sysVM
        function onDataChanged() {
            cpuCanvas.requestPaint()
            memCanvas.requestPaint()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // ==================== Status bar ====================
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "System Monitor"
                color: "#cccccc"
                font.pixelSize: 16
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 12; height: 12; radius: 6
                color: "#33cc66"
            }
            Label {
                text: "Collecting"
                color: "#33cc66"
                font.pixelSize: 13
            }
        }

        // ==================== CPU ====================
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: "CPU Usage"
                color: "#cccccc"
                font.pixelSize: 14
                Layout.preferredWidth: 100
            }
            ProgressBar {
                id: cpuBar
                Layout.fillWidth: true
                from: 0; to: 100
                value: sysVM.cpuTotalPct
                contentItem: Item {
                    Rectangle {
                        height: parent.height
                        width: parent.width * (cpuBar.value - cpuBar.from) / (cpuBar.to - cpuBar.from)
                        radius: 4
                        color: "#4D9DE0"
                    }
                }
            }
            Label {
                text: sysVM.cpuTotalPct.toFixed(1) + "%"
                color: "#cccccc"
                font.pixelSize: 13
                Layout.preferredWidth: 60
                horizontalAlignment: Text.AlignRight
            }
        }

        // ==================== Memory ====================
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: "Memory  "
                color: "#cccccc"
                font.pixelSize: 14
                Layout.preferredWidth: 100
            }
            ProgressBar {
                id: memBar
                Layout.fillWidth: true
                from: 0; to: 100
                value: sysVM.memUsedPct
                contentItem: Item {
                    Rectangle {
                        height: parent.height
                        width: parent.width * (memBar.value - memBar.from) / (memBar.to - memBar.from)
                        radius: 4
                        color: "#4DE0A0"
                    }
                }
            }
            Label {
                text: sysVM.memUsedPct.toFixed(1) + "% | "
                      + sysVM.memUsedMb + " MB / " + sysVM.memTotalMb + " MB"
                color: "#cccccc"
                font.pixelSize: 13
                Layout.preferredWidth: 260
                horizontalAlignment: Text.AlignRight
            }
        }

        // ==================== Disk I/O ====================
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: "Disk I/O"
                color: "#cccccc"
                font.pixelSize: 14
                Layout.preferredWidth: 100
            }
            Label {
                text: "R: " + sysVM.diskReadMbps.toFixed(1) + " MB/s  W: " + sysVM.diskWriteMbps.toFixed(1) + " MB/s"
                color: "#aaaaaa"
                font.pixelSize: 13
            }
        }

        // ==================== Network throughput (from /proc) ====================
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: "Network "
                color: "#cccccc"
                font.pixelSize: 14
                Layout.preferredWidth: 100
            }
            Label {
                text: "RX: " + sysVM.netRxMbps.toFixed(1) + " MB/s  TX: " + sysVM.netTxMbps.toFixed(1) + " MB/s"
                color: "#aaaaaa"
                font.pixelSize: 13
            }
        }

        // ==================== History chart ====================
        Label {
            text: "Resource History (last 60 s)"
            color: "#cccccc"
            font.pixelSize: 14
            font.bold: true
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160

            // CPU line chart
            Canvas {
                id: cpuCanvas
                anchors.fill: parent
                anchors.rightMargin: parent.width * 0.5
                onPaint: drawLineChart(cpuCanvas, sysVM.cpuHistory, "#4D9DE0")
            }

            // Memory line chart (overlaid on right half)
            Canvas {
                id: memCanvas
                anchors.fill: parent
                anchors.leftMargin: parent.width * 0.5
                onPaint: drawLineChart(memCanvas, sysVM.memHistory, "#4DE0A0")
            }

            // Legend overlay
            RowLayout {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 4
                spacing: 12
                RowLayout {
                    spacing: 4
                    Rectangle { width: 10; height: 4; radius: 2; color: "#4D9DE0" }
                    Label { text: "CPU"; color: "#888888"; font.pixelSize: 11 }
                }
                RowLayout {
                    spacing: 4
                    Rectangle { width: 10; height: 4; radius: 2; color: "#4DE0A0" }
                    Label { text: "Memory"; color: "#888888"; font.pixelSize: 11 }
                }
            }
        }
    }
}
