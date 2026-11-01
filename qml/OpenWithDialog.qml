pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls

// The Open with board: the apps that open a file's type (the default first, marked, and picked to
// start with), every other app behind Show all apps, and "Always use for <type>", which makes the
// pick the desktop's default (on to start with when the type has none). ↑↓ pick, Enter opens.
Controls.Popup {
    id: root

    property string path
    property var info: ({}) // Apps.forFile
    property string kind
    property bool showAll
    property bool remember
    property string chosen // an app's id

    // the type's apps, then with Show all a label and every other app
    readonly property var rows: {
        const own = info.apps ?? [];
        if (!showAll)
            return own;
        const ids = own.map(app => app.id);
        return own.concat([{ label: "All apps" }], Apps.all().filter(app => !ids.includes(app.id)));
    }
    readonly property var choices: rows.filter(row => !row.label)

    function show(path: string) {
        root.path = path;
        info = Apps.forFile(path);
        kind = FileOps.properties(path).kind;
        const apps = info.apps;
        showAll = apps.length === 0;
        remember = !apps.some(app => app.isDefault);
        chosen = apps.length > 0 ? apps[0].id : "";
        open();
    }

    // "Markdown documents"; a few words have no plural ("source code", "audio")
    function plural(type: string): string {
        return /(s|code|data|text|audio|video|music)$/i.test(type) ? type : type + "s";
    }

    function step(by: int) {
        const ids = choices.map(app => app.id);
        const next = Math.max(0, Math.min(ids.length - 1, ids.indexOf(chosen) + by));
        if (ids.length > 0) {
            chosen = ids[next];
            list.positionViewAtIndex(rows.findIndex(row => row.id === chosen), ListView.Contain);
        }
    }

    function openChosen() {
        if (chosen !== "" && Apps.open(chosen, path, remember))
            close();
    }

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: 440
    padding: 28
    modal: true
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

    // after the menu that opened it has handed the focus back to the files
    onOpened: contentItem.forceActiveFocus()

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
        focus: true // the popup's focus passes down to the keys here
        spacing: 14
        Keys.onUpPressed: root.step(-1)
        Keys.onDownPressed: root.step(1)
        Keys.onReturnPressed: root.openChosen()
        Keys.onEnterPressed: root.openChosen()

        Rectangle {
            width: 56
            height: 56
            radius: 28
            color: Theme.overlay

            Icon {
                anchors.centerIn: parent
                size: 24
                paths: Icons.forKind(root.kind || "file")
                color: Theme.kindColor(root.kind || "file")
            }
        }

        Column {
            width: parent.width
            spacing: 6

            Text {
                width: parent.width
                text: "Open “" + root.path.slice(root.path.lastIndexOf("/") + 1) + "” with"
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
                color: Theme.subtext
                font.family: Theme.fontUi
                font.pixelSize: 12
                font.weight: Font.Medium
            }
        }

        // the apps and Show all, under a rule
        Item {
            width: parent.width
            height: apps.y + apps.height

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.overlay2
            }

            Column {
                id: apps

                y: 15
                width: parent.width
                spacing: 10

                ListView {
                    id: list

                    width: parent.width
                    height: Math.min(contentHeight, 4 * 44 + 3 * 4) // four rows, then it scrolls
                    spacing: 4
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.rows
                    Controls.ScrollBar.vertical: ThinScrollBar {}

                    delegate: Item {
                        id: row

                        required property var modelData
                        readonly property bool chosen: !modelData.label && modelData.id === root.chosen

                        width: list.width
                        height: modelData.label ? 24 : 44

                        Text {
                            visible: !!row.modelData.label
                            x: 16
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 2
                            text: (row.modelData.label ?? "").toUpperCase()
                            color: Theme.subtext
                            font.family: Theme.fontUi
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.66
                        }

                        Rectangle {
                            visible: !row.modelData.label
                            anchors.fill: parent
                            radius: 22
                            color: row.chosen ? Theme.pistachio : rowMouse.containsMouse ? Theme.overlay2 : Theme.overlay
                            Accessible.role: Accessible.ListItem
                            Accessible.name: row.modelData.name ?? ""
                            Accessible.selected: row.chosen

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.hoverDuration
                                    easing.type: Theme.easing
                                }
                            }

                            // the app's icon from the theme, or its first letter where there's none
                            Image {
                                id: appIcon
                                x: 6
                                anchors.verticalCenter: parent.verticalCenter
                                width: 32
                                height: 32
                                sourceSize: Qt.size(64, 64)
                                source: row.modelData.icon ? "image://appicon/" + row.modelData.icon : ""
                                visible: status === Image.Ready
                            }

                            Rectangle {
                                visible: !appIcon.visible
                                x: 6
                                anchors.verticalCenter: parent.verticalCenter
                                width: 32
                                height: 32
                                radius: 16
                                color: row.chosen ? Theme.bg : Theme.overlay2

                                Text {
                                    anchors.centerIn: parent
                                    text: (row.modelData.name ?? "?").charAt(0).toUpperCase()
                                    color: row.chosen ? Theme.pistachio : Theme.text
                                    font.family: Theme.fontUi
                                    font.pixelSize: 13
                                    font.weight: Font.ExtraBold
                                }
                            }

                            Text {
                                x: 6 + 32 + 12
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - x - 16 - meta.implicitWidth - 8
                                text: row.modelData.name ?? ""
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                color: row.chosen ? Theme.bg : Theme.text
                                font.family: Theme.fontUi
                                font.pixelSize: 13
                                font.weight: row.chosen ? Font.ExtraBold : Font.Bold
                            }

                            Text {
                                id: meta
                                anchors.right: parent.right
                                anchors.rightMargin: 16
                                anchors.verticalCenter: parent.verticalCenter
                                text: row.modelData.isDefault ? "Default" : row.modelData.terminal ? "Terminal" : ""
                                color: row.chosen ? Theme.bg : Theme.subtext
                                font.family: Theme.fontUi
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.chosen = row.modelData.id
                                onDoubleClicked: {
                                    root.chosen = row.modelData.id;
                                    root.openChosen();
                                }
                            }
                        }
                    }

                    // more below: the rows fade out
                    Rectangle {
                        visible: !list.atYEnd
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 20
                        gradient: Gradient {
                            GradientStop { position: 0; color: Qt.rgba(Theme.bgAlt.r, Theme.bgAlt.g, Theme.bgAlt.b, 0) }
                            GradientStop { position: 1; color: Theme.bgAlt }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 44
                    radius: 22
                    color: allMouse.containsMouse ? Theme.overlay2 : Theme.overlay
                    Accessible.role: Accessible.Button
                    Accessible.name: "Show all apps"

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.hoverDuration
                            easing.type: Theme.easing
                        }
                    }

                    Text {
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Show all apps"
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
                        rotation: root.showAll ? 0 : -90

                        Behavior on rotation {
                            NumberAnimation {
                                duration: Theme.motionDuration
                                easing.type: Theme.easing
                            }
                        }
                    }

                    MouseArea {
                        id: allMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showAll = !root.showAll
                    }
                }
            }
        }

        // Always use: a switch
        Item {
            width: parent.width
            height: 44
            Accessible.role: Accessible.CheckBox
            Accessible.name: always.text
            Accessible.checked: root.remember

            Text {
                id: always
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - track.width - 14
                text: "Always use for " + root.plural(root.info.type ?? "this type")
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontUi
                font.pixelSize: 13
                font.weight: Font.Bold
            }

            Rectangle {
                id: track
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 48
                height: 28
                radius: 14
                color: root.remember ? Theme.pistachio : Theme.overlay2

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.motionDuration
                        easing.type: Theme.easing
                    }
                }

                Rectangle {
                    x: root.remember ? 24 : 4
                    y: 4
                    width: 20
                    height: 20
                    radius: 10
                    color: root.remember ? Theme.bg : Theme.subtext

                    Behavior on x {
                        NumberAnimation {
                            duration: Theme.motionDuration
                            easing.type: Theme.easing
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.remember = !root.remember
            }
        }

        Row {
            anchors.right: parent.right
            topPadding: 4
            spacing: 10

            DialogButton {
                text: "Cancel"
                onClicked: root.close()
            }

            DialogButton {
                text: "Open"
                textColor: Theme.textBright
                strong: true
                enabled: root.chosen !== ""
                onClicked: root.openChosen()
            }
        }
    }
}
