.pragma library

function hasValue(map, key) {
    return map !== null
        && map !== undefined
        && Object.prototype.hasOwnProperty.call(map, key)
}

function value(map, key, fallback) {
    return hasValue(map, key) ? map[key] : fallback
}

function copyValues(source, destination) {
    if (source === null || source === undefined)
        return

    const keys = Object.keys(source)
    for (let index = 0; index < keys.length; ++index)
        destination[keys[index]] = source[keys[index]]
}

function setValue(map, key, newValue) {
    const result = {}
    copyValues(map, result)
    result[key] = newValue
    return result
}

function removeValue(map, key) {
    const result = {}
    if (map === null || map === undefined)
        return result

    const keys = Object.keys(map)
    for (let index = 0; index < keys.length; ++index) {
        if (keys[index] !== key)
            result[keys[index]] = map[keys[index]]
    }
    return result
}

function equivalent(left, right) {
    if (left === right)
        return true
    if (typeof left === "number" && typeof right === "number")
        return Math.abs(left - right) < 0.000001
    return false
}

function setComparedValue(map, key, newValue, baseline) {
    return equivalent(newValue, baseline)
        ? removeValue(map, key)
        : setValue(map, key, newValue)
}

function merge(map, values) {
    const result = {}
    copyValues(map, result)
    copyValues(values, result)
    return result
}

function clear() {
    return {}
}

function keyCount(map) {
    return map === null || map === undefined ? 0 : Object.keys(map).length
}

function nestedMap(map, key) {
    const candidate = value(map, key, null)
    return candidate !== null
        && candidate !== undefined
        && typeof candidate === "object"
        ? candidate
        : {}
}

function setNestedMap(map, key, nested) {
    return keyCount(nested) === 0
        ? removeValue(map, key)
        : setValue(map, key, nested)
}

function nestedKeyCount(map) {
    if (map === null || map === undefined)
        return 0

    const keys = Object.keys(map)
    let count = 0
    for (let index = 0; index < keys.length; ++index)
        count += keyCount(nestedMap(map, keys[index]))
    return count
}

function isDirty(panelMap, settingsMap, screenDirty, themeAction) {
    const pendingThemeAction = typeof themeAction === "string"
        ? themeAction.trim().length > 0
        : Boolean(themeAction)
    return keyCount(panelMap) > 0
        || keyCount(settingsMap) > 0
        || Boolean(screenDirty)
        || pendingThemeAction
}

function isSessionDirty(panelMaps, settingsMap, screenMaps, themeMaps) {
    return nestedKeyCount(panelMaps) > 0
        || keyCount(settingsMap) > 0
        || keyCount(screenMaps) > 0
        || keyCount(themeMaps) > 0
}
