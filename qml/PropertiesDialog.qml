pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls

// One item's properties, on the alert card of the move-to-trash board: its kind's badge, name and type,
// then size, location, dates, permissions. A folder's size is added up in the background and fills in.
Controls.Popup {
    id: root

    property string path
    property var info: ({})
    property string size

    // label, value, and whether it's a path (mono); rows with nothing to say are left out
    readonly property var rows: [
        ["Size", size, false],
        ["Location", info.location, true],
        ["Points to", info.link, true],
        ["Created", info.created, false],
        ["Modified", info.modified, false],
        ["Permissions", info.permissions, false],
    ].filter(row => !!row[1])

    function show(path: string) {
        root.path = path;
        info = FileOps.properties(path);
        size = info.measure ? "Counting…" : info.size;
        if (info.measure)
            FileOps.measure(path);
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
                        width: row.width - 92 - row.spacing
                        text: row.modelData[1]
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        color: Theme.text
                        font.family: row.modelData[2] ? Theme.fontMono : Theme.fontUi
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        font.features: { "tnum": 1 }
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
