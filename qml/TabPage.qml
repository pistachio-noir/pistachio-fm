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
    property real splitRatio: 0.5
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

    onWidthChanged: {
        const available = Math.max(1, width - 25);
        const minimum = Math.min(160, available / 2) / available;
        splitRatio = Math.max(minimum, Math.min(1 - minimum, splitRatio));
    }

    function nameOf(path: string): string {
        return path === "/" ? "System" : path.slice(path.lastIndexOf("/") + 1);
    }

    function snapshot() {
        return { left: first.folder.path, right: split && second.item ? second.item.folder.path : "",
                 current: currentPane, ratio: splitRatio };
    }

    function restore(state) {
        if (state.right !== "") {
            splitRatio = state.ratio ?? 0.5;
            split = true;
            if (second.item)
                second.item.folder.go(state.right);
            currentPane = state.current;
        }
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

        width: root.split ? Math.max(0, root.width - 25) * root.splitRatio : root.width
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
        id: dividerLine
        visible: root.split
        x: first.width + 12
        width: 1
        height: root.height - 16
        color: divider.containsMouse || divider.activeFocus ? Theme.subtext : Theme.overlay2
        Behavior on color { ColorAnimation { duration: Theme.hoverDuration; easing.type: Theme.easing } }

        MouseArea {
            id: divider
            objectName: "splitDivider"
            x: -12
            width: 25
            height: parent.height
            hoverEnabled: true
            cursorShape: Qt.SplitHCursor
            activeFocusOnTab: true
            Accessible.name: "Resize split panes"
            function adjust(position) {
                const available = Math.max(1, root.width - 25);
                const minimum = Math.min(160, available / 2);
                root.splitRatio = Math.max(minimum / available, Math.min(1 - minimum / available, position / available));
            }
            onPressed: forceActiveFocus()
            onPositionChanged: event => {
                if (pressed)
                    adjust(mapToItem(root, event.x, event.y).x - 12.5);
            }
            Keys.onLeftPressed: adjust(first.width - 20)
            Keys.onRightPressed: adjust(first.width + 20)
        }
    }

    Loader {
        id: second

        active: root.split
        x: first.width + 25
        width: Math.max(0, root.width - x)
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
