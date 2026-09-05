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

// A free panel whose content type includes running applications must follow
// the shared task model; a launcher-only free panel deliberately does not, so
// task churn cannot make it refetch.
function followsTaskModel(contentType) {
    const type = String(contentType || "").trim().toLowerCase();
    return type === "tasks" || type === "hybrid";
}

function entryFor(entries, appId) {
    const list = Array.isArray(entries) ? entries : [];
    for (let index = 0; index < list.length; ++index) {
        if (list[index] && list[index].appId === appId)
            return list[index];
    }
    return null;
}

// The panel-specific id an entry reorders and unpins by, or "" for a
// running-only entry that the panel does not own.
function panelEntryId(entries, appId) {
    const entry = entryFor(entries, appId);
    return entry && entry.panelEntryId ? String(entry.panelEntryId) : "";
}

// The application id window actions must target: a pinned desktop entry with
// a running instance carries it separately so the entry keeps its own identity.
function actionAppId(entries, appId) {
    const entry = entryFor(entries, appId);
    return entry && entry.runningAppId ? String(entry.runningAppId) : String(appId || "");
}
