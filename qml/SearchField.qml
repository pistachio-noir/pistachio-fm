import QtQuick

// "Search files": narrows the current folder to names containing the text, as you type. Typing
// over the files lands here too (append). Escape clears it; Enter or Down hands the matches to the
// keyboard (done).
Rectangle {
    id: root

    required property FolderModel folder

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

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.IBeamCursor
        onClicked: field.forceActiveFocus()
    }

    Icon {
        id: glass
        x: 16
        anchors.verticalCenter: parent.verticalCenter
        size: 18
        paths: Icons.search
        color: Theme.subtext
    }

    TextInput {
        id: field

        anchors.left: glass.right
        anchors.leftMargin: 10
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
            text: "Search files"
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
