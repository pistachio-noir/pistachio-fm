pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls

// One item's properties, on the alert card of the move-to-trash board: its kind's badge, name and type,
// then size, location (where a trashed item was deleted from, and when), dates, permissions. A folder's
// size is added up in the background and fills in.
Controls.Popup {
    id: root

    property string path
    property var info: ({})
    property string size
    property int measureToken
    property string opensWith // a file's default app, "None"; "" for a folder

    signal chooseApp(string path) // Change / Choose next to Opens with

    // label, value, whether it's a path (mono), and a chip's label; rows with nothing to say are left out
    readonly property var rows: [
        ["Size", size, false],
        ["Location", info.location, true],
        ["Deleted from", info.deletedFrom, true],
        ["Deleted", info.deleted, false],
        ["Opens with", opensWith, false, opensWith === "None" ? "Choose" : "Change"],
        ["Points to", info.link, true],
        ["Created", info.created, false],
        ["Modified", info.modified, false],
        ["Permissions", info.permissions, false],
    ].filter(row => !!row[1])

    function show(path: string) {
        ++measureToken;
        root.path = path;
        info = FileOps.properties(path);
        size = info.measure ? "Counting…" : info.size;
        opensWith = info.kind === "folder" ? "" : Apps.defaultName(path) || "None";
        if (info.measure)
            FileOps.measure(path);
        open();
    }

    function showMany(paths) {
        ++measureToken;
        path = "";
        info = { name: paths.length + " selected", kind: "file", type: "Multiple items" };
        size = "Counting…";
        opensWith = "";
        FileOps.measureSelection(paths, measureToken);
        open();
    }

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: 440
    padding: 28
    modal: true
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

    onOpened: done.forceActiveFocus()

    Connections {
        target: FileOps

        function onSelectionMeasured(request, size) {
            if (request === root.measureToken && root.path === "")
                root.size = size;
        }

        function onMeasured(path, size) {
            if (path === root.path)
                root.size = size;
        }
    }

    Controls.Overlay.modal: Rectangle {
        color: Theme.scrim
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.hoverDuration; easing.type: Theme.easing }
    }

    background: Item {
        Shadow {
            anchors.fill: card
            radius: card.radius
            kind: Theme.shadowModal
        }

        Rectangle {
            id: card
            anchors.fill: parent
            radius: Theme.panelRadius
            color: Theme.bgAlt
            border.color: Theme.overlay2
            border.width: 1
        }
    }

    contentItem: Column {
        spacing: 14

        Rectangle {
            width: 56
            height: 56
            radius: 28
            color: Theme.overlay

            Icon {
                anchors.centerIn: parent
                size: 24
                paths: Icons.forKind(root.info.kind ?? "file")
                color: Theme.kindColor(root.info.kind ?? "file")
            }
        }

        Column {
            width: parent.width
            spacing: 6

            Text {
                width: parent.width
                text: root.info.name ?? ""
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                color: Theme.textBright
                font.family: Theme.fontUi
                font.pixelSize: 18
                font.weight: Font.ExtraBold
            }

            Text {
                width: parent.width
                text: root.info.type ?? ""
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.subtext
                font.family: Theme.fontUi
                font.pixelSize: 12
                font.weight: Font.Medium
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.overlay2
        }

        Column {
            width: parent.width
            spacing: 10

            Repeater {
                model: root.rows

                Row {
                    id: row

                    required property var modelData

                    width: parent.width
                    spacing: 12

                    Text {
                        width: 92
                        text: row.modelData[0]
                        textFormat: Text.PlainText
                        color: Theme.subtext
                        font.family: Theme.fontUi
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: row.width - 92 - row.spacing - (rowChip.visible ? rowChip.width + row.spacing : 0)
                        text: row.modelData[1]
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        color: Theme.text
                        font.family: row.modelData[2] ? Theme.fontMono : Theme.fontUi
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        font.features: { "tnum": 1 }
                    }

                    Chip {
                        id: rowChip
                        visible: !!row.modelData[3]
                        text: row.modelData[3] ?? ""
                        onClicked: root.chooseApp(root.path)
                    }
                }
            }
        }

        DialogButton {
            id: done
            anchors.right: parent.right
            text: "Close"
            onClicked: root.close()
        }
    }
}
