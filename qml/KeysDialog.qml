pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls

// Every key, from the toolbar's keyboard button (or F1, ?): three columns on the dialogs' card, the keys
// as the shell launcher's cheat sheet draws them. Keep in step with Main's Shortcuts and README's table.
Controls.Popup {
    id: root

    readonly property var groups: [
        { title: "Moving around", keys: [
            [["Enter"], "Open"],
            [["↑", "↓", "←", "→"], "Move; Shift extends"],
            [["Backspace"], "Up a folder"],
            [["Alt", "←"], "Back"],
            [["Alt", "→"], "Forward"],
            [["Alt", "Home"], "Home"],
            [["Ctrl", "L"], "Type a path"],
            [["Ctrl", "F"], "Search (or just type)"],
            [["Esc"], "Clear the search, then the selection"],
            [["F5"], "Reload"],
        ] },
        { title: "Files", keys: [
            [["Ctrl", "C"], "Copy"],
            [["Ctrl", "X"], "Cut"],
            [["Ctrl", "V"], "Paste"],
            [["Ctrl", "Z"], "Undo"],
            [["F2"], "Rename"],
            [["Ctrl", "Shift", "N"], "New folder"],
            [["Ctrl", "Alt", "N"], "New file"],
            [["Del"], "Move to trash"],
            [["Shift", "Del"], "Delete forever"],
            [["Alt", "Enter"], "Properties"],
        ] },
        { title: "View and tabs", keys: [
            [["Ctrl", "A"], "Select all"],
            [["Ctrl", "H"], "Hidden files"],
            [["Ctrl", "1"], "Grid"],
            [["Ctrl", "2"], "List"],
            [["Ctrl", "T"], "New tab"],
            [["Ctrl", "W"], "Close the tab"],
            [["Ctrl", "Tab"], "Next tab"],
            [["F3"], "Split view"],
            [["Ctrl", "Q"], "Quit"],
            [["F1"], "These keys"],
        ] },
    ]

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(880, parent ? parent.width - 48 : 880)
    padding: 28
    modal: true
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

    // after the menu or button that opened it has handed the focus back
    onOpened: done.forceActiveFocus()

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

        Text {
            text: "Keyboard shortcuts"
            color: Theme.textBright
            font.family: Theme.fontUi
            font.pixelSize: 18
            font.weight: Font.ExtraBold
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.overlay2
        }

        // the columns; a narrow window wraps them and scrolls
        Flickable {
            width: parent.width
            height: Math.min(columns.implicitHeight, (root.parent ? root.parent.height : 600) - 210)
            contentHeight: columns.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Controls.ScrollBar.vertical: ThinScrollBar {}

            Flow {
                id: columns
                width: parent.width
                spacing: 28

                Repeater {
                    model: root.groups

                    Column {
                        id: group

                        required property var modelData

                        width: (columns.width - 2 * columns.spacing) / 3 > 230 ? (columns.width - 2 * columns.spacing) / 3 : columns.width
                        spacing: 4

                        SectionLabel {
                            text: group.modelData.title
                            bottomPadding: 4
                        }

                        Repeater {
                            model: group.modelData.keys

                            Item {
                                id: line

                                required property var modelData

                                width: group.width
                                height: Math.max(28, what.implicitHeight + 8)
                                Accessible.role: Accessible.StaticText
                                Accessible.name: line.modelData[0].join(" ") + ": " + line.modelData[1]

                                Row {
                                    id: chips
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4

                                    Repeater {
                                        model: line.modelData[0]

                                        Rectangle {
                                            required property string modelData

                                            width: Math.max(24, key.implicitWidth + 14)
                                            height: 24
                                            radius: 7
                                            color: Theme.overlay

                                            Text {
                                                id: key
                                                anchors.centerIn: parent
                                                text: parent.modelData
                                                textFormat: Text.PlainText
                                                color: Theme.text
                                                font.family: Theme.fontUi
                                                font.pixelSize: 12
                                                font.weight: Font.ExtraBold
                                            }
                                        }
                                    }
                                }

                                Text {
                                    id: what
                                    x: 128
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - x
                                    text: line.modelData[1]
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
                                    color: Theme.subtext
                                    font.family: Theme.fontUi
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                }
                            }
                        }
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
