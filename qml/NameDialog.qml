import QtQuick
import QtQuick.Controls.Basic as Controls

// Asks for a name (rename, new folder) in the trash dialog's card, checking it as you type.
// ask() takes { title, action, icon, dir, except, text, run }; run(name) returns "" when done, or
// a problem to show under the field.
Controls.Popup {
    id: root

    property string title
    property string action
    property list<string> icon: Icons.pencil
    property string dir
    property string except // the entry being renamed: its own name isn't taken
    property var run: null
    property string error // from run()

    readonly property string problem: FileOps.checkName(dir, field.text, except)

    function ask(options) {
        title = options.title;
        action = options.action;
        icon = options.icon ?? Icons.pencil;
        dir = options.dir;
        except = options.except ?? "";
        run = options.run;
        error = "";
        field.text = options.text ?? "";
        open();
    }

    function submit() {
        if (problem !== "")
            return;
        error = run(field.text);
        if (error === "")
            close();
    }

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: 440
    padding: 28
    modal: true
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

    // the name without its extension, ready to type over
    onOpened: {
        field.forceActiveFocus();
        const dot = field.text.lastIndexOf(".");
        field.select(0, dot > 0 ? dot : field.text.length);
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
                paths: root.icon
                color: Theme.text
            }
        }

        Text {
            width: parent.width
            text: root.title
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
            color: Theme.textBright
            font.family: Theme.fontUi
            font.pixelSize: 18
            font.weight: Font.ExtraBold
        }

        Column {
            width: parent.width
            spacing: 8

            Rectangle {
                width: parent.width
                height: 44
                radius: 22
                color: Theme.bg

                TextInput {
                    id: field

                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    color: Theme.text
                    selectionColor: Theme.pistachio
                    selectedTextColor: Theme.bg
                    selectByMouse: true
                    font.family: Theme.fontUi
                    font.pixelSize: 13
                    font.weight: Font.Bold

                    onTextEdited: root.error = ""
                    onAccepted: root.submit()
                }
            }

            Text {
                x: 18
                width: parent.width - 36
                // "Type a name" only once there's something to say it about
                text: root.error || (field.text !== "" ? root.problem : "")
                visible: text !== ""
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                lineHeight: 17
                lineHeightMode: Text.FixedHeight
                color: Theme.terracotta
                font.family: Theme.fontUi
                font.pixelSize: 12
                font.weight: Font.Medium
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
                text: root.action
                textColor: Theme.textBright
                strong: true
                enabled: root.problem === ""
                onClicked: root.submit()
            }
        }
    }
}
