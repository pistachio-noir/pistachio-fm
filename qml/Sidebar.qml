pragma ComponentBehavior: Bound

import QtQuick

// Places (home and its standard folders) and, under a rule, devices (the system drive, mounted
// drives, Trash). The place holding the current folder most closely is the active chip.
Flickable {
    id: root

    required property FolderModel folder
    readonly property string activePath: Places.placeFor(folder.path)

    contentHeight: column.height

    Connections {
        target: Places

        function onMounted(path) {
            root.folder.go(path);
        }
    }

    boundsBehavior: Flickable.StopAtBounds
    clip: true

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
