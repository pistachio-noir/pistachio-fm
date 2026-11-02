pragma ComponentBehavior: Bound

import QtQuick

// A tab: one pane, or two side by side once split (F3). The pane you're in (activePane) leads the
// toolbar, sidebar and status line; closing the split keeps it.
Item {
    id: root

    required property string start
    property string mode: "grid"
    property bool split
    property int currentPane // 0 the first, 1 the second
    property string dropSide // "left" or "right" while a tab is dragged there: where it would land

    readonly property Pane activePane: split && currentPane === 1 && second.item ? second.item as Pane : first
    // "Documents", or "Documents · Pictures" when split
    readonly property string title: split && second.item ? nameOf(first.folder.path) + " · " + nameOf((second.item as Pane).folder.path)
                                                          : nameOf(first.folder.path)

    signal activated(int row)
    signal menuRequested(point at)
    signal typed(string text)
    signal openInTab(string path)
    signal failed(string message)

    function nameOf(path: string): string {
        return path === "/" ? "System" : path.slice(path.lastIndexOf("/") + 1);
    }

    // a second pane on the same folder; again, back to one: the pane you're in
    function toggleSplit() {
        if (!split) {
            split = true;
            currentPane = 1;
        } else {
            if (currentPane === 1 && second.item)
                first.folder.go((second.item as Pane).folder.path);
            split = false;
            currentPane = 0;
        }
        Qt.callLater(() => activePane.view.forceActiveFocus());
    }

    // a tab dropped on a side: `path` in the pane there (splitting first), and that's the pane you're in
    function openBeside(path: string, side: string) {
        split = true;
        currentPane = side === "left" ? 0 : 1;
        activePane.folder.go(path);
        Qt.callLater(() => activePane.view.forceActiveFocus());
    }

    Pane {
        id: first

        width: root.split ? (root.width - 25) / 2 : root.width
        height: root.height
        start: root.start
        split: root.split
        current: root.currentPane === 0
        mode: root.mode
        focus: true
        onEntered: root.currentPane = 0
        onActivated: row => root.activated(row)
        onMenuRequested: at => root.menuRequested(at)
        onTyped: text => root.typed(text)
        onOpenInTab: path => root.openInTab(path)
        onFailed: message => root.failed(message)
    }

    Rectangle {
        visible: root.split
        x: first.width + 12
        width: 1
        height: root.height - 16
        color: Theme.overlay2
    }

    Loader {
        id: second

        active: root.split
        x: first.width + 25
        width: first.width
        height: root.height
        sourceComponent: Pane {
            start: first.folder.path
            split: true
            current: root.currentPane === 1
            mode: root.mode
            onEntered: root.currentPane = 1
            onActivated: row => root.activated(row)
            onMenuRequested: at => root.menuRequested(at)
            onTyped: text => root.typed(text)
            onOpenInTab: path => root.openInTab(path)
            onFailed: message => root.failed(message)
        }
    }

    // where a dragged tab would land, lit as a folder that files are dragged over
    Rectangle {
        visible: root.dropSide !== ""
        x: root.dropSide === "left" ? 0 : (root.width + 25) / 2
        width: (root.width - 25) / 2
        height: root.height - 16
        radius: Theme.tileRadius
        color: Theme.overlay2

        Icon {
            anchors.centerIn: parent
            size: 32
            paths: Icons.columns
            color: Theme.subtext
        }
    }
}
