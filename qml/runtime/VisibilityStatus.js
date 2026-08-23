.pragma library

function hasValue(map, key) {
    return map !== null
        && map !== undefined
        && Object.prototype.hasOwnProperty.call(map, key)
}

function canonicalMode(value) {
    const mode = String(value || "").trim().toLowerCase()
    if (mode === "always" || mode === "none" || mode === "always-visible")
        return "always"
    if (mode === "auto-hide" || mode === "autohide")
        return "auto-hide"
    if (mode === "dodge" || mode === "dodge-windows"
            || mode === "dodgewindows")
        return "dodge"
    if (mode === "cover" || mode === "hide-maximized"
            || mode === "hide-for-maximized-or-fullscreen")
        return "cover"
    return ""
}

function supportedModeValues(status, nativeHost) {
    if (!Boolean(nativeHost))
        return []

    const source = status && Array.isArray(status.supportedModes)
        ? status.supportedModes : ["always"]
    const result = ["always"]
    for (let index = 0; index < source.length; ++index) {
        const mode = canonicalMode(source[index])
        if (mode.length > 0 && result.indexOf(mode) < 0)
            result.push(mode)
    }
    return result
}

function modeOptions(status, nativeHost, labels) {
    const modes = supportedModeValues(status, nativeHost)
    const result = []
    for (let index = 0; index < modes.length; ++index) {
        const mode = modes[index]
        result.push({
            label: labels && hasValue(labels, mode) ? labels[mode] : mode,
            value: mode
        })
    }
    return result
}

function containsMode(status, nativeHost, mode) {
    return supportedModeValues(status, nativeHost)
        .indexOf(canonicalMode(mode)) >= 0
}

function statusLabel(status) {
    switch (String(status || "")) {
    case "applied":
        return "Applied and verified"
    case "fallback-applied":
        return "Fallback applied and verified"
    case "deferred-no-host":
        return "Saved; no native host is available"
    case "rolled-back":
        return "Failed; host restored"
    case "rollback-failed":
        return "Failed; host restoration failed"
    case "fallback-failed":
        return "Requested mode and fallback failed"
    case "unsupported":
        return "Unsupported for this host"
    case "failed":
        return "Failed before verification"
    default:
        return "No visibility result"
    }
}

function isProblem(result) {
    return result === null
        || result === undefined
        || !Boolean(result.success)
        || Boolean(result.fallbackApplied)
}

function problemText(result) {
    if (result === null || result === undefined)
        return "not-attempted"

    if (Boolean(result.fallbackApplied)) {
        return "Requested " + String(result.requestedMode || "mode")
            + " could not be applied ("
            + String(result.fallbackReason || result.errorCode || "unsupported")
            + "). Always visible was applied and verified."
    }

    const parts = []
    if (hasValue(result, "errorCode") && String(result.errorCode).length > 0)
        parts.push(String(result.errorCode))
    if (Boolean(result.fallbackAttempted)
            && String(result.fallbackErrorCode || "").length > 0) {
        parts.push("always-visible fallback=" + String(result.fallbackErrorCode))
    }
    if (Boolean(result.rollbackAttempted) && !Boolean(result.rollbackSucceeded))
        parts.push("rollback=" + String(result.rollbackErrorCode || "failed"))
    return parts.join(" | ")
}
