pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls

// The folder as tiles (grid) or rows (list), with the handling both share: press to select, Ctrl
// to add or drop, Shift for a range, double-click or Enter to open, right-click for the menu; empty
// space clears the selection. Arrows move the selection (Shift extends it); typing goes to search.
FocusScope {
    id: root

    required property FolderModel folder
    property string mode: "grid"

    readonly property var view: mode === "grid" ? grid : list
    readonly property int columns: mode === "grid" ? grid.columns : 1

    // what dragging the selection carries, for this window and other apps; nothing from the trash,
    // which Restore empties
    readonly property var dragData: Places.inTrash(folder.path) ? null : {
        "text/uri-list": folder.selectedPaths.map(path => "file://" + path.split("/").map(encodeURIComponent).join("/"))
    }

    signal activated(int row) // open it: a folder to browse, a file in its app
    signal menuRequested(point at, bool onItem) // `at` in this item's coordinates
    signal typed(string text)
    signal middleClicked(int row) // a folder: open it in a new tab

    function press(row: int, selected: bool, button: int, modifiers: int, at: point) {
        root.forceActiveFocus();
        if (button === Qt.MiddleButton) {
            root.middleClicked(row);
            return;
        }
        if (button === Qt.RightButton) {
            if (!selected)
                folder.select(row, FolderModel.Replace);
            root.menuRequested(at, true);
        } else if (modifiers & Qt.ControlModifier) {
            folder.select(row, FolderModel.Toggle);
        } else if (modifiers & Qt.ShiftModifier) {
            folder.select(row, FolderModel.Extend);
        } else if (!selected) {
            folder.select(row, FolderModel.Replace);
        }
        // a press on a selected item keeps the rest: they may be dragged together
    }

    // a click that didn't drag: just this one
    function click(row: int, modifiers: int) {
        if (!(modifiers & (Qt.ControlModifier | Qt.ShiftModifier)))
            folder.select(row, FolderModel.Replace);
    }

    function move(target: int, extend: bool) {
        if (folder.count === 0)
            return;
        target = Math.max(0, Math.min(folder.count - 1, target));
        folder.select(target, extend ? FolderModel.Extend : FolderModel.Replace);
        view.positionViewAtIndex(target, ListView.Contain);
    }

    Keys.onPressed: event => {
        const current = folder.currentRow;
        const extend = (event.modifiers & Qt.ShiftModifier) !== 0;
        const page = Math.max(1, Math.floor(view.height / (mode === "grid" ? grid.cellHeight : 48))) * columns;
        switch (event.key) {
        case Qt.Key_Right:
            if (mode === "grid")
                move(current + 1, extend);
            break;
        case Qt.Key_Left:
            if (mode === "grid")
                move(current < 0 ? 0 : current - 1, extend);
            break;
        case Qt.Key_Down:
            move(current < 0 ? 0 : current + columns, extend);
            break;
        case Qt.Key_Up:
            move(current < 0 ? 0 : current - columns, extend);
            break;
        case Qt.Key_PageDown:
            move(current + page, extend);
            break;
        case Qt.Key_PageUp:
            move(current - page, extend);
            break;
        case Qt.Key_Home:
            move(0, extend);
            break;
        case Qt.Key_End:
            move(folder.count - 1, extend);
            break;
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (current >= 0)
                root.activated(current);
            break;
        case Qt.Key_Backspace:
            folder.up();
            break;
        case Qt.Key_Escape:
            if (folder.filter !== "")
                folder.filter = "";
            else
                folder.clearSelection();
            break;
        default:
            if (event.text.length === 1 && event.text >= " " && event.text !== "\u007f"
                    && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
                root.typed(event.text);
                break;
            }
            return; // not ours: leave it to the window's shortcuts
        }
        event.accepted = true;
    }

    // the rows a box (in the view's content coordinates) touches
    function rowsIn(box: rect): list<int> {
        const rows = [];
        if (mode === "grid") {
            for (let r = Math.max(0, Math.floor(box.y / grid.cellHeight)); r * grid.cellHeight <= box.y + box.height; ++r) {
                const top = r * grid.cellHeight;
                if (box.y > top + grid.cellHeight - 10 || box.y + box.height < top) // in the gap under the row
                    continue;
                for (let c = Math.max(0, Math.floor(box.x / grid.cellWidth)); c < grid.columns && c * grid.cellWidth <= box.x + box.width; ++c) {
                    const index = r * grid.columns + c;
                    if (index < folder.count && box.x <= (c + 1) * grid.cellWidth - 10)
                        rows.push(index);
                }
            }
        } else {
            const step = 44 + list.spacing;
            for (let i = Math.max(0, Math.floor(box.y / step)); i < folder.count && i * step <= box.y + box.height; ++i) {
                if (box.y <= i * step + 44)
                    rows.push(i);
            }
        }
        return rows;
    }

    // empty space, under the views: they pass on presses no tile or row takes. A drag there draws a box
    // that selects what it touches (with Ctrl, on top of what was selected)
    MouseArea {
        id: space

        property point from // in the view's content, so it stays put while the wheel scrolls
        property var kept: []
        property bool boxing

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: event => {
            root.forceActiveFocus();
            boxing = false;
            from = mapToItem(root.view.contentItem, event.x, event.y);
            kept = event.modifiers & Qt.ControlModifier ? root.folder.selectedPaths : [];
            if (!(event.modifiers & Qt.ControlModifier))
                root.folder.clearSelection();
            if (event.button === Qt.RightButton)
                root.menuRequested(Qt.point(event.x, event.y), false);
        }
        onPositionChanged: event => {
            if (!(event.buttons & Qt.LeftButton))
                return;
            const to = mapToItem(root.view.contentItem, event.x, event.y);
            if (!boxing && Math.hypot(to.x - from.x, to.y - from.y) < Application.styleHints.startDragDistance)
                return;
            boxing = true;
            const box = Qt.rect(Math.min(from.x, to.x), Math.min(from.y, to.y), Math.abs(to.x - from.x), Math.abs(to.y - from.y));
            const start = root.view.contentItem.mapToItem(root, box.x, box.y);
            band.x = start.x;
            band.y = start.y;
            band.width = box.width;
            band.height = box.height;
            root.folder.selectPaths(kept.concat(root.rowsIn(box).map(row => root.folder.get(row).path)));
        }
        onReleased: boxing = false
        onCanceled: boxing = false
    }

    // the box being drawn, neutral as everything that isn't the selection itself
    Rectangle {
        id: band
        z: 1
        visible: space.boxing
        radius: 6
        color: Qt.rgba(Theme.overlay2.r, Theme.overlay2.g, Theme.overlay2.b, 0.45)
        border.color: Theme.overlay2
        border.width: 1
    }

    // drops on empty space, or on a file, land in this folder
    FolderDrop {
        anchors.fill: parent
        dir: root.folder.path
    }

    GridView {
        id: grid

        readonly property int columns: Math.max(1, Math.floor(width / (160 + 10))) // tiles at least 160 wide

        // 10 px wider than the space, so the last column's trailing gap fits and four columns
        // of (width - 30) / 4 come out as on the board
        anchors.fill: parent
        anchors.rightMargin: -10
        visible: root.mode === "grid"
        model: visible ? root.folder : null
        cellWidth: width / columns
        cellHeight: 108 + 10
        bottomMargin: 16
        acceptedButtons: Qt.NoButton // no dragging to scroll with the mouse; the wheel still scrolls
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        Controls.ScrollBar.vertical: ThinScrollBar {}

        delegate: FileTile {
            id: tile
            width: grid.cellWidth - 10
            onPress: (button, modifiers, at) => root.press(tile.index, tile.selected, button, modifiers, tile.mapToItem(root, at))
            onActivate: root.activated(tile.index)
            onClick: modifiers => root.click(tile.index, modifiers)
            dragData: root.dragData
        }
    }

    // the column names stay put over the scrolling rows; a click sorts by that column
    Item {
        id: columnHeader

        visible: root.mode === "list"
        width: parent.width
        height: 28

        Repeater {
            model: [
                { text: "Name", key: FolderModel.ByName, x: 6 + 32 + 12, alignRight: false },
                { text: "Modified", key: FolderModel.ByModified, x: columnHeader.width - 18 - 80 - 110, alignRight: false },
                { text: "Size", key: FolderModel.BySize, x: columnHeader.width - 18, alignRight: true },
            ]

            SectionLabel {
                id: label
                required property var modelData
                x: label.modelData.alignRight ? label.modelData.x - implicitWidth : label.modelData.x
                anchors.verticalCenter: columnHeader.verticalCenter
                text: label.modelData.text
                color: sortMouse.containsMouse ? Theme.text : Theme.subtext

                MouseArea {
                    id: sortMouse
                    anchors.fill: parent
                    anchors.margins: -6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.folder.sortKey = label.modelData.key
                }
            }
        }
    }

    ListView {
        id: list

        anchors.fill: parent
        anchors.topMargin: columnHeader.height + 4
        visible: root.mode === "list"
        model: visible ? root.folder : null
        spacing: 4
        bottomMargin: 16
        acceptedButtons: Qt.NoButton
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        Controls.ScrollBar.vertical: ThinScrollBar {}

        delegate: FileRow {
            id: row
            width: list.width
            onPress: (button, modifiers, at) => root.press(row.index, row.selected, button, modifiers, row.mapToItem(root, at))
            onActivate: root.activated(row.index)
            onClick: modifiers => root.click(row.index, modifiers)
            dragData: root.dragData
        }
    }

    Text {
        id: empty

        // "Loading…" only for a slow folder, so quick ones don't flash it
        property bool slow

        anchors.centerIn: parent
        anchors.verticalCenterOffset: -24
        width: parent.width - 48
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        visible: text !== ""
        text: {
            if (root.folder.error !== "")
                return root.folder.error;
            if (root.folder.count > 0)
                return "";
            if (root.folder.loading)
                return slow ? "Loading…" : "";
            if (root.folder.filter !== "")
                return "Nothing here matches “" + root.folder.filter + "”";
            return "This folder is empty";
        }
        textFormat: Text.PlainText
        color: Theme.subtext
        font.family: Theme.fontUi
        font.pixelSize: 13
        font.weight: Font.Bold

        Timer {
            interval: 400
            running: root.folder.loading
            onTriggered: empty.slow = true
        }

        Connections {
            target: root.folder

            function onLoadingChanged() {
                if (!root.folder.loading)
                    empty.slow = false;
            }
        }
    }
}
