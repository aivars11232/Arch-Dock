.pragma library

// The panel presentation state machine, expressed as pure data.
//
// A panel has two independent layers and this library keeps them that way:
//
//   * the **surface** is open or collapsed - what Arch Dock draws inside its
//     own applet;
//   * the **host** is revealed or concealed - whether Plasma is showing the
//     panel at all.
//
// A panel can therefore be host-visible and visually collapsed, which is the
// "visible but closed, opens on hover" behaviour the master plan asks for.
// Collapsing is never concealment and concealment is never collapsing; the
// only thing they share is the resting surface a reveal returns to.
//
// It is deliberately free of QML types so every legal transition, every
// illegal request and every cancellation can be proved without a window, and
// it is the only place the transition table exists: the live host, the shared
// scene and the Studio preview all read it, so a state cannot mean one thing
// in the applet and another in a preview.

var STATES = [
    "open", "collapsing", "collapsed", "opening",
    "concealing", "concealed", "revealing"
];

// The two states a panel may come to rest in. A transition is never a resting
// state, which is why a restart can never resume mid-animation.
var RESTING_STATES = ["open", "collapsed"];

var REQUESTS = ["open", "collapse", "conceal", "reveal"];

// Everything that means "the panel is in use right now".
//
// A panel may not close or conceal while any of these holds. They are listed
// once, here, so a host cannot quietly forget one: a guard that is not fed is
// a guard that is off, and a dock that closes under an open context menu is
// the exact defect this list exists to prevent.
//
// `windowPreviewOpen` has no producer yet - grouped window previews are owned
// by TASK-0037 - but it is a first-class guard so the preview work has a seam
// to report into rather than a controller to modify.
var GUARDS = [
    "popupOpen",
    "windowPreviewOpen",
    "dragActive",
    "pointerInside",
    "revealZoneActive",
    "keyboardFocus",
    "editMode",
    "previewLock"
];

function isGuard(name) {
    return GUARDS.indexOf(String(name)) >= 0;
}

// The guards currently holding the panel open, in declaration order. Returning
// the names rather than a bare boolean is deliberate: when a panel refuses to
// close, the reason has to be answerable.
function activeGuards(guards) {
    var source = guards && typeof guards === "object" ? guards : {};
    var result = [];
    for (var index = 0; index < GUARDS.length; ++index) {
        var name = GUARDS[index];
        if (source[name] === true)
            result.push(name);
    }
    return result;
}

function anyGuardActive(guards) {
    return activeGuards(guards).length > 0;
}

// Only closing is guarded. Opening a panel that is already in use is never
// wrong, and a host that has genuinely taken the panel off screen is a fact
// rather than a request, so neither passes through here.
function isClosingRequest(request) {
    var name = String(request || "").trim().toLowerCase();
    return name === "collapse" || name === "conceal";
}

function isRequestBlocked(request, guards) {
    return isClosingRequest(request) && anyGuardActive(guards);
}

// States in which the host still has the panel on screen. "concealing" is
// included: the panel is animating out and is still visible while it does.
var HOST_VISIBLE_STATES = [
    "open", "collapsing", "collapsed", "opening", "concealing", "revealing"
];

var TRANSITIONAL_STATES = [
    "collapsing", "opening", "concealing", "revealing"
];

function isState(name) {
    return STATES.indexOf(String(name)) >= 0;
}

function isRequest(name) {
    return REQUESTS.indexOf(String(name)) >= 0;
}

function isRestingState(name) {
    return RESTING_STATES.indexOf(String(name)) >= 0;
}

function isTransitional(state) {
    return TRANSITIONAL_STATES.indexOf(String(state)) >= 0;
}

// True while the host still shows the panel, regardless of what the surface
// is doing. This is the host layer and nothing else may be inferred from it.
function isHostVisible(state) {
    return HOST_VISIBLE_STATES.indexOf(String(state)) >= 0;
}

// True once the host layer is involved at all. Kept separate from
// isTransitional so a caller can never confuse a collapse with a conceal.
function isConcealPhase(state) {
    var value = String(state);
    return value === "concealing" || value === "concealed"
        || value === "revealing";
}

function normalizeRestingState(value, fallback) {
    var candidate = String(value || "").trim().toLowerCase();
    if (isRestingState(candidate))
        return candidate;
    var alternative = String(fallback || "").trim().toLowerCase();
    return isRestingState(alternative) ? alternative : "open";
}

// A restart begins here. The configured resting state is the only input, so a
// panel always comes back in a valid state that the user actually chose and
// never in a transition that was interrupted by the restart.
function initialMachine(restingState) {
    var resting = normalizeRestingState(restingState, "open");
    return { state: resting, restTarget: resting };
}

