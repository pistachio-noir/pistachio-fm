import QtQuick

// The window's foot: item count, then the selection or the free space, the sort chip, in the
// trash an Empty trash chip, and the version in the corner. flash()
// puts a problem there in terracotta for a few seconds, note() a plain message ("Undid rename");
// while a paste takes a while, its progress shows instead of the count, with a Stop chip.
Item {
    id: root

    required property FolderModel folder
    property string message
    property bool problem
    property bool trash // a trash with something in it: offer to empty it

    signal sortRequested(point at) // `at` in this item's coordinates, the chip's top-left
    signal emptyTrashRequested

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
        width: (stop.visible ? stop.x : root.trash ? emptyTrash.x : chip.x) - 24 - 16
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

    Text {
        id: version
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        text: "v" + Qt.application.version
        textFormat: Text.PlainText
        color: Theme.subtext
        font.family: Theme.fontUi
        font.pixelSize: 12
        font.weight: Font.Medium
        font.features: { "tnum": 1 }
    }

    Chip {
        id: chip
        anchors.right: version.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        text: "Sorted by " + ["name", "date", "size", "type"][root.folder.sortKey]
        onClicked: root.sortRequested(Qt.point(chip.x, chip.y))
    }

    // in a trash with something in it; terracotta, as every action that can't be undone
    Chip {
        id: emptyTrash
        objectName: "emptyTrash"
        visible: root.trash
        anchors.right: chip.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: "Empty trash"
        textColor: Theme.terracotta
        onClicked: root.emptyTrashRequested()
    }

    Chip {
        id: stop
        objectName: "stopPaste"
        visible: FileOps.progress !== ""
        anchors.right: root.trash ? emptyTrash.left : chip.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: "Stop"
        onClicked: FileOps.stop()
    }
}
