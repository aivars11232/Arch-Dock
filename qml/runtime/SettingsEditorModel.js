.pragma library

// Panel Studio's editor session as plain data: the snapshot it loaded, the
// user's draft changes to panel and shared settings, the candidates sent to
// the backend for projection and Apply, and the renderer candidates the
// preview, theme cards and preset cards draw. Pure functions, so the session
// can be tested without a window.

function hasOwn(map, key) {
    return map !== null && map !== undefined && Object.prototype.hasOwnProperty.call(map, key);
}

function copyMap(source) {
    const result = {};
    if (source === null || source === undefined)
        return result;

    const keys = Object.keys(source);
    for (let index = 0; index < keys.length; ++index)
        result[keys[index]] = source[keys[index]];
    return result;
}

function copyValue(source) {
    // Qt's sequential containers have length and slice, but Array.isArray()
    // returns false. Preserve nested resource arrays when copying snapshots.
    if (Array.isArray(source) || (source !== null && typeof source === "object"
            && Number.isInteger(source.length) && source.length >= 0
            && typeof source.slice === "function")) {
        const result = [];
        for (let index = 0; index < source.length; ++index)
            result.push(copyValue(source[index]));
        return result;
    }
    if (source !== null && source !== undefined
            && typeof source === "object") {
        const result = {};
        const keys = Object.keys(source);
        for (let index = 0; index < keys.length; ++index)
            result[keys[index]] = copyValue(source[keys[index]]);
        return result;
    }
    return source;
}

function merge(base, changes) {
    const result = copyMap(base);
    const keys = changes === null || changes === undefined ? [] : Object.keys(changes);
    for (let index = 0; index < keys.length; ++index)
        result[keys[index]] = changes[keys[index]];
    return result;
}

function equivalent(left, right) {
    if (left === right)
        return true;
    if (typeof left === "number" && typeof right === "number")
        return Math.abs(left - right) < 0.000001;
    if (left !== null && right !== null && typeof left === "object" && typeof right === "object") {
        const leftKeys = Object.keys(left).sort();
        const rightKeys = Object.keys(right).sort();
        if (leftKeys.length !== rightKeys.length)
            return false;
        for (let index = 0; index < leftKeys.length; ++index) {
            const key = leftKeys[index];
            if (key !== rightKeys[index] || !equivalent(left[key], right[key]))
                return false;
        }
        return true;
    }
    return false;
}

function withoutKey(map, key) {
    const result = {};
    const keys = map === null || map === undefined ? [] : Object.keys(map);
    for (let index = 0; index < keys.length; ++index) {
        if (keys[index] !== key)
            result[keys[index]] = map[keys[index]];
    }
    return result;
}

function changedValue(changes, baseline, key, value) {
    if (equivalent(value, baseline[key]))
        return withoutKey(changes, key);
    const result = copyMap(changes);
    result[key] = value;
    return result;
}

function validSnapshot(snapshot) {
    return snapshot !== null && snapshot !== undefined && snapshot.success === true && String(snapshot.status || "") === "loaded" && String(snapshot.panelId || "").length > 0;
}

function fieldKeys(fields) {
    const result = [];
    const source = fields || [];
    for (let index = 0; index < source.length; ++index) {
        const key = String(source[index].key || "");
        if (key.length > 0 && result.indexOf(key) < 0)
            result.push(key);
    }
    return result;
}

function projectedBaseline(baseline, oldPresentedKeys, fields, values) {
    let result = copyMap(baseline);
    const currentKeys = fieldKeys(fields);
    const previousKeys = oldPresentedKeys || [];
    for (let index = 0; index < previousKeys.length; ++index) {
        if (currentKeys.indexOf(previousKeys[index]) < 0)
            result = withoutKey(result, previousKeys[index]);
    }
    for (let index = 0; index < currentKeys.length; ++index) {
        const key = currentKeys[index];
        if (!hasOwn(result, key) && hasOwn(values, key))
            result[key] = values[key];
    }
    return result;
}

function projectedChanges(changes, oldPresentedKeys, fields) {
    const result = {};
    const currentKeys = fieldKeys(fields);
    const previousKeys = oldPresentedKeys || [];
    const keys = changes === null || changes === undefined ? [] : Object.keys(changes);
    for (let index = 0; index < keys.length; ++index) {
        if (previousKeys.indexOf(keys[index]) < 0 || currentKeys.indexOf(keys[index]) >= 0)
            result[keys[index]] = changes[keys[index]];
    }
    return result;
}

