import QtQuick

// The round badge with a file kind's glyph in its colour; on a selected item, a dark badge with a
// pistachio glyph. A picture Qt can read shows itself in the badge instead.
Rectangle {
    id: root

    property string kind
    property string path // a picture Qt can show, for its thumbnail; "" for the glyph
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
        visible: thumbnail.status !== Image.Ready
        size: 18
        paths: Icons.forKind(root.kind)
        color: root.selected ? Theme.pistachio : Theme.kindColor(root.kind)
    }

    // shortcut: decoded from the file each time a tile shows (only at badge size, in the background, and kept in
    // Qt's cache), not from ~/.cache/thumbnails; switch to that if folders of huge pictures feel slow
    Image {
        id: thumbnail
        anchors.fill: parent
        visible: status === Image.Ready
        source: root.path ? "image://thumbnail/" + encodeURIComponent(root.path) : ""
        sourceSize: Qt.size(root.size * 2, root.size * 2) // sharp at a scale of 2
        asynchronous: true
    }
}
