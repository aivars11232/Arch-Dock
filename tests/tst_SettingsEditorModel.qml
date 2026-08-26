import QtQuick 2.15
import QtTest 1.3
import "../qml/runtime/SettingsEditorModel.js" as EditorModel

TestCase {
    name: "SettingsEditorModel"

    function snapshot(panelId, revision) {
        return {
            success: true,
            status: "loaded",
            panelId: panelId,
            revision: revision,
            consumer: "studio",
            panelValues: {
                visible: true,
                opacity: 0.9,
                layout: "adaptive"
            },
            globalValues: {
                showTooltips: true
            },
            panelFields: [
                {
                    key: "visible",
                    scope: "panel",
                    control: "switch"
                },
                {
                    key: "opacity",
                    scope: "panel",
                    control: "slider"
                },
                {
                    key: "layout",
                    scope: "panel",
                    control: "combo"
                }
            ],
            globalFields: [
                {
                    key: "showTooltips",
                    scope: "global",
                    control: "switch"
                }
            ],
            themes: [
                {
                    id: "obsidian-glass"
                }
            ],
            capabilityResolution: {
                available: true
            }
        };
    }

    function test_loadCopiesOnlyTheServerEditorSnapshot() {
        const source = snapshot("bottom", 4);
        const session = EditorModel.load(source);

        verify(session.loaded);
        compare(session.panelId, "bottom");
        compare(session.revision, 4);
        compare(EditorModel.panelValue(session, "opacity", 0), 0.9);
        compare(EditorModel.globalValue(session, "showTooltips", false), true);
        verify(!EditorModel.dirty(session));

        source.panelValues.opacity = 0.1;
        compare(EditorModel.panelValue(session, "opacity", 0), 0.9);
        verify(!Object.prototype.hasOwnProperty.call(EditorModel.panelCandidate(session), "id"));
    }

    function test_editProducesOneCompleteCandidateWithoutMutatingBaseline() {
        const original = EditorModel.load(snapshot("bottom", 7));
        let edited = EditorModel.setPanelValue(original, "opacity", 0.55);
        edited = EditorModel.setGlobalValue(edited, "showTooltips", false);

        verify(EditorModel.dirty(edited));
        compare(EditorModel.panelValue(edited, "opacity", 0), 0.55);
        compare(EditorModel.panelValue(original, "opacity", 0), 0.9);
        compare(EditorModel.panelCandidate(edited).visible, true);
        compare(EditorModel.panelCandidate(edited).layout, "adaptive");
        compare(EditorModel.panelCandidate(edited).opacity, 0.55);
        compare(EditorModel.globalCandidate(edited).showTooltips, false);
        compare(original.panelChanges.opacity, undefined);
    }

    function test_unknownClientKeysCannotEnterTheCandidate() {
        const session = EditorModel.load(snapshot("bottom", 1));
        const protectedAttempt = EditorModel.setPanelValue(session, "screenId", "forged");
        const hiddenAttempt = EditorModel.setPanelValue(session, "surface3D", {
            depth: 12
        });

        verify(protectedAttempt === session);
        verify(hiddenAttempt === session);
        verify(!Object.prototype.hasOwnProperty.call(EditorModel.panelCandidate(session), "screenId"));
        verify(!Object.prototype.hasOwnProperty.call(EditorModel.panelCandidate(session), "surface3D"));
    }

    function test_cancelDiscardsOnlyLocalChanges() {
        let session = EditorModel.load(snapshot("bottom", 2));
        session = EditorModel.setPanelValue(session, "opacity", 0.4);
        session = EditorModel.setGlobalValue(session, "showTooltips", false);
        const cancelled = EditorModel.cancel(session);

        verify(!EditorModel.dirty(cancelled));
        compare(EditorModel.panelValue(cancelled, "opacity", 0), 0.9);
        compare(EditorModel.globalValue(cancelled, "showTooltips", false), true);
        compare(cancelled.revision, 2);
        compare(cancelled.status, "cancelled");
    }

    function test_dirtyPanelSwitchingIsExplicitlyBlocked() {
        let session = EditorModel.load(snapshot("bottom", 3));
        verify(EditorModel.canSwitchPanel(session, "side"));
        session = EditorModel.setPanelValue(session, "visible", false);
        verify(EditorModel.canSwitchPanel(session, "bottom"));
        verify(!EditorModel.canSwitchPanel(session, "side"));
        verify(EditorModel.canSwitchPanel(EditorModel.cancel(session), "side"));
    }

    function test_projectionRefreshesVisibilityWithoutPersistence() {
        let session = EditorModel.load(snapshot("bottom", 5));
        session = EditorModel.setPanelValue(session, "layout", "horizontal");
        const projected = EditorModel.withProjection(session, {
            success: true,
            status: "resolved",
            panelFields: [
                {
                    key: "layout",
                    scope: "panel",
                    control: "combo"
                },
                {
                    key: "layoutRadius",
                    scope: "panel",
                    control: "spin"
                }
            ],
            globalFields: session.globalFields,
            panelValues: {
                layout: "horizontal",
                layoutRadius: 150
            },
            globalValues: session.globalBaseline,
            themes: [],
            capabilityResolution: {
                available: true,
                themeId: "procedural"
            }
        });

        verify(EditorModel.dirty(projected));
        compare(projected.panelFields.length, 2);
        compare(projected.panelFields[0].key, "layout");
        compare(EditorModel.panelValue(projected, "layoutRadius", 0), 150);
        compare(EditorModel.panelCandidate(projected).layout, "horizontal");
        compare(projected.revision, 5);

        const radiusDraft = EditorModel.setPanelValue(projected, "layoutRadius", 230);
        const hiddenAgain = EditorModel.withProjection(radiusDraft, {
            success: true,
            status: "resolved",
            panelFields: [
                {
                    key: "layout",
                    scope: "panel",
                    control: "combo"
                }
            ],
            globalFields: session.globalFields,
            panelValues: {
                layout: "horizontal"
            },
            globalValues: session.globalBaseline,
            themes: [],
            capabilityResolution: {
                available: true
            }
        });
        verify(!Object.prototype.hasOwnProperty.call(EditorModel.panelCandidate(hiddenAgain), "layoutRadius"));
    }

    function test_failedAndConflictingTransactionsRetainTheDraft() {
        let session = EditorModel.load(snapshot("bottom", 8));
        session = EditorModel.setPanelValue(session, "opacity", 0.61);

        const failed = EditorModel.adoptResult(session, {
            success: false,
            status: "validation-failed",
            errorCode: "capability-unavailable",
            errorMessage: "host-layout-unsupported"
        }, null);
        verify(EditorModel.dirty(failed));
        compare(EditorModel.panelValue(failed, "opacity", 0), 0.61);
        compare(failed.status, "failed");
        compare(failed.errorCode, "capability-unavailable");

        const conflict = EditorModel.adoptResult(session, {
            success: false,
            status: "revision-conflict",
            errorCode: "stale-revision"
        }, null);
        verify(EditorModel.dirty(conflict));
        compare(conflict.status, "conflict");
        compare(conflict.revision, 8);
    }

    function test_successReloadsTheActualServerRevision() {
        let session = EditorModel.load(snapshot("bottom", 9));
        session = EditorModel.setPanelValue(session, "opacity", 0.37);
        const refreshed = snapshot("bottom", 10);
        refreshed.panelValues.opacity = 0.37;

        const adopted = EditorModel.adoptResult(session, {
            success: true,
            status: "succeeded",
            revision: 10
        }, refreshed);
        verify(adopted.loaded);
        verify(!EditorModel.dirty(adopted));
        compare(adopted.revision, 10);
        compare(EditorModel.panelValue(adopted, "opacity", 0), 0.37);
        compare(adopted.status, "loaded");
    }

    function test_serverThemeValuesStageOnlyExistingEditorKeys() {
        let session = EditorModel.load(snapshot("bottom", 6));
        session = EditorModel.stagePanelValues(session, {
            opacity: 0.72,
            layout: "horizontal",
            builtIn: false,
            surface3D: {
                depth: 10
            }
        });

        verify(EditorModel.dirty(session));
        compare(EditorModel.panelCandidate(session).opacity, 0.72);
        compare(EditorModel.panelCandidate(session).layout, "horizontal");
        verify(!Object.prototype.hasOwnProperty.call(EditorModel.panelCandidate(session), "builtIn"));
        verify(!Object.prototype.hasOwnProperty.call(EditorModel.panelCandidate(session), "surface3D"));
    }
}
