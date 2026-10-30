import QtQuick

// Where files can be dropped, from this window or another app, into `dir`. The app that started the
// drag is always told it was a copy: this app does any moving itself, and no source should delete
// its originals on top of that.
DropArea {
    id: root

    required property string dir

    keys: ["text/uri-list"]
    onDropped: drop => {
        FileOps.drop(drop.urls, root.dir);
        drop.accept(Qt.CopyAction);
    }
}
