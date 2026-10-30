pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls

// The move-to-trash board: an alert card over the dimmed window, with the items it's about behind
// a Details toggle (open to start with, as on the board). Also asks before deleting for good and
// emptying the trash. ask() takes { title, description, action, icon, paths, safeDefault, run }.
Controls.Popup {
    id: root

    property string title
    property string description
    property string action
    property list<string> icon: Icons.trash2
    property var paths: []
    // focus starts on Cancel, so Enter can't do what can't be undone
    property bool safeDefault
    property var run: null
    property bool detailsOpen: true

    readonly property int shown: 6 // paths listed before "and N more"

    function ask(options) {
        title = options.title;
        description = options.description;
        action = options.action;
        icon = options.icon ?? Icons.trash2;
        paths = options.paths ?? [];
        safeDefault = options.safeDefault ?? false;
        run = options.run;
        detailsOpen = true;
        open();
    }

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: 440
    padding: 28
    modal: true
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

    onOpened: (safeDefault ? cancel : confirm).forceActiveFocus()

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
                paths: root.icon
                color: Theme.terracotta
            }
        }

        Column {
            width: parent.width
            spacing: 6

            Text {
                width: parent.width
                text: root.title
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.textBright
                font.family: Theme.fontUi
                font.pixelSize: 18
                font.weight: Font.ExtraBold
            }

            Text {
                width: parent.width
                text: root.description
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                lineHeight: 17
                lineHeightMode: Text.FixedHeight
                color: Theme.subtext
                font.family: Theme.fontUi
                font.pixelSize: 12
                font.weight: Font.Medium
            }
        }

        // Details, under a rule
        Item {
            visible: root.paths.length > 0
            width: parent.width
            height: details.y + details.height

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.overlay2
            }

            Column {
                id: details

                y: 15
                width: parent.width
                spacing: 10

                Rectangle {
                    width: parent.width
                    height: 44
                    radius: 22
                    color: toggleMouse.containsMouse ? Theme.overlay2 : Theme.overlay
                    Accessible.role: Accessible.Button
                    Accessible.name: "Details"

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.hoverDuration
                            easing.type: Theme.easing
                        }
                    }

                    Text {
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Details"
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pixelSize: 13
                        font.weight: Font.Bold
                    }

                    Icon {
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        size: 18
                        paths: Icons.chevronDown
                        color: Theme.subtext
                        rotation: root.detailsOpen ? 0 : -90

                        Behavior on rotation {
                            NumberAnimation {
                                duration: Theme.motionDuration
                                easing.type: Theme.easing
                            }
                        }
                    }

                    MouseArea {
                        id: toggleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.detailsOpen = !root.detailsOpen
                    }
                }

                Rectangle {
                    visible: root.detailsOpen
                    width: parent.width
                    height: lines.implicitHeight + 28
                    radius: Theme.tileRadius
                    color: Theme.bg

                    Column {
                        id: lines

                        x: 16
                        y: 14
                        width: parent.width - 32
                        spacing: 6

                        Repeater {
                            model: root.paths.slice(0, root.paths.length > root.shown + 1 ? root.shown : root.shown + 1)

                            Text {
                                id: line
                                required property string modelData
                                width: lines.width
                                text: FileOps.displayPath(line.modelData)
                                textFormat: Text.PlainText
                                elide: Text.ElideMiddle
                                lineHeight: 17
                                lineHeightMode: Text.FixedHeight
                                color: Theme.subtext
                                font.family: Theme.fontMono
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                        }

                        Text {
                            visible: root.paths.length > root.shown + 1
                            text: "and " + (root.paths.length - root.shown) + " more"
                            textFormat: Text.PlainText
                            lineHeight: 17
                            lineHeightMode: Text.FixedHeight
                            color: Theme.subtext
                            font.family: Theme.fontUi
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }
                }
            }
        }

        Row {
            anchors.right: parent.right
            topPadding: 4
            spacing: 10

            DialogButton {
                id: cancel
                text: "Cancel"
                onClicked: root.close()
            }

            DialogButton {
                id: confirm
                text: root.action
                textColor: Theme.terracotta
                strong: true
                onClicked: {
                    const run = root.run;
                    root.close();
                    run();
                }
            }
        }
    }
}
