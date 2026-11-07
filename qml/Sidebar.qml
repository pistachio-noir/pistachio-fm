pragma ComponentBehavior: Bound

import QtQuick

// Places (home, its standard folders, Trash), bookmarks (once there are any) and, under a rule, devices
// (the system drive and the drives plugged in). The place holding the current folder most closely is
// the active chip. A folder dropped on the sidebar, off a place, is bookmarked.
Flickable {
    id: root

    required property FolderModel folder
    // again when bookmarks or drives come and go, which the call alone wouldn't tell the binding
    readonly property string activePath: Places.bookmarks && Places.devices ? Places.placeFor(folder.path) : ""

    signal bookmarkMenu(string path, point at)

    contentHeight: column.height

    Connections {
        target: Places

        function onMounted(path) {
            root.folder.go(path);
        }
    }

    boundsBehavior: Flickable.StopAtBounds
    clip: true

    // drops on a place go into it; anywhere else here, a dropped folder becomes a bookmark
    DropArea {
        width: root.width
        height: Math.max(root.height, column.height)
        keys: ["text/uri-list"]
        onDropped: drop => {
            Places.addBookmarks(drop.urls);
            drop.accept(Qt.LinkAction);
        }
    }

    Column {
        id: column

        width: root.width
        leftPadding: 20
        rightPadding: 12
        bottomPadding: 16
        spacing: 4

        readonly property real innerWidth: width - leftPadding - rightPadding

        SectionLabel {
            text: "Places"
            topPadding: 4
            leftPadding: 14
            bottomPadding: 6
        }

        Repeater {
            model: Places.places

            PlaceButton {
                id: place

                required property var modelData

                width: column.innerWidth
                label: place.modelData.name
                path: place.modelData.path
                icon: Icons[place.modelData.icon]
                active: place.modelData.path === root.activePath
                onClicked: root.folder.go(place.modelData.path)
            }
        }

        SectionLabel {
            visible: Places.bookmarks.length > 0
            text: "Bookmarks"
            topPadding: 14
            leftPadding: 14
            bottomPadding: 6
        }

        Repeater {
            model: Places.bookmarks

            PlaceButton {
                id: bookmark

                required property var modelData

                width: column.innerWidth
                label: bookmark.modelData.name
                path: bookmark.modelData.path
                icon: Icons[bookmark.modelData.icon]
                active: bookmark.modelData.path === root.activePath
                onClicked: root.folder.go(bookmark.modelData.path)
                onMenuRequested: at => root.bookmarkMenu(bookmark.modelData.path, at)
            }
        }

        // the boards' rule: 10 px gap, the line, 14 px padding (4 of it from the column's spacing)
        Item {
            width: column.innerWidth
            height: 21

            Rectangle {
                y: 10
                width: parent.width
                height: 1
                color: Theme.overlay2
            }
        }

        SectionLabel {
            text: "Devices"
            leftPadding: 14
            bottomPadding: 6
        }

        Repeater {
            model: Places.devices

            PlaceButton {
                id: device

                required property var modelData

                width: column.innerWidth
                label: device.modelData.name
                path: device.modelData.path
                icon: Icons[device.modelData.icon]
                active: device.modelData.path === root.activePath
                ejectable: !!device.modelData.device && device.modelData.path !== ""
                // a drive that isn't mounted is mounted first, then opens
                onClicked: device.modelData.path !== "" ? root.folder.go(device.modelData.path) : Places.mount(device.modelData.device)
                onEject: {
                    const mount = device.modelData.path;
                    if (root.folder.path === mount || root.folder.path.startsWith(mount + "/"))
                        root.folder.go(Places.home);
                    Places.unmount(device.modelData.device);
                }
            }
        }
    }
}
