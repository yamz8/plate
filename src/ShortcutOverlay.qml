import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Plate

// Every shortcut, shown with ?.
Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    padding: 24
    // Three columns when the window is wide enough; otherwise two, scrolling when short.
    width: Math.min(1120, parent ? parent.width - 40 : 820)
    height: Math.min(implicitHeight, parent ? parent.height - 40 : implicitHeight)
    Material.roundedScale: theme.radius
    focus: true

    Overlay.modal: Rectangle { color: "#a0000000" }

    background: Rectangle {
        color: "#17171b"
        border.color: "#2a2a31"
        radius: theme.radius
    }

    readonly property var groups: [
        { title: qsTr("Plate"), keys: [
            ["1 – 6", qsTr("Pick the sample type")],
            ["← → ↑ ↓", qsTr("Move the cursor")],
            ["Shift + ← → ↑ ↓", qsTr("Extend the selection")],
            ["Space", qsTr("Assign the type")],
            [qsTr("Click"), qsTr("Assign the type to a well")],
            [qsTr("Drag"), qsTr("Fill a rectangle")],
            [qsTr("Shift + drag"), qsTr("Fill whole rows or columns")],
            [qsTr("Ctrl + drag"), qsTr("Select without assigning")],
            ["Esc", qsTr("Select just the cursor's well")]
        ]},
        { title: qsTr("Wells"), keys: [
            ["L", qsTr("Label the selection")],
            ["C", qsTr("Set the concentration")],
            ["D", qsTr("Serial dilution")],
            ["R", qsTr("Replicate to the right")],
            ["Shift + R", qsTr("Replicate below")],
            ["Delete", qsTr("Clear the selection")],
            ["Ctrl + C", qsTr("Copy the labels")],
            ["Ctrl + V", qsTr("Paste names as labels")],
            ["Ctrl + Z", qsTr("Undo")],
            ["Ctrl + Shift + Z", qsTr("Redo (or Ctrl + Y)")]
        ]},
        { title: qsTr("File"), keys: [
            ["Ctrl + N", qsTr("New plate")],
            ["Ctrl + O", qsTr("Open")],
            ["Ctrl + S", qsTr("Save")],
            ["Ctrl + Shift + S", qsTr("Save as")],
            ["Ctrl + E", qsTr("Export CSV, SVG, or PDF")],
            ["Ctrl + I", qsTr("Import sample names")],
            ["Ctrl + T", qsTr("Save as template")],
            ["?", qsTr("Show or hide shortcuts")],
            ["Q", qsTr("Quit")]
        ]}
    ]

    contentItem: Flickable {
        id: scroller
        implicitHeight: content.implicitHeight
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        focus: true
        ScrollBar.vertical: ScrollBar { policy: scroller.contentHeight > scroller.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }
        Keys.onPressed: (event) => {
            if (event.text === "?") {
                root.close()
                event.accepted = true
                return
            }
            // When the window is too short for every shortcut, the arrows scroll.
            const step = event.key === Qt.Key_Down ? 40 : event.key === Qt.Key_Up ? -40
                       : event.key === Qt.Key_PageDown ? height : event.key === Qt.Key_PageUp ? -height : 0
            if (step !== 0) {
                contentY = Math.max(0, Math.min(contentHeight - height, contentY + step))
                event.accepted = true
            }
        }

        ColumnLayout {
            id: content
            width: scroller.width - (scroller.contentHeight > scroller.height ? 14 : 0)
            spacing: 16
            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 18
                font.bold: true
            }
            GridLayout {
                columns: root.width >= 1060 ? 3 : 2
                columnSpacing: 32
                rowSpacing: 18
                Repeater {
                    model: root.groups
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.alignment: Qt.AlignTop
                        Layout.fillWidth: true
                        spacing: 6
                        Label {
                            text: modelData.title
                            color: theme.accent
                            font.pixelSize: 12
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 0.6
                        }
                        Repeater {
                            model: modelData.keys
                            delegate: RowLayout {
                                required property var modelData
                                spacing: 12
                                Rectangle {
                                    Layout.preferredWidth: 128
                                    Layout.preferredHeight: 22
                                    radius: Math.min(4, theme.radius)
                                    color: "#232329"
                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData[0]
                                        font.pixelSize: 12
                                        color: "#d6d6dc"
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData[1]
                                    font.pixelSize: 13
                                    color: "#b4b4bc"
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
            RowLayout {
                spacing: 14
                Repeater {
                    model: 6
                    delegate: RowLayout {
                        required property int index
                        spacing: 6
                        WellSwatch {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            type: index
                            accent: theme.accent
                        }
                        Label {
                            text: backend.typeName(index)
                            font.pixelSize: 12
                            color: "#b4b4bc"
                        }
                    }
                }
            }
        }
    }
}
