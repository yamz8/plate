import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Plate
import "Numbers.js" as Numbers

// The panel beside the plate: what's open, the sample types, and the selected wells.
Rectangle {
    id: root
    color: "#131316"
    Material.roundedScale: theme.radius

    signal diluteRequested()
    signal newRequested()
    signal openRequested()
    signal saveRequested()
    signal importRequested()
    // The person is done typing; the plate takes the keyboard back.
    signal done()

    function editLabel() { labelField.forceActiveFocus(); labelField.selectAll() }
    function editConcentration() { concentrationField.forceActiveFocus(); concentrationField.selectAll() }

    function formatNumber(value) {
        translator.language
        return Numbers.format(Qt.locale(), value)
    }
    // Leaves a field without saving what was typed, and shows the selection's value again.
    function cancel(field) {
        field.edited = false
        done()
        refresh()
    }

    // Fields show the selection's shared values; while one has the keyboard it's left alone.
    function refresh() {
        const info = backend.selectionInfo()
        if (!labelField.activeFocus) {
            labelField.text = info.label || ""
            // A long label shows from its start, not from where the cursor ended up.
            labelField.cursorPosition = 0
            labelField.mixed = info.labelMixed === true
        }
        if (!concentrationField.activeFocus) {
            concentrationField.text = info.concentration !== undefined ? formatNumber(info.concentration) : ""
            concentrationField.mixed = info.concentrationMixed === true
        }
        if (!unitField.activeFocus)
            unitField.text = backend.unit
        selectionTitle.text = qsTr("%1  ·  %n well(s)", "", info.count).arg(info.name)
    }
    Component.onCompleted: refresh()
    Connections {
        target: backend
        function onSelectionChanged() { root.refresh() }
        function onPlateChanged() { root.refresh() }
    }
    Connections {
        target: translator
        function onChanged() { root.refresh() }
    }

    // Scrolls only when the window is too short for the whole panel.
    Flickable {
        id: scroller
        anchors.fill: parent
        contentHeight: Math.max(column.implicitHeight + 36, height)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: scroller.contentHeight > scroller.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

        ColumnLayout {
            id: column
            x: 18
            y: 18
            width: scroller.width - 36
            height: scroller.contentHeight - 36
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: backend.fileName + (backend.dirty ? "  •" : "")
                font.pixelSize: 17
                font.bold: true
                elide: Text.ElideMiddle
            }
            Label {
                Layout.fillWidth: true
                Layout.topMargin: -6
                text: qsTr("%1-well plate").arg(backend.rows * backend.columns)
                color: "#8b8b96"
                font.pixelSize: 13
            }

            Label {
                Layout.topMargin: 8
                text: qsTr("Sample type")
                color: "#8b8b96"
                font.pixelSize: 12
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.6
            }

            Repeater {
                model: 6
                delegate: Rectangle {
                    id: typeRow
                    required property int index
                    readonly property bool current: backend.currentType === index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    radius: Math.min(4, theme.radius)
                    color: current ? Qt.rgba(Material.accentColor.r, Material.accentColor.g, Material.accentColor.b, 0.16)
                         : typeHover.hovered ? "#1d1d22" : "transparent"
                    border.color: current ? theme.accent : "transparent"

                    HoverHandler { id: typeHover }
                    TapHandler { onTapped: { backend.currentType = typeRow.index; root.done() } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10
                        WellSwatch {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            type: typeRow.index
                            accent: theme.accent
                        }
                        Label {
                            Layout.fillWidth: true
                            text: (backend.revision, backend.typeName(typeRow.index))
                            elide: Text.ElideRight
                            font.pixelSize: 14
                        }
                        Label {
                            text: backend.typeCounts[typeRow.index]
                            color: "#8b8b96"
                            font.pixelSize: 13
                        }
                        Rectangle {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            radius: Math.min(4, theme.radius)
                            color: "#232329"
                            Label {
                                anchors.centerIn: parent
                                text: typeRow.index + 1
                                font.pixelSize: 11
                                color: "#b4b4bc"
                            }
                        }
                    }
                }
            }

            Label {
                Layout.topMargin: 12
                text: qsTr("Selection")
                color: "#8b8b96"
                font.pixelSize: 12
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.6
            }
            Label {
                id: selectionTitle
                Layout.fillWidth: true
                font.pixelSize: 14
                elide: Text.ElideRight
            }

            TextField {
                id: labelField
                property bool mixed: false
                // Only typing changes the wells; focusing the field and leaving it doesn't.
                property bool edited: false
                Layout.fillWidth: true
                placeholderText: mixed ? qsTr("Label (mixed)") : qsTr("Label")
                onTextEdited: edited = true
                onEditingFinished: {
                    if (!edited)
                        return
                    edited = false
                    backend.setSelectionLabel(text)
                }
                onAccepted: root.done()
                Keys.onEscapePressed: root.cancel(labelField)
            }
            TextField {
                id: concentrationField
                property bool mixed: false
                property bool edited: false
                Layout.fillWidth: true
                placeholderText: (mixed ? qsTr("Concentration (mixed)") : qsTr("Concentration"))
                                 + (backend.unit ? " (" + backend.unit + ")" : "")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                onTextEdited: edited = true
                onEditingFinished: {
                    if (!edited)
                        return
                    edited = false
                    if (text.trim() === "") {
                        backend.setSelectionConcentration("")
                        return
                    }
                    const value = Numbers.parse(Qt.locale(), text)
                    // The backend reports a number it can't take, such as a negative one.
                    backend.setSelectionConcentration(isNaN(value) ? -1 : value)
                    root.refresh()
                }
                onAccepted: root.done()
                Keys.onEscapePressed: root.cancel(concentrationField)
            }
            TextField {
                id: unitField
                property bool edited: false
                Layout.fillWidth: true
                placeholderText: qsTr("Unit for the plate, such as µM")
                onTextEdited: edited = true
                onEditingFinished: {
                    if (!edited)
                        return
                    edited = false
                    if (text.trim() !== backend.unit)
                        backend.setUnit(text)
                }
                onAccepted: root.done()
                Keys.onEscapePressed: root.cancel(unitField)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 2
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Dilute…")
                    flat: true
                    focusPolicy: Qt.NoFocus
                    font.pixelSize: 13
                    leftPadding: 6
                    rightPadding: 6
                    onClicked: root.diluteRequested()
                }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Replicate →")
                    flat: true
                    focusPolicy: Qt.NoFocus
                    font.pixelSize: 13
                    leftPadding: 6
                    rightPadding: 6
                    onClicked: { backend.replicate(0); root.done() }
                }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Replicate ↓")
                    flat: true
                    focusPolicy: Qt.NoFocus
                    font.pixelSize: 13
                    leftPadding: 6
                    rightPadding: 6
                    onClicked: { backend.replicate(2); root.done() }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: -8
                spacing: 2
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Import names…")
                    flat: true
                    focusPolicy: Qt.NoFocus
                    font.pixelSize: 13
                    leftPadding: 6
                    rightPadding: 6
                    onClicked: root.importRequested()
                }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Clear")
                    flat: true
                    focusPolicy: Qt.NoFocus
                    font.pixelSize: 13
                    leftPadding: 6
                    rightPadding: 6
                    onClicked: { backend.clearSelection(); root.done() }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                Button { Layout.fillWidth: true; flat: true; focusPolicy: Qt.NoFocus; text: qsTr("New"); onClicked: root.newRequested() }
                Button { Layout.fillWidth: true; flat: true; focusPolicy: Qt.NoFocus; text: qsTr("Open"); onClicked: root.openRequested() }
                Button { Layout.fillWidth: true; flat: true; focusPolicy: Qt.NoFocus; text: qsTr("Save"); onClicked: root.saveRequested() }
                Button { Layout.fillWidth: true; flat: true; focusPolicy: Qt.NoFocus; text: qsTr("Export"); onClicked: { root.done(); backend.exportPlate() } }
            }
        }
    }
}
