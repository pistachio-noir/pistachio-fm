import QtQuick

// The window's foot: item count, then the selection or the free space, and the sort chip. flash()
// puts a problem there in terracotta for a few seconds, note() a plain message ("Undid rename");
// while a paste takes a while, its progress shows instead of the count.
Item {
    id: root

    required property FolderModel folder
    property string message
    property bool problem

    signal sortRequested(point at) // `at` in this item's coordinates, the chip's top-left

    function flash(text: string) {
        problem = true;
        message = text;
        clearMessage.restart();
    }

    function note(text: string) {
        problem = false;
        message = text;
        clearMessage.restart();
    }

    implicitHeight: 52

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.overlay2
    }

    Text {
        x: 24
        anchors.verticalCenter: parent.verticalCenter
        width: chip.x - 24 - 16
        elide: Text.ElideRight
        text: root.message || FileOps.progress || root.folder.summary
        textFormat: Text.PlainText
        color: !root.message ? Theme.subtext : root.problem ? Theme.terracotta : Theme.text
        font.family: Theme.fontUi
        font.pixelSize: 12
        font.weight: Font.Medium
        font.features: { "tnum": 1 }
    }

    Timer {
        id: clearMessage
        interval: 4000
        onTriggered: root.message = ""
    }

    Rectangle {
        id: chip

        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        width: label.implicitWidth + 28
        height: 32
        radius: 16
        color: chipMouse.containsMouse ? Theme.overlay2 : Theme.overlay
        Accessible.role: Accessible.Button
        Accessible.name: label.text

        Behavior on color {
            ColorAnimation {
                duration: Theme.hoverDuration
                easing.type: Theme.easing
            }
        }

        SectionLabel {
            id: label
            anchors.centerIn: parent
            text: "Sorted by " + ["name", "date", "size", "type"][root.folder.sortKey]
            color: chipMouse.containsMouse ? Theme.text : Theme.subtext
        }

        MouseArea {
            id: chipMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.sortRequested(Qt.point(chip.x, chip.y))
        }
    }
}
