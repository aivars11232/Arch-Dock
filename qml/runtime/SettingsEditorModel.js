.pragma library

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
    return result;
}

function rendererThemeCandidate(session, theme) {
    let result = rendererCandidate(session);
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
    if (source.capabilityResolution
            && typeof source.capabilityResolution === "object") {
        result.capabilityResolution = copyValue(
            source.capabilityResolution);
        const renderer = result.capabilityResolution.renderer;
        result.effectiveRendererTier = String(renderer
            && renderer.effectiveTier || result.rendererTier
            || "procedural2d");
    }
    return result;
}

function cancel(session) {
    if (!session || !session.loaded)
        return session;
    const result = copySession(session);
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
