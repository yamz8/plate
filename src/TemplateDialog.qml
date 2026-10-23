import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Names the current layout and keeps it as a template for new plates.
Dialog {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 420
    title: qsTr("Save as template")
    Material.roundedScale: theme.radius

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
        standardButtons: Dialog.Ok | Dialog.Cancel
        background: Item {}
    }

    readonly property string name: nameField.text.trim()
    // The name becomes a file name.
    readonly property bool valid: name !== "" && !name.startsWith(".") && name.indexOf("/") < 0 && name.indexOf("\\") < 0
    readonly property bool replaces: backend.templates.some(t => t.name === name && !t.builtIn)

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: qsTr("Template name")
            onAccepted: if (root.valid) root.accept()
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: root.replaces || (!root.valid && root.name !== "") ? "#f7768e" : "#b4b4bc"
            text: root.replaces ? qsTr("Replaces your template %1.").arg(root.name)
                : !root.valid && root.name !== "" ? qsTr("A template name can't start with a dot or hold a slash.")
                : qsTr("Keeps the types, labels, concentrations, and unit. Pick it under New plate.")
        }
    }

    onAboutToShow: {
        nameField.text = backend.filePath !== "" ? backend.fileName.replace(/\.plate$/, "") : ""
        nameField.forceActiveFocus()
        nameField.selectAll()
    }
    onAccepted: if (valid) backend.saveTemplate(name)
    Component.onCompleted: footer.standardButton(DialogButtonBox.Ok).enabled = Qt.binding(() => root.valid)
}
