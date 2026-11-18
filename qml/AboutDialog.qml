import QtQuick
import QtQuick.Controls.Basic as Controls

Controls.Popup {
    id: root
    objectName: "aboutDialog"
    readonly property var info: Apps.buildInfo()
    readonly property var versionParts: Qt.application.version.split(" · ")
    property string error

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, parent.width - 32)
    height: Math.min(implicitHeight, parent.height - 16)
    padding: 28
    modal: true
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
    onOpened: closeButton.forceActiveFocus()
    Controls.Overlay.modal: Rectangle { color: Theme.scrim }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.hoverDuration; easing.type: Theme.easing } }
    background: Item {
        Shadow { anchors.fill: card; radius: card.radius; kind: Theme.shadowModal }
        Rectangle { id: card; anchors.fill: parent; radius: Theme.panelRadius; color: Theme.bgAlt; border.color: Theme.overlay2 }
    }
    contentItem: Flickable {
        id: viewport
        implicitHeight: content.implicitHeight
        contentHeight: content.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Controls.ScrollBar.vertical: ThinScrollBar {}
        Column {
            id: content
            width: viewport.width
            spacing: 14
            Text { text: "About Pistachio FM"; color: Theme.textBright; font.family: Theme.fontUi; font.pixelSize: 18; font.weight: Font.ExtraBold }
            Text { text: "A file manager for the Pistachio shell."; color: Theme.subtext; font.family: Theme.fontUi; font.pixelSize: 12 }
            Rectangle { width: parent.width; height: 1; color: Theme.overlay2 }
            Repeater {
                model: [ ["Version", root.versionParts[0]], ["Commit", root.versionParts.length > 1 ? root.versionParts[1] : "Unavailable"],
                         ["Compiler", root.info.compiler], ["Qt", root.info.qt], ["Meson", root.info.meson],
                         ["System", root.info.system], ["Kernel", root.info.kernel], ["Architecture", root.info.architecture] ]
                Row {
                    required property var modelData
                    width: content.width; spacing: 12
                    Text { width: 95; text: modelData[0]; color: Theme.subtext; font.family: Theme.fontUi; font.pixelSize: 12 }
                    Text { width: parent.width - 107; text: modelData[1]; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.text; font.family: Theme.fontUi; font.pixelSize: 12 }
                }
            }
            Text { width: parent.width; visible: root.error !== ""; text: root.error; color: Theme.terracotta; wrapMode: Text.Wrap; font.family: Theme.fontUi; font.pixelSize: 12 }
            Row {
                anchors.right: parent.right; spacing: 10
                DialogButton {
                    text: "GitHub"
                    onClicked: if (!Qt.openUrlExternally("https://github.com/sxnvte666/pistachio-fm")) root.error = "Couldn't open GitHub"
                }
                DialogButton { id: closeButton; text: "Close"; onClicked: root.close() }
            }
        }
    }
}
