import QtQuick

// The round badge with a file kind's glyph in its colour; on a selected item, a dark badge with a
// pistachio glyph.
Rectangle {
    id: root

    property string kind
    property bool selected
    property bool hovered // on a hovered tile, which is overlay2 itself
    property real size: 36

    width: size
    height: size
    radius: size / 2
    color: selected ? Theme.bg : hovered ? Theme.overlay : Theme.overlay2

    Behavior on color {
        ColorAnimation {
            duration: Theme.hoverDuration
            easing.type: Theme.easing
        }
    }

    Icon {
        anchors.centerIn: parent
        size: 18
        paths: Icons.forKind(root.kind)
        color: root.selected ? Theme.pistachio : Theme.kindColor(root.kind)
    }
}
