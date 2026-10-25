import QtQuick
import QtQuick.Dialogs
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
    // The panels a profile shows: each renamed, edited or removed in the
    // profile, a built-in edge panel hidden instead; removing asks first.
    function test_profilePanelsAreRenamedEditedAndRemovedInTheProfile() {
        const service = { profilePanels: function(id) { return id !== "profile-first" ? [] : [
            {id: "free-1", name: "Ring panel", edge: "free", builtIn: false, shown: true},
            {id: "bottom", name: "Bottom panel", edge: "bottom", builtIn: true, shown: true},
            {id: "top", name: "Top panel", edge: "top", builtIn: true, shown: false}]; } };
        const item = page({profiles: [{id: "profile-first", name: "Desk", revision: 3, panelCount: 3}],
                           service: service});
        const requests = createTemporaryObject(spyComponent, testCase,
            {target: item, signalName: "panelActionRequested"});
        verify(requests.valid);
        verify(findChild(item, "profile-panel-name-free-1") !== null);
        verify(findChild(item, "profile-panel-name-bottom") !== null);
        verify(findChild(item, "profile-panel-name-top") === null);
        mouseClick(findChild(item, "profile-panel-edit-free-1"));
        const sent = function(index) { return Array.prototype.slice.call(requests.signalArguments[index]); };
        compare(sent(0), ["edit", "profile-first", 3, "free-1", ""]);
        const name = findChild(item, "profile-panel-name-free-1");
        name.text = "  Work dock ";
        name.editingFinished();
        compare(sent(1), ["rename", "profile-first", 3, "free-1", "Work dock"]);
        // An unchanged or empty name renames nothing.
        name.text = "  ";
        name.editingFinished();
        compare(requests.count, 2);
        compare(name.text, "Ring panel");
        compare(findChild(item, "profile-panel-remove-bottom").text, "Hide");
        mouseClick(findChild(item, "profile-panel-remove-free-1"));
        compare(requests.count, 2);
        const question = findChild(item, "profile-panel-remove-question");
        verify(question !== null);
        verify(question.text.indexOf("Ring panel") >= 0 && question.text.indexOf("Desk") >= 0);
        question.buttonClicked(MessageDialog.Ok, MessageDialog.AcceptRole);
        compare(sent(2), ["remove", "profile-first", 3, "free-1", ""]);
        // Blocked while Panel Studio holds changes.
        item.actionsBlocked = true;
        verify(!findChild(item, "profile-panel-edit-free-1").enabled);
        verify(!findChild(item, "profile-panel-remove-free-1").enabled);
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
    function test_summaryCountsTheShownPanels() {
        // One free panel saved showed "4 panels",
        // counting the hidden built-in edge panels every arrangement keeps.
        const item = page({profiles: [{id: "profile-first", name: "Desk", revision: 1, panelCount: 4,
            shownPanelCount: 1}]});
        compare(findChild(item, "profile-summary").text.indexOf("1 panel · revision 1"), 0);
        item.profiles = [{id: "profile-first", name: "Desk", revision: 2, panelCount: 5, shownPanelCount: 2}];
        compare(findChild(item, "profile-summary").text.indexOf("2 panels · revision 2"), 0);
        item.profiles = [{id: "profile-first", name: "Desk", revision: 3, panelCount: 3}];
        compare(findChild(item, "profile-summary").text.indexOf("3 panels · revision 3"), 0,
                "a profile listed without the shown count keeps its panel count");
    }
    function test_freePanelsAnApplyWouldCreateAreAnnounced() {
        const item = page({profiles: [{id: "profile-first", name: "Desk", revision: 4, panelCount: 2}]});
        verify(!findChild(item, "profile-free-panel-notice").visible);
        verify(findChild(item, "profile-summary").text.indexOf("free panel") < 0);
        item.profiles = [{id: "profile-first", name: "Desk", revision: 4, panelCount: 3,
            freePanelCount: 1, freePanelsToCreate: 1, freePanelLimit: 4}];
        verify(findChild(item, "profile-summary").text.indexOf("Apply creates 1 free panel on") >= 0);
        verify(!findChild(item, "profile-free-panel-notice").visible);
        item.profiles = [{id: "profile-first", name: "Desk", revision: 4, panelCount: 24,
            freePanelCount: 21, freePanelsToCreate: 21, freePanelLimit: 4}];
        verify(findChild(item, "profile-summary").text.indexOf("Apply creates 21 free panels on") >= 0);
        verify(findChild(item, "profile-free-panel-notice").visible);
        verify(findChild(item, "profile-free-panel-notice").text.indexOf("21") >= 0);
        verify(findChild(item, "profile-free-panel-notice").text.indexOf("4") >= 0);
        verify(button(item, "apply").enabled);
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
