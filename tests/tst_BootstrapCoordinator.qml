import QtQuick
import QtTest
import "../plasma-widget/contents/ui"

TestCase {
    id: testCase

    name: "BootstrapCoordinator"
    when: windowShown

    Component {
        id: coordinatorComponent

        BootstrapCoordinator {
            baseRetryInterval: 1
            maximumRetryInterval: 2
        }
    }

    SignalSpy {
        id: requestSpy

        signalName: "request"
    }

    function createCoordinator(properties) {
        const coordinator = createTemporaryObject(
            coordinatorComponent,
            testCase,
            properties || {});
        verify(coordinator !== null);
        requestSpy.target = coordinator;
        requestSpy.clear();
        wait(1);
        return coordinator;
    }

    function makeReady(coordinator, panelId) {
        coordinator.action = "create-circular-free-panel";
        coordinator.token = "archdock-free-template-" + panelId;
        coordinator.panelId = panelId;
    }

    function test_lateConfigurationTriggersOnce() {
        const coordinator = createCoordinator();
        coordinator.action = "create-circular-free-panel";
        wait(1);
        compare(requestSpy.count, 0);
        coordinator.token = "archdock-free-template-17";
        wait(1);
        compare(requestSpy.count, 0);
        coordinator.panelId = 17;

        tryCompare(requestSpy, "count", 1);
        compare(requestSpy.signalArguments[0][0], 17);
        compare(requestSpy.signalArguments[0][1], "archdock-free-template-17");
        wait(5);
        compare(requestSpy.count, 1);
    }

    function test_nudgesAreCoalescedWhileRequestIsPending() {
        const coordinator = createCoordinator();
        makeReady(coordinator, 19);
        tryCompare(requestSpy, "count", 1);
        compare(coordinator.requestPending, true);
        compare(coordinator.attempts, 1);

        coordinator.nudge();
        coordinator.nudge();
        coordinator.nudge();
        wait(5);

        compare(requestSpy.count, 1);
        compare(coordinator.requestPending, true);
        compare(coordinator.attempts, 1);

        coordinator.resolved("free-1");
        wait(5);
        compare(requestSpy.count, 1);
        compare(coordinator.requestPending, false);
        compare(coordinator.attempts, 0);
    }

    function test_errorAndEmptyResultRetryUntilSuccess() {
        const coordinator = createCoordinator();
        makeReady(coordinator, 23);
        tryCompare(requestSpy, "count", 1);

        coordinator.rejected();
        tryCompare(requestSpy, "count", 2);
        coordinator.resolved("");
        tryCompare(requestSpy, "count", 3);
        coordinator.resolved("free-1");
        wait(5);
        compare(requestSpy.count, 3);
        compare(coordinator.attempts, 0);
    }

    function test_retryIsBounded() {
        const coordinator = createCoordinator({maximumAttempts: 3});
        makeReady(coordinator, 31);
        tryCompare(requestSpy, "count", 1);

        coordinator.rejected();
        tryCompare(requestSpy, "count", 2);
        coordinator.rejected();
        tryCompare(requestSpy, "count", 3);
        coordinator.rejected();
        wait(5);
        compare(requestSpy.count, 3);
    }
}