function emptySession(errorCode, errorMessage) {
    return {
        loaded: false,
        panelId: "",
        revision: 0,
        consumer: "",
        panelBaseline: {},
        initialPanelKeys: [],
        globalBaseline: {},
        panelChanges: {},
        globalChanges: {},
        panelFields: [],
        globalFields: [],
        panelPresentedKeys: [],
        globalPresentedKeys: [],
        themes: [],
        themeDefinition: {},
        themeProjectionStatus: "unavailable",
        themeProjectionError: "",
        iconStyles: [],
        animationProfiles: [],
        iconStyleDefinition: {},
        iconStyleProjectionStatus: "unavailable",
        iconStyleProjectionError: "",
        capabilityResolution: {},
        status: "load-failed",
        errorCode: String(errorCode || "invalid-editor-snapshot"),
        errorMessage: String(errorMessage || "")
    };
}

function load(snapshot) {
    if (!validSnapshot(snapshot)) {
        return emptySession(snapshot ? snapshot.errorCode : "", snapshot ? snapshot.errorMessage : "");
    }
    return {
        loaded: true,
        panelId: String(snapshot.panelId),
        revision: Number(snapshot.revision),
        consumer: String(snapshot.consumer || ""),
        panelBaseline: copyMap(snapshot.panelValues),
        flatLookValues: copyValue(snapshot.flatLookValues || {}),
        initialFlatLookValues: copyValue(snapshot.flatLookValues || {}),
        restoredLookKeys: [],
        initialPanelKeys: Object.keys(snapshot.panelValues),
        globalBaseline: copyMap(snapshot.globalValues),
        panelChanges: {},
        globalChanges: {},
        panelFields: (snapshot.panelFields || []).slice(),
        globalFields: (snapshot.globalFields || []).slice(),
        panelPresentedKeys: fieldKeys(snapshot.panelFields),
        globalPresentedKeys: fieldKeys(snapshot.globalFields),
        themes: (snapshot.themes || []).slice(),
        themeDefinition: copyValue(snapshot.themeDefinition || {}),
        themeProjectionStatus: String(
            snapshot.themeProjectionStatus || "unavailable"),
        themeProjectionError: String(snapshot.themeProjectionError || ""),
        iconStyles: (snapshot.iconStyles || []).slice(),
        animationProfiles: copyValue(snapshot.animationProfiles || []),
        iconStyleDefinition: copyValue(snapshot.iconStyleDefinition || {}),
        iconStyleProjectionStatus: String(
            snapshot.iconStyleProjectionStatus || "unavailable"),
        iconStyleProjectionError: String(
            snapshot.iconStyleProjectionError || ""),
        capabilityResolution: copyMap(snapshot.capabilityResolution),
        status: "loaded",
        errorCode: "",
        errorMessage: ""
    };
}

function copySession(session) {
    const result = {};
    const keys = session === null || session === undefined ? [] : Object.keys(session);
    for (let index = 0; index < keys.length; ++index)
        result[keys[index]] = session[keys[index]];
    return result;
}

function setPanelValue(session, key, value) {
    if (!session || !session.loaded || !hasOwn(session.panelBaseline, key))
        return session;
    const result = copySession(session);
    result.panelChanges = changedValue(session.panelChanges, session.panelBaseline, key, value);
    result.status = "editing";
    result.errorCode = "";
    result.errorMessage = "";
    return result;
}

function setGlobalValue(session, key, value) {
    if (!session || !session.loaded || !hasOwn(session.globalBaseline, key))
        return session;
    const result = copySession(session);
    result.globalChanges = changedValue(session.globalChanges, session.globalBaseline, key, value);
    result.status = "editing";
    result.errorCode = "";
    result.errorMessage = "";
    return result;
}

function stagePanelValues(session, values) {
    let result = session;
    const keys = values === null || values === undefined ? [] : Object.keys(values);
    for (let index = 0; index < keys.length; ++index)
        result = setPanelValue(result, keys[index], values[keys[index]]);
    return result;
}

function platformTier(tier) {
    return ["baked2.5d", "true3d"].includes(String(tier || ""));
}

function currentFlatLook(session) {
    const values = copyValue(session.flatLookValues || {});
    for (const key of Object.keys(values))
        if (hasOwn(session.panelChanges, key)) values[key] = copyValue(session.panelChanges[key]);
    return values;
}

