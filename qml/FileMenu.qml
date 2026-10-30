pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls

// The boards' menu: a 228 px card on overlay with a panel shadow. `entries` are
// { text, icon, danger, checked, run } or { separator: true }. Arrows and Enter work too.
Controls.Popup {
    id: root

    property var entries: []
    property int current: -1

    // window coordinates; the menu stays inside the window
    function openAt(px: real, py: real) {
        current = -1;
        x = Math.max(8, Math.min(px, parent.width - width - 8));
        y = Math.max(8, Math.min(py, parent.height - implicitHeight - 8));
        open();
    }

    function step(by: int) {
        let next = current;
        for (let i = 0; i < entries.length; ++i) {
            next = (next + by + entries.length) % entries.length;
            if (!entries[next].separator) {
                current = next;
                return;
            }
        }
    }

    function trigger(index: int) {
        const entry = entries[index];
        if (!entry || entry.separator)
            return;
        close();
        entry.run();
    }

    parent: Controls.Overlay.overlay
    width: 228
    padding: 8
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.hoverDuration; easing.type: Theme.easing }
    }

    background: Item {
        Shadow {
            anchors.fill: card
            radius: card.radius
            kind: Theme.shadowPanel
        }

        Rectangle {
            id: card
            anchors.fill: parent
            radius: 20
            color: Theme.overlay
            border.color: Theme.overlay2
            border.width: 1
        }
    }

    contentItem: Column {
        focus: true // the popup's focus passes down to the keys here
        Keys.onUpPressed: root.step(-1)
        Keys.onDownPressed: root.step(1)
        Keys.onReturnPressed: root.trigger(root.current)
        Keys.onEnterPressed: root.trigger(root.current)

        Repeater {
            model: root.entries

            MenuEntry {
                id: entry

                required property var modelData
                required property int index

                width: parent.width
                text: entry.modelData.text ?? ""
                icon: entry.modelData.icon ?? []
                danger: entry.modelData.danger ?? false
                separator: entry.modelData.separator ?? false
                checked: entry.modelData.checked ?? false
                highlighted: entry.index === root.current
                onHovered: root.current = entry.index
                onTriggered: root.trigger(entry.index)
            }
        }
    }
}
