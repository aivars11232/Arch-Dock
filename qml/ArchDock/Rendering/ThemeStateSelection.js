.pragma library

// Theme Package v2 state and layer selection.
//
// One authoritative answer to "which declared layers are active right now, in
// what order, and at what opacity". Both the skinned 2D renderer and the baked
// 2.5D renderer read it, so a state, a hover and an opening crossfade cannot
// mean two different things depending on which renderer drew the panel.
//
// Everything here is pure data. Nothing loads an asset, touches a host, or
// decides that a renderer is available.

function values(value) {
    return value && value.length !== undefined ? value : [];
}

function objectById(collection, id) {
    const expected = String(id || "");
    const candidates = values(collection);
    for (let index = 0; index < candidates.length; ++index) {
        if (String(candidates[index].id || "") === expected)
            return candidates[index];
    }
    return null;
}

function stateById(theme, id) {
    return objectById(theme ? theme.states : [], id);
}

function stateLayerIds(theme, stateId) {
    const state = stateById(theme, stateId);
    return state ? values(state.layers) : [];
}

function layerInState(theme, layerId, stateId) {
    return stateLayerIds(theme, stateId).map(function(value) {
        return String(value || "");
    }).includes(String(layerId || ""));
}

// The resting states a panel can be drawn in. Hover is a decoration on top of
// one of these, not a fourth resting state.
function normalizedState(value) {
    const requested = String(value || "normal").toLowerCase();
    return ["normal", "open", "collapsed"].includes(requested)
        ? requested : "normal";
}

function normalizedTransition(value) {
    const requested = String(value || "idle").toLowerCase();
    return ["opening", "closing"].includes(requested) ? requested : "idle";
}

// An opening transition runs from collapsed to open; a closing one runs the
// same track backwards.
function transitionFromState(transitionState) {
    return normalizedTransition(transitionState) === "opening"
        ? "collapsed" : "open";
}

function transitionToState(transitionState) {
    return normalizedTransition(transitionState) === "opening"
        ? "open" : "collapsed";
}

function interpolationActive(theme, transitionState, progress) {
    const candidate = Number(progress);
    return ["opening", "closing"].includes(normalizedTransition(transitionState))
        && isFinite(candidate) && candidate >= 0 && candidate <= 1
        && stateById(theme, transitionFromState(transitionState)) !== null
        && stateById(theme, transitionToState(transitionState)) !== null;
}

function effectiveStateId(theme, presentationState, transitionState, progress,
                          hovered) {
    if (!interpolationActive(theme, transitionState, progress)
            && hovered && stateById(theme, "hover"))
        return "hover";
    return normalizedState(presentationState);
}

// Layers shared by both endpoints of a transition stay put; endpoint-only
// layers crossfade. Manifest order inside each state is preserved.
function orderedActiveLayerIds(theme, presentationState, transitionState,
                               progress, hovered) {
    const interpolating = interpolationActive(theme, transitionState, progress);
    const first = interpolating
        ? stateLayerIds(theme, transitionFromState(transitionState))
        : stateLayerIds(theme, effectiveStateId(
            theme, presentationState, transitionState, progress, hovered));
    const result = [];
    for (let index = 0; index < first.length; ++index) {
        const id = String(first[index] || "");
        if (id.length > 0 && !result.includes(id))
            result.push(id);
    }
    if (interpolating) {
        const second = stateLayerIds(theme, transitionToState(transitionState));
        for (let index = 0; index < second.length; ++index) {
            const id = String(second[index] || "");
            if (id.length > 0 && !result.includes(id))
                result.push(id);
        }
    }
    return result;
}

// The declared opacity of one layer, after any crossfade. Synthetic layers a
// renderer built for itself are prefixed with "__" and never crossfade.
function stateLayerOpacity(theme, layer, transitionState, progress) {
    const base = Math.max(0, Math.min(1, Number(
        layer ? layer.opacity === undefined ? 1 : layer.opacity : 0)));
    if (!interpolationActive(theme, transitionState, progress)
            || String(layer.id || "").startsWith("__"))
        return base;
    const inFrom = layerInState(
        theme, layer.id, transitionFromState(transitionState));
    const inTo = layerInState(
        theme, layer.id, transitionToState(transitionState));
    if (inFrom && inTo)
        return base;
    const value = Math.max(0, Math.min(1, Number(progress)));
    if (inFrom)
        return base * (1 - value);
    if (inTo)
        return base * value;
    return 0;
}

// Slices, content regions and input masks are all selected the same way: the
// first record matching this state and orientation.
function recordFor(collection, state, orientation) {
    const candidates = values(collection);
    for (let index = 0; index < candidates.length; ++index) {
        const candidate = candidates[index];
        if (String(candidate.state || "") === String(state)
                && String(candidate.orientation || "") === String(orientation))
            return candidate;
    }
    return null;
}

// A track without a state applies to every state, so an exact state match wins
// and a stateless track is the fallback.
function trackFor(theme, state) {
    const candidates = values(theme ? theme.tracks : []);
    let fallback = null;
    for (let index = 0; index < candidates.length; ++index) {
        const candidate = candidates[index];
        const candidateState = String(candidate.state || "");
        if (candidateState === String(state))
            return candidate;
        if (candidateState.length === 0 && fallback === null)
            fallback = candidate;
    }
    return fallback;
}

function layerForRole(theme, stateId, role) {
    const layerIds = stateLayerIds(theme, stateId);
    for (let index = 0; index < layerIds.length; ++index) {
        const layer = objectById(theme ? theme.layers : [], layerIds[index]);
        if (layer && String(layer.role || "") === String(role))
            return layer;
    }
    return null;
}

// Absolute paths the backend validated inside the managed package. A path that
// is not already a URL is turned into one; anything else resolves to nothing
// rather than being guessed at.
function assetUrl(theme, assetId) {
    const paths = theme && theme.assetPaths
        && typeof theme.assetPaths === "object" ? theme.assetPaths : ({});
    const path = String(paths[String(assetId || "")] || "");
    if (path.startsWith("file:") || path.startsWith("qrc:")
            || path.startsWith("image:"))
        return path;
    if (path.startsWith("/"))
        return "file://" + encodeURI(path);
    return "";
}

function hasFeature(theme, feature) {
    const capabilities = theme && theme.capabilities ? theme.capabilities : ({});
    return values(capabilities.features).map(function(value) {
        return String(value || "").toLowerCase();
    }).includes(String(feature));
}

// A package is structurally a Theme v2 projection the renderer may read.
function isThemeProjection(theme) {
    return Boolean(theme) && theme.valid === true
        && String(theme.format || "") === "org.archdock.theme"
        && Number(theme.version || 0) === 2;
}
