import QtQuick

// One folder in a tab: its own model (history, selection, search) and its files. In a split a header
// names it, the pane you're in with the pistachio dot and the bright name (the tabs-split board).
FocusScope {
    id: root

    required property string start
    property bool split
    property bool current
    property string mode: "grid"
    readonly property FolderModel folder: model
    readonly property FileView view: files

    signal entered // it took the keyboard: it's the pane you're in now
    signal activated(int row)
    signal menuRequested(point at) // window coordinates
    signal typed(string text)
    signal openInTab(string path)
    signal failed(string message)

    onActiveFocusChanged: {
        if (activeFocus)
            root.entered();
    }

    Component.onCompleted: model.go(start)

    FolderModel {
        id: model
        onFailed: message => root.failed(message)
    }

    Row {
        id: header

        visible: root.split
        height: visible ? 28 : 0
        x: 4
        spacing: 8

        Rectangle {
            visible: root.current
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -3
            width: 6
            height: 6
            radius: 3
            color: Theme.pistachio
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -3
            width: root.width - 24
            text: model.path === "/" ? "System" : model.path.slice(model.path.lastIndexOf("/") + 1)
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: root.current ? Theme.textBright : Theme.subtext
            font.family: Theme.fontUi
            font.pixelSize: 13
            font.weight: Font.ExtraBold
        }
    }

    FileView {
        id: files

        anchors.fill: parent
        anchors.topMargin: header.height
        folder: model
        mode: root.mode
        focus: true
        onActivated: row => root.activated(row)
        onMenuRequested: at => {
            const scene = files.mapToItem(null, at);
            root.menuRequested(Qt.point(scene.x, scene.y));
        }
        onTyped: text => root.typed(text)
        onMiddleClicked: row => {
            const item = model.get(row);
            if (item.isDir)
                root.openInTab(item.path);
        }
    }
}