function machineFrom(candidate, restingState) {
    var source = candidate && typeof candidate === "object" ? candidate : {};
    var state = String(source.state || "");
    if (!isState(state))
        return initialMachine(restingState);
    return {
        state: state,
        restTarget: normalizeRestingState(
            source.restTarget, normalizeRestingState(restingState, "open"))
    };
}

// The surface state a renderer should draw. During a transition this is the
// destination, because the renderer interpolates from the opposite endpoint
// using transitionOf() and an explicit progress; naming the destination is
// what makes the end of a transition land on the exact same frame as the
// resting state that follows it.
function surfaceStateOf(machine) {
    var value = machineFrom(machine);
    switch (value.state) {
    case "open":
    case "opening":
        return "open";
    case "collapsed":
    case "collapsing":
        return "collapsed";
    default:
        return value.restTarget;
    }
}

// The surface interpolation a renderer should run. Concealing and revealing
// deliberately report "idle": they are host visibility, not a surface
// collapse, and a renderer must not crossfade open/collapsed artwork for them.
function transitionOf(machine) {
    switch (machineFrom(machine).state) {
    case "opening":
        return "opening";
    case "collapsing":
        return "closing";
    default:
        return "idle";
    }
}

// The host-layer phase, reported separately so a host can animate a conceal
// without the surface renderer ever seeing it as a collapse.
function hostPhaseOf(machine) {
    var state = machineFrom(machine).state;
    return isConcealPhase(state) ? state : "revealed";
}

function result(machine, outcome, previous) {
    return {
        state: machine.state,
        restTarget: machine.restTarget,
        outcome: String(outcome),
        changed: previous === undefined || previous === null
            ? true
            : previous.state !== machine.state
                || previous.restTarget !== machine.restTarget
    };
}

// Applies one request.
//
// Every request is answered deterministically. A request that cannot change
// the phase never fails silently and never throws: it either retargets the
// state the panel will rest in, or it is reported as a no-op. Reversing a
// transition mid-flight is a first-class outcome rather than a special case,
// because a pointer that leaves and returns during a 200 ms animation is
// ordinary, not exceptional.
//
// Outcomes:
//   accepted   - a new transition started
//   reversed   - a transition in flight turned around
//   retargeted - the resting target changed; the phase did not
//   noop       - already in, or already heading to, the requested condition
//   invalid    - the request name is not in the vocabulary
function applyRequest(machine, request, restingState) {
    var current = machineFrom(machine, restingState);
    var name = String(request || "").trim().toLowerCase();
    if (!isRequest(name))
        return result(current, "invalid", current);

    switch (name) {
    case "open":
        if (isConcealPhase(current.state)) {
            return result({ state: current.state, restTarget: "open" },
                          "retargeted", current);
        }
        if (current.state === "open" || current.state === "opening")
            return result(current, "noop", current);
        return result({ state: "opening", restTarget: "open" },
                      current.state === "collapsing" ? "reversed" : "accepted",
                      current);

    case "collapse":
        if (isConcealPhase(current.state)) {
            return result({ state: current.state, restTarget: "collapsed" },
                          "retargeted", current);
        }
        if (current.state === "collapsed" || current.state === "collapsing")
            return result(current, "noop", current);
        return result({ state: "collapsing", restTarget: "collapsed" },
                      current.state === "opening" ? "reversed" : "accepted",
                      current);

    case "conceal":
        if (current.state === "concealing" || current.state === "concealed")
            return result(current, "noop", current);
        // The surface target is carried through concealment untouched, so a
        // reveal restores what the user was actually looking at.
        return result({ state: "concealing", restTarget: current.restTarget },
                      current.state === "revealing" ? "reversed" : "accepted",
                      current);

    case "reveal":
        if (!isConcealPhase(current.state))
            return result(current, "noop", current);
        if (current.state === "revealing")
            return result(current, "noop", current);
        return result({ state: "revealing", restTarget: current.restTarget },
                      current.state === "concealing" ? "reversed" : "accepted",
                      current);
    }

    return result(current, "invalid", current);
}

// Lands a transition on its resting state. A reveal resolves to the configured
// open or collapsed target rather than assuming open, which is what makes a
// panel that was collapsed before an auto-hide still collapsed after it.
function completeTransition(machine, restingState) {
    var current = machineFrom(machine, restingState);
    switch (current.state) {
    case "opening":
        return result({ state: "open", restTarget: "open" },
                      "accepted", current);
    case "collapsing":
        return result({ state: "collapsed", restTarget: "collapsed" },
                      "accepted", current);
    case "concealing":
        return result({ state: "concealed", restTarget: current.restTarget },
                      "accepted", current);
    case "revealing":
        return result({ state: current.restTarget,
                        restTarget: current.restTarget },
                      "accepted", current);
    default:
        return result(current, "noop", current);
    }
}