// A theme/remembered look can change tier and therefore reveal fields that
// were absent from the old tier's controls. Only the backend's complete
// appearance projection supplies these extra keys; ordinary controls
// still use setPanelValue's fail-closed snapshot rule.
function stageLookValues(session, values) {
    let result = copySession(session);
    result.panelBaseline = copyMap(session.panelBaseline);
    for (const key of Object.keys(values || {})) {
        if (!hasOwn(result.panelBaseline, key) && hasOwn(session.flatLookValues, key))
            result.panelBaseline[key] = copyValue(session.flatLookValues[key]);
        result = setPanelValue(result, key, copyValue(values[key]));
    }
    return result;
}

function rememberFlatLook(session) {
    if (platformTier(panelValue(session, "rendererTier", ""))) return session;
    return setPanelValue(session, "previousFlatLook", currentFlatLook(session));
}

function platformOff(session) {
    const saved = copyValue(panelValue(session, "previousFlatLook", {}));
    let result;
    if (Object.keys(saved).length) {
        result = stageLookValues(session, saved);
        result.restoredLookKeys = Object.keys(saved).filter(key => hasOwn(session.flatLookValues, key));
    } else {
        // Legacy platforms have no previous snapshot. Their preserved
        // procedural appearance is the deterministic flat fallback.
        result = stageLookValues(session, { rendererTier: "procedural2d",
            panelThemeId: "", completeThemeId: "" });
    }
    return setPanelValue(result, "previousFlatLook", {});
}

function stageThemeLook(session, candidate) {
    let values = copyValue(candidate.values || {});
    const wasPlatform = platformTier(panelValue(session, "rendererTier", ""));
    const becomesPlatform = platformTier(values.rendererTier);
    let result = session;
    if (becomesPlatform && !wasPlatform)
        values.previousFlatLook = currentFlatLook(session);
    else if (!becomesPlatform && wasPlatform) {
        result = platformOff(session);
        values = copyValue(candidate.themeValues || values);
        values.previousFlatLook = {};
    }
    return stageLookValues(result, values);
}

function panelValue(session, key, fallback) {
    if (!session)
        return fallback;
    if (hasOwn(session.panelChanges, key))
        return session.panelChanges[key];
    return hasOwn(session.panelBaseline, key) ? session.panelBaseline[key] : fallback;
}

function globalValue(session, key, fallback) {
    if (!session)
        return fallback;
    if (hasOwn(session.globalChanges, key))
        return session.globalChanges[key];
    return hasOwn(session.globalBaseline, key) ? session.globalBaseline[key] : fallback;
}

function keyCount(map) {
    return map === null || map === undefined ? 0 : Object.keys(map).length;
}

function dirty(session) {
    return Boolean(session && session.loaded) && (keyCount(session.panelChanges) > 0 || keyCount(session.globalChanges) > 0);
}

function panelCandidate(session) {
    return session && session.loaded ? merge(session.panelBaseline, session.panelChanges) : {};
}

function transactionPanelCandidate(session) {
    const candidate = panelCandidate(session);
    // Projected controls supply preview values. Submit newly exposed fields
    // only when edited, so switching back does not send inactive defaults.
    for (const key of Object.keys(candidate)) {
        if (!(session.initialPanelKeys || []).includes(key)
                && !hasOwn(session.panelChanges, key))
            delete candidate[key];
    }
    return candidate;
}

function globalCandidate(session) {
    return session && session.loaded ? merge(session.globalBaseline, session.globalChanges) : {};
}

function rendererCandidate(session) {
    if (!session || !session.loaded)
        return {};
    const result = merge(globalCandidate(session), panelCandidate(session));
    result.capabilityResolution = copyValue(session.capabilityResolution);
    const renderer = result.capabilityResolution
        && typeof result.capabilityResolution.renderer === "object"
        ? result.capabilityResolution.renderer : {};
    result.effectiveRendererTier = String(renderer.effectiveTier
        || result.rendererTier || "procedural2d");
    result.iconStyleDefinition = copyValue(
        session.iconStyleDefinition || {});
    result.animationProfiles = copyValue(session.animationProfiles || []);
    const segments = (session.panelFields || []).find(function(field) { return field.key === "segments"; });
    if (segments) {
        result.segmentCapabilities = copyValue(segments.segmentCapabilities || {});
        result.segmentEntries = copyValue(segments.segmentEntries || []);
    }
    return result;
}

