.pragma library

// Trigger dispatch and deterministic track composition for animation profiles.
//
// This library is intentionally free of QML types so the composition rules can
// be tested without a window, a renderer or a running animation. It decides
// which tracks should run; IconMotionController is the only thing that runs
// them.

// Mirrors ArchDock::animationTriggerVocabulary() in src/model/AnimationProfile.cpp.
var EVENTS = [
    "idle",
    "hover-enter",
    "hover-hold",
    "hover-exit",
    "press",
    "click",
    "launch-requested",
    "launch-succeeded",
    "launch-failed",
    "running-started",
    "running-stopped",
    "urgent",
    "drop-entered",
    "drop-committed",
    "panel-reveal",
    "panel-conceal",
    "panel-open",
    "panel-collapse",
    "profile-changed",
    "command"
];

// State triggers run for as long as their condition holds. Pulse triggers are
// discrete: they fire once and run for the profile's own duration.
var STATE_EVENTS = [
    "idle",
    "hover-hold",
    "running-started",
    "urgent",
    "drop-entered",
    "panel-reveal",
    "panel-open"
];

// Legacy `animationTrigger` values that predate the profile vocabulary.
// `launch` deliberately maps to launch-requested, never to launch-succeeded:
// the old setting could only observe that an activation was asked for.
var LEGACY_TRIGGERS = {
    "hover": "hover-hold",
    "click": "click",
    "launch": "launch-requested",
    "running": "running-started",
    "drop": "drop-entered",
    "reveal": "panel-reveal",
    "idle": "idle"
};

function isEvent(name) {
    return EVENTS.indexOf(String(name)) >= 0;
}

function isStateEvent(name) {
    return STATE_EVENTS.indexOf(String(name)) >= 0;
}

function isPulseEvent(name) {
    return isEvent(name) && !isStateEvent(name);
}

function normalizeTrigger(trigger) {
    var value = String(trigger || "");
    if (isEvent(value))
        return value;
    if (LEGACY_TRIGGERS.hasOwnProperty(value))
        return LEGACY_TRIGGERS[value];
    return "";
}

// The resting value a property returns to when no track claims it.
function restingValue(property) {
    switch (String(property)) {
    case "scale":
    case "scale-x":
    case "scale-y":
    case "opacity":
        return 1;
    default:
        return 0;
    }
}

function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
}

function number(value, fallback) {
    var parsed = Number(value);
    return isFinite(parsed) ? parsed : fallback;
}

function defaultState() {
    return {
        hovered: false,
        pressed: false,
        running: false,
        urgent: false,
        dropActive: false,
        revealed: false,
        visible: true,
        reducedMotion: false,
        entryIndex: 0,
        intensity: 1,
        speed: 1,
        pulses: {}
    };
}

function mergeState(overrides) {
    var state = defaultState();
    var source = overrides || {};
    for (var key in source) {
        if (source.hasOwnProperty(key))
            state[key] = source[key];
    }
    if (!state.pulses)
        state.pulses = {};
    return state;
}

// A state trigger is satisfied by the current condition; a pulse trigger is
// satisfied only while its event is live in `state.pulses`.
function triggerSatisfied(trigger, state) {
    var event = normalizeTrigger(trigger);
    if (event === "")
        return false;
    if (isPulseEvent(event))
        return Boolean(state.pulses && state.pulses[event]);
    switch (event) {
    case "idle":
        return true;
    case "hover-hold":
        return Boolean(state.hovered);
    case "running-started":
        return Boolean(state.running);
    case "urgent":
        return Boolean(state.urgent);
    case "drop-entered":
        return Boolean(state.dropActive);
    case "panel-reveal":
    case "panel-open":
        return Boolean(state.revealed);
    }
    return false;
}

function profileId(profile) {
    return String((profile && profile.id) || "");
}

function profileTracks(profile) {
    return (profile && profile.tracks) || [];
}

function isColorProperty(property) {
    return String(property) === "tint";
}

// Reduced motion is resolved before composition so a substitute profile is
// composed exactly like any other profile.
function resolveReducedMotion(profile, catalog) {
    var reduced = (profile && profile.reducedMotion) || {};
    var mode = String(reduced.mode || "none");
    if (mode === "substitute") {
        var substitute = catalog && catalog[String(reduced.substituteProfileId || "")];
        if (!substitute)
            return { kind: "rest", profile: null, hold: null };
        // A substitute may not itself substitute; the validator guarantees
        // this, and honouring only one level keeps resolution terminating.
        return { kind: "profile", profile: substitute, hold: null };
    }
    if (mode === "static") {
        return {
            kind: "hold",
            profile: null,
            hold: {
                property: String(reduced.property || ""),
                value: number(reduced.value, 0),
                color: String(reduced.color || "")
            }
        };
    }
    return { kind: "rest", profile: null, hold: null };
}

function scaledDuration(duration, state) {
    var speed = clamp(number(state.speed, 1), 0.05, 10);
    return Math.max(0, Math.round(number(duration, 0) / speed));
}

// Intensity scales both endpoints about the property's resting value, so a
// symmetric oscillation narrows towards its centre rather than drifting off it:
// a -14..14 tilt becomes -7..7 at half intensity, and a 1 -> 1.16 swell becomes
// 1 -> 1.08. This reproduces the pre-migration arithmetic exactly.
function scaledEndpoint(value, property, track, state) {
    var rest = restingValue(property);
    var intensity = clamp(number(state.intensity, 1), 0, 10);
    var trackScale = clamp(number(track.intensityScale, 1), 0, 10);
    return rest + (number(value, 0) - rest) * intensity * trackScale;
}

