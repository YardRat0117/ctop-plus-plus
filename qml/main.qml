import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    title: "ctop++"
    color: "#1e1e1e"

    TabBar {
        id: tabBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 1

        TabButton { text: "System Stats" }
        TabButton { text: "Network Traffic" }
    }

    StackLayout {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        currentIndex: tabBar.currentIndex

        SystemStatsPage {}
        NetworkTrafficPage {}
    }
}
