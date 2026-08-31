.pragma library

function hasValue(map, key) {
    return map !== null && map !== undefined
        && Object.prototype.hasOwnProperty.call(map, key);
}

function copyValues(source, destination) {
    if (source === null || source === undefined)
        return;
    const keys = Object.keys(source);
    for (let index = 0; index < keys.length; ++index)
        destination[keys[index]] = source[keys[index]];
}

function normalizedString(value) {
    return value === null || value === undefined ? "" : String(value).trim();
}

function snapshotOverride(snapshot) {
    if (snapshot === null || snapshot === undefined
            || snapshot.override === null
            || snapshot.override === undefined
            || typeof snapshot.override !== "object")
        return {};
    const result = {};
    copyValues(snapshot.override, result);
    return result;
}

function editorValues(snapshot) {
    const stored = snapshotOverride(snapshot);
    return {
        customGlyph: normalizedString(stored.customGlyph),
        customLabel: normalizedString(stored.customLabel),
        tileMode: hasValue(stored, "tileEnabled")
            ? (stored.tileEnabled === true ? "enabled" : "disabled")
            : "panel-default",
        styleReference: normalizedString(stored.styleReference)
    };
}

function overrideFromEditor(snapshot, values) {
    const stored = snapshotOverride(snapshot);
    const result = {};

    // These fields are persisted for forward compatibility but are deliberately
    // not exposed until their behavior exists. Editing another property must not
    // erase them.
    if (hasValue(stored, "animationProfileReference"))
        result.animationProfileReference = stored.animationProfileReference;
    if (hasValue(stored, "extensions"))
        result.extensions = stored.extensions;

    const glyph = normalizedString(values && values.customGlyph);
    const label = normalizedString(values && values.customLabel);
    const style = normalizedString(values && values.styleReference);
    const tileMode = normalizedString(values && values.tileMode);
    if (glyph.length > 0)
        result.customGlyph = glyph;
    if (label.length > 0)
        result.customLabel = label;
    if (tileMode === "enabled")
        result.tileEnabled = true;
    else if (tileMode === "disabled")
        result.tileEnabled = false;
    if (style.length > 0)
        result.styleReference = style;
    return result;
}

function canonicalOverride(overrideValues) {
    const result = {};
    const source = overrideValues || {};
    const orderedKeys = [
        "customGlyph",
        "customLabel",
        "tileEnabled",
        "styleReference",
        "animationProfileReference",
        "extensions"
    ];
    for (let index = 0; index < orderedKeys.length; ++index) {
        const key = orderedKeys[index];
        if (hasValue(source, key))
            result[key] = source[key];
    }
    return JSON.stringify(result);
}

function isDirty(snapshot, values) {
    return canonicalOverride(snapshotOverride(snapshot))
        !== canonicalOverride(overrideFromEditor(snapshot, values));
}

function isEmpty(overrideValues) {
    return overrideValues === null || overrideValues === undefined
        || Object.keys(overrideValues).length === 0;
}

function styleChoices(iconStyles) {
    const result = [{ id: "", name: qsTr("Use panel style") }];
    const source = iconStyles !== null && iconStyles !== undefined
            && typeof iconStyles.length === "number"
        ? iconStyles : [];
    for (let index = 0; index < source.length; ++index) {
        const entry = source[index] || {};
        const id = normalizedString(entry.id);
        const name = normalizedString(entry.name);
        if (id.length > 0 && name.length > 0)
            result.push({ id: id, name: name });
    }
    return result;
}

function styleIndex(choices, styleReference) {
    const requested = normalizedString(styleReference);
    for (let index = 0; index < choices.length; ++index) {
        if (normalizedString(choices[index].id) === requested)
            return index;
    }
    return 0;
}

function transactionSucceeded(result) {
    return result !== null && result !== undefined
        && result.success === true && String(result.status || "") === "succeeded";
}

function transactionConflict(result) {
    return result !== null && result !== undefined
        && (String(result.status || "") === "revision-conflict"
            || String(result.errorCode || "") === "stale-revision");
}
