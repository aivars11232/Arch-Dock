import QtQuick
import "PresentationStates.js" as Presentation

// The single presentation state machine for one panel.
//
// It owns the panel's open/collapsed surface state and its concealed/revealed
// host state, keeps them independent, and publishes what a renderer needs:
// a surface state, a surface interpolation, an explicit progress, and a
// separate host phase. It renders nothing and mutates no host.
//
// Only the configured resting state and the delays are persisted, by the
// settings schema. The current phase, the direction of travel and the progress
// are transient by construction: they live here and nowhere else, so a restart
// resumes from a resting state the user chose rather than from the middle of
// an animation that was interrupted.
Item {
    id: root

    // Configuration projection. `restingState` is the panel's configured
    // initial and fallback surface state; an unknown value falls back to open
    // rather than leaving the panel in a state no renderer can draw.
    property string restingState: "open"
    property int openDelay: 0
    property int closeDelay: 0
    property int transitionDuration: 170
    property bool reducedMotion: false

    // Interaction guards. While any of these holds the panel may not close or
    // conceal. They are inputs, not opinions: the host reports what is true and
    // the controller decides what that means.
    property bool popupOpen: false
    property bool windowPreviewOpen: false
    property bool dragActive: false
    property bool pointerInside: false
    property bool revealZoneActive: false
    property bool keyboardFocus: false
    property bool editMode: false
    property bool previewLock: false

    readonly property var guardState: ({
        popupOpen: root.popupOpen,
        windowPreviewOpen: root.windowPreviewOpen,
        dragActive: root.dragActive,
        pointerInside: root.pointerInside,
        revealZoneActive: root.revealZoneActive,
        keyboardFocus: root.keyboardFocus,
        editMode: root.editMode,
        previewLock: root.previewLock
    })
    readonly property var activeGuards:
        Presentation.activeGuards(root.guardState)
    readonly property bool guardsActive: root.activeGuards.length > 0

    // Transient. Never written back to configuration.
    property var machine: Presentation.initialMachine(root.restingState)
    property real progress: -1
    property string pendingRequest: ""
    // A close that has been asked for but cannot run yet. It survives until it
    // runs or until an open supersedes it, which is what lets a panel close on
    // its own once the menu the user opened is finally dismissed.
    property string deferredRequest: ""

    readonly property string phase: String(root.machine.state)
    readonly property string restTarget: String(root.machine.restTarget)
    readonly property string surfaceState:
        Presentation.surfaceStateOf(root.machine)
    readonly property string transitionState:
        Presentation.transitionOf(root.machine)
    readonly property string hostPhase:
        Presentation.hostPhaseOf(root.machine)
    readonly property bool hostVisible:
        Presentation.isHostVisible(root.phase)
    readonly property bool transitionActive:
        Presentation.isTransitional(root.phase)
    readonly property bool pendingRequestActive: root.pendingRequest.length > 0

    // Reduced motion still changes state; it simply arrives immediately. The
    // state machine is behaviour, not decoration, so it is never disabled.
    readonly property int effectiveDuration: root.reducedMotion
        ? 0 : Math.max(0, Number(root.transitionDuration) || 0)

    signal requestResolved(string request, string outcome)
    signal transitionCompleted(string phase)

    function adopt(outcome) {
        root.machine = {
            state: outcome.state,
            restTarget: outcome.restTarget
        }
    }

    // Starts the surface interpolation at an explicit point. A reversal enters
    // at the complement of the progress it had, so turning around mid-flight
    // continues from where the panel actually is instead of snapping.
    function beginTransition(startProgress) {
        progressAnimation.stop()
        root.progress = Math.max(0, Math.min(1, Number(startProgress) || 0))
        if (root.effectiveDuration <= 0) {
            root.progress = 1
            root.finishTransition()
            return
        }
        progressAnimation.duration = Math.max(
            1, Math.round(root.effectiveDuration * (1 - root.progress)))
        progressAnimation.start()
    }

    function finishTransition() {
        progressAnimation.stop()
        const outcome = Presentation.completeTransition(
            root.machine, root.restingState)
        root.adopt(outcome)
        root.progress = -1
        root.transitionCompleted(outcome.state)
    }

    // Applies one request immediately, bypassing the configured delay.
    function deliver(request) {
        const before = root.machine
        const wasTransitional = Presentation.isTransitional(before.state)
        const outcome = Presentation.applyRequest(
            before, request, root.restingState)
        if (outcome.outcome === "invalid") {
            root.requestResolved(String(request), "invalid")
            return false
        }

        // A close is spent once it runs; anything else leaves it outstanding.
        if (Presentation.isClosingRequest(request))
            root.deferredRequest = ""

        root.adopt(outcome)
        if (outcome.outcome === "reversed") {
            root.beginTransition(1 - Math.max(0, root.progress))
        } else if (Presentation.isTransitional(outcome.state)) {
            // A repeat request for a transition already in flight must not
            // restart it; only a genuinely new transition begins at zero.
            if (!wasTransitional)
                root.beginTransition(0)
        } else {
            progressAnimation.stop()
            root.progress = -1
        }
        root.requestResolved(String(request), outcome.outcome)
        return outcome.changed
    }

    // Schedules one request. The most recent intent always wins: a new request
    // replaces whatever was pending, so a pointer that leaves and returns
    // cannot leave a stale close armed behind it.
    function schedule(request, delay) {
        pendingTimer.stop()
        const wait = Math.max(0, Number(delay) || 0)
        if (wait <= 0) {
            root.pendingRequest = ""
            return root.deliver(request)
        }
        root.pendingRequest = String(request)
        pendingTimer.interval = wait
        pendingTimer.restart()
        return false
    }

    function cancelPending() {
        const had = root.pendingRequestActive
        pendingTimer.stop()
        root.pendingRequest = ""
        return had
    }

    // Opening is never guarded: a panel that is in use is a panel the user is
    // pointing at, and refusing to open it would be absurd. An open also
    // supersedes any close that was waiting to run.
    function requestOpen() {
        root.deferredRequest = ""
        return root.schedule("open", root.openDelay)
    }

    function requestCollapse() {
        root.deferredRequest = "collapse"
        return root.evaluateDeferred()
    }

    function requestConceal() {
        root.deferredRequest = "conceal"
        return root.evaluateDeferred()
    }

    function requestReveal() {
        root.deferredRequest = ""
        return root.deliver("reveal")
    }

    // Runs the outstanding close when, and only when, every guard has cleared
    // and the configured delay has expired. While a guard holds, the timer is
    // cancelled rather than allowed to fire: a close must not merely be
    // postponed to the moment the user is still using the panel.
    function evaluateDeferred() {
        if (root.deferredRequest.length === 0)
            return false
        if (root.guardsActive) {
            pendingTimer.stop()
            root.pendingRequest = ""
            return false
        }
        if (root.pendingRequest === root.deferredRequest)
            return false
        return root.schedule(root.deferredRequest, root.closeDelay)
    }

    // The host has actually taken the panel off screen, or put it back. This
    // is an observed fact and is applied whatever the guards say: a panel that
    // Plasma has already hidden cannot be reported as visible because a menu
    // happens to be open.
    function applyHostVisibility(visible) {
        if (visible) {
            root.deferredRequest = ""
            return root.deliver("reveal")
        }
        pendingTimer.stop()
        root.pendingRequest = ""
        root.deferredRequest = ""
        return root.deliver("conceal")
    }

    onGuardsActiveChanged: root.evaluateDeferred()

    // Returns to the configured resting state with nothing in flight. This is
    // the restart path and the theme/profile-change path: a state machine that
    // is reconfigured mid-transition must not keep animating towards a target
    // that no longer exists.
    function reset() {
        progressAnimation.stop()
        root.cancelPending()
        root.deferredRequest = ""
        root.machine = Presentation.initialMachine(root.restingState)
        root.progress = -1
    }

    width: 0
    height: 0
    visible: false

    NumberAnimation {
        id: progressAnimation

        target: root
        property: "progress"
        to: 1
        easing.type: Easing.InOutQuad
        onFinished: root.finishTransition()
    }

    Timer {
        id: pendingTimer

        repeat: false
        onTriggered: {
            const request = root.pendingRequest
            root.pendingRequest = ""
            if (request.length === 0)
                return
            // Re-checked at the moment of firing, not only when armed: a guard
            // can be raised inside the delay window and must still win.
            if (Presentation.isRequestBlocked(request, root.guardState)) {
                root.requestResolved(request, "guarded")
                return
            }
            root.deliver(request)
        }
    }
}
