.pragma library

const freeUrlPrefix = "free-url:";

function encodedUrl(appId) {
    if (typeof appId !== "string" || !appId.startsWith(freeUrlPrefix))
        return "";

    const value = appId.substring(freeUrlPrefix.length);
    return value.startsWith("file:/") ? value : "";
}

function interactionEnabled(plasmaEditMode) {
    return !Boolean(plasmaEditMode);
}

function keepEntriesOnServiceFailure(freeSurface) {
    return Boolean(freeSurface);
}
