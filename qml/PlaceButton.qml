import QtQuick

// A sidebar entry: icon and name on a 44 px pill, the current place as the pistachio chip. A drive
// that can be unmounted has an eject button at the end.
Rectangle {
    id: root

    required property string label
    required property list<string> icon
    required property string path // where drops on it go; "" for a drive that isn't mounted
    property bool active
    property bool ejectable

    signal clicked
    signal eject

    implicitHeight: 44
    radius: 22
    color: active ? Theme.pistachio : mouse.containsMouse || folderDrop.containsDrag ? Theme.overlay : Qt.rgba(Theme.overlay.r, Theme.overlay.g, Theme.overlay.b, 0)
    Accessible.role: Accessible.Button
    Accessible.name: label

    Behavior on color {
        ColorAnimation {
            duration: Theme.hoverDuration
            easing.type: Theme.easing
        }
    }

    Row {
        x: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            size: 18
            paths: root.icon
            color: root.active ? Theme.bg : Theme.subtext
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: root.width - 14 - 18 - 12 - 14 - (root.ejectable ? 18 + 8 : 0)
            text: root.label
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: root.active ? Theme.bg : Theme.text
            font.family: Theme.fontUi
            font.pixelSize: 13
            font.weight: root.active ? Font.ExtraBold : Font.Bold
        }
    }

    FolderDrop {
        id: folderDrop
        anchors.fill: parent
        dir: root.path
        enabled: root.path !== ""
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Icon {
        id: ejectIcon
        visible: root.ejectable
        x: root.width - 14 - 18
        anchors.verticalCenter: parent.verticalCenter
        size: 18
        paths: Icons.eject
        color: root.active ? Theme.bg : ejectMouse.containsMouse ? Theme.text : Theme.subtext

        MouseArea {
            id: ejectMouse
            anchors.fill: parent
            anchors.margins: -8
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.eject()
            Accessible.role: Accessible.Button
            Accessible.name: "Unmount " + root.label
        }
    }
}
