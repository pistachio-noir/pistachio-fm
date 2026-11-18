import QtQuick
import QtQuick.Controls.Basic as Controls

Controls.Popup {
    id: root

    property var paths: []
    property string findText
    property string replacement
    property string error
    signal renamed(var paths)

    function name(path) { return path.slice(path.lastIndexOf("/") + 1); }
    function newName(path) {
        const original = name(path);
        return findText === "" ? original : original.split(findText).join(replacement);
    }
    function show(items) {
        paths = items;
        findText = "";
        replacement = "";
        error = "";
        open();
    }
    function submit() {
        error = FileOps.renameMany(paths, findText, replacement);
        if (error === "") {
            const result = paths.map(path => path.slice(0, path.lastIndexOf("/") + 1) + newName(path));
            close();
            renamed(result);
        }
    }

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, parent.width - 32)
    height: Math.min(implicitHeight, parent.height - 16)
    padding: 28
    modal: true
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
    onOpened: { viewport.contentY = 0; findInput.forceActiveFocus(); }
    onErrorChanged: if (error !== "") Qt.callLater(() => viewport.contentY = Math.max(0, viewport.contentHeight - viewport.height))
    Controls.Overlay.modal: Rectangle { color: Theme.scrim }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.hoverDuration; easing.type: Theme.easing } }
    background: Item {
        Shadow { anchors.fill: card; radius: card.radius; kind: Theme.shadowModal }
        Rectangle {
            id: card
            anchors.fill: parent
            radius: Theme.panelRadius
            color: Theme.bgAlt
            border.color: Theme.overlay2
        }
    }

    contentItem: Flickable {
        id: viewport
        implicitHeight: content.implicitHeight
        contentHeight: content.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Controls.ScrollBar.vertical: ThinScrollBar {}
        Column {
            id: content
            width: viewport.width
            spacing: 14
            Text {
                text: "Rename " + root.paths.length + " items"
                color: Theme.textBright; font.family: Theme.fontUi; font.pixelSize: 18; font.weight: Font.ExtraBold
            }
            Text { text: "Find text"; color: Theme.subtext; font.family: Theme.fontUi; font.pixelSize: 12 }
            Rectangle {
                width: parent.width; height: 44; radius: 22; color: Theme.bg
                TextInput {
                    id: findInput
                    objectName: "batchFind"
                    Accessible.name: "Find text"
                    anchors.fill: parent; anchors.margins: 12
                    verticalAlignment: TextInput.AlignVCenter; clip: true; selectByMouse: true; activeFocusOnTab: true
                    color: Theme.text; selectionColor: Theme.pistachio; selectedTextColor: Theme.bg
                    font.family: Theme.fontUi; font.pixelSize: 13
                    text: root.findText
                    onTextEdited: { root.findText = text; root.error = ""; }
                    Keys.onReturnPressed: root.submit()
                }
            }
            Text { text: "Replace with"; color: Theme.subtext; font.family: Theme.fontUi; font.pixelSize: 12 }
            Rectangle {
                width: parent.width; height: 44; radius: 22; color: Theme.bg
                TextInput {
                    id: replacementInput
                    objectName: "batchReplacement"
                    Accessible.name: "Replace with"
                    anchors.fill: parent; anchors.margins: 12
                    verticalAlignment: TextInput.AlignVCenter; clip: true; selectByMouse: true; activeFocusOnTab: true
                    color: Theme.text; selectionColor: Theme.pistachio; selectedTextColor: Theme.bg
                    font.family: Theme.fontUi; font.pixelSize: 13
                    text: root.replacement
                    onTextEdited: { root.replacement = text; root.error = ""; }
                    Keys.onReturnPressed: root.submit()
                }
            }
            Text { text: "Preview"; color: Theme.subtext; font.family: Theme.fontUi; font.pixelSize: 12 }
            ListView {
                width: parent.width
                height: Math.min(160, Math.max(36, root.parent.height - 370), contentHeight)
                clip: true
                model: root.paths
                spacing: 4
                Controls.ScrollBar.vertical: ThinScrollBar {}
                delegate: Text {
                    required property string modelData
                    width: ListView.view.width
                    text: root.name(modelData) + "  →  " + root.newName(modelData)
                    textFormat: Text.PlainText; elide: Text.ElideMiddle
                    color: Theme.text; font.family: Theme.fontUi; font.pixelSize: 12
                }
            }
            Text {
                width: parent.width; visible: root.error !== ""; text: root.error
                textFormat: Text.PlainText; wrapMode: Text.Wrap
                color: Theme.terracotta; font.family: Theme.fontUi; font.pixelSize: 12
            }
            Row {
                anchors.right: parent.right; spacing: 10
                DialogButton { text: "Cancel"; onClicked: root.close() }
                DialogButton { text: "Rename"; strong: true; enabled: root.findText !== ""; onClicked: root.submit() }
            }
        }
    }
}
