pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

Controls.Popup {
    id: root

    readonly property var content: preview.content
    property string path
    property real zoom: 1

    function setZoom(value: real) {
        imageView.cancelFlick();
        const previous = zoom;
        const x = imageView.contentX + imageView.width / 2 - picture.x;
        const y = imageView.contentY + imageView.height / 2 - picture.y;
        zoom = Math.max(1, Math.min(4, value));
        imageView.contentX = Math.max(0, Math.min(imageView.contentWidth - imageView.width, picture.x + x * zoom / previous - imageView.width / 2));
        imageView.contentY = Math.max(0, Math.min(imageView.contentHeight - imageView.height, picture.y + y * zoom / previous - imageView.height / 2));
    }

    function show(path: string) {
        root.path = path;
        setZoom(1);
        preview.load(path);
        open();
    }

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(760, parent ? parent.width - 48 : 760)
    height: Math.min(520, parent ? parent.height - 48 : 520)
    padding: 28
    modal: true
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
    onOpened: textScroll.visible ? textScroll.forceActiveFocus() : body.forceActiveFocus()
    onClosed: preview.clear()

    Preview { id: preview }

    Controls.Overlay.modal: Rectangle { color: Theme.scrim }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionDuration; easing.type: Theme.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.hoverDuration; easing.type: Theme.easing }
    }

    background: Item {
        Shadow {
            anchors.fill: card
            radius: card.radius
            kind: Theme.shadowModal
        }
        Rectangle {
            id: card
            anchors.fill: parent
            radius: Theme.panelRadius
            color: Theme.bgAlt
            border.color: Theme.overlay2
            border.width: 1
        }
    }

    contentItem: ColumnLayout {
        spacing: 14
        Keys.onSpacePressed: root.close()
        Keys.onPressed: event => {
            if (!imageView.visible)
                return;
            if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal)
                root.setZoom(root.zoom + 0.25);
            else if (event.key === Qt.Key_Minus)
                root.setZoom(root.zoom - 0.25);
            else if (event.key === Qt.Key_0)
                root.setZoom(1);
            else
                return;
            event.accepted = true;
        }

        Text {
            Layout.fillWidth: true
            text: root.content.name ?? "Preview"
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
            color: Theme.textBright
            font.family: Theme.fontUi
            font.pixelSize: 18
            font.weight: Font.ExtraBold
        }

        Text {
            Layout.fillWidth: true
            text: FileOps.displayPath(root.path)
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
            color: Theme.subtext
            font.family: Theme.fontMono
            font.pixelSize: 12
        }

        Rectangle {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.tileRadius
            color: Theme.bg
            clip: true
            focus: true
            Keys.onSpacePressed: root.close()
            Keys.onLeftPressed: {
                if (root.content.kind === "pdf")
                    preview.setPage(root.content.page - 1);
            }
            Keys.onRightPressed: {
                if (root.content.kind === "pdf")
                    preview.setPage(root.content.page + 1);
            }

            Flickable {
                id: imageView
                objectName: "previewImageView"
                readonly property real imageRatio: picture.implicitHeight > 0 ? picture.implicitWidth / picture.implicitHeight : 1
                readonly property real fitWidth: Math.min(width, height * imageRatio)
                readonly property real fitHeight: fitWidth / imageRatio
                anchors.fill: parent
                anchors.margins: 16
                visible: root.content.kind === "image" || root.content.kind === "pdf"
                contentWidth: Math.max(width, fitWidth * root.zoom)
                contentHeight: Math.max(height, fitHeight * root.zoom)
                interactive: root.zoom > 1
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                Controls.ScrollBar.vertical: ThinScrollBar {}
                Controls.ScrollBar.horizontal: ThinScrollBar { width: imageView.width; height: 10 }

                Image {
                    id: picture
                    objectName: "previewImage"
                    width: imageView.fitWidth * root.zoom
                    height: imageView.fitHeight * root.zoom
                    x: (imageView.contentWidth - width) / 2
                    y: (imageView.contentHeight - height) / 2
                    source: root.visible && imageView.visible ? (root.content.url ?? "") : ""
                    sourceSize: Qt.size(Math.min(4096, imageView.width * root.zoom * 2), Math.min(4096, imageView.height * root.zoom * 2))
                    asynchronous: true
                    retainWhileLoading: true
                    autoTransform: true
                    cache: false
                    fillMode: Image.PreserveAspectFit
                }

                WheelHandler {
                    target: null
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    acceptedModifiers: Qt.ControlModifier
                    onWheel: event => {
                        const delta = event.angleDelta.y || event.pixelDelta.y;
                        if (delta !== 0)
                            root.setZoom(root.zoom + (delta > 0 ? 0.25 : -0.25));
                        event.accepted = true;
                    }
                }
            }

            Controls.ScrollView {
                id: textScroll
                anchors.fill: parent
                anchors.margins: 16
                visible: root.content.kind === "text" && root.content.text !== ""
                clip: true
                contentWidth: availableWidth
                Keys.onSpacePressed: root.close()
                onVisibleChanged: {
                    if (visible && root.opened)
                        forceActiveFocus();
                }
                Controls.ScrollBar.vertical: ThinScrollBar {}

                TextEdit {
                    objectName: "previewText"
                    width: textScroll.availableWidth
                    text: root.content.text ?? ""
                    textFormat: TextEdit.PlainText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: Theme.text
                    selectionColor: Theme.pistachio
                    selectedTextColor: Theme.bg
                    font.family: Theme.fontMono
                    font.pixelSize: 13
                    Keys.onSpacePressed: root.close()
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 48
                text: preview.loading || (root.content.kind === "image" && picture.status === Image.Loading)
                    ? "Loading…" : imageView.visible && picture.status === Image.Error
                    ? "Couldn't load this image" : root.content.kind !== "image" ? (root.content.note ?? "") : ""
                visible: root.content.kind !== "text" || root.content.text === ""
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: root.content.kind === "error" || (imageView.visible && picture.status === Image.Error)
                    ? Theme.terracotta : Theme.subtext
                font.family: Theme.fontUi
                font.pixelSize: 13
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: root.content.kind === "text" && root.content.text !== "" && root.content.note
                    ? root.content.note : "Space or Esc to close"
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: Theme.subtext
                font.family: Theme.fontUi
                font.pixelSize: 12
            }

            RowLayout {
                visible: imageView.visible
                spacing: 8

                IconButton {
                    objectName: "previewZoomOut"
                    icon: Icons.minus
                    label: "Zoom out"
                    enabled: root.zoom > 1
                    activeFocusOnTab: true
                    active: activeFocus
                    Keys.onReturnPressed: clicked()
                    onClicked: root.setZoom(root.zoom - 0.25)
                }

                DialogButton {
                    objectName: "previewZoomFit"
                    text: root.zoom === 1 ? "Fit" : Math.round(root.zoom * 100) + "%"
                    Accessible.name: "Reset zoom to fit"
                    Keys.onSpacePressed: root.close()
                    onClicked: root.setZoom(1)
                }

                IconButton {
                    objectName: "previewZoomIn"
                    icon: Icons.plus
                    label: "Zoom in"
                    enabled: root.zoom < 4
                    activeFocusOnTab: true
                    active: activeFocus
                    Keys.onReturnPressed: clicked()
                    onClicked: root.setZoom(root.zoom + 0.25)
                }
            }

            RowLayout {
                visible: root.content.kind === "pdf"
                spacing: 8

                IconButton {
                    objectName: "previewPrevious"
                    icon: Icons.back
                    label: "Previous page"
                    enabled: !preview.loading && root.content.page > 1
                    activeFocusOnTab: true
                    Keys.onReturnPressed: clicked()
                    onClicked: preview.setPage(root.content.page - 1)
                }

                Text {
                    text: (root.content.page ?? 1) + " / " + (root.content.pages ?? 1)
                    textFormat: Text.PlainText
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                }

                IconButton {
                    objectName: "previewNext"
                    icon: Icons.forward
                    label: "Next page"
                    enabled: !preview.loading && root.content.page < root.content.pages
                    activeFocusOnTab: true
                    Keys.onReturnPressed: clicked()
                    onClicked: preview.setPage(root.content.page + 1)
                }
            }

            DialogButton {
                text: "Close"
                onClicked: root.close()
            }
        }
    }
}
