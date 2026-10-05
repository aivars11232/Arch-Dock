.pragma library

function normalized(value) {
    if (Array.isArray(value) || (value !== null && typeof value === "object"
            && Number.isInteger(value.length) && value.length >= 0
            && typeof value.slice === "function")) {
        const result = []
        for (let index = 0; index < value.length; ++index)
            result.push(normalized(value[index]))
        return result
    }
    if (value !== null && value !== undefined && typeof value === "object") {
        const keys = Object.keys(value)
        if (keys.length === 1 && keys[0] === "value")
            return normalized(value.value)
        const result = {}
        for (let index = 0; index < keys.length; ++index) {
            const key = keys[index]
            result[key] = normalized(value[key])
        }
        return result
    }
    return value
}

function unavailableDecision(id) {
    return {
        id: String(id || ""),
        available: false,
        reasonCode: "invalid-capability-result",
        blockedBy: ""
    }
}

function decision(resolution, group, id) {
    const source = normalized(resolution)
    const values = source && Array.isArray(source[group]) ? source[group] : []
    const expectedId = String(id || "")
    for (let index = 0; index < values.length; ++index) {
        const candidate = values[index]
        if (candidate && String(candidate.id || candidate.tier || "") === expectedId)
            return candidate
    }
    return unavailableDecision(expectedId)
}

function rendererChoice(resolution, tier) {
    return decision(resolution, "rendererChoices", tier)
}

function scene3DOffTier(resolution, theme) {
    const selected = normalized(theme)
    const tiers = selected && selected.capabilities
        ? selected.capabilities.rendererTiers || [] : []
    if (!Array.isArray(tiers))
        return ""
    for (const tier of ["baked2.5d", "skinned2d", "procedural2d"]) {
        if (tiers.includes(tier) && rendererChoice(resolution, tier).available === true)
            return tier
    }
    return ""
}

function scenePresentationMechanisms(resolution, theme, effectiveTier) {
    const resolved = normalized(resolution) || {}
    const selected = normalized(theme) || {}
    const parts = (selected.scene3D || {}).parts || []
    const meshMechanisms = (Array.isArray(parts) ? parts : [])
        .map(function(part) { return String((part || {}).mechanism || "") })
    return availableItems(resolved.presentationMechanisms)
        .filter(function(choice) {
            return effectiveTier === "true3d" || !meshMechanisms.includes(choice.id)
        }).map(function(choice) { return choice.id })
}

function scene3DControlsAvailable(resolution, theme, consumer) {
    const selected = normalized(theme)
    const runtime = normalized(consumer)
    return rendererChoice(resolution, "true3d").available === true
        && runtime && runtime.rendererAvailable === true
        && selected && selected.valid === true
        && selected.scene3D && selected.scene3DResources
        && selected.scene3DResources.mesh && selected.scene3DResources.iconMesh
        && selected.scene3DResources.material
        && selected.capabilities && Array.isArray(selected.capabilities.rendererTiers)
        && selected.capabilities.rendererTiers.includes("true3d")
}

function availableOptions(options, resolution, group) {
    const source = Array.isArray(options) ? options : []
    const result = []
    for (let index = 0; index < source.length; ++index) {
        const option = source[index]
        if (!option)
            continue
        if (decision(resolution, group, option.value).available === true)
            result.push(option)
    }
    return result
}

function availableItems(items) {
    const source = normalized(items)
    if (!Array.isArray(source))
        return []
    const result = []
    for (let index = 0; index < source.length; ++index) {
        const item = source[index]
        if (item && item.available === true)
            result.push(item)
    }
    return result
}

// What a reason code means to a person. Internal codes are for logs and
// details; a sentence in Panel Studio uses these clauses instead. Each reads
// after a colon or "because", with the theme or preset as "it".
function reasonLabel(code) {
    const key = String(code || "")
    const labels = {
        "theme-not-found": qsTr("its theme is not installed"),
        "theme-package-unavailable": qsTr("its theme package cannot be loaded"),
        "preset-not-found": qsTr("it is no longer installed"),
        "preset-resources-unavailable": qsTr("some of its resources are missing"),
        "required-capability-unavailable": qsTr("a capability it requires is not available"),
        "icon-style-unavailable": qsTr("its icon style is not installed"),
        "motion-profile-unavailable": qsTr("its motion profile is not installed"),
        "theme-capability-undeclared": qsTr("its theme does not support a feature it uses"),
        "theme-host-unsupported": qsTr("its theme does not support this kind of panel"),
        "theme-layout-unsupported": qsTr("its theme does not support this layout"),
        "theme-field-unavailable": qsTr("one of its settings does not fit this panel"),
        "invalid-theme-candidate": qsTr("its settings do not fit this panel"),
        "invalid-theme-capabilities": qsTr("its description could not be read"),
        "invalid-capability-input": qsTr("its description could not be read"),
        "host-layout-unsupported": qsTr("this kind of panel cannot use its layout"),
        "host-capability-unavailable": qsTr("this kind of panel does not support a feature it uses"),
        "platform-unsupported": qsTr("this system is not supported"),
        "renderer-not-installed": qsTr("the renderer it needs is not installed"),
        "renderer-disabled": qsTr("the renderer it needs is switched off"),
        "renderer-scene-unavailable": qsTr("3D rendering is not available in this session"),
        "renderer-host-unsupported": qsTr("this kind of panel cannot use its renderer"),
        "renderer-platform-unsupported": qsTr("this system cannot use its renderer"),
        "renderer-rotation-unavailable": qsTr("its renderer cannot turn the whole panel"),
        "rotation-range-incompatible": qsTr("its rotation is outside what this panel allows"),
        "presentation-mechanism-unavailable": qsTr("its open and close motion is not available here"),
        "no-safe-renderer-fallback": qsTr("none of the renderers it declares can be used here")
    }
    if (labels[key] !== undefined)
        return labels[key]
    if (key.indexOf("scene3d-") === 0)
        return qsTr("its 3D resources could not be loaded")
    return key.length > 0 ? qsTr("an unrecognised problem was reported")
                          : qsTr("no reason was reported")
}

// A renderer tier as a person would name it.
function rendererLabel(tier) {
    const labels = {
        "procedural2d": qsTr("2D"),
        "skinned2d": qsTr("skinned 2D"),
        "baked2.5d": qsTr("baked 2.5D"),
        "true3d": qsTr("3D")
    }
    const key = String(tier || "")
    return labels[key] !== undefined ? labels[key] : qsTr("2D")
}

function reasonText(value) {
    const source = normalized(value)
    if (!source || source.available === true)
        return ""
    const reason = String(source.reasonCode || "invalid-capability-result")
    const blocker = String(source.blockedBy || "")
    return blocker.length > 0 ? reason + " (" + blocker + ")" : reason
}

function rendererSummary(resolution) {
    const source = normalized(resolution)
    const renderer = source && source.renderer && typeof source.renderer === "object"
        ? source.renderer : null
    if (!renderer)
        return "invalid-capability-result"
    const effective = String(renderer.effectiveTier || "")
    const requested = String(renderer.requestedTier || "")
    const reason = String(renderer.reasonCode || "invalid-capability-result")
    if (effective.length === 0)
        return reason
    if (renderer.fallbackApplied === true)
        return requested + " -> " + effective + " (" + reason + ")"
    return effective
}
