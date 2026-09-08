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
