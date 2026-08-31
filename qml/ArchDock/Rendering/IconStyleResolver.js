.pragma library

const statePrecedence = [
    "edit", "disabled", "drop", "urgent", "pressed", "hover",
    "launching", "active", "minimized", "running", "normal"
];

function objectValue(value) {
    return value !== null && value !== undefined && typeof value === "object"
        ? value : {};
}

function arrayValue(value) {
    if (Array.isArray(value))
        return value;
    return value !== null && value !== undefined
        && typeof value === "object" && value.length !== undefined
        ? value : [];
}

function finiteNumber(value, fallback) {
    const candidate = Number(value);
    return isFinite(candidate) ? candidate : fallback;
}

function bounded(value, minimum, maximum, fallback) {
    return Math.max(minimum, Math.min(maximum,
        finiteNumber(value, fallback)));
}

function copyMap(source) {
    const input = objectValue(source);
    const result = {};
    const keys = Object.keys(input);
    for (let index = 0; index < keys.length; ++index)
        result[keys[index]] = input[keys[index]];
    return result;
}

function usableDefinition(styleDefinition) {
    const style = objectValue(styleDefinition);
    return style.valid === true
        && style.loadable !== false
        && String(style.format || "") === "org.archdock.icon-style"
        && Number(style.version || 0) === 1
        && String(style.id || "").length > 0
        && arrayValue(style.states).length > 0;
}

function stateId(flags) {
    const values = objectValue(flags);
    const enabled = {
        edit: Boolean(values.edit || values.editMode),
        disabled: Boolean(values.disabled),
        drop: Boolean(values.drop || values.dropTarget),
        urgent: Boolean(values.urgent),
        pressed: Boolean(values.pressed),
        hover: Boolean(values.hover || values.hovered),
        launching: Boolean(values.launching),
        active: Boolean(values.active),
        minimized: Boolean(values.minimized),
        running: Boolean(values.running),
        normal: true
    };
    for (let index = 0; index < statePrecedence.length; ++index) {
        const candidate = statePrecedence[index];
        if (enabled[candidate])
            return candidate;
    }
    return "normal";
}

function fallbackState(id) {
    const minimized = id === "minimized";
    const disabled = id === "disabled";
    return {
        id: id,
        rearOpacity: disabled ? 0.5 : 1,
        baseOpacity: disabled ? 0.5 : 1,
        frontOpacity: disabled ? 0.5 : 1,
        glyphOpacity: disabled ? 0.4 : minimized ? 0.68 : 1,
        glyphScale: 1,
        borderColor: "transparent",
        glowColor: "transparent",
        glowOpacity: 0,
        reflectionOpacity: 0,
        indicatorColor: id === "urgent" ? "#ff4d6d" : "#78a9ff",
        indicatorOpacity: ["active", "running", "urgent"].includes(id)
            ? 1 : id === "minimized" || id === "launching" || id === "edit"
                ? 0.5 : 0
    };
}

function definitionState(styleDefinition, requestedState) {
    const states = arrayValue(objectValue(styleDefinition).states);
    for (let index = 0; index < states.length; ++index) {
        if (String(states[index].id || "") === requestedState)
            return states[index];
    }
    for (let index = 0; index < states.length; ++index) {
        if (String(states[index].id || "") === "normal")
            return states[index];
    }
    return fallbackState(requestedState);
}

function normalizedState(styleDefinition, requestedState) {
    const source = definitionState(styleDefinition, requestedState);
    const fallback = fallbackState(requestedState);
    return {
        id: requestedState,
        rearOpacity: bounded(source.rearOpacity, 0, 1,
                             fallback.rearOpacity),
        baseOpacity: bounded(source.baseOpacity, 0, 1,
                             fallback.baseOpacity),
        frontOpacity: bounded(source.frontOpacity, 0, 1,
                              fallback.frontOpacity),
        glyphOpacity: bounded(source.glyphOpacity, 0, 1,
                              fallback.glyphOpacity),
        glyphScale: bounded(source.glyphScale, 0.5, 1.5,
                            fallback.glyphScale),
        borderColor: String(source.borderColor || fallback.borderColor),
        glowColor: String(source.glowColor || fallback.glowColor),
        glowOpacity: bounded(source.glowOpacity, 0, 1,
                             fallback.glowOpacity),
        reflectionOpacity: bounded(source.reflectionOpacity, 0, 1,
                                   fallback.reflectionOpacity),
        indicatorColor: String(source.indicatorColor
                               || fallback.indicatorColor),
        indicatorOpacity: bounded(source.indicatorOpacity, 0, 1,
                                  fallback.indicatorOpacity)
    };
}

function normalizedInset(styleDefinition) {
    const source = objectValue(objectValue(styleDefinition).safeGlyphInset);
    return {
        left: bounded(source.left, 0, 0.45, 0),
        top: bounded(source.top, 0, 0.45, 0),
        right: bounded(source.right, 0, 0.45, 0),
        bottom: bounded(source.bottom, 0, 0.45, 0)
    };
}

