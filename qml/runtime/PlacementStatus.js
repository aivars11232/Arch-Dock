.pragma library

function hasValue(map, key) {
    return map !== null
        && map !== undefined
        && Object.prototype.hasOwnProperty.call(map, key)
}

function printableValue(value) {
    if (value === null || value === undefined)
        return "<unavailable>"
    if (typeof value === "boolean")
        return value ? "true" : "false"
    if (Array.isArray(value))
        return value.join(", ")
    return String(value)
}

function mapText(map) {
    if (map === null || map === undefined || typeof map !== "object")
        return "<unavailable>"

    const keys = Object.keys(map).sort()
    if (keys.length === 0)
        return "<unavailable>"

    const values = []
    for (let index = 0; index < keys.length; ++index) {
        const key = keys[index]
        values.push(key + "=" + printableValue(map[key]))
    }
    return values.join("; ")
}

function diagnostics(result, key) {
    if (!hasValue(result, key) || !Array.isArray(result[key]))
        return []

    const source = result[key]
    const values = []
    for (let index = 0; index < source.length; ++index) {
        const item = source[index]
        if (item === null || item === undefined || typeof item !== "object")
            continue

        let text = printableValue(item.field)
            + ": requested=" + printableValue(item.requested)
        if (hasValue(item, "observed"))
            text += ", observed=" + printableValue(item.observed)
        text += ", error=" + printableValue(item.errorCode)
        values.push(text)
    }
    return values
}

function statusLabel(status) {
    switch (String(status || "")) {
    case "applied":
        return "Applied and verified"
    case "unsupported":
        return "Unsupported"
    case "rolled-back":
        return "Failed; host restored"
    case "rollback-failed":
        return "Failed; host restoration failed"
    case "failed":
        return "Failed before completion"
    default:
        return "No placement result"
    }
}

function problemText(result) {
    if (result === null || result === undefined)
        return "not-attempted"

    const parts = []
    if (hasValue(result, "errorCode") && String(result.errorCode).length > 0)
        parts.push(String(result.errorCode))
    const unsupported = diagnostics(result, "unsupported")
    const failed = diagnostics(result, "failed")
    for (let index = 0; index < unsupported.length; ++index)
        parts.push("unsupported " + unsupported[index])
    for (let index = 0; index < failed.length; ++index)
        parts.push("failed " + failed[index])
    if (Boolean(result.rollbackAttempted) && !Boolean(result.rollbackSucceeded)) {
        parts.push("rollback=" + printableValue(result.rollbackErrorCode))
    }
    return parts.join(" | ")
}

function isProblem(result) {
    return result === null
        || result === undefined
        || !Boolean(result.success)
}

function savedIntentText(result) {
    return mapText(hasValue(result, "savedIntent") ? result.savedIntent : null)
}

function hostStateText(result) {
    return mapText(hasValue(result, "hostState") ? result.hostState : null)
}
