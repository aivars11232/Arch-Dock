.pragma library

// Resolves an icon style for one icon state: which declared layers are
// drawn, with what colours, opacity and treatment, by the style's state
// precedence (edit and disabled first, normal last). Pure functions used by
// IconScene and IconStyle2D; the 3D scene draws the textures they produce.

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

function entryTileEnabled(entry, fallback) {
    const source = objectValue(entry);
    const resolution = objectValue(source.iconOverrideResolution);
    // A resolved entry carries its explicit override separately. Use the
    // draft panel default when it is absent, rather than a stale saved value.
    if (resolution.override !== undefined) {
        const override = objectValue(resolution.override);
        return override.tileEnabled === undefined ? fallback : Boolean(override.tileEnabled);
    }
    if (source.tileEnabled !== undefined && source.tileEnabled !== null)
        return Boolean(source.tileEnabled);
    if (resolution.tileEnabled !== undefined
            && resolution.tileEnabled !== null)
        return Boolean(resolution.tileEnabled);
    return fallback;
}

// A colorful application glyph must never be recolored just because a style
// asked for it. Compatibility is an explicit entry declaration; the symbolic
// naming convention is accepted as a second, equally explicit signal.
function glyphCompatible(entry) {
    const source = objectValue(entry);
    if (source.glyphCompatible !== undefined && source.glyphCompatible !== null)
        return Boolean(source.glyphCompatible);
    if (source.symbolicGlyph !== undefined && source.symbolicGlyph !== null)
        return Boolean(source.symbolicGlyph);
    return /-symbolic$/.test(originalGlyph(entry));
}

function plainGlyph(entry, requested, compatible, reason) {
    return {
        source: originalGlyph(entry),
        replacementApplied: false,
        policy: requested,
        treatment: "original",
        tint: "",
        isMask: false,
        compatible: compatible,
        reason: reason
    };
}

