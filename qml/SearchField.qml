import QtQuick
import QtQuick.Controls.Basic as Controls

// "Search files": filters locally; the search icon toggles traversal of subfolders. Typing
// over the files lands here too (append). Escape clears it; Enter or Down hands the matches to the
// keyboard (done).
Rectangle {
    id: root

    required property FolderModel folder
    property bool allowRecursive: true

    signal done

    function focusField() {
        field.forceActiveFocus();
        field.selectAll();
    }

    function append(text: string) {
        field.text += text;
        folder.filter = field.text;
        field.forceActiveFocus();
    }

    implicitHeight: 44
    radius: 22
    color: Theme.bg

    onFolderChanged: field.text = folder.filter // another tab or pane: its own search

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.IBeamCursor
        onClicked: field.forceActiveFocus()
    }

    IconButton {
        id: glass
        objectName: "searchScope"
        anchors.verticalCenter: parent.verticalCenter
        icon: Icons.search
        label: root.folder.recursive ? "Search subfolders: on" : "Search subfolders: off"
        active: root.folder.recursive
        color: active || scopeHover.hovered ? Theme.overlay2 : Theme.bg
        restColor: Theme.subtext
        enabled: root.allowRecursive
        activeFocusOnTab: true
        Keys.onSpacePressed: clicked()
        Keys.onReturnPressed: clicked()
        Accessible.checkable: true
        Accessible.checked: root.folder.recursive
        onClicked: {
            root.folder.recursive = !root.folder.recursive;
            field.forceActiveFocus();
        }
        Controls.ToolTip.visible: scopeHover.hovered
        Controls.ToolTip.text: label + " · Click to change"
        HoverHandler { id: scopeHover }
    }

    TextInput {
        id: field

        anchors.left: glass.right
        anchors.leftMargin: 0
        anchors.right: clear.visible ? clear.left : parent.right
        anchors.rightMargin: clear.visible ? 4 : 16
        anchors.verticalCenter: parent.verticalCenter
        clip: true
        color: Theme.text
        selectionColor: Theme.pistachio
        selectedTextColor: Theme.bg
        selectByMouse: true
        font.family: Theme.fontUi
        font.pixelSize: 13
        font.weight: Font.DemiBold

        onTextEdited: root.folder.filter = text
        onAccepted: root.done()
        Keys.onDownPressed: root.done()
        Keys.onEscapePressed: {
            root.folder.filter = "";
            root.done();
        }

        Text {
            visible: !field.text
            text: root.folder.recursive ? "Search subfolders" : "Search files"
            textFormat: Text.PlainText
            color: Theme.subtext
            font: field.font
        }

        Connections {
            target: root.folder

            // a new folder clears the search
            function onFilterChanged() {
                if (field.text !== root.folder.filter)
                    field.text = root.folder.filter;
            }
        }
    }

    Rectangle {
        id: clear

        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: field.text !== ""
        width: 28
        height: 28
        radius: 14
        color: clearMouse.containsMouse ? Theme.overlay : Qt.rgba(Theme.overlay.r, Theme.overlay.g, Theme.overlay.b, 0)
        Accessible.role: Accessible.Button
        Accessible.name: "Clear search"

        Icon {
            anchors.centerIn: parent
            size: 14
            paths: Icons.x
            color: clearMouse.containsMouse ? Theme.text : Theme.subtext
        }

        MouseArea {
            id: clearMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.folder.filter = "";
                root.done();
            }
        }
    }
}
