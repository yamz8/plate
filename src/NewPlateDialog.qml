import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 400
    title: qsTr("New plate")
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
        standardButtons: Dialog.Cancel
        background: Item {}
    }

    function choose(wells) {
        close()
        backend.newPlate(wells)
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
            KeyNavigation.down: large
        }
        Button {
            id: large
            Layout.fillWidth: true
            highlighted: activeFocus
            text: qsTr("384 wells  ·  16 rows × 24 columns")
            onClicked: root.choose(384)
            Keys.onReturnPressed: root.choose(384)
            KeyNavigation.up: small
        }
    }
    onOpened: (backend.rows === 16 ? large : small).forceActiveFocus()
}