function resolvedGlyph(styleDefinition, entry) {
    const style = objectValue(styleDefinition);
    const policy = objectValue(style.glyphPolicy);
    const requested = String(policy.mode || "original");
    const compatibleOnly = policy.compatibleOnly === undefined
        || policy.compatibleOnly === null
        ? true : Boolean(policy.compatibleOnly);
    const compatible = glyphCompatible(entry);

    if (requested === "original")
        return plainGlyph(entry, requested, compatible, "");

    if (requested === "mapped-replacement") {
        // No complete replacement without an exact application-identity map.
        const identity = entryIdentity(entry);
        const replacements = objectValue(style.mappedReplacements);
        const relativePath = identity.length > 0
            ? String(replacements[identity] || "") : "";
        const paths = objectValue(style.assetPaths);
        const replacement = relativePath.length > 0
            ? String(paths[relativePath] || "") : "";
        if (replacement.length === 0) {
            return plainGlyph(entry, requested, compatible,
                              "no-application-mapping");
        }
        return {
            source: replacement,
            replacementApplied: true,
            policy: requested,
            treatment: "mapped-replacement",
            tint: "",
            isMask: false,
            compatible: compatible,
            reason: ""
        };
    }

    // tinted and monochrome both require a declared tint and, unless the
    // style explicitly opts out, a glyph that is safe to recolor.
    const tint = String(policy.tint || "");
    if (tint.length === 0)
        return plainGlyph(entry, requested, compatible, "missing-tint");
    if (compatibleOnly && !compatible)
        return plainGlyph(entry, requested, compatible, "glyph-not-compatible");

    return {
        source: originalGlyph(entry),
        replacementApplied: false,
        policy: requested,
        treatment: requested,
        tint: tint,
        isMask: requested === "monochrome",
        compatible: compatible,
        reason: ""
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

// The mask role shapes the styled composite; it is not itself a visible
// layer, so it never counts toward hasRenderableLayers().
function maskLayerFor(styleDefinition) {
    const layers = layersForRole(styleDefinition, "mask");
    if (layers.length === 0)
        return null;
    const layer = objectValue(layers[0]);
    const source = assetSource(styleDefinition, layer);
    if (source.length === 0)
        return null;
    return {
        id: String(layer.id || "mask"),
        source: source,
        inset: bounded(layer.inset, 0, 0.45, 0),
        opacity: bounded(layer.opacity, 0, 1, 1)
    };
}

function hasRenderableLayers(styleDefinition) {
    const roles = ["rear", "base", "front", "reflection", "shadow", "glow"];
    for (let index = 0; index < roles.length; ++index) {
        if (layersForRole(styleDefinition, roles[index]).length > 0)
            return true;
    }
    return false;
}

function iconShape(styleDefinition, settings) {
    const shapes = ["rounded", "square", "squircle", "circle", "hexagon", "diamond"];
    const parameters = objectValue(objectValue(objectValue(styleDefinition).extensions)
        ["org.archdock.iconParameters"]);
    const signature = String(parameters.defaultShape || "rounded");
    const selected = String(objectValue(settings).iconShape || "style-default");
    return shapes.includes(selected) ? selected
        : shapes.includes(signature) ? signature : "rounded";
}

function parameterizedDefinition(styleDefinition, settings, shape, logicalSize) {
    // PD-20/21: transform declared layers, never branch on a built-in ID.
    // Copy before changing a layer: manifests are shared by panel previews,
    // live hosts and per-entry overrides.
    const style = copyMap(styleDefinition);
    const layers = {};
    const source = objectValue(style.layers);
    const values = objectValue(settings);
    const size = Math.max(1, finiteNumber(logicalSize, finiteNumber(values.iconSize, 52)));
    const diameter = bounded(values.iconDiameter, 40, 100, 100) / 100;
    const outline = bounded(values.iconOutlineWidth, -1, 12, -1);
    const bodyColor = String(values.iconBodyColor || "");
    const outlineColor = String(values.iconOutlineColor || "");
    const glowColor = String(values.iconGlowColor || "");
    const pedestalColor = String(values.iconPedestalColor || "");
    for (const role of Object.keys(source)) {
        const resolved = [];
        for (const declared of layersForRole(styleDefinition, role)) {
            if (declared.option === "pedestal" && !values.iconPedestalEnabled)
                continue;
            if (declared.option === "shape-override"
                    && !["rounded", "square", "squircle", "circle", "hexagon", "diamond"]
                        .includes(String(values.iconShape || "style-default")))
                continue;
            const layer = copyMap(declared);
            layer.inset = (1 - (1 - 2 * bounded(layer.inset, 0, 0.45, 0)) * diameter) / 2;
            if (outline >= 0 && Number(layer.borderWidth || 0) > 0)
                layer.borderWidth = outline / size;
            if (outlineColor && Number(layer.borderWidth || 0) > 0 && role !== "glow")
                layer.borderColor = outlineColor;
            if (role === "base" && layer.option !== "pedestal" && bodyColor) {
                layer.color = bodyColor;
                layer.secondaryColor = Qt.lighter(bodyColor, 1.35).toString();
            }
            if (role === "glow" && glowColor) {
                layer.color = glowColor;
                layer.borderColor = glowColor;
            }
            if (layer.option === "pedestal") {
                layer.heightFactor = bounded(values.iconPedestalHeight, 5, 50, 20) / 100;
                if (pedestalColor) {
                    layer.color = pedestalColor;
                    layer.secondaryColor = Qt.lighter(pedestalColor, 1.35).toString();
                }
            }
            if (layer.followsIconShape === true) {
                if (layer.shape === "ring") {
                    layer.color = "transparent";
                    layer.secondaryColor = "transparent";
                }
                layer.shape = shape === "rounded" ? "rounded-rect" : shape;
                layer.radius = shape === "square" ? 0
                    : shape === "squircle" ? 0.32
                    : shape === "circle" ? 0.5
                    : shape === "diamond" ? 0.04 : 0.22;
            }
            resolved.push(layer);
        }
        if (["rear", "base", "front"].includes(role))
            layers[role] = resolved;
        else if (resolved.length > 0)
            layers[role] = resolved[0];
    }
    style.layers = layers;
    return style;
}

function glyphInterior(styleDefinition, settings, shape, logicalSize) {
    const values = objectValue(settings);
    const size = Math.max(1, finiteNumber(logicalSize, finiteNumber(values.iconSize, 52)));
    const diameter = bounded(values.iconDiameter, 40, 100, 100) / 100;
    const outline = bounded(values.iconOutlineWidth, -1, 12, -1);
    let inset = 0.06;
    let border = 0.015;
    for (const layer of layersForRole(styleDefinition, "base")) {
        if (layer.option === "pedestal" || layer.kind === "asset")
            continue;
        inset = bounded(layer.inset, 0, 0.45, inset);
        border = bounded(layer.borderWidth, 0, 0.25, border);
        break;
    }
    if (values.iconTileMode === "custom") {
        inset = 0.04;
        border = bounded(values.iconTileBorderWidth, 0, 8, 1) / size;
    } else if (outline >= 0) {
        border = outline / size;
    }
    const shapeFactor = shape === "diamond" ? 0.96 * Math.SQRT1_2 : 1;
    return Math.max(0.05, (1 - 2 * inset) * diameter * shapeFactor - 2 * border);
}

function resolve(styleDefinition, flags, entry, tileSettings, logicalSize) {
    const entryStyle = entryStyleDefinition(entry);
    const hasEntryStyle = Object.keys(entryStyle).length > 0;
    const entryStyleUsable = usableDefinition(entryStyle);
    const panelStyleUsable = usableDefinition(styleDefinition);
    const usable = entryStyleUsable || panelStyleUsable;
    const sourceStyle = entryStyleUsable ? entryStyle
        : panelStyleUsable ? styleDefinition : {};
    const tiles = objectValue(tileSettings);
    const shape = iconShape(sourceStyle, tiles);
    const style = parameterizedDefinition(sourceStyle, tiles, shape, logicalSize);
    const requestedState = stateId(flags);
    const state = usable ? normalizedState(style, requestedState) : fallbackState(requestedState);
    if (tiles.iconOutlineColor)
        state.borderColor = String(tiles.iconOutlineColor);
    if (tiles.iconGlowColor)
        state.glowColor = String(tiles.iconGlowColor);
    const parametersActive = usable && objectValue(sourceStyle.extensions)
        ["org.archdock.iconParameters"] !== undefined;
    const innerDiameter = glyphInterior(sourceStyle, tiles, shape, logicalSize);
    if (parametersActive) {
        const logo = innerDiameter * bounded(tiles.iconLogoSize, 55, 100, 95) / 100;
        const normalScale = normalizedState(sourceStyle, "normal").glyphScale;
        // Preserve each state's declared relative enlargement; compensate
        // its normal scale so Logo size measures the painted inner share.
        const inset = bounded((1 - logo / normalScale) / 2, 0, 0.45, 0);
        style.safeGlyphInset = {left: inset, top: inset, right: inset, bottom: inset};
    }
    const glyph = resolvedGlyph(style, entry);
    const styleId = usable ? String(style.id) : "plain-original";
    const tileEnabled = entryTileEnabled(entry, tiles.iconTilesEnabled === undefined
        ? true : Boolean(tiles.iconTilesEnabled));
    return {
        valid: usable,
        styleId: styleId,
        styleDefinition: style,
        iconShape: shape,
        parametersActive: parametersActive,
        innerDiameter: innerDiameter,
        diameter: bounded(tiles.iconDiameter, 40, 100, 100) / 100,
        stateId: requestedState,
        state: state,
        glyphSource: glyph.source,
        glyphPolicy: glyph.policy,
        glyphTreatment: usable ? glyph.treatment : "original",
        glyphTint: usable ? glyph.tint : "",
        glyphIsMask: usable ? glyph.isMask : false,
        glyphCompatible: glyph.compatible,
        glyphTreatmentReason: usable ? glyph.reason : "",
        maskLayer: usable ? maskLayerFor(style) : null,
        replacementApplied: glyph.replacementApplied,
        safeGlyphInset: usable ? normalizedInset(style)
                               : {left: 0, top: 0, right: 0, bottom: 0},
        layers: usable ? objectValue(style.layers) : {},
        assetPaths: usable ? objectValue(style.assetPaths) : {},
        tileEnabled: tileEnabled,
        renderStyledLayers: usable && tileEnabled
            && tiles.iconTileMode !== "custom" && hasRenderableLayers(style),
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
