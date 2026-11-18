pragma ComponentBehavior: Bound

import QtQuick

// The path as breadcrumbs on a dark pill: the steps before in subtext (each one a link), where you
// are in textBright. A long path keeps its end in view. edit() (Ctrl+L, or a click on the pill's
// empty end) swaps in a field to type a path into: "~/…", absolute, or relative to this folder.
Rectangle {
    id: root

    required property FolderModel folder
    property bool editing

    signal done

    function edit() {
        field.text = FileOps.displayPath(folder.path);
        editing = true;
        field.forceActiveFocus();
        field.selectAll();
    }

    function finish() {
        editing = false;
        root.done();
    }

    implicitHeight: 44
    radius: 22
    color: Theme.bg

    MouseArea {
        anchors.fill: parent
        enabled: !root.editing
        cursorShape: Qt.IBeamCursor
        onClicked: root.edit()
    }

    Item {
        id: viewport

        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        visible: !root.editing
        clip: true

        Row {
            id: crumbs

            x: Math.min(0, viewport.width - width)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Repeater {
                id: steps

                model: Places.crumbs(root.folder.path)

                Row {
                    id: step

                    required property var modelData
                    required property int index
                    readonly property bool last: step.index === steps.count - 1

                    spacing: 8

                    Icon {
                        visible: step.index > 0
                        anchors.verticalCenter: parent.verticalCenter
                        size: 14
                        paths: Icons.chevronRight
                        color: Theme.subtext
                    }

                    Text {
                        objectName: "breadcrumb:" + step.modelData.path
                        anchors.verticalCenter: parent.verticalCenter
                        text: step.modelData.name
                        textFormat: Text.PlainText
                        color: step.last || breadcrumbDrop.containsDrag ? Theme.textBright : link.containsMouse ? Theme.text : Theme.subtext
                        font.family: Theme.fontUi
                        font.pixelSize: 13
                        font.weight: Font.Bold

                        FolderDrop {
                            id: breadcrumbDrop
                            anchors.fill: parent
                            anchors.margins: -6
                            dir: step.modelData.path
                        }

                        MouseArea {
                            id: link
                            anchors.fill: parent
                            anchors.margins: -6
                            enabled: !step.last
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.folder.go(step.modelData.path)
                        }
                    }
                }
            }
        }
    }

    TextInput {
        id: field

        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        visible: root.editing
        verticalAlignment: TextInput.AlignVCenter
        clip: true
        color: Theme.text
        selectionColor: Theme.pistachio
        selectedTextColor: Theme.bg
        selectByMouse: true
        font.family: Theme.fontUi
        font.pixelSize: 13
        font.weight: Font.Bold

        onAccepted: {
            if (root.folder.go(text))
                root.finish();
        }
        Keys.onTabPressed: event => {
            if (cursorPosition === text.length) {
                const completion = root.folder.completePath(text);
                if (completion !== text) {
                    text = completion;
                    cursorPosition = text.length;
                    event.accepted = true;
                }
            }
        }
        Keys.onEscapePressed: root.finish()
        onActiveFocusChanged: {
            if (!activeFocus)
                root.editing = false;
        }
    }
}
