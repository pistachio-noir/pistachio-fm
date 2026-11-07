import QtQuick

// A list row: badge, then name, modified and size in the header's columns (110 and 80 px at the
// right). Selected, it's the pistachio chip with bg-coloured text.
Rectangle {
    id: root

    required property int index
    required property string name
    required property string path
    required property string kind
    required property string modifiedText
    required property string sizeText
    required property bool selected
    required property bool isDir
    required property bool picture
    property var dragData // what dragging carries (the selection's URLs), or null where nothing may be dragged

    signal press(int button, int modifiers, point at)
    signal activate
    signal click(int modifiers) // a left click with no drag

    height: 44
    radius: 22
    color: selected ? Theme.pistachio : mouse.containsMouse || folderDrop.containsDrag ? Theme.overlay2 : Theme.overlay
    opacity: FileOps.cutPaths.indexOf(path) >= 0 ? Theme.cutOpacity : 1
    Accessible.role: Accessible.ListItem
    Accessible.name: name
    Accessible.selected: selected

    Behavior on color {
        ColorAnimation {
            duration: Theme.hoverDuration
            easing.type: Theme.easing
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: Theme.motionDuration
            easing.type: Theme.easing
        }
    }

    KindBadge {
        id: badge
        x: 6
        anchors.verticalCenter: parent.verticalCenter
        size: 32
        kind: root.kind
        path: root.picture ? root.path : ""
        selected: root.selected
        hovered: mouse.containsMouse || folderDrop.containsDrag
    }

    Item {
        id: columns

        anchors.left: badge.right
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 18
        height: parent.height

        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 110 - 80
            text: root.name
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: root.selected ? Theme.bg : Theme.text
            font.family: Theme.fontUi
            font.pixelSize: 13
            font.weight: root.selected ? Font.ExtraBold : Font.Bold
        }

        Text {
            x: parent.width - 110 - 80
            anchors.verticalCenter: parent.verticalCenter
            width: 110
            text: root.modifiedText
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: root.selected ? Theme.bg : Theme.subtext
            font.family: Theme.fontUi
            font.pixelSize: 12
            font.weight: root.selected ? Font.DemiBold : Font.Medium
            font.features: { "tnum": 1 }
        }

        Text {
            x: parent.width - 80
            anchors.verticalCenter: parent.verticalCenter
            width: 80
            horizontalAlignment: Text.AlignRight
            text: root.sizeText
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: root.selected ? Theme.bg : Theme.subtext
            font.family: Theme.fontUi
            font.pixelSize: 12
            font.weight: root.selected ? Font.DemiBold : Font.Medium
            font.features: { "tnum": 1 }
        }
    }

    // what the mouse moves while dragging; never seen, the desktop draws the drag
    Item {
        id: dragged
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
        Drag.mimeData: root.dragData ?? {}
        Drag.active: mouse.drag.active
    }

    // a folder takes drops, unless it's among what's being dragged
    FolderDrop {
        id: folderDrop
        anchors.fill: parent
        dir: root.path
        enabled: root.isDir && !root.selected
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        drag.target: root.dragData ? dragged : null
        onPressed: event => root.press(event.button, event.modifiers, Qt.point(event.x, event.y))
        onClicked: event => {
            if (event.button === Qt.LeftButton)
                root.click(event.modifiers);
        }
        onDoubleClicked: event => {
            if (event.button === Qt.LeftButton)
                root.activate();
        }
    }
}
