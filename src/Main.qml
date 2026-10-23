import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Plate

ApplicationWindow {
    id: win
    width: 1180
    height: 720
    minimumWidth: 820
    minimumHeight: 520
    visible: true
    title: (backend.dirty ? "• " : "") + backend.fileName + " — Plate"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    // What to do once unsaved work is saved or discarded: "quit", "new", or "open".
    property string pendingAction: ""
    property bool quitting: false
    // Single-key shortcuts stand aside while a field has the keyboard or a dialog is up.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.hasOwnProperty("cursorPosition")
    readonly property bool dialogOpen: dilution.visible || unsaved.visible || newPlate.visible || help.visible
    readonly property bool keysFree: !typing && !dialogOpen

    function focusPlate() { plateArea.forceActiveFocus() }

    // Runs action now, or after asking about unsaved work.
    function guard(action) {
        if (backend.dirty) {
            pendingAction = action
            unsaved.open()
        } else {
            perform(action)
        }
    }
    // A save of its own; a quit, new, or open the unsaved dialog was waiting on no longer applies.
    function save() {
        pendingAction = ""
        backend.save()
    }
    function perform(action) {
        pendingAction = ""
        if (action === "quit") {
            quitting = true
            Qt.quit()
        } else if (action === "new") {
            newPlate.open()
        } else if (action === "open") {
            backend.open()
        }
    }

    onClosing: (close) => {
        if (!quitting && backend.dirty) {
            close.accepted = false
            guard("quit")
        }
    }

    Connections {
        target: backend
        function onSaved() {
            if (win.pendingAction !== "")
                win.perform(win.pendingAction)
        }
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing && !unsaved.visible && !dilution.visible && !newPlate.visible; onActivated: help.visible ? help.close() : help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: win.guard("quit") }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.assignSelection() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.currentType = 0 }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.currentType = 1 }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.currentType = 2 }
    Shortcut { sequence: "4"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.currentType = 3 }
    Shortcut { sequence: "5"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.currentType = 4 }
    Shortcut { sequence: "6"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.currentType = 5 }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.moveCursor(0, -1) }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.moveCursor(0, 1) }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.moveCursor(-1, 0) }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.moveCursor(1, 0) }
    Shortcut { sequence: "Shift+Left"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.moveCursor(0, -1, true) }
    Shortcut { sequence: "Shift+Right"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.moveCursor(0, 1, true) }
    Shortcut { sequence: "Shift+Up"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.moveCursor(-1, 0, true) }
    Shortcut { sequence: "Shift+Down"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.moveCursor(1, 0, true) }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: { backend.setCursor(backend.cursorRow, backend.cursorColumn); backend.clearMessage() } }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: dilution.open() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.replicate(0) }
    Shortcut { sequence: "Shift+R"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.replicate(2) }
    Shortcut { sequence: "L"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: sidebar.editLabel() }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: sidebar.editConcentration() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.undo() }
    Shortcut { sequences: ["Ctrl+Shift+Z", StandardKey.Redo]; context: Qt.ApplicationShortcut; enabled: win.keysFree; onActivated: backend.redo() }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen; onActivated: { win.focusPlate(); win.save() } }
    Shortcut { sequence: "Ctrl+Shift+S"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen; onActivated: { win.focusPlate(); win.pendingAction = ""; backend.saveAs() } }
    Shortcut { sequence: "Ctrl+E"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen; onActivated: { win.focusPlate(); backend.exportPlate() } }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen; onActivated: { win.focusPlate(); win.guard("open") } }
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen; onActivated: { win.focusPlate(); win.guard("new") } }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Item {
                    id: plateArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    focus: true
                    // Where the keyboard map can't turn Shift+/ into "?", the
                    // shortcut never matches; catch the typed character instead.
                    Keys.onPressed: (event) => {
                        if (event.text === "?" && win.keysFree) {
                            help.open()
                            event.accepted = true
                        }
                    }

                    PlateView {
                        id: view
                        anchors.fill: parent
                        anchors.margins: 20
                        source: backend
                        accent: theme.accent
                        radius: theme.radius
                        // The plate is a physical object, so it stays left to right in every language.
                        LayoutMirroring.enabled: false

                        MouseArea {
                            id: pointer
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            preventStealing: true

                            // "assign" fills the dragged rectangle, "lines" whole rows or
                            // columns (Shift), and "select" only selects (Ctrl or right button).
                            property string mode: ""
                            property point start: Qt.point(-1, -1)

                            function update(mouse) {
                                const here = view.nearestWell(mouse.x, mouse.y)
                                if (mode === "lines") {
                                    // Mostly sideways fills the rows dragged over, mostly
                                    // up or down the columns; a Shift+click fills its row.
                                    if (Math.abs(here.x - start.x) >= Math.abs(here.y - start.y))
                                        backend.select(start.y, 0, here.y, backend.columns - 1)
                                    else
                                        backend.select(0, start.x, backend.rows - 1, here.x)
                                } else {
                                    backend.select(start.y, start.x, here.y, here.x)
                                }
                            }

                            onPressed: (mouse) => {
                                win.focusPlate()
                                const well = view.wellAt(mouse.x, mouse.y)
                                if (well.x < 0) {
                                    mode = ""
                                    return
                                }
                                start = well
                                mode = mouse.button === Qt.RightButton || (mouse.modifiers & Qt.ControlModifier) ? "select"
                                     : (mouse.modifiers & Qt.ShiftModifier) ? "lines" : "assign"
                                update(mouse)
                            }
                            onPositionChanged: (mouse) => {
                                view.hover = view.wellAt(mouse.x, mouse.y)
                                if (pressed && mode !== "")
                                    update(mouse)
                            }
                            onReleased: (mouse) => {
                                if (mode === "assign" || mode === "lines")
                                    backend.assignSelection()
                                mode = ""
                            }
                            onExited: view.hover = Qt.point(-1, -1)
                        }
                    }
                }

                Sidebar {
                    id: sidebar
                    Layout.preferredWidth: 320
                    Layout.fillHeight: true
                    onDiluteRequested: dilution.open()
                    onNewRequested: win.guard("new")
                    onOpenRequested: win.guard("open")
                    onSaveRequested: { win.focusPlate(); win.save() }
                    onDone: win.focusPlate()
                }
            }

            // The status line: the hovered well, else the latest message, else the selection.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                color: "#131316"

                // A well hovered on the previous plate may not exist on this one.
                readonly property var hovered: view.hover.x >= 0 && view.hover.x < backend.columns && view.hover.y < backend.rows ? (backend.revision, backend.wellInfo(view.hover.y, view.hover.x)) : null

                function describe(info) {
                    let parts = [info.name, info.typeName]
                    if (info.label)
                        parts.push(info.label)
                    if (info.concentration !== undefined) {
                        translator.language
                        const value = Number(info.concentration).toLocaleString(Qt.locale(), 'g', 6)
                        parts.push(backend.unit ? value + " " + backend.unit : value)
                    }
                    return parts.join("  ·  ")
                }

                Rectangle {
                    width: 3
                    height: parent.height
                    color: backend.messageIsError ? "#f7768e" : "transparent"
                    visible: !parent.hovered && backend.message !== ""
                }

                Label {
                    anchors.left: parent.left
                    anchors.right: hint.left
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    color: !parent.hovered && backend.messageIsError ? "#f7768e" : "#d6d6dc"
                    text: parent.hovered ? parent.describe(parent.hovered)
                        : backend.message !== "" ? backend.message
                        : (backend.selection, backend.revision, qsTr("%1 selected  ·  %n well(s)", "", backend.selectionInfo().count).arg(backend.selectionName()))
                }
                Label {
                    id: hint
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 12
                    color: "#7a7a85"
                    text: qsTr("Press ? for shortcuts")
                }
            }
        }

        DilutionDialog {
            id: dilution
            onClosed: Qt.callLater(win.focusPlate)
        }
        UnsavedDialog {
            id: unsaved
            onSaveChosen: backend.save()
            onDiscardChosen: win.perform(win.pendingAction)
            onCancelChosen: win.pendingAction = ""
            onClosed: Qt.callLater(win.focusPlate)
        }
        NewPlateDialog {
            id: newPlate
            onClosed: Qt.callLater(win.focusPlate)
        }
        ShortcutOverlay {
            id: help
            onClosed: Qt.callLater(win.focusPlate)
        }
    }
}