// The record a theme card hands the shared renderer. A card shows a theme, not
// the panel it would be loaded on: the panel still supplies its entries and
// icons, but its angle, scale, rotation, collapse, tilt, camera, radius and
// renderer tier are one panel's state, and they bent every other theme's card.
// A theme that sets one of these in its own style keeps it.
function rendererThemeCandidate(session, theme) {
    let result = merge(rendererCandidate(session), {
        layoutAngle: 0, layoutScale: 1, panelRotationMode: "none",
        presentationMode: "open", collapseMechanism: "open"
    });
    const ownDefaults = [
        "rendererTier", "layoutRadius", "bakedTilt", "scene3DCameraPitch",
        "scene3DCameraYaw", "scene3DThickness", "scene3DIconElevation",
        "scene3DRoll", "scene3DPositionX", "scene3DPositionY", "scene3DPositionZ",
        "scene3DScale", "scene3DFieldOfView", "scene3DKeyLight", "scene3DFillLight",
        "scene3DTransitions", "scene3DFloat", "scene3DBand", "scene3DBend", "scene3DFold",
        "scene3DColor", "scene3DMaterial", "scene3DTexture", "sparkleIntensity"
    ];
    for (let index = 0; index < ownDefaults.length; ++index)
        delete result[ownDefaults[index]];
    const source = theme && typeof theme === "object" ? theme : {};
    const styleGroups = [
        "panelStyle", "iconStyle", "tileStyle", "indicatorStyle",
        "animationStyle", "layoutStyle"
    ];
    for (let index = 0; index < styleGroups.length; ++index) {
        const style = source[styleGroups[index]];
        if (style && typeof style === "object")
            result = merge(result, copyValue(style));
    }
    const themeId = String(source.id || "");
    if (themeId.length > 0) {
        result.panelThemeId = themeId;
        result.completeThemeId = themeId;
    }
    result.recommendedIconStyleId = String(source.iconStyleRef
        && source.iconStyleRef.id || "");
    // A wide platform is drawn at card scale, so its icons stay readable.
    const cardRadius = 110;
    const radius = Number(result.layoutRadius);
    result.layoutRadius = radius > 0 ? Math.min(radius, cardRadius) : cardRadius;
    // The tier is the one the theme was resolved with; the selected panel's
    // own resolution describes a different candidate and is not passed on.
    result.capabilityResolution = copyValue(
        source.capabilityResolution
        && typeof source.capabilityResolution === "object"
        ? source.capabilityResolution : {});
    const renderer = result.capabilityResolution.renderer;
    result.rendererTier = String(renderer && renderer.requestedTier
        || result.rendererTier || "procedural2d");
    result.effectiveRendererTier = String(renderer
        && renderer.effectiveTier || result.rendererTier);
    return result;
}

// The record a preset card hands the shared renderer, in the shape
// rendererCandidate() produces, or {} when the preset cannot be drawn here.
// A still candidate keeps the preset's look and stops its motion.
function presetRendererCandidate(card, still) {
    const preview = card ? card.preview : null;
    const definition = preview ? preview.panelDefinition : null;
    if (definition === null || definition === undefined || typeof definition !== "object")
        return {};
    // A record from the backend carries native lists. The renderer is handed
    // plain arrays, as it is by rendererCandidate().
    const result = copyValue(definition);
    if (still === true)
        result.reducedMotion = true;
    return result;
}

// The theme a preset card is drawn on, or {} for the procedural surface.
function presetPreviewTheme(card) {
    const preview = card ? card.preview : null;
    const theme = preview ? preview.themeDefinition : null;
    return theme !== null && theme !== undefined && typeof theme === "object"
        ? copyValue(theme) : {};
}

// How a preset card presents itself: "ready", "fallback" when it is usable
// through the safe fallback it declares, or "incompatible" when there is
// nothing to draw or apply.
function presetState(card) {
    const compatibility = card && card.compatibility ? card.compatibility : {};
    if (compatibility.available !== true || keyCount(presetRendererCandidate(card, false)) === 0)
        return "incompatible";
    return compatibility.fallbackApplied === true ? "fallback" : "ready";
}

function cancel(session) {
    if (!session || !session.loaded)
        return session;
    const result = copySession(session);
    result.flatLookValues = copyValue(session.initialFlatLookValues || {});
    result.restoredLookKeys = [];
    result.panelChanges = {};
    result.globalChanges = {};
    result.status = "cancelled";
    result.errorCode = "";
    result.errorMessage = "";
    return result;
}

function canSwitchPanel(session, panelId) {
    if (!session || !session.loaded)
        return true;
    return String(panelId || "") === session.panelId || !dirty(session);
}

