pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kcmutils as KCM
import org.kde.plasma.workspace.dbus as PlasmaDBus

KCM.SimpleKCM {
    id: root

    // Plasma injects every key from main.xml into each configuration page.
    property string cfg_panelId: ""
    property string cfg_panelIdDefault: ""
    property string cfg_panelType: "hybrid"
    property string cfg_panelTypeDefault: "hybrid"
    property string cfg_ownerToken: ""
    property string cfg_ownerTokenDefault: ""
    property bool cfg_bootstrapFreeDock: false
    property bool cfg_bootstrapFreeDockDefault: false

    property string panelIdOverride: ""
    property var dockCallOverride: null
    property var editorSnapshot: ({})
    property var panelBaseline: ({})
    property var globalBaseline: ({})
    property var panelChanges: ({})
    property var globalChanges: ({})
    property var panelFields: []
    property var globalFields: []
    property var values: ({})
    property var capabilityResolution: ({})
    property bool applyInFlight: false
    property string draftStatus: "loading"
    property string draftErrorCode: ""
    property string draftErrorMessage: ""
    property int transactionRequestCount: 0

    readonly property var appletConfiguration: typeof plasmoid !== "undefined"
        && plasmoid ? plasmoid.configuration : ({})
    readonly property string panelId: panelIdOverride.length > 0
        ? panelIdOverride : String(appletConfiguration.panelId || "")
    readonly property bool draftDirty: Object.keys(panelChanges).length > 0
        || Object.keys(globalChanges).length > 0
    readonly property var visibleFields: panelFields.concat(globalFields)

    function normalizeReply(reply) {
        if (Array.isArray(reply))
            return reply.map(normalizeReply)
        if (reply && typeof reply === "object") {
            const keys = Object.keys(reply)
            if (keys.length === 1 && keys[0] === "value")
                return normalizeReply(reply.value)
            const result = {}
            for (const key of keys)
                result[key] = normalizeReply(reply[key])
            return result
        }
        return reply
    }

    function singleReply(reply) {
        const normalized = normalizeReply(reply)
        return Array.isArray(normalized) && normalized.length === 1
            ? normalized[0] : normalized
    }

    function copyMap(source) {
        const result = {}
        if (!source)
            return result
        const keys = Object.keys(source)
        for (let index = 0; index < keys.length; ++index)
            result[keys[index]] = source[keys[index]]
        return result
    }

    function mergeMaps(base, changes) {
        const result = copyMap(base)
        const keys = changes ? Object.keys(changes) : []
        for (let index = 0; index < keys.length; ++index)
            result[keys[index]] = changes[keys[index]]
        return result
    }

    function equivalent(left, right) {
        if (left === right)
            return true
        if (typeof left === "number" && typeof right === "number")
            return Math.abs(left - right) < 0.000001
        return false
    }

    function setComparedValue(changes, baseline, key, value) {
        const result = {}
        const keys = Object.keys(changes || {})
        for (let index = 0; index < keys.length; ++index) {
            if (keys[index] !== key)
                result[keys[index]] = changes[keys[index]]
        }
        if (!equivalent(value, baseline[key]))
            result[key] = value
        return result
    }

    function descriptorFor(key) {
        const sources = [panelFields, globalFields]
        for (let sourceIndex = 0; sourceIndex < sources.length; ++sourceIndex) {
            const source = sources[sourceIndex]
            for (let index = 0; index < source.length; ++index) {
                if (String(source[index].key) === key) {
                    return {
                        field: source[index],
                        scope: sourceIndex === 0 ? "panel" : "global"
                    }
                }
            }
        }
        return null
    }

    function rebuildValues() {
        const merged = mergeMaps(
            mergeMaps(panelBaseline, panelChanges),
            mergeMaps(globalBaseline, globalChanges))
        merged.capabilityResolution = capabilityResolution
        merged.effectiveRendererTier = capabilityResolution
            && capabilityResolution.renderer
            ? capabilityResolution.renderer.effectiveTier : ""
        values = merged
    }

    function valueFor(field) {
        const key = String(field.key || "")
        return values[key] === undefined ? field.defaultValue : values[key]
    }

    function setValue(key, value) {
        if (applyInFlight)
            return false
        const descriptor = descriptorFor(String(key))
        if (!descriptor)
            return false
        if (descriptor.scope === "panel") {
            panelChanges = setComparedValue(
                panelChanges, panelBaseline, String(key), value)
        } else {
            globalChanges = setComparedValue(
                globalChanges, globalBaseline, String(key), value)
        }
        draftStatus = draftDirty ? "editing" : "loaded"
        draftErrorCode = ""
        draftErrorMessage = ""
        rebuildValues()
        return true
    }

    function panelCandidate() {
        return mergeMaps(panelBaseline, panelChanges)
    }

    function globalCandidate() {
        return mergeMaps(globalBaseline, globalChanges)
    }

    function callDock(member, args, resolved, rejected) {
        if (dockCallOverride) {
            dockCallOverride(member, args || [], resolved, rejected)
            return
        }

        const message = new PlasmaDBus.dbusMessage({
            service: "org.archdock.ArchDock",
            path: "/Control",
            member: member
        })
        message.iface = "local.PanelWindow"
        message.arguments = args || []
        const reply = PlasmaDBus.SessionBus.asyncCall(message)
            as PlasmaDBus.DBusPendingReply
        reply.finished.connect(function() {
            try {
                if (reply.isError) {
                    if (rejected) {
                        rejected({
                            name: reply.error.name,
                            message: reply.error.message
                        })
                    }
                    return
                }
                if (resolved) {
                    const value = JSON.parse(JSON.stringify(reply.value))
                    resolved(root.singleReply(value))
                }
            } finally {
                reply.destroy()
            }
        })
    }

    function failDraft(errorCode, errorMessage) {
        applyInFlight = false
        draftStatus = errorCode === "stale-revision" ? "conflict" : "failed"
        draftErrorCode = String(errorCode || "request-failed")
        draftErrorMessage = String(errorMessage || "")
    }

    function adoptSnapshot(snapshot) {
        const normalized = singleReply(snapshot)
        if (!normalized || normalized.success !== true
                || String(normalized.status || "") !== "loaded") {
            failDraft(normalized ? normalized.errorCode : "invalid-editor-snapshot",
                      normalized ? normalized.errorMessage : "")
            return false
        }
        editorSnapshot = normalized
        panelBaseline = copyMap(normalized.panelValues)
        globalBaseline = copyMap(normalized.globalValues)
        panelChanges = {}
        globalChanges = {}
        panelFields = (normalized.panelFields || []).slice()
        globalFields = (normalized.globalFields || []).slice()
        capabilityResolution = copyMap(normalized.capabilityResolution)
        applyInFlight = false
        draftStatus = "loaded"
        draftErrorCode = ""
        draftErrorMessage = ""
        rebuildValues()
        return true
    }

    function refresh() {
        if (panelId.length === 0) {
            failDraft("panel-not-found",
                      qsTr("This widget has no Arch Dock panel identity."))
            return false
        }
        draftStatus = "loading"
        callDock("panelSettingsEditorSnapshot", [panelId, "native"],
            function(reply) {
                root.adoptSnapshot(reply)
            }, function(error) {
                root.failDraft(error.name, error.message)
            })
        return true
    }

    function applyDraft() {
        if (!draftDirty || applyInFlight || !editorSnapshot.success)
            return false
        applyInFlight = true
        draftStatus = "applying"
        draftErrorCode = ""
        draftErrorMessage = ""
        transactionRequestCount += 1
        callDock("applyPanelSettingsTransaction", [
            panelId,
            Number(editorSnapshot.revision),
            panelCandidate(),
            globalCandidate()
        ], function(reply) {
            const result = root.singleReply(reply)
            if (!result || result.success !== true
                    || String(result.status || "") !== "succeeded") {
                root.failDraft(
                    result ? result.errorCode : "transaction-failed",
                    result ? result.errorMessage : "")
                return
            }
            root.draftStatus = "refreshing"
            root.callDock("panelSettingsEditorSnapshot", [root.panelId, "native"],
                function(snapshot) {
                    root.adoptSnapshot(snapshot)
                }, function(error) {
                    root.failDraft(error.name, error.message)
                })
        }, function(error) {
            root.failDraft(error.name, error.message)
        })
        return true
    }

    function cancelDraft() {
        if (applyInFlight)
            return false
        panelChanges = {}
        globalChanges = {}
        draftStatus = "cancelled"
        draftErrorCode = ""
        draftErrorMessage = ""
        rebuildValues()
        return true
    }

    Component.onCompleted: refresh()
    Component.onDestruction: cancelDraft()
}