function entryIdentity(entry) {
    const source = objectValue(entry);
    return String(source.stableIdentity || source.desktopEntryId
                  || source.appId || source.id || "").trim();
}

function originalGlyph(entry) {
    const source = objectValue(entry);
    return String(source.resolvedGlyph || source.customGlyph
                  || source.iconName || source.iconSource
                  || "application-x-executable");
}

function entryStyleDefinition(entry) {
    const source = objectValue(entry);
    const direct = objectValue(source.resolvedIconStyleDefinition);
    if (Object.keys(direct).length > 0)
        return direct;
    return objectValue(
        objectValue(source.iconOverrideResolution).iconStyleDefinition);
}

function entryTileEnabled(entry) {
    const source = objectValue(entry);
    if (source.tileEnabled !== undefined && source.tileEnabled !== null)
        return Boolean(source.tileEnabled);
    const resolution = objectValue(source.iconOverrideResolution);
    if (resolution.tileEnabled !== undefined
            && resolution.tileEnabled !== null)
        return Boolean(resolution.tileEnabled);
    return true;
}

function resolvedGlyph(styleDefinition, entry) {
    const style = objectValue(styleDefinition);
    const original = originalGlyph(entry);
    const policy = objectValue(style.glyphPolicy);
    if (String(policy.mode || "original") !== "mapped-replacement") {
        return {
            source: original,
            replacementApplied: false,
            policy: String(policy.mode || "original")
        };
    }
    const identity = entryIdentity(entry);
    const replacements = objectValue(style.mappedReplacements);
    const relativePath = identity.length > 0
        ? String(replacements[identity] || "") : "";
    const paths = objectValue(style.assetPaths);
    const replacement = relativePath.length > 0
        ? String(paths[relativePath] || "") : "";
    return {
        source: replacement.length > 0 ? replacement : original,
        replacementApplied: replacement.length > 0,
        policy: "mapped-replacement"
    };
}

function layersForRole(styleDefinition, role) {
    const layers = objectValue(objectValue(styleDefinition).layers);
    if (["rear", "base", "front"].includes(role))
        return arrayValue(layers[role]);
    const optional = layers[role];
    return optional !== null && optional !== undefined
        && typeof optional === "object" ? [optional] : [];
}

function assetSource(styleDefinition, layer) {
    const source = objectValue(layer);
    if (String(source.kind || "procedural") !== "asset")
        return "";
    const relativePath = String(source.asset || "");
    return String(objectValue(objectValue(styleDefinition).assetPaths)
                  [relativePath] || "");
}

function hasRenderableLayers(styleDefinition) {
    const roles = ["rear", "base", "front", "reflection", "shadow", "glow"];
    for (let index = 0; index < roles.length; ++index) {
        if (layersForRole(styleDefinition, roles[index]).length > 0)
            return true;
    }
    return false;
}

function resolve(styleDefinition, flags, entry) {
    const entryStyle = entryStyleDefinition(entry);
    const hasEntryStyle = Object.keys(entryStyle).length > 0;
    const entryStyleUsable = usableDefinition(entryStyle);
    const panelStyleUsable = usableDefinition(styleDefinition);
    const usable = entryStyleUsable || panelStyleUsable;
    const style = entryStyleUsable ? entryStyle
        : panelStyleUsable ? styleDefinition : {};
    const requestedState = stateId(flags);
    const glyph = resolvedGlyph(style, entry);
    const styleId = usable ? String(style.id) : "plain-original";
    const tileEnabled = entryTileEnabled(entry);
    return {
        valid: usable,
        styleId: styleId,
        styleDefinition: style,
        stateId: requestedState,
        state: usable ? normalizedState(style, requestedState)
                      : fallbackState(requestedState),
        glyphSource: glyph.source,
        glyphPolicy: glyph.policy,
        replacementApplied: glyph.replacementApplied,
        safeGlyphInset: usable ? normalizedInset(style)
                               : {left: 0, top: 0, right: 0, bottom: 0},
        layers: usable ? objectValue(style.layers) : {},
        assetPaths: usable ? objectValue(style.assetPaths) : {},
        tileEnabled: tileEnabled,
        renderStyledLayers: usable && styleId !== "plain-original"
            && tileEnabled && hasRenderableLayers(style),
        fallbackApplied: !usable || (hasEntryStyle && !entryStyleUsable),
        fallbackReason: !usable ? "style-unavailable"
            : hasEntryStyle && !entryStyleUsable
                ? "entry-style-unavailable" : ""
    };
}

function indicatorStyle(baseStyle, resolvedStyle) {
    const result = copyMap(baseStyle);
    const resolved = objectValue(resolvedStyle);
    const state = objectValue(resolved.state);
    if (resolved.valid && String(state.indicatorColor || "").length > 0) {
        result.color = state.indicatorColor;
        result.activeColor = state.indicatorColor;
        result.urgentColor = state.indicatorColor;
    }
    return result;
}

function layerIds(styleDefinition, role) {
    const result = [];
    const layers = layersForRole(styleDefinition, role);
    for (let index = 0; index < layers.length; ++index)
        result.push(String(layers[index].id || ""));
    return result;
}
