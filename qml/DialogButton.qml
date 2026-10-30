import QtQuick

// A dialog's pill button: 44 px on overlay, lighter on hover or keyboard focus. The action keeps
// overlay too and only its text takes a colour (terracotta to trash), as on the boards.
Rectangle {
    id: root

    property string text
    property color textColor: Theme.text
    property bool strong // the action: ExtraBold

    signal clicked

    implicitWidth: label.implicitWidth + 44
    implicitHeight: 44
    radius: 22
    color: enabled && (mouse.containsMouse || activeFocus) ? Theme.overlay2 : Theme.overlay
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text

    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    Behavior on color {
        ColorAnimation {
            duration: Theme.hoverDuration
            easing.type: Theme.easing
        }
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        textFormat: Text.PlainText
        color: root.enabled ? root.textColor : Theme.subtext
        font.family: Theme.fontUi
        font.pixelSize: 13
        font.weight: root.strong ? Font.ExtraBold : Font.Bold
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
