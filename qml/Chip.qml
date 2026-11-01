import QtQuick

// A status bar pill (Sorted by, Empty trash): a section-style label on overlay, lighter on hover.
Rectangle {
    id: root

    property string text
    property color textColor: mouse.containsMouse ? Theme.text : Theme.subtext

    signal clicked

    width: label.implicitWidth + 28
    height: 32
    radius: 16
    color: mouse.containsMouse ? Theme.overlay2 : Theme.overlay
    Accessible.role: Accessible.Button
    Accessible.name: text

    Behavior on color {
        ColorAnimation {
            duration: Theme.hoverDuration
            easing.type: Theme.easing
        }
    }

    SectionLabel {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.textColor
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
