import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Numbers.js" as Numbers

// A serial dilution across the selection: the first well in the chosen
// direction gets the start concentration, each next one start / factor^step.
Dialog {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 420
    title: (backend.selection, qsTr("Serial dilution across %1").arg(backend.selectionName()))
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

    property real lastStart: 100
    property real lastFactor: 2

    function parse(text) { return Numbers.parse(Qt.locale(), text) }
    function format(value) { return Numbers.format(Qt.locale(), value) }
    readonly property real start: parse(startField.text)
    readonly property real factor: parse(factorField.text)
    readonly property bool valid: start > 0 && factor > 0
    readonly property int steps: direction.currentIndex < 2 ? backend.selection.width : backend.selection.height

    // The run of concentrations the dialog will write, shortened in the middle.
    readonly property string preview: {
        if (!valid)
            return qsTr("Enter a start concentration and a factor above zero.")
        let values = []
        for (let i = 0; i < steps; ++i)
            values.push(format(start / Math.pow(factor, i)))
        if (values.length > 6)
            values = values.slice(0, 3).concat(["…"], values.slice(-2))
        return values.join(", ") + (backend.unit ? " " + backend.unit : "")
    }

    onAboutToShow: {
        const info = backend.selectionInfo()
        startField.text = format(info.concentration !== undefined && info.concentration > 0 ? info.concentration : lastStart)
        factorField.text = format(lastFactor)
        // Wider than tall runs across the row; otherwise down the column.
        direction.currentIndex = backend.selection.width >= backend.selection.height ? 0 : 2
        startField.forceActiveFocus()
        startField.selectAll()
    }
    onAccepted: {
        if (!valid)
            return
        lastStart = start
        lastFactor = factor
        backend.dilute(start, factor, direction.currentIndex)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        TextField {
            id: startField
            Layout.fillWidth: true
            placeholderText: qsTr("Start concentration") + (backend.unit ? " (" + backend.unit + ")" : "")
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onAccepted: root.accept()
        }
        TextField {
            id: factorField
            Layout.fillWidth: true
            placeholderText: qsTr("Dilution factor (2 halves each step)")
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onAccepted: root.accept()
        }
        ComboBox {
            id: direction
            Layout.fillWidth: true
            model: [qsTr("Left to right"), qsTr("Right to left"), qsTr("Top to bottom"), qsTr("Bottom to top")]
        }
        Label {
            Layout.fillWidth: true
            text: root.preview
            wrapMode: Text.Wrap
            color: root.valid ? "#b4b4bc" : "#f7768e"
            font.pixelSize: 13
        }
    }

    Component.onCompleted: footer.standardButton(DialogButtonBox.Ok).enabled = Qt.binding(() => root.valid)
}