function withProjection(session, projection) {
    if (!session || !session.loaded || !projection || projection.success !== true || String(projection.status || "") !== "resolved")
        return session;
    const result = copySession(session);
    result.panelFields = (projection.panelFields || []).slice();
    result.globalFields = (projection.globalFields || []).slice();
    result.panelBaseline = projectedBaseline(session.panelBaseline, session.panelPresentedKeys, result.panelFields, projection.panelValues || {});
    result.globalBaseline = projectedBaseline(session.globalBaseline, session.globalPresentedKeys, result.globalFields, projection.globalValues || {});
    result.panelChanges = projectedChanges(session.panelChanges, session.panelPresentedKeys, result.panelFields);
    for (const key of session.restoredLookKeys || [])
        if (hasOwn(session.panelChanges, key)) result.panelChanges[key] = session.panelChanges[key];
    result.flatLookValues = copyValue(projection.flatLookValues || session.flatLookValues || {});
    result.globalChanges = projectedChanges(session.globalChanges, session.globalPresentedKeys, result.globalFields);
    result.panelPresentedKeys = fieldKeys(result.panelFields);
    result.globalPresentedKeys = fieldKeys(result.globalFields);
    result.themes = (projection.themes || []).slice();
    result.themeDefinition = copyValue(projection.themeDefinition || {});
    result.themeProjectionStatus = String(
        projection.themeProjectionStatus || "unavailable");
    result.themeProjectionError = String(
        projection.themeProjectionError || "");
    result.iconStyles = (projection.iconStyles || []).slice();
    result.animationProfiles = copyValue(projection.animationProfiles || session.animationProfiles || []);
    result.iconStyleDefinition = copyValue(
        projection.iconStyleDefinition || {});
    result.iconStyleProjectionStatus = String(
        projection.iconStyleProjectionStatus || "unavailable");
    result.iconStyleProjectionError = String(
        projection.iconStyleProjectionError || "");
    result.capabilityResolution = copyMap(projection.capabilityResolution);
    result.status = dirty(session) ? "editing" : "loaded";
    result.errorCode = "";
    result.errorMessage = "";
    return result;
}

function transactionSucceeded(result) {
    return result !== null && result !== undefined && result.success === true && String(result.status || "") === "succeeded";
}

function transactionConflict(result) {
    return result !== null && result !== undefined && (String(result.status || "") === "revision-conflict" || String(result.errorCode || "") === "stale-revision");
}

function retainFailure(session, result) {
    if (!session || !session.loaded)
        return session;
    const failed = copySession(session);
    failed.status = transactionConflict(result) ? "conflict" : "failed";
    failed.errorCode = String(result && result.errorCode ? result.errorCode : "transaction-failed");
    failed.errorMessage = String(result && result.errorMessage ? result.errorMessage : "");
    return failed;
}

function adoptResult(session, result, refreshedSnapshot) {
    if (!transactionSucceeded(result))
        return retainFailure(session, result);
    const refreshed = load(refreshedSnapshot);
    if (!refreshed.loaded)
        return retainFailure(session, {
            errorCode: "refresh-failed",
            errorMessage: refreshed.errorMessage
        });
    return refreshed;
}

// Refresh runtime feedback without turning changing source availability into a
// user edit, or dropping an unsaved preference when its source disappears.
function withContentFeedback(session, snapshot) {
    if (!session || !session.loaded || !snapshot || snapshot.success !== true)
        return session;
    const result = copySession(session);
    const liveFields = snapshot.panelFields || [];
    const overlayKeys = ["showBadges", "showProgress"];
    result.panelFields = result.panelFields.filter(function(field) {
        return !overlayKeys.includes(field.key);
    });
    for (const field of liveFields) {
        if (!overlayKeys.includes(field.key)) continue;
        result.panelFields.push(copyValue(field));
        if (result.panelBaseline[field.key] === undefined)
            result.panelBaseline[field.key] = snapshot.panelValues[field.key];
    }
    const live = liveFields.find(function(field) { return field.key === "segments"; });
    const target = result.panelFields.find(function(field) { return field.key === "segments"; });
    if (live && target) {
        target.segmentCapabilities = copyValue(live.segmentCapabilities || {});
        target.availableEntries = copyValue(live.availableEntries || []);
        target.segmentEntries = (target.segmentEntries || []).map(function(entry) {
            const next = copyValue(entry);
            const source = target.availableEntries.find(function(row) { return row.appId === entry.appId; }) || {};
            for (const key of ["badgeText", "progress", "urgent", "temporaryStatus", "overlayAvailable",
                               "statusAvailable", "statusText"])
                next[key] = source[key] !== undefined ? source[key]
                    : key === "progress" ? -1 : key.endsWith("Available") || key === "urgent" ? false : "";
            return next;
        });
    }
    result.panelPresentedKeys = fieldKeys(result.panelFields);
    return result;
}
