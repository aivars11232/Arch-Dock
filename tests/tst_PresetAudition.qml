import QtQuick
import QtQuick.Controls
import QtTest
import "../qml/runtime" as Runtime

TestCase {
    id: testCase
    name: "PresetAudition"
    when: windowShown
    width: 880
    height: 480
    visible: true

    Component {
        id: barComponent
        Runtime.PresetAuditionBar { width: 850 }
    }
    Component { id: spyComponent; SignalSpy {} }

    function bar(properties) {
        const item = createTemporaryObject(barComponent, testCase, properties || {});
        verify(item !== null);
        verify(waitForRendering(item));
        return item;
    }
    function spy(item) {
        const result = createTemporaryObject(spyComponent, testCase,
            { target: item, signalName: "actionRequested" });
        verify(result.valid);
        return result;
    }
    function button(item, action) { return findChild(item, "preset-audition-" + action); }

    function test_idleCannotCommitOrSave() {
        const item = bar({ presetId: "obsidian-dock", resourceAvailable: true });
        const requests = spy(item);
        verify(!button(item, "apply").enabled);
        verify(!button(item, "cancel").enabled);
        verify(!button(item, "restore").enabled);
        verify(button(item, "default").enabled);
        mouseClick(button(item, "default"));
        compare(requests.signalArguments[0][0], "set-default");
        item.selectedDefault = true;
        compare(button(item, "default").text, "Remove as Default");
        mouseClick(button(item, "default"));
        compare(requests.signalArguments[1][0], "remove-default");
        item.resourceAvailable = false;
        verify(!button(item, "default").enabled);
    }

    function test_activeActionsAndAccessibility() {
        const item = bar({ auditionStatus: { state: "ACTIVE", temporary: true },
            presetId: "obsidian-dock", resourceAvailable: true });
        const requests = spy(item);
        verify(findChild(item, "preset-audition-status").text.indexOf("temporary") >= 0);
        verify(findChild(item, "preset-audition-visibility").visible);
        for (const action of ["apply", "cancel", "revert", "restore", "default"]) {
            verify(button(item, action).enabled);
            verify(button(item, action).Accessible.name.length > 0);
        }
        verify(!button(item, "save").enabled);
        findChild(item, "preset-audition-name").text = "  Custom dock  ";
        verify(button(item, "save").enabled);
        mouseClick(button(item, "save"));
        compare(requests.signalArguments[0][0], "save-custom");
        compare(requests.signalArguments[0][1], "Custom dock");
        for (const pair of [["apply", "apply"], ["restore", "restore-built-in"],
                            ["revert", "revert"], ["cancel", "cancel"]]) {
            mouseClick(button(item, pair[0]));
            compare(requests.signalArguments[requests.count - 1][0], pair[1]);
        }
        item.forceActiveFocus();
        keyClick(Qt.Key_Escape);
        compare(requests.signalArguments[requests.count - 1][0], "cancel");
    }

    function test_guardsDisableMutationAndExplainConflict_data() {
        return [{ tag: "edit", guard: "edit-mode-active", text: "Edit Mode" },
                { tag: "popup", guard: "popup-open", text: "popup" },
                { tag: "drag", guard: "drag-active", text: "drag" }];
    }
    function test_guardsDisableMutationAndExplainConflict(data) {
        const item = bar({ auditionStatus: { state: "ACTIVE" }, guardError: data.guard });
        verify(!button(item, "apply").enabled);
        verify(!button(item, "restore").enabled);
        verify(button(item, "cancel").enabled);
        verify(button(item, "revert").enabled);
        verify(findChild(item, "preset-audition-status").text.indexOf(data.text) >= 0);
    }

    function test_blockedAndTransitionStatesCannotCommit() {
        const item = bar({ auditionStatus: { state: "BLOCKED", errorCode: "ownership-mismatch" } });
        verify(!button(item, "apply").enabled);
        verify(!button(item, "default").enabled);
        verify(button(item, "cancel").enabled);
        verify(findChild(item, "preset-audition-status").text.indexOf("ownership-mismatch") >= 0);
        for (const state of ["PREPARING", "COMMITTING", "ROLLING_BACK", "COMMITTED"]) {
            item.auditionStatus = { state: state };
            verify(!button(item, "apply").enabled);
            verify(!button(item, "default").enabled);
            verify(!button(item, "cancel").enabled);
        }
    }
}
