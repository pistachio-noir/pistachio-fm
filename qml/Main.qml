import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

// The file manager's window, laid out as the boards: toolbar, tabs (from two on), places, files, status
// line. Each tab has one pane or two (split); everything here works on the pane you're in. The rounded
// corners, border and shadow around it are the compositor's.
Controls.ApplicationWindow {
    id: window

    required property string startPath
    property string viewMode: "grid"
    property int currentTab
    property string dropSide // a tab being dragged over a side of the files
    property int draggedTab: -1
    property point dragAt // window coordinates
    readonly property TabPage page: pages.count > currentTab ? pages.itemAt(currentTab) as TabPage : null
    readonly property Pane activePane: page ? page.activePane : null
    // the pane you're in's folder (an empty one only until the first tab is up)
    readonly property FolderModel folderModel: activePane ? activePane.folder : idle
    readonly property FolderModel folder: folderModel // for pfm-shots
    readonly property var tabTitles: {
        const list = [];
        for (let i = 0; i < pages.count; ++i) {
            const tab = pages.itemAt(i) as TabPage;
            if (tab)
                list.push({ title: tab.title, split: tab.split });
        }
        return list;
    }

    readonly property bool inTrash: Places.inTrash(folderModel.path)
    readonly property bool atTrash: Places.isTrash(folderModel.path) // its entries can be restored

    function join(dir: string, name: string): string {
        return (dir === "/" ? "" : dir) + "/" + name;
    }

    function nameOf(path: string): string {
        return path.slice(path.lastIndexOf("/") + 1);
    }

    function setView(mode: string) {
        viewMode = mode;
        const row = folderModel.currentRow;
        if (row >= 0 && activePane)
            Qt.callLater(() => activePane.view.view.positionViewAtIndex(row, ListView.Contain));
    }

    function focusFiles() {
        if (activePane)
            activePane.view.forceActiveFocus();
    }

    function newTab(path: string) {
        tabList.append({ start: path });
        currentTab = tabList.count - 1;
        Qt.callLater(focusFiles);
    }

    // the window goes with the last tab
    function closeTab(index: int) {
        if (tabList.count <= 1) {
            close();
            return;
        }
        tabList.remove(index);
        if (currentTab > index || currentTab === tabList.count)
            currentTab -= 1;
        Qt.callLater(focusFiles);
    }

    // where a tab dragged to `at` (window coordinates) would land: the outer thirds of the files, never
    // on its own page
    function sideAt(index: int, at: point): string {
        if (!page || index === currentTab)
            return "";
        const p = page.mapFromItem(null, at.x, at.y);
        if (p.y < 0 || p.y > page.height || p.x < 0 || p.x > page.width)
            return "";
        return p.x < page.width / 3 ? "left" : p.x > page.width * 2 / 3 ? "right" : "";
    }

    // the dragged tab's folder (its pane you were in) beside this one's, and the tab goes
    function dropTab(index: int, at: point) {
        const side = sideAt(index, at);
        dropSide = "";
        draggedTab = -1;
        if (side) {
            page.openBeside((pages.itemAt(index) as TabPage).activePane.folder.path, side);
            closeTab(index);
        }
    }

    function openTabMenu(index: int, at: point) {
        const tab = pages.itemAt(index) as TabPage;
        menu.entries = [{ text: tab.split ? "Close split" : "Split view", icon: Icons.columns, run: () => {
                              currentTab = index;
                              tab.toggleSplit();
                          } },
                        { text: "Close tab", icon: Icons.x, run: () => closeTab(index) }];
        menu.openAt(at.x, at.y);
    }

    function nextTab(by: int) {
        if (tabList.count > 1) {
            currentTab = (currentTab + by + tabList.count) % tabList.count;
            Qt.callLater(focusFiles);
        }
    }

    // double-click or Enter on a row: a folder opens here, a file in its app; on a row that's part
    // of a bigger selection, the whole selection opens
    function activate(row: int) {
        const item = folderModel.get(row);
        if (folderModel.selectedCount > 1 && folderModel.selectedPaths.indexOf(item.path) >= 0)
            openSelection();
        else if (item.isDir)
            folderModel.go(item.path);
        else
            Apps.openFiles([item.path]);
    }

    function openSelection() {
        const paths = folderModel.selectedPaths;
        if (paths.length === 0)
            return;
        const items = paths.map(path => folderModel.get(folderModel.rowOf(path)));
        const filesOnly = items.filter(item => !item.isDir).map(item => item.path);
        if (filesOnly.length > 0)
            Apps.openFiles(filesOnly);
        else
            folderModel.go(items[0].path);
    }

    function askTrash() {
        const paths = folderModel.selectedPaths;
        if (paths.length === 0)
            return;
        if (inTrash) {
            askDelete(paths);
            return;
        }
        const one = paths.length === 1;
        confirm.ask({
            title: one ? "Move “" + nameOf(paths[0]) + "” to trash?" : "Move " + paths.length + " items to trash?",
            description: one ? "It stays in Trash until you empty it, so you can still restore it."
                             : "They stay in Trash until you empty it, so you can still restore them.",
            action: "Move to trash",
            paths: paths,
            run: () => FileOps.trash(paths)
        });
    }

    function askDelete(paths) {
        if (paths.length === 0)
            return;
        confirm.ask({
            title: paths.length === 1 ? "Delete “" + nameOf(paths[0]) + "” forever?" : "Delete " + paths.length + " items forever?",
            description: "This can't be undone.",
            action: "Delete forever",
            paths: paths,
            safeDefault: true,
            run: () => FileOps.deleteForever(paths)
        });
    }

    // the trash this is: the home one, or a drive's
    function askEmptyTrash() {
        const files = folderModel.path;
        confirm.ask({
            title: "Empty the trash?",
            description: "Everything in it is deleted for good. This can't be undone.",
            action: "Empty trash",
            safeDefault: true,
            run: () => FileOps.emptyTrash(files)
        });
    }

    function askRename() {
        const paths = folderModel.selectedPaths;
        if (paths.length !== 1 || inTrash)
            return;
        const path = paths[0];
        const dir = path.slice(0, path.lastIndexOf("/")) || "/";
        names.ask({
            title: "Rename “" + nameOf(path) + "”",
            action: "Rename",
            icon: Icons.pencil,
            dir: dir,
            except: path,
            text: nameOf(path),
            run: name => {
                const problem = FileOps.rename(path, name);
                if (problem === "")
                    folderModel.selectPaths([join(dir, name)]);
                return problem;
            }
        });
    }

    function askNewFolder() {
        if (inTrash)
            return;
        const dir = folderModel.path;
        names.ask({
            title: "New folder",
            action: "Create",
            icon: Icons.folderPlus,
            dir: dir,
            text: "New folder",
            run: name => {
                const problem = FileOps.makeFolder(dir, name);
                if (problem === "")
                    folderModel.selectPaths([join(dir, name)]);
                return problem;
            }
        });
    }

    function copySelection() {
        FileOps.copy(folderModel.selectedPaths);
    }

    // not in the trash: what's there goes back with Restore
    function cutSelection() {
        if (!inTrash)
            FileOps.cut(folderModel.selectedPaths);
    }

    function pasteHere() {
        FileOps.paste(folderModel.path);
    }

    function showProperties() {
        const paths = folderModel.selectedPaths;
        if (paths.length === 1)
            properties.show(paths[0]);
    }

    function showPreview() {
        const paths = folderModel.selectedPaths;
        if (paths.length === 1 && !folderModel.get(folderModel.rowOf(paths[0])).isDir)
            preview.show(paths[0]);
    }

    // an empty file of any name; the name before the extension is selected, so typing keeps ".txt"
    function askNewFile() {
        if (inTrash)
            return;
        const dir = folderModel.path;
        names.ask({
            title: "New file",
            action: "Create",
            icon: Icons.filePlus,
            dir: dir,
            text: "New file.txt",
            run: name => {
                const problem = FileOps.makeFile(dir, name);
                if (problem === "")
                    folderModel.selectPaths([join(dir, name)]);
                return problem;
            }
        });
    }

    function itemEntries() {
        const paths = folderModel.selectedPaths;
        // one item has properties to show
        const info = paths.length === 1 ? [{ text: "Properties", icon: Icons.info, run: showProperties }] : [];
        const copyPath = { text: "Copy path", icon: Icons.link, run: () => FileOps.copyPaths(paths) };
        const remove = { text: "Delete forever", icon: Icons.trash2, danger: true, run: () => askDelete(paths) };
        if (atTrash)
            return [{ text: "Restore", icon: Icons.restore, run: () => FileOps.restore(paths) }, copyPath, ...info, { separator: true }, remove];
        if (inTrash)
            return [{ text: "Open", icon: Icons.file, run: openSelection }, copyPath, ...info, { separator: true }, remove];
        const entries = [{ text: "Open", icon: Icons.file, run: openSelection }];
        if (paths.length === 1 && folderModel.get(folderModel.rowOf(paths[0])).isDir) {
            entries.push({ text: "Open in new tab", icon: Icons.plus, run: () => newTab(paths[0]) },
                         Places.isBookmarked(paths[0]) ? { text: "Remove from bookmarks", icon: Icons.bookmark, run: () => Places.removeBookmark(paths[0]) }
                                                      : { text: "Add to bookmarks", icon: Icons.bookmark, run: () => Places.addBookmark(paths[0]) });
        }
        if (paths.length === 1 && !folderModel.get(folderModel.rowOf(paths[0])).isDir)
            entries.push({ text: "Open with…", icon: Icons.externalLink, run: () => openWith.show(paths[0]) });
        if (paths.length === 1)
            entries.push({ text: "Rename", icon: Icons.pencil, run: askRename });
        entries.push({ text: "Cut", icon: Icons.scissors, run: () => FileOps.cut(paths) },
                     { text: "Copy", icon: Icons.copy, run: () => FileOps.copy(paths) },
                     copyPath,
                     { text: "Compress", icon: Icons.archive, run: () => FileOps.compress(paths, folderModel.path) });
        if (paths.length === 1 && folderModel.get(folderModel.rowOf(paths[0])).kind === "archive")
            entries.push({ text: "Extract here", icon: Icons.extract, run: () => FileOps.extract(paths[0], folderModel.path) });
        entries.push(...info,
                     { separator: true },
                     { text: "Move to trash", icon: Icons.trash2, danger: true, run: askTrash });
        return entries;
    }

    function backgroundEntries() {
        const hidden = { text: "Show hidden files", icon: Icons.eye, checked: folderModel.showHidden,
                         run: () => folderModel.showHidden = !folderModel.showHidden };
        if (atTrash)
            return [hidden, { separator: true }, { text: "Empty trash", icon: Icons.trash2, danger: true, run: askEmptyTrash }];
        if (inTrash)
            return [hidden];
        const entries = [{ text: "New folder", icon: Icons.folderPlus, run: askNewFolder },
                         { text: "New file", icon: Icons.filePlus, run: askNewFile }];
        if (FileOps.canPaste)
            entries.push({ text: "Paste", icon: Icons.clipboard, run: pasteHere });
        entries.push({ text: "Open in terminal", icon: Icons.terminal, run: () => Apps.openTerminal(folderModel.path) }, hidden);
        return entries;
    }

    // window coordinates
    function openMenuAt(x: real, y: real) {
        menu.entries = folderModel.selectedCount > 0 ? itemEntries() : backgroundEntries();
        menu.openAt(x, y);
    }

    function openSortMenu(chipTopLeft: point) {
        const keys = [["Name", FolderModel.ByName], ["Date modified", FolderModel.ByModified],
                      ["Size", FolderModel.BySize], ["Type", FolderModel.ByType]];
        menu.entries = keys.map(([text, key]) => ({
            text: text,
            icon: [],
            checked: folderModel.sortKey === key,
            run: () => folderModel.sortKey = key
        }));
        const at = status.mapToItem(null, chipTopLeft);
        menu.openAt(at.x, at.y - menu.implicitHeight - 8);
    }

    // the boards' 960 x 516 window, a little taller so Devices fits without scrolling
    width: 960
    height: 600
    minimumWidth: 640
    minimumHeight: 400
    visible: true
    title: "Files"
    color: Theme.bgAlt

    Component.onCompleted: tabList.append({ start: startPath })

    FolderModel {
        id: idle
    }

    Connections {
        target: FileOps

        function onFailed(message) {
            status.flash(message);
        }

        function onNotice(message) {
            status.note(message);
        }

        // what was pasted, compressed or brought back by undo
        function onFinished(job, paths) {
            if (paths.length > 0)
                folderModel.selectPaths(paths);
        }
    }

    Connections {
        target: Places

        function onFailed(message) {
            status.flash(message);
        }
    }

    Connections {
        target: Apps

        function onFailed(message) {
            status.flash(message);
        }
    }

    // the mouse's back and forward buttons, under everything else
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        onPressed: event => event.button === Qt.BackButton ? folderModel.back() : folderModel.forward()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            spacing: 10

            IconButton {
                icon: Icons.back
                label: "Back"
                enabled: folderModel.canGoBack
                onClicked: folderModel.back()
            }

            IconButton {
                icon: Icons.forward
                label: "Forward"
                enabled: folderModel.canGoForward
                onClicked: folderModel.forward()
            }

            IconButton {
                icon: Icons.up
                label: "Up one folder"
                enabled: folderModel.canGoUp
                onClicked: folderModel.up()
            }

            PathBar {
                id: pathBar
                Layout.fillWidth: true
                folder: folderModel
                onDone: window.focusFiles()
            }

            SearchField {
                id: search
                Layout.preferredWidth: 200
                folder: folderModel
                allowRecursive: !window.inTrash
                onDone: {
                    window.focusFiles();
                    if (folderModel.count > 0 && folderModel.selectedCount === 0)
                        folderModel.select(0, FolderModel.Replace);
                }
            }

            Row {
                spacing: 4

                IconButton {
                    icon: Icons.grid
                    label: "Grid view"
                    active: window.viewMode === "grid"
                    restColor: Theme.subtext
                    onClicked: window.setView("grid")
                }

                IconButton {
                    icon: Icons.list
                    label: "List view"
                    active: window.viewMode === "list"
                    restColor: Theme.subtext
                    onClicked: window.setView("list")
                }

                IconButton {
                    objectName: "keysButton"
                    icon: Icons.keyboard
                    label: "Keyboard shortcuts"
                    restColor: Theme.subtext
                    onClicked: keys.open()
                }

                IconButton {
                    objectName: "settingsButton"
                    icon: Icons.settings
                    label: "Settings"
                    restColor: Theme.subtext
                    onClicked: settings.show()
                }
            }
        }

        TabBar {
            Layout.fillWidth: true
            visible: tabList.count > 1
            tabs: window.tabTitles
            current: window.currentTab
            onPicked: index => {
                window.currentTab = index;
                window.focusFiles();
            }
            onClosed: index => window.closeTab(index)
            onAdded: window.newTab(window.folderModel.path)
            onMenuRequested: (index, at) => window.openTabMenu(index, at)
            onDragged: (index, at) => {
                window.draggedTab = index;
                window.dragAt = at;
                window.dropSide = window.sideAt(index, at);
            }
            onDropped: (index, at) => window.dropTab(index, at)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Sidebar {
                Layout.preferredWidth: 208
                Layout.fillHeight: true
                folder: folderModel
                onBookmarkMenu: (path, at) => {
                    menu.entries = [{ text: "Remove from bookmarks", icon: Icons.bookmark, run: () => Places.removeBookmark(path) }];
                    menu.openAt(at.x, at.y);
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 4
                Layout.rightMargin: 20
                currentIndex: window.currentTab

                Repeater {
                    id: pages

                    model: ListModel {
                        id: tabList
                    }

                    delegate: TabPage {
                        mode: window.viewMode
                        dropSide: window.dropSide // only the page on show is seen
                        focus: true
                        onActivated: row => window.activate(row)
                        onMenuRequested: at => window.openMenuAt(at.x, at.y)
                        onTyped: text => search.append(text)
                        onOpenInTab: path => window.newTab(path)
                        onFailed: message => status.flash(message)
                    }
                }
            }
        }

        StatusBar {
            id: status
            Layout.fillWidth: true
            folder: folderModel
            trash: window.atTrash && folderModel.totalCount > 0
            onSortRequested: at => window.openSortMenu(at)
            onEmptyTrashRequested: window.askEmptyTrash()
        }
    }

    // the dragged tab, its chip following the pointer
    Item {
        id: dragged

        readonly property var tab: window.tabTitles[window.draggedTab]

        visible: window.draggedTab >= 0 && tab !== undefined
        x: window.dragAt.x + 14
        y: window.dragAt.y + 10
        width: Math.min(260, ghostLabel.implicitWidth + (ghostGlyph.visible ? 22 : 0) + 32)
        height: 36

        Shadow {
            anchors.fill: ghost
            radius: ghost.radius
            kind: Theme.shadowPanel
        }

        Rectangle {
            id: ghost
            anchors.fill: parent
            radius: 18
            color: Theme.bg // apart from the tiles and the lit side under it
            border.color: Theme.overlay2
            border.width: 1

            Icon {
                id: ghostGlyph
                visible: dragged.tab ? dragged.tab.split : false
                x: 16
                anchors.verticalCenter: parent.verticalCenter
                size: 14
                paths: Icons.columns
                color: Theme.subtext
            }

            Text {
                id: ghostLabel
                x: ghostGlyph.visible ? 16 + 22 : 16
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, ghost.width - x - 16)
                text: dragged.tab ? dragged.tab.title : ""
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontUi
                font.pixelSize: 13
                font.weight: Font.Bold
            }
        }
    }

    FileMenu {
        id: menu
        onAboutToHide: window.focusFiles()
    }

    ConfirmDialog {
        id: confirm
        objectName: "confirm"
        onAboutToHide: window.focusFiles()
    }

    NameDialog {
        id: names
        onAboutToHide: window.focusFiles()
    }

    PropertiesDialog {
        id: properties
        objectName: "properties"
        onAboutToHide: window.focusFiles()
        onChooseApp: path => {
            close();
            openWith.show(path);
        }
    }

    KeysDialog {
        id: keys
        objectName: "keys"
        onAboutToHide: window.focusFiles()
    }

    PreviewDialog {
        id: preview
        objectName: "preview"
        onAboutToHide: window.focusFiles()
    }

    OpenWithDialog {
        id: openWith
        objectName: "openWith"
        onAboutToHide: window.focusFiles()
    }

    SettingsDialog {
        id: settings
        objectName: "settings"
        onAboutToHide: window.focusFiles()
    }

    Shortcut { sequence: "Ctrl+,"; onActivated: settings.show() }
    Shortcut { sequences: [StandardKey.Find]; onActivated: search.focusField() }
    Shortcut {
        sequence: "Space"
        enabled: window.activePane !== null && window.activePane.view.activeFocus
        onActivated: window.showPreview()
    }
    Shortcut { sequence: "Ctrl+L"; onActivated: pathBar.edit() }
    Shortcut { sequence: "Ctrl+H"; onActivated: folderModel.showHidden = !folderModel.showHidden }
    Shortcut { sequence: "Ctrl+1"; onActivated: window.setView("grid") }
    Shortcut { sequence: "Ctrl+2"; onActivated: window.setView("list") }
    Shortcut { sequence: "Ctrl+Shift+N"; onActivated: window.askNewFolder() }
    Shortcut { sequence: "Ctrl+Alt+N"; onActivated: window.askNewFile() }
    Shortcut { sequence: "F2"; onActivated: window.askRename() }
    Shortcut { sequences: ["Alt+Return", "Alt+Enter"]; onActivated: window.showProperties() }
    Shortcut { sequence: "Delete"; onActivated: window.askTrash() }
    Shortcut { sequence: "Shift+Delete"; onActivated: window.askDelete(folderModel.selectedPaths) }
    Shortcut { sequences: [StandardKey.SelectAll]; onActivated: folderModel.selectAll() }
    // a text field with the focus keeps these keys for its own text
    Shortcut { sequences: [StandardKey.Copy]; onActivated: window.copySelection() }
    Shortcut { sequences: [StandardKey.Cut]; onActivated: window.cutSelection() }
    Shortcut { sequences: [StandardKey.Paste]; onActivated: window.pasteHere() }
    Shortcut { sequences: [StandardKey.Undo]; onActivated: FileOps.undo() }
    Shortcut { sequence: "Alt+Left"; onActivated: folderModel.back() }
    Shortcut { sequence: "Alt+Right"; onActivated: folderModel.forward() }
    Shortcut { sequence: "Alt+Up"; onActivated: folderModel.up() }
    Shortcut { sequence: "Alt+Home"; onActivated: folderModel.go(Places.home) }
    Shortcut { sequences: ["F5", "Ctrl+R"]; onActivated: folderModel.reload() }
    Shortcut { sequence: "Ctrl+T"; onActivated: window.newTab(window.folderModel.path) }
    Shortcut { sequence: "Ctrl+W"; onActivated: window.closeTab(window.currentTab) }
    Shortcut { sequences: ["Ctrl+Tab", "Ctrl+PgDown"]; onActivated: window.nextTab(1) }
    Shortcut { sequences: ["Ctrl+Shift+Tab", "Ctrl+Backtab", "Ctrl+Shift+Backtab", "Ctrl+PgUp"]; onActivated: window.nextTab(-1) }
    Shortcut { sequence: "F3"; onActivated: if (window.page) window.page.toggleSplit() }
    Shortcut { sequence: "Ctrl+D"; onActivated: if (!window.inTrash) Places.addBookmark(window.folderModel.path) }
    Shortcut { sequences: [StandardKey.Quit]; onActivated: window.close() }
    Shortcut { sequences: ["F1", "?"]; onActivated: keys.open() }
}
