import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtTest
import "../qml/runtime" as Runtime
import "../plasma-dock-widget/contents/ui" as DockUi

TestCase {
    id: testCase
    name: "CoreAccessibility"
    when: windowShown
    visible: true
    width: Math.min(800, Screen.width)
    height: Math.min(680, Screen.height)
    property var launches: []
    property int folders: 0

    Component { id: entryComponent; DockUi.DockEntry {} }
    Component { id: editorComponent; Runtime.IconProperties {} }
    Component { id: formComponent; Runtime.StudioForm {} }
    Component { id: cardComponent; Runtime.PresetCard {} }
    Component { id: auditionComponent; Runtime.PresetAuditionBar {} }
    Component { id: spyComponent; SignalSpy {} }

    QtObject {
        id: fields
        property var values: ({enabled: false, count: 2, label: "old"})
        function fieldValue(row) { return values[row.key] || 0 }
        function setFieldValue(row, value) { const copy = Object.assign({}, values); copy[row.key] = value; values = copy }
        function fieldOptionIndex(row) { return 0 }
        function displayFieldValue(row) { return String(fieldValue(row)) }
        function formatFieldValue(row, value) { return String(value) }
        function colorPreviewValue(row) { return "transparent" }
        function colorDisplayValue(row) { return "Default" }
    }
    QtObject {
        id: transactions
        property var calls: []
        function applyIconOverrideTransaction(panelId, revision, identity, values) {
            calls.push({panelId: panelId, revision: revision, identity: identity, values: values})
            return {success: true, status: "succeeded", revision: revision + 1}
        }
    }

    function makeEntry(index, x) {
        return createTemporaryObject(entryComponent, testCase, {
            x: x, y: 20, entryIndex: index, entry: {appId: "app-" + index,
                displayName: "Application " + index, iconName: "applications-system"},
            vertical: false, baseSize: 48, magnification: 1, magnificationEnabled: false,
            hoveredIndex: -1, tileShape: "rounded", appearance: "glass", showReflection: false,
            showIndicator: true, showTooltip: false, motion: "none", motionTrigger: "hover",
            motionIntensity: 0, motionDuration: 0, reducedMotion: true, inputEnabled: true,
            editMode: false, acceptDrops: false, invoke: function(method, appId) { launches.push(appId) },
            reorder: function() {}, pinUrls: function() {}, setHoveredIndex: function() {},
            openPanelStudio: function() {}, openIconProperties: function() {},
            openFolderExpansion: function() { ++folders; return true }
        })
    }
    function init() {
        launches = []; folders = 0; transactions.calls = []
        fields.values = {enabled: false, count: 2, label: "old"}
    }
    function test_dockTabActivationAndGuards() {
        const first = makeEntry(0, 20), second = makeEntry(1, 90)
        verify(first && second)
        compare(first.Accessible.name, "Application 0")
        compare(first.Accessible.role, Accessible.Button)
        first.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Return)
        compare(launches.length, 1); compare(launches[0], "app-0")
        keyClick(Qt.Key_Tab)
        verify(second.activeFocus, "Tab reaches the next real dock entry")
        keyClick(Qt.Key_Space); compare(launches.length, 2)
        second.inputEnabled = false
        keyClick(Qt.Key_Return); compare(launches.length, 2)
        compare(second.activeFocus, false)
        second.inputEnabled = true; second.dragging = true
        second.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Return); compare(launches.length, 2)
        second.dragging = false
        second.entry = {appId: "folder", displayName: "Folder", isFolder: true}
        keyClick(Qt.Key_Return); compare(folders, 1); compare(launches.length, 2)
        second.entry = {appId: "status", displayName: "Clock", isStatus: true}
        keyClick(Qt.Key_Return); compare(launches.length, 2)
        compare(second.activeFocusOnTab, false)
    }
    function test_studioFieldsUseKeyboardAndNamesInNarrowLayout() {
        const form = createTemporaryObject(formComponent, testCase, {width: Math.min(440, width),
            height: Math.min(330, height), studio: fields, rows: [
                {kind: "switch", key: "enabled", label: "Enable magnification"},
                {kind: "spin", key: "count", label: "Icon count", from: 0, to: 10},
                {kind: "text", key: "label", label: "Panel name"}]})
        verify(form)
        const toggle = findChild(form, "studio-switch-enabled")
        const count = findChild(form, "studio-spin-count")
        const label = findChild(form, "studio-text-label")
        compare(toggle.Accessible.name, "Enable magnification")
        compare(count.Accessible.name, "Icon count")
        compare(label.Accessible.name, "Panel name")
        toggle.forceActiveFocus(Qt.TabFocusReason); keyClick(Qt.Key_Space)
        compare(fields.values.enabled, true)
        keyClick(Qt.Key_Tab); verify(count.activeFocus)
        keyClick(Qt.Key_Up); compare(fields.values.count, 3)
        label.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_A, Qt.ControlModifier); keyClick(Qt.Key_Z)
        keyClick(Qt.Key_Tab); compare(fields.values.label, "z")
    }
    function test_iconEditorTabOrderAndApplyRemainTransactional() {
        const editor = createTemporaryObject(editorComponent, testCase, {
            width: Math.min(600, width), height: Math.min(320, height),
            transactionController: transactions, editorSnapshot: {success: true, status: "loaded",
                panelId: "bottom", revision: 17, entryIdentity: "desktop.example.desktop",
                baseGlyph: "applications-system", baseLabel: "Example", override: {}, iconStyles: []}})
        verify(editor)
        const glyph = findChild(editor, "customGlyphField")
        const choose = findChild(editor, "chooseGlyphButton")
        const label = findChild(editor, "customLabelField")
        compare(glyph.Accessible.name, "Custom glyph")
        compare(label.Accessible.name, "Custom label")
        glyph.forceActiveFocus(Qt.TabFocusReason); keyClick(Qt.Key_Tab); verify(choose.activeFocus)
        keyClick(Qt.Key_Tab); verify(label.activeFocus)
        keyClick(Qt.Key_X)
        const apply = findChild(editor, "applyButton")
        apply.forceActiveFocus(Qt.TabFocusReason); keyClick(Qt.Key_Space)
        compare(transactions.calls.length, 1)
        compare(transactions.calls[0].revision, 17)
        compare(transactions.calls[0].identity, "desktop.example.desktop")
        compare(transactions.calls[0].values.customLabel, "x")
    }
    function test_presetSelectionAndAuditionActionsUseRealKeys() {
        const card = createTemporaryObject(cardComponent, testCase, {width: Math.min(600, width),
            preset: {id: "keyboard", kind: "panel", name: "Keyboard preset", builtIn: true,
                compatibility: {available: false}, description: "A keyboard fixture"}})
        verify(card)
        const selected = createTemporaryObject(spyComponent, testCase, {target: card, signalName: "selectRequested"})
        verify(card.Accessible.name.indexOf("Keyboard preset") >= 0)
        card.forceActiveFocus(Qt.TabFocusReason); keyClick(Qt.Key_Return)
        compare(selected.count, 1)
        card.visible = false
        const bar = createTemporaryObject(auditionComponent, testCase, {width: Math.min(600, width),
            height: 260, presetId: "keyboard", resourceAvailable: true,
            auditionStatus: {state: "ACTIVE", temporary: true}})
        verify(bar)
        const actions = createTemporaryObject(spyComponent, testCase, {target: bar, signalName: "actionRequested"})
        const name = findChild(bar, "preset-audition-name")
        const apply = findChild(bar, "preset-audition-apply")
        name.forceActiveFocus(Qt.TabFocusReason); keyClick(Qt.Key_Tab)
        verify(apply.activeFocus, "Tab skips disabled Save and reaches Apply")
        keyClick(Qt.Key_Space); compare(actions.count, 1); compare(actions.signalArguments[0][0], "apply")
        keyClick(Qt.Key_Escape); compare(actions.count, 2); compare(actions.signalArguments[1][0], "cancel")
    }
}
