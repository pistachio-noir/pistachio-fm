pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

Controls.Popup {
    id: root

    property string query
    property string chosen
    property bool showAll
    readonly property MimeSettings manager: defaults
    readonly property var matches: {
        const search = query.trim().toLowerCase();
        const found = defaults.types.filter(row => search
            ? row.mime.toLowerCase().includes(search) || row.name.toLowerCase().includes(search) : row.popular);
        return found.length || !search.includes("/") ? found : [{ name: "Custom MIME type", mime: search }];
    }
    readonly property var choices: showAll ? (defaults.info.apps ?? []) : (defaults.info.recommended ?? [])

    function selectType(mime: string) {
        if (defaults.saving)
            return;
        showAll = false;
        chosen = "";
        defaults.inspect(mime);
    }

    function show() {
        if (!defaults.saving) {
            query = "";
            selectType("text/plain");
        }
        open();
    }

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(800, parent ? parent.width - 48 : 800)
    height: Math.min(540, parent ? parent.height - 48 : 540)
    padding: 28
    modal: true
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
    onOpened: search.forceActiveFocus()

    MimeSettings {
        id: defaults
        onChanged: {
            if (!busy)
                root.chosen = info.currentId ?? "";
        }
    }

    Controls.Overlay.modal: Rectangle { color: Theme.scrim }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.hoverDuration; easing.type: Theme.easing }
    }
    background: Item {
        Shadow { anchors.fill: card; radius: card.radius; kind: Theme.shadowModal }
        Rectangle {
            id: card
            anchors.fill: parent
            radius: Theme.panelRadius
            color: Theme.bgAlt
            border.color: Theme.overlay2
            border.width: 1
        }
    }

    component Choice: Rectangle {
        id: row
        required property string title
        required property string subtitle
        property bool selected
        property bool highlighted
        signal picked
        height: 52
        radius: Theme.tileRadius
        color: selected ? Theme.pistachio : highlighted || mouse.containsMouse ? Theme.overlay2 : Theme.overlay
        Accessible.role: Accessible.ListItem
        Accessible.name: title + " " + subtitle
        Accessible.selected: selected
        Column {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 7
            spacing: 2
            Text {
                width: parent.width
                text: row.title
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: row.selected ? Theme.bg : Theme.text
                font.family: Theme.fontUi
                font.pixelSize: 13
                font.weight: Font.Bold
            }
            Text {
                width: parent.width
                text: row.subtitle
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: row.selected ? Theme.bg : Theme.subtext
                font.family: Theme.fontMono
                font.pixelSize: 11
            }
        }
        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: row.picked()
        }
    }

    contentItem: ColumnLayout {
        spacing: 14
        Text {
            text: "Default applications"
            color: Theme.textBright
            font.family: Theme.fontUi
            font.pixelSize: 18
            font.weight: Font.ExtraBold
        }
        Text {
            Layout.fillWidth: true
            text: "Changes apply across your desktop. Select an app, then Apply."
            wrapMode: Text.Wrap
            color: Theme.subtext
            font.family: Theme.fontUi
            font.pixelSize: 12
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            radius: 22
            color: Theme.bg
            TextInput {
                id: search
                objectName: "settingsSearch"
                anchors.fill: parent
                anchors.margins: 12
                text: root.query
                enabled: !defaults.saving
                clip: true
                selectByMouse: true
                color: Theme.text
                selectionColor: Theme.pistachio
                selectedTextColor: Theme.bg
                font.family: Theme.fontUi
                font.pixelSize: 13
                onTextEdited: root.query = text
                onAccepted: {
                    if (root.matches.length)
                        root.selectType(root.matches[0].mime);
                }
                Text {
                    visible: !search.text
                    text: "Search MIME types (PDF, image/png…)"
                    color: Theme.subtext
                    font: search.font
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16
            ListView {
                id: types
                objectName: "settingsTypes"
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: root.matches
                enabled: !defaults.saving
                activeFocusOnTab: true
                boundsBehavior: Flickable.StopAtBounds
                Controls.ScrollBar.vertical: ThinScrollBar {}
                Keys.onReturnPressed: {
                    if (currentIndex >= 0)
                        root.selectType(root.matches[currentIndex].mime);
                }
                delegate: Choice {
                    required property var modelData
                    required property int index
                    width: types.width
                    title: modelData.name
                    subtitle: modelData.mime
                    selected: modelData.mime === defaults.info.mime
                    highlighted: types.activeFocus && types.currentIndex === index
                    onPicked: {
                        types.currentIndex = index;
                        types.forceActiveFocus();
                        root.selectType(modelData.mime);
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10
                Text {
                    Layout.fillWidth: true
                    text: defaults.info.mime ?? "Select a MIME type"
                    textFormat: Text.PlainText
                    elide: Text.ElideMiddle
                    color: Theme.textBright
                    font.family: Theme.fontMono
                    font.pixelSize: 13
                }
                Text {
                    Layout.fillWidth: true
                    text: defaults.busy ? (defaults.saving ? "Saving…" : "Loading…") : "Current: " + (defaults.info.currentName ?? "Unknown")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: Theme.subtext
                    font.family: Theme.fontUi
                    font.pixelSize: 12
                }
                ListView {
                    id: apps
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: root.choices
                    enabled: !defaults.busy
                    activeFocusOnTab: true
                    boundsBehavior: Flickable.StopAtBounds
                    Controls.ScrollBar.vertical: ThinScrollBar {}
                    onCurrentIndexChanged: {
                        if (activeFocus && currentIndex >= 0 && currentIndex < root.choices.length)
                            root.chosen = root.choices[currentIndex].id;
                    }
                    delegate: Choice {
                        required property var modelData
                        required property int index
                        width: apps.width
                        title: modelData.name + (modelData.id === defaults.info.currentId ? " · Default" : "")
                        subtitle: modelData.id
                        selected: modelData.id === root.chosen
                        onPicked: {
                            apps.currentIndex = index;
                            apps.forceActiveFocus();
                            root.chosen = modelData.id;
                        }
                    }
                }
                Chip {
                    text: root.showAll ? "Show recommended" : "Show all apps"
                    onClicked: root.showAll = !root.showAll
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Text {
                Layout.fillWidth: true
                text: defaults.error || defaults.message
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: defaults.error ? Theme.terracotta : Theme.subtext
                font.family: Theme.fontUi
                font.pixelSize: 12
            }
            DialogButton { text: "Close"; onClicked: root.close() }
            DialogButton {
                objectName: "settingsApply"
                text: "Apply"
                strong: true
                enabled: !defaults.busy && root.chosen !== "" && root.chosen !== defaults.info.currentId
                onClicked: defaults.save(root.chosen)
            }
        }
    }
}
