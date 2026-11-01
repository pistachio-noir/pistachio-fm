pragma ComponentBehavior: Bound

import QtQuick

// The tab row of the tabs-split board, under the toolbar while there are two tabs or more: the current
// tab is the pistachio chip (as the current place), each with its ×, a split one with the split glyph;
// + adds one. Middle-click closes a tab.
Item {
    id: root

    property var tabs: [] // [{ title, split }]
    property int current

    signal picked(int index)
    signal closed(int index)
    signal added

    implicitHeight: 48

    Row {
        x: 20
        spacing: 6

        Repeater {
            model: root.tabs

            Rectangle {
                id: tab

                required property var modelData
                required property int index
                readonly property bool isCurrent: index === root.current

                width: Math.min(260, label.implicitWidth + (splitGlyph.visible ? 22 : 0) + 16 + 8 + 24 + 6)
                height: 36
                radius: 18
                color: isCurrent ? Theme.pistachio : tabMouse.containsMouse ? Theme.overlay2 : Theme.overlay
                Accessible.role: Accessible.PageTab
                Accessible.name: tab.modelData.title
                Accessible.selected: isCurrent

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.hoverDuration
                        easing.type: Theme.easing
                    }
                }

                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                    onClicked: event => event.button === Qt.MiddleButton ? root.closed(tab.index) : root.picked(tab.index)
                }

                Icon {
                    id: splitGlyph
                    visible: tab.modelData.split
                    x: 16
                    anchors.verticalCenter: parent.verticalCenter
                    size: 14
                    paths: Icons.columns
                    color: tab.isCurrent ? Theme.bg : Theme.subtext
                }

                Text {
                    id: label
                    x: splitGlyph.visible ? 16 + 22 : 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, tab.width - x - 8 - 24 - 6)
                    text: tab.modelData.title
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: tab.isCurrent ? Theme.bg : Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: 13
                    font.weight: tab.isCurrent ? Font.ExtraBold : Font.Bold
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24
                    height: 24
                    radius: 12
                    color: closeMouse.containsMouse ? (tab.isCurrent ? Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, 0.15) : Theme.overlay)
                                                    : "transparent"
                    Accessible.role: Accessible.Button
                    Accessible.name: "Close tab"

                    Icon {
                        anchors.centerIn: parent
                        size: 14
                        paths: Icons.x
                        color: tab.isCurrent ? Theme.bg : Theme.subtext
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.closed(tab.index)
                    }
                }
            }
        }

        Rectangle {
            width: 36
            height: 36
            radius: 18
            color: addMouse.containsMouse ? Theme.overlay2 : Theme.overlay
            Accessible.role: Accessible.Button
            Accessible.name: "New tab"

            Icon {
                anchors.centerIn: parent
                size: 16
                paths: Icons.plus
                color: Theme.subtext
            }

            MouseArea {
                id: addMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.added()
            }
        }
    }
}
