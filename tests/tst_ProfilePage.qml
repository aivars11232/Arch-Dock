import QtQuick
import QtTest
import "../qml/runtime" as Runtime

TestCase {
    id: testCase
    name: "ProfilePage"
    when: windowShown
    width: 920
    height: 600
    visible: true
    Component { id: pageComponent; Runtime.ProfilePage { width: 900; height: 560 } }
    Component { id: spyComponent; SignalSpy {} }
    function page(properties) {
        const item = createTemporaryObject(pageComponent, testCase, properties || {});
        verify(item !== null);
        verify(waitForRendering(item));
        return item;
    }
    function button(item, action) { return findChild(item, "profile-" + action); }
    function spy(item) {
        const value = createTemporaryObject(spyComponent, testCase, {target: item, signalName: "actionRequested"});
        verify(value.valid);
        return value;
    }
    function test_emptyStoreCannotApplyAndSelectionDoesNotApply() {
        const item = page();
        const requests = spy(item);
        verify(!button(item, "apply").enabled);
        verify(!button(item, "save").enabled);
        verify(!button(item, "delete").enabled);
        item.profiles = [{id: "profile-first", name: "Desk", revision: 4, panelCount: 2}];
        verify(button(item, "apply").enabled);
        compare(requests.count, 0);
        item.selectedId = "profile-first";
        compare(requests.count, 0);
    }
    function test_explicitButtonsCarryCurrentStableIdentity() {
        const item = page({profiles: [{id: "profile-first", name: "Desk", revision: 4, panelCount: 2}]});
        const requests = spy(item);
        findChild(item, "profile-name").text = "  Work  ";
        for (const action of ["create", "save", "apply", "rename", "duplicate", "delete"]) {
            const control = button(item, action);
            verify(control.enabled);
            verify(control.Accessible.name.length > 0);
            mouseClick(control);
            const args = requests.signalArguments[requests.count - 1];
            compare(args[0], action);
            compare(args[1], "profile-first");
            compare(args[2], 4);
            if (["create", "rename", "duplicate"].indexOf(action) >= 0) compare(args[3], "Work");
        }
        compare(requests.count, 6);
    }
    function test_draftsAndTransactionsDisableOrdinaryActions() {
        const item = page({profiles: [{id: "profile-first", name: "Desk", revision: 1, panelCount: 1}], actionsBlocked: true});
        findChild(item, "profile-name").text = "Work";
        for (const action of ["create", "save", "apply", "rename", "duplicate", "delete", "import", "export"])
            verify(!button(item, action).enabled);
        item.actionsBlocked = false;
        item.applyStatus = {state: "BLOCKED", recoveryRequired: true, errorCode: "profile-partial-rollback"};
        verify(!button(item, "apply").enabled);
        verify(button(item, "recover").enabled);
        verify(findChild(item, "profile-recovery-status").text.indexOf("profile-partial-rollback") >= 0);
        const requests = spy(item);
        mouseClick(button(item, "recover"));
        compare(requests.signalArguments[0][0], "recover");
        item.actionsBlocked = true;
        verify(!button(item, "recover").enabled);
    }
    function test_shortcutControlsAreExplicitAndConflictsAreVisible() {
        const item = page({shortcutsOnly: true,
            profiles: [{id: "profile-first", name: "Desk", revision: 4, panelCount: 2}]});
        const requests = spy(item);
        const enable = button(item, "shortcuts-enabled");
        verify(enable.enabled);
        compare(requests.count, 0);
        findChild(item, "profile-shortcut-sequence").text = "Ctrl+Alt+F9";
        mouseClick(button(item, "shortcut-set"));
        compare(requests.signalArguments[0][0], "shortcut-set");
        compare(requests.signalArguments[0][1], "profile-first");
        compare(requests.signalArguments[0][3], "Ctrl+Alt+F9");
        mouseClick(enable);
        compare(requests.signalArguments[1][0], "shortcuts-enabled");
        compare(requests.signalArguments[1][3], "true");
        item.shortcutStatus = {enabled: true, bindings: [{profileId: "profile-first", sequence: "Ctrl+Alt+F9",
            registeredKeys: ["Ctrl+Alt+F9"]}], errorCode: "profile-shortcut-conflict", conflicts: [{name: "Show Desktop"}]};
        compare(requests.count, 2);
        verify(findChild(item, "profile-shortcut-registration").text.indexOf("Ctrl+Alt+F9") >= 0);
        verify(findChild(item, "profile-shortcut-conflict").text.indexOf("Show Desktop") >= 0);
        mouseClick(button(item, "shortcut-clear"));
        compare(requests.signalArguments[2][0], "shortcut-clear");
        item.actionsBlocked = true;
        for (const action of ["shortcuts-enabled", "shortcut-set", "shortcut-clear"])
            verify(!button(item, action).enabled);
    }
}
