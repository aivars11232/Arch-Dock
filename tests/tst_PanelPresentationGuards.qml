import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// TASK-0032 Phase B: interaction guards and the close timer.
//
// One property is defended here above all others: a panel that is being used
// does not close. Not while a context menu is open, not while something is
// being dragged onto it, not while it holds keyboard focus, and not because a
// close timer that was armed earlier happened to expire at the wrong moment.
//
// The second property is that the panel does eventually close. A guard defers
// a close; it does not discard it. A dock that stays open forever because a
// menu was opened once is as broken as one that closes underneath the menu.
TestCase {
    id: testCase

    name: "PanelPresentationGuards"
    when: windowShown
    width: 200
    height: 120

    Component {
        id: controllerComponent

        PanelPresentationController {}
    }

    function createController(properties) {
        const controller = createTemporaryObject(
            controllerComponent, testCase, properties || {})
        verify(controller !== null)
        return controller
    }

    function setGuard(controller, name, active) {
        controller[name] = active
    }

    // Every guard the contract defines. If a guard is added to the vocabulary
    // without being added here, this list and PresentationStates.GUARDS
    // disagree and the vocabulary test below fails.
    function guardNames() {
        return [
            "popupOpen", "windowPreviewOpen", "dragActive", "pointerInside",
            "revealZoneActive", "keyboardFocus", "editMode", "previewLock"
        ]
    }

    function test_theGuardVocabularyIsExactlyWhatTheContractDeclares() {
        const declared = PresentationStates.GUARDS
        const expected = guardNames()
        compare(declared.length, expected.length)
        for (let index = 0; index < expected.length; ++index) {
            verify(declared.indexOf(expected[index]) >= 0,
                   "undeclared guard: " + expected[index])
            verify(PresentationStates.isGuard(expected[index]))
        }
        verify(!PresentationStates.isGuard("somethingElse"))
    }

    function test_aControllerFeedsEveryDeclaredGuard() {
        const controller = createController({})
        const names = guardNames()
        // A guard the controller cannot be told about is a guard that is
        // always off, which is the silent version of not having it at all.
        for (let index = 0; index < names.length; ++index) {
            compare(controller[names[index]], false,
                    names[index] + " is not a controller input")
        }
        compare(controller.guardsActive, false)
        compare(controller.activeGuards.length, 0)
    }

    function test_noSingleGuardLetsThePanelClose_data() {
        const names = guardNames()
        const result = []
        for (let index = 0; index < names.length; ++index)
            result.push({ tag: names[index], guard: names[index] })
        return result
    }

    function test_noSingleGuardLetsThePanelClose(data) {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 0
        })
        setGuard(controller, data.guard, true)
        compare(controller.guardsActive, true)
        compare(controller.activeGuards[0], data.guard)

        controller.requestCollapse()
        compare(controller.phase, "open",
                "closed while " + data.guard + " was active")
        compare(controller.deferredRequest, "collapse")
        verify(!controller.pendingRequestActive)

        // Clearing the one guard releases the close it was holding.
        setGuard(controller, data.guard, false)
        compare(controller.phase, "collapsed")
        compare(controller.deferredRequest, "")
    }

    function test_noSingleGuardLetsThePanelConceal_data() {
        return test_noSingleGuardLetsThePanelClose_data()
    }

    function test_noSingleGuardLetsThePanelConceal(data) {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 0
        })
        setGuard(controller, data.guard, true)

        controller.requestConceal()
        compare(controller.phase, "open",
                "concealed while " + data.guard + " was active")
        compare(controller.hostVisible, true)
        compare(controller.deferredRequest, "conceal")

        setGuard(controller, data.guard, false)
        compare(controller.phase, "concealed")
    }

    function test_overlappingGuardsAllMustClear() {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 0
        })
        controller.popupOpen = true
        controller.dragActive = true
        controller.keyboardFocus = true
        compare(controller.activeGuards.length, 3)

        controller.requestCollapse()
        compare(controller.phase, "open")

        controller.popupOpen = false
        compare(controller.phase, "open", "closed with two guards remaining")
        controller.dragActive = false
        compare(controller.phase, "open", "closed with one guard remaining")
        controller.keyboardFocus = false
        compare(controller.phase, "collapsed",
                "did not close once every guard cleared")
    }

    function test_aGuardRaisedInsideTheDelayWindowCancelsTheClose() {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 300
        })
        controller.requestCollapse()
        verify(controller.pendingRequestActive)

        // The user opened a context menu while the close was counting down.
        controller.popupOpen = true
        verify(!controller.pendingRequestActive,
               "the armed close survived a guard")
        compare(controller.deferredRequest, "collapse")

        wait(400)
        compare(controller.phase, "open",
                "a cancelled close fired anyway")

        controller.popupOpen = false
        verify(controller.pendingRequestActive)
        tryVerify(function() { return controller.phase === "collapsed" }, 2000)
    }

    function test_theCloseResumesOnlyAfterGuardsClearAndTheDelayExpires() {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 200
        })
        controller.pointerInside = true
        controller.requestCollapse()
        compare(controller.phase, "open")
        verify(!controller.pendingRequestActive,
               "a close was armed while the pointer was still inside")

        controller.pointerInside = false
        verify(controller.pendingRequestActive)
        // The delay is a real delay: it has not closed yet.
        compare(controller.phase, "open")
        tryVerify(function() { return controller.phase === "collapsed" }, 2000)
    }

    function test_openingIsNeverGuarded_data() {
        return test_noSingleGuardLetsThePanelClose_data()
    }

    function test_openingIsNeverGuarded(data) {
        const controller = createController({
            restingState: "collapsed", transitionDuration: 0, openDelay: 0
        })
        setGuard(controller, data.guard, true)
        controller.requestOpen()
        compare(controller.phase, "open",
                "refused to open while " + data.guard + " was active")
    }

    function test_anOpenSupersedesADeferredClose() {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 0
        })
        controller.pointerInside = true
        controller.requestCollapse()
        compare(controller.deferredRequest, "collapse")

        controller.requestOpen()
        compare(controller.deferredRequest, "",
                "an open left a close outstanding")

        // Clearing the guard must not now close a panel the user reopened.
        controller.pointerInside = false
        compare(controller.phase, "open")
    }

    function test_anObservedHostConcealIgnoresEveryGuard_data() {
        return test_noSingleGuardLetsThePanelClose_data()
    }

    function test_anObservedHostConcealIgnoresEveryGuard(data) {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 400
        })
        setGuard(controller, data.guard, true)

        // Plasma has already removed the panel from the screen. Reporting it
        // as visible because a guard is set would be a lie about the host.
        controller.applyHostVisibility(false)
        compare(controller.phase, "concealed")
        compare(controller.hostVisible, false)
    }

    function test_guardedCloseIsReportedRatherThanSilentlyDropped() {
        const controller = createController({
            restingState: "open", transitionDuration: 0, closeDelay: 60
        })
        const resolved = signalSpy.createObject(testCase, {
            target: controller, signalName: "requestResolved"
        })

        controller.requestCollapse()
        verify(controller.pendingRequestActive)
        // Raised after the timer was armed but without going through the
        // property change that cancels it, so the timer itself must refuse.
        controller.previewLock = true
        pendingTimerProbe.armAndFire(controller)

        tryVerify(function() { return resolved.count > 0 }, 2000)
        let sawGuarded = false
        for (let index = 0; index < resolved.count; ++index) {
            if (resolved.signalArguments[index][1] === "guarded")
                sawGuarded = true
        }
        verify(sawGuarded, "a blocked close was dropped without a reason")
        compare(controller.phase, "open")
    }

    // ---- state invariants that keep input honest --------------------------

    function test_everyStateHasADrawableSurfaceAndAnHonestHostFlag_data() {
        return [
            { tag: "open", state: "open", hostVisible: true },
            { tag: "collapsing", state: "collapsing", hostVisible: true },
            { tag: "collapsed", state: "collapsed", hostVisible: true },
            { tag: "opening", state: "opening", hostVisible: true },
            { tag: "concealing", state: "concealing", hostVisible: true },
            { tag: "concealed", state: "concealed", hostVisible: false },
            { tag: "revealing", state: "revealing", hostVisible: true }
        ]
    }

    function test_everyStateHasADrawableSurfaceAndAnHonestHostFlag(data) {
        const value = { state: data.state, restTarget: "open" }
        const surface = PresentationStates.surfaceStateOf(value)
        // There is never a moment with no surface to draw and therefore no
        // moment where a host could show pixels with no matching input region,
        // or accept clicks with nothing drawn under them.
        verify(PresentationStates.isRestingState(surface),
               data.state + " had no drawable surface")
        compare(PresentationStates.isHostVisible(data.state), data.hostVisible)
    }

    function test_transitionsNeverReportAnUndefinedProgressToARenderer() {
        const controller = createController({
            restingState: "open", transitionDuration: 200
        })
        compare(controller.progress, -1)

        controller.requestCollapse()
        // While a transition runs the progress is a real fraction, so a
        // renderer interpolating on it can never be handed a partial frame it
        // cannot resolve.
        verify(controller.progress >= 0 && controller.progress <= 1,
               "progress was " + controller.progress + " during a transition")
        tryVerify(function() { return controller.phase === "collapsed" }, 3000)
        compare(controller.progress, -1)
    }

    QtObject {
        id: pendingTimerProbe

        // Drives the controller's own delay to expiry without touching its
        // internals, so the timer's guard re-check is what is under test.
        function armAndFire(controller) {
            controller.closeDelay = 1
            controller.deferredRequest = "collapse"
            controller.schedule("collapse", 1)
        }
    }

    Component {
        id: signalSpy

        SignalSpy {}
    }
}
