import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A blank plate of either size, or one laid out from a template.
Dialog {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 440
    title: qsTr("New plate")
    Material.roundedScale: theme.radius

    signal saveTemplateRequested()

    background: Rectangle {
        color: "#1b1b20"
        border.color: "#2c2c34"
        radius: theme.radius
    }
    Overlay.modal: Rectangle { color: "#a0000000" }
    header: Label {
        text: root.title
        padding: 24
        bottomPadding: 4
        font.pixelSize: 17
        font.bold: true
        elide: Text.ElideRight
    }
    footer: DialogButtonBox {
        background: Item {}
        Button {
            text: qsTr("Save this layout as a template…")
            flat: true
            focusPolicy: Qt.NoFocus
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: { root.close(); root.saveTemplateRequested() }
        }
        Button {
            text: qsTr("Cancel")
            flat: true
            focusPolicy: Qt.NoFocus
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }

    function choose(wells) {
        close()
        backend.newPlate(wells)
    }
    function chooseTemplate(name) {
        close()
        backend.newFromTemplate(name)
    }
    // Up and down walk the choices, as they would in a list.
    function step(item, forward) {
        const next = item.nextItemInFocusChain(forward)
        if (next)
            next.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Button {
            id: small
            Layout.fillWidth: true
            highlighted: activeFocus
            text: qsTr("96 wells  ·  8 rows × 12 columns")
            onClicked: root.choose(96)
            Keys.onReturnPressed: root.choose(96)
            Keys.onDownPressed: root.step(small, true)
        }
        Button {
            id: large
            Layout.fillWidth: true
            highlighted: activeFocus
            text: qsTr("384 wells  ·  16 rows × 24 columns")
            onClicked: root.choose(384)
            Keys.onReturnPressed: root.choose(384)
            Keys.onUpPressed: root.step(large, false)
            Keys.onDownPressed: root.step(large, true)
        }

        Label {
            Layout.topMargin: 10
            visible: backend.templates.length > 0
            text: qsTr("From a template")
            color: "#8b8b96"
            font.pixelSize: 12
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
        }
        Repeater {
            model: backend.templates
            delegate: RowLayout {
                id: entry
                required property var modelData
                // Deleting takes a second click, since a saved template is the person's own file.
                property bool confirming: false
                Layout.fillWidth: true
                spacing: 4
                Button {
                    id: use
                    Layout.fillWidth: true
                    highlighted: activeFocus
                    text: qsTr("%1  ·  %2 wells").arg(entry.modelData.name).arg(entry.modelData.wells)
                    onClicked: root.chooseTemplate(entry.modelData.name)
                    Keys.onReturnPressed: root.chooseTemplate(entry.modelData.name)
                    Keys.onUpPressed: root.step(use, false)
                    Keys.onDownPressed: root.step(use, true)
                }
                Button {
                    visible: !entry.modelData.builtIn
                    flat: true
                    focusPolicy: Qt.NoFocus
                    text: entry.confirming ? qsTr("Delete") : "×"
                    Material.foreground: entry.confirming ? "#f7768e" : "#8b8b96"
                    ToolTip.visible: hovered && !entry.confirming
                    ToolTip.text: qsTr("Delete this template")
                    onClicked: {
                        if (entry.confirming)
                            backend.deleteTemplate(entry.modelData.name)
                        else
                            entry.confirming = true
                    }
                    onHoveredChanged: if (!hovered) entry.confirming = false
                }
            }
        }
    }
    onOpened: (backend.rows === 16 ? large : small).forceActiveFocus()
}
