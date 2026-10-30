import QtQuick

// One line of FileMenu: 44 px, icon in subtext, the highlighted line on overlay2 in textBright;
// a dangerous one in terracotta; a `separator` is the 1 px rule with 4 px around it.
Item {
    id: root

    property string text
    property list<string> icon
    property bool danger
    property bool separator
    property bool checked // the sort menu's current order
    property bool highlighted

    signal triggered
    signal hovered

    implicitHeight: separator ? 9 : 44
    Accessible.role: separator ? Accessible.Separator : Accessible.MenuItem
    Accessible.name: text

    Rectangle {
        visible: root.separator
        x: 6
        y: 4
        width: parent.width - 12
        height: 1
        color: Theme.overlay2
    }

    Rectangle {
        visible: !root.separator
        anchors.fill: parent
        radius: 16
        color: root.highlighted ? Theme.overlay2 : Qt.rgba(Theme.overlay2.r, Theme.overlay2.g, Theme.overlay2.b, 0)

        Behavior on color {
            ColorAnimation {
                duration: Theme.hoverDuration
                easing.type: Theme.easing
            }
        }

        Row {
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                size: 18
                paths: root.icon
                color: root.danger ? Theme.terracotta : Theme.subtext
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                textFormat: Text.PlainText
                color: root.danger ? Theme.terracotta : root.highlighted ? Theme.textBright : Theme.text
                font.family: Theme.fontUi
                font.pixelSize: 13
                font.weight: Font.Bold
            }
        }

        Icon {
            visible: root.checked
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            size: 16
            paths: Icons.check
            color: Theme.pistachio
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: !root.separator
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: root.hovered()
        onClicked: root.triggered()
    }
}