function resolveTrack(profile, track, state) {
    var target = String(track.target || profile.target || "icon");
    var property = String(track.property || "");
    var resolved = {
        key: target + "/" + property,
        target: target,
        property: property,
        profileId: profileId(profile),
        trackId: String(track.id || ""),
        easing: String(track.easing || "linear"),
        direction: String(track.direction || "normal"),
        repeat: number(track.repeat, 1),
        blend: String(track.blend || "replace"),
        priority: number(track.priority, 0),
        duration: scaledDuration(track.duration, state),
        delay: scaledDuration(number(track.delay, 0)
                              + number(track.phase, 0)
                                * Math.max(0, number(state.entryIndex, 0)),
                              state)
    };
    if (isColorProperty(property)) {
        resolved.fromColor = String(track.fromColor || "");
        resolved.toColor = String(track.toColor || "");
        resolved.from = 0;
        resolved.to = 0;
    } else {
        resolved.from = scaledEndpoint(track.from, property, track, state);
        resolved.to = scaledEndpoint(track.to, property, track, state);
    }
    return resolved;
}

// Deterministic ordering: the same event sequence must always produce the same
// track list, so ordering may never depend on object key order or arrival time.
function compareTracks(first, second) {
    if (first.target !== second.target)
        return first.target < second.target ? -1 : 1;
    if (first.property !== second.property)
        return first.property < second.property ? -1 : 1;
    if (first.priority !== second.priority)
        return second.priority - first.priority;
    if (first.profileId !== second.profileId)
        return first.profileId < second.profileId ? -1 : 1;
    if (first.trackId !== second.trackId)
        return first.trackId < second.trackId ? -1 : 1;
    return 0;
}

function conflictBetween(first, second) {
    if (first.key !== second.key)
        return false;
    if (first.blend === "add" && second.blend === "add")
        return false;
    return first.priority === second.priority;
}

// Composes the tracks that should be running for the given profiles and state.
//
// Returns:
//   tracks    — deterministically ordered, conflict-free resolved tracks
//   holds     — static reduced-motion values to apply once
//   conflicts — rejected same-property writers, with both claimants named
//   active    — ids of the profiles that contributed
function compose(profiles, state, catalog) {
    var runtimeState = mergeState(state);
    var profileList = (profiles || []).slice().sort(function(first, second) {
        var firstId = profileId(first);
        var secondId = profileId(second);
        if (firstId === secondId)
            return 0;
        return firstId < secondId ? -1 : 1;
    });

    var candidates = [];
    var holds = [];
    var active = [];

    for (var index = 0; index < profileList.length; ++index) {
        var profile = profileList[index];
        if (!profile || profile.valid === false)
            continue;
        if (!triggerSatisfied(profile.trigger, runtimeState))
            continue;

        var effective = profile;
        if (runtimeState.reducedMotion) {
            var reduced = resolveReducedMotion(profile, catalog || {});
            if (reduced.kind === "rest")
                continue;
            if (reduced.kind === "hold") {
                if (reduced.hold.property !== "") {
                    holds.push({
                        target: String(profile.target || "icon"),
                        property: reduced.hold.property,
                        value: reduced.hold.value,
                        color: reduced.hold.color,
                        profileId: profileId(profile)
                    });
                }
                active.push(profileId(profile));
                continue;
            }
            effective = reduced.profile;
        }

        var tracks = profileTracks(effective);
        var contributed = false;
        for (var trackIndex = 0; trackIndex < tracks.length; ++trackIndex) {
            var track = tracks[trackIndex];
            if (!track || !track.property)
                continue;
            candidates.push(resolveTrack(effective, track, runtimeState));
            contributed = true;
        }
        if (contributed)
            active.push(profileId(profile));
    }

    candidates.sort(compareTracks);

    // Fail closed: an unresolved same-property claim withdraws every claimant
    // rather than letting arrival order decide which one wins.
    var conflicts = [];
    var rejectedKeys = {};
    for (var outer = 0; outer < candidates.length; ++outer) {
        for (var inner = outer + 1; inner < candidates.length; ++inner) {
            if (!conflictBetween(candidates[outer], candidates[inner]))
                continue;
            rejectedKeys[candidates[outer].key] = true;
            conflicts.push({
                key: candidates[outer].key,
                target: candidates[outer].target,
                property: candidates[outer].property,
                claimants: [
                    candidates[outer].profileId + ":" + candidates[outer].trackId,
                    candidates[inner].profileId + ":" + candidates[inner].trackId
                ]
            });
        }
    }

    var accepted = [];
    var claimedKeys = {};
    for (var candidateIndex = 0; candidateIndex < candidates.length;
         ++candidateIndex) {
        var candidate = candidates[candidateIndex];
        if (rejectedKeys[candidate.key])
            continue;
        // Under `replace`, the highest priority already sorts first; later
        // writers on the same key are shadowed and must not also run.
        if (candidate.blend !== "add") {
            if (claimedKeys[candidate.key])
                continue;
            claimedKeys[candidate.key] = true;
        }
        accepted.push(candidate);
    }

    return {
        tracks: accepted,
        holds: holds,
        conflicts: conflicts,
        active: active
    };
}

