import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// TASK-0032 Phase A: the presentation state machine.
//
// The transition table is pure data, so every legal edge, every illegal
// request and every cancellation is proved here without a window, a renderer
// or a host. The controller is then driven for the things only a running
// object can show: progress, delays and reset.
//
// The property this file exists to defend is that collapsing and concealing
// are different things. A panel that is visually collapsed is still on screen;
// a panel the host concealed is not. Nothing here may let one imply the other.
TestCase {
    id: testCase

    name: "PanelPresentation"
    when: windowShown
    width: 200
    height: 120

    readonly property var restingStates: ["open", "collapsed"]

    Component {
        id: controllerComponent

        PanelPresentationController {}
    }

    function machine(state, restTarget) {
        return { state: state, restTarget: restTarget || state }
    }

    function createController(properties) {
        const controller = createTemporaryObject(
            controllerComponent, testCase, properties || {})
        verify(controller !== null)
        return controller
    }

    // ---- pure transition table -------------------------------------------

    function test_restartBeginsFromAConfiguredRestingState_data() {
        return [
            { tag: "open", configured: "open", expected: "open" },
            { tag: "collapsed", configured: "collapsed",
              expected: "collapsed" },
            { tag: "unknown", configured: "half-way", expected: "open" },
            { tag: "empty", configured: "", expected: "open" },
            // A transition is not a resting state: a restart can never resume
            // in the middle of an animation that something interrupted.
            { tag: "transition", configured: "collapsing", expected: "open" },
            { tag: "concealed", configured: "concealed", expected: "open" }
        ]
    }

    function test_restartBeginsFromAConfiguredRestingState(data) {
        const initial = PresentationStates.initialMachine(data.configured)
        compare(initial.state, data.expected)
        compare(initial.restTarget, data.expected)
        verify(PresentationStates.isRestingState(initial.state))
        verify(!PresentationStates.isTransitional(initial.state))
    }

    function test_everyLegalSurfaceTransitionIsExplicit() {
        let value = PresentationStates.initialMachine("open")

        let step = PresentationStates.applyRequest(value, "collapse")
        compare(step.state, "collapsing")
        compare(step.outcome, "accepted")
        verify(step.changed)

        step = PresentationStates.completeTransition(step)
        compare(step.state, "collapsed")

        step = PresentationStates.applyRequest(step, "open")
        compare(step.state, "opening")
        compare(step.outcome, "accepted")

        step = PresentationStates.completeTransition(step)
        compare(step.state, "open")
        compare(step.restTarget, "open")
    }

    function test_everyLegalHostTransitionIsExplicit_data() {
        return [
            { tag: "from open", from: machine("open") },
            { tag: "from collapsed", from: machine("collapsed") },
            { tag: "from opening", from: machine("opening", "open") },
            { tag: "from collapsing", from: machine("collapsing", "collapsed") }
        ]
    }

    function test_everyLegalHostTransitionIsExplicit(data) {
        let step = PresentationStates.applyRequest(data.from, "conceal")
        compare(step.state, "concealing")
        compare(step.outcome, "accepted")
        verify(PresentationStates.isHostVisible(step.state),
               "a panel animating out is still on screen")

        step = PresentationStates.completeTransition(step)
        compare(step.state, "concealed")
        verify(!PresentationStates.isHostVisible(step.state))

        step = PresentationStates.applyRequest(step, "reveal")
        compare(step.state, "revealing")
        compare(step.outcome, "accepted")

        step = PresentationStates.completeTransition(step)
        // A reveal lands on the surface the panel was actually resting in.
        compare(step.state, data.from.restTarget)
        verify(PresentationStates.isRestingState(step.state))
    }

    function test_reversingATransitionIsAFirstClassOutcome_data() {
        return [
            { tag: "closing then open", from: machine("collapsing", "collapsed"),
              request: "open", expected: "opening" },
            { tag: "opening then collapse", from: machine("opening", "open"),
              request: "collapse", expected: "collapsing" },
            { tag: "concealing then reveal", from: machine("concealing", "open"),
              request: "reveal", expected: "revealing" },
            { tag: "revealing then conceal", from: machine("revealing", "open"),
              request: "conceal", expected: "concealing" }
        ]
    }

    function test_reversingATransitionIsAFirstClassOutcome(data) {
        const step = PresentationStates.applyRequest(data.from, data.request)
        compare(step.state, data.expected)
        compare(step.outcome, "reversed")
        verify(step.changed)
    }

    function test_reentrantRequestsResolvePredictably_data() {
        return [
            { tag: "open when open", from: machine("open"), request: "open" },
            { tag: "open when opening", from: machine("opening", "open"),
              request: "open" },
            { tag: "collapse when collapsed", from: machine("collapsed"),
              request: "collapse" },
            { tag: "collapse when collapsing",
              from: machine("collapsing", "collapsed"), request: "collapse" },
            { tag: "conceal when concealed", from: machine("concealed", "open"),
              request: "conceal" },
            { tag: "conceal when concealing",
              from: machine("concealing", "open"), request: "conceal" },
            { tag: "reveal when revealing", from: machine("revealing", "open"),
              request: "reveal" },
            { tag: "reveal when already visible", from: machine("open"),
              request: "reveal" },
            { tag: "reveal when opening", from: machine("opening", "open"),
              request: "reveal" }
        ]
    }

    function test_reentrantRequestsResolvePredictably(data) {
        const step = PresentationStates.applyRequest(data.from, data.request)
        compare(step.outcome, "noop")
        compare(step.state, data.from.state)
        compare(step.restTarget, data.from.restTarget)
        verify(!step.changed)
    }

    function test_illegalRequestsAreReportedAndChangeNothing_data() {
        return [
            { tag: "unknown", request: "explode" },
            { tag: "empty", request: "" },
            { tag: "state name", request: "collapsing" },
            { tag: "undefined", request: undefined }
        ]
    }

    function test_illegalRequestsAreReportedAndChangeNothing(data) {
        const from = machine("open")
        const step = PresentationStates.applyRequest(from, data.request)
        compare(step.outcome, "invalid")
        compare(step.state, "open")
        compare(step.restTarget, "open")
    }

    function test_aSurfaceRequestWhileConcealedRetargetsWithoutRevealing_data() {
        return [
            { tag: "open while concealed", from: machine("concealed", "collapsed"),
              request: "open", expected: "open" },
            { tag: "collapse while concealed", from: machine("concealed", "open"),
              request: "collapse", expected: "collapsed" },
            { tag: "open while concealing", from: machine("concealing", "collapsed"),
              request: "open", expected: "open" },
            { tag: "collapse while revealing", from: machine("revealing", "open"),
              request: "collapse", expected: "collapsed" }
        ]
    }

    function test_aSurfaceRequestWhileConcealedRetargetsWithoutRevealing(data) {
        const step = PresentationStates.applyRequest(data.from, data.request)
        compare(step.outcome, "retargeted")
        // The host phase is untouched: asking the surface to open must never
        // drag a concealed panel back onto the screen by itself.
        compare(step.state, data.from.state)
        compare(step.restTarget, data.expected)

        const landed = PresentationStates.completeTransition(
            PresentationStates.applyRequest(
                { state: "concealed", restTarget: step.restTarget },
                "reveal"))
        compare(landed.state, data.expected)
    }

    // ---- the two layers stay separate ------------------------------------

    function test_concealIsNeverReportedAsASurfaceInterpolation_data() {
        return [
            { tag: "concealing", state: "concealing" },
            { tag: "concealed", state: "concealed" },
            { tag: "revealing", state: "revealing" }
        ]
    }

    function test_concealIsNeverReportedAsASurfaceInterpolation(data) {
        const value = machine(data.state, "open")
        // A renderer must not crossfade open/collapsed artwork for a conceal.
        compare(PresentationStates.transitionOf(value), "idle")
        compare(PresentationStates.hostPhaseOf(value), data.state)
        verify(PresentationStates.isConcealPhase(data.state))
    }

    function test_surfaceStateSurvivesAConcealCycle_data() {
        return [
            { tag: "open survives", resting: "open" },
            { tag: "collapsed survives", resting: "collapsed" }
        ]
    }

    function test_surfaceStateSurvivesAConcealCycle(data) {
        let value = PresentationStates.initialMachine(data.resting)
        compare(PresentationStates.surfaceStateOf(value), data.resting)

        value = PresentationStates.applyRequest(value, "conceal")
        compare(PresentationStates.surfaceStateOf(value), data.resting)
        value = PresentationStates.completeTransition(value)
        compare(PresentationStates.surfaceStateOf(value), data.resting)

        value = PresentationStates.completeTransition(
            PresentationStates.applyRequest(value, "reveal"))
        compare(value.state, data.resting)
        compare(PresentationStates.surfaceStateOf(value), data.resting)
    }

    function test_surfaceStateNamesTheDestinationDuringATransition_data() {
        return [
            { tag: "opening", state: "opening", target: "open",
              surface: "open", transition: "opening" },
            { tag: "collapsing", state: "collapsing", target: "collapsed",
              surface: "collapsed", transition: "closing" },
            { tag: "open", state: "open", target: "open",
              surface: "open", transition: "idle" },
            { tag: "collapsed", state: "collapsed", target: "collapsed",
              surface: "collapsed", transition: "idle" }
        ]
    }

    function test_surfaceStateNamesTheDestinationDuringATransition(data) {
        const value = machine(data.state, data.target)
        compare(PresentationStates.surfaceStateOf(value), data.surface)
        compare(PresentationStates.transitionOf(value), data.transition)
        compare(PresentationStates.hostPhaseOf(value), "revealed")
    }

    // ---- the running controller ------------------------------------------

    function test_controllerStartsAtItsConfiguredRestingState_data() {
        return [
            { tag: "open", resting: "open" },
            { tag: "collapsed", resting: "collapsed" }
        ]
    }

    function test_controllerStartsAtItsConfiguredRestingState(data) {
        const controller = createController({ restingState: data.resting })
        compare(controller.phase, data.resting)
        compare(controller.surfaceState, data.resting)
        compare(controller.transitionState, "idle")
        compare(controller.hostPhase, "revealed")
        compare(controller.hostVisible, true)
        compare(controller.transitionActive, false)
        compare(controller.progress, -1)
    }

    function test_controllerRunsATransitionToCompletion() {
        const controller = createController({
            restingState: "open", transitionDuration: 60
        })
        const completed = signalSpy.createObject(testCase, {
            target: controller, signalName: "transitionCompleted"
        })

        controller.requestCollapse()
        compare(controller.phase, "collapsing")
        compare(controller.transitionState, "closing")
        verify(controller.progress >= 0 && controller.progress <= 1)

        tryVerify(function() { return controller.phase === "collapsed" }, 2000)
        compare(controller.transitionState, "idle")
        compare(controller.progress, -1)
        compare(completed.count, 1)
    }

    function test_reducedMotionStillChangesStateImmediately() {
        const controller = createController({
            restingState: "open", transitionDuration: 400, reducedMotion: true
        })
        controller.requestCollapse()
        // Reduced motion removes the animation, never the behaviour.
        compare(controller.phase, "collapsed")
        compare(controller.progress, -1)
        compare(controller.transitionActive, false)
    }

    function test_reversalResumesFromWhereThePanelActuallyIs() {
        const controller = createController({
            restingState: "open", transitionDuration: 400
        })
        controller.requestCollapse()
        tryVerify(function() { return controller.progress > 0.2 }, 2000)
        const atReversal = controller.progress

        controller.requestOpen()
        compare(controller.phase, "opening")
        // It enters at the complement, so turning around continues from the
        // frame on screen instead of snapping back to the start.
        verify(controller.progress <= 1 - atReversal + 0.15,
               "reversal restarted instead of resuming: progress "
               + controller.progress + " after reversing at " + atReversal)
        verify(controller.progress > 0,
               "reversal snapped to the beginning")
        tryVerify(function() { return controller.phase === "open" }, 3000)
    }

    function test_aRepeatedRequestDoesNotRestartATransitionInFlight() {
        const controller = createController({
            restingState: "open", transitionDuration: 400
        })
        controller.requestCollapse()
        tryVerify(function() { return controller.progress > 0.25 }, 2000)
        const advanced = controller.progress

        controller.requestCollapse()
        compare(controller.phase, "collapsing")
        verify(controller.progress >= advanced,
               "a repeated request rewound a transition already in flight")
    }

    function test_theMostRecentIntentReplacesAPendingRequest() {
        const controller = createController({
            restingState: "open", transitionDuration: 0,
            closeDelay: 400, openDelay: 0
        })
        controller.requestCollapse()
        verify(controller.pendingRequestActive)
        compare(controller.phase, "open")

        // The pointer came back before the close armed.
        controller.requestOpen()
        verify(!controller.pendingRequestActive)
        compare(controller.phase, "open")

        wait(500)
        compare(controller.phase, "open",
                "a stale close fired after the intent had changed")
    }

    function test_aConcealReplacesAPendingCollapse() {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 400
        })
        controller.requestCollapse()
        compare(controller.deferredRequest, "collapse")

        controller.requestConceal()
        // Both are closes and both honour the configured delay, so the second
        // supersedes the first rather than queuing behind it.
        compare(controller.deferredRequest, "conceal")
        compare(controller.phase, "open")
    }

    function test_anObservedHostConcealIsImmediateAndPreservesTheSurface() {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 400
        })
        controller.requestCollapse()
        verify(controller.pendingRequestActive)

        // Plasma has already taken the panel off screen. That is a fact, not a
        // request, so no delay and no guard may hold it back.
        controller.applyHostVisibility(false)
        verify(!controller.pendingRequestActive)
        compare(controller.deferredRequest, "")
        compare(controller.phase, "concealed")

        // The surface never collapsed; it was concealed while open, so a
        // reveal must bring back the open panel.
        compare(controller.restTarget, "open")
        controller.applyHostVisibility(true)
        compare(controller.phase, "open")
    }

    function test_resetReturnsToTheRestingStateWithNothingInFlight() {
        const controller = createController({
            restingState: "collapsed", transitionDuration: 400, closeDelay: 300
        })
        controller.requestConceal()
        controller.requestOpen()
        verify(controller.pendingRequestActive || controller.transitionActive
               || controller.phase !== "collapsed")

        controller.reset()
        compare(controller.phase, "collapsed")
        compare(controller.restTarget, "collapsed")
        compare(controller.progress, -1)
        compare(controller.transitionActive, false)
        verify(!controller.pendingRequestActive)
    }

    function test_controllerReportsEveryRequestOutcome() {
        const controller = createController({
            restingState: "open", transitionDuration: 0
        })
        const resolved = signalSpy.createObject(testCase, {
            target: controller, signalName: "requestResolved"
        })

        controller.deliver("collapse")
        compare(resolved.count, 1)
        compare(resolved.signalArguments[0][1], "accepted")

        controller.deliver("collapse")
        compare(resolved.count, 2)
        compare(resolved.signalArguments[1][1], "noop")

        controller.deliver("explode")
        compare(resolved.count, 3)
        compare(resolved.signalArguments[2][1], "invalid")
        compare(controller.phase, "collapsed")
    }

    Component {
        id: signalSpy

        SignalSpy {}
    }
}
