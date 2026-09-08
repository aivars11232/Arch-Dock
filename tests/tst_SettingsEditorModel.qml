import QtQuick 2.15
import QtTest 1.3
import "../qml/runtime/SettingsEditorModel.js" as EditorModel

TestCase {
    name: "SettingsEditorModel"

    QtObject {
        id: nativeSequences
        property list<string> tiers: ["true3d", "procedural2d"]
        property list<real> position: [1, 2, 3]
    }

    function test_nativeThemeSequencesRemainArrays() {
        const source = snapshot("free-1", 1)
        source.themeDefinition.capabilities = { rendererTiers: nativeSequences.tiers }
        source.themeDefinition.scene3DResources = { mesh: { positions: [nativeSequences.position] } }
        const copied = EditorModel.load(source).themeDefinition
        verify(Array.isArray(copied.capabilities.rendererTiers))
        compare(copied.capabilities.rendererTiers, ["true3d", "procedural2d"])
        verify(Array.isArray(copied.scene3DResources.mesh.positions[0]))
        compare(copied.scene3DResources.mesh.positions[0], [1, 2, 3])
    }

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
                layout: "adaptive",
                iconStyle: "plain-original"
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
                },
                {
                    key: "iconStyle",
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
            themeDefinition: {
                id: "fixture-split-skin",
                valid: true,
                assetPaths: { surface: "/managed/surface.svg" }
            },
            themeProjectionStatus: "ready",
            themeProjectionError: "",
            iconStyles: [
                {
                    id: "plain-original",
                    name: "Plain Original"
                }
            ],
            iconStyleDefinition: {
                id: "plain-original",
                valid: true,
                glyphPolicy: { mode: "original" }
            },
            iconStyleProjectionStatus: "ready",
            iconStyleProjectionError: "",
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
        compare(session.themeDefinition.id, "fixture-split-skin");
        compare(session.themeProjectionStatus, "ready");
        compare(session.themeProjectionError, "");
        compare(session.iconStyleDefinition.id, "plain-original");
        compare(session.iconStyleProjectionStatus, "ready");
        compare(session.iconStyles.length, 1);
        verify(!EditorModel.dirty(session));

        source.panelValues.opacity = 0.1;
        source.themeDefinition.assetPaths.surface = "/forged/surface.svg";
        source.iconStyleDefinition.glyphPolicy.mode = "mapped-replacement";
        compare(EditorModel.panelValue(session, "opacity", 0), 0.9);
        compare(session.themeDefinition.assetPaths.surface,
                "/managed/surface.svg");
        compare(session.iconStyleDefinition.glyphPolicy.mode, "original");
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

    function test_rendererCandidateMergesDraftGlobalsAndResolvedCapabilities() {
        const source = snapshot("bottom", 7);
        source.panelValues.rendererTier = "skinned2d";
        source.capabilityResolution = {
            available: true,
            renderer: {
                requestedTier: "skinned2d",
                effectiveTier: "procedural2d",
                fallbackApplied: true,
                reasonCode: "renderer-unavailable"
            }
        };
        let edited = EditorModel.load(source);
        edited = EditorModel.setPanelValue(edited, "opacity", 0.55);
        edited = EditorModel.setGlobalValue(edited, "showTooltips", false);

        const candidate = EditorModel.rendererCandidate(edited);
        compare(candidate.opacity, 0.55);
        compare(candidate.layout, "adaptive");
        compare(candidate.showTooltips, false);
        compare(candidate.effectiveRendererTier, "procedural2d");
        compare(candidate.capabilityResolution.renderer.requestedTier,
                "skinned2d");
        compare(candidate.capabilityResolution.renderer.fallbackApplied,
                true);
        compare(candidate.iconStyleDefinition.id, "plain-original");

        candidate.opacity = 0.1;
        candidate.capabilityResolution.renderer.effectiveTier = "true3d";
        candidate.iconStyleDefinition.id = "forged";
        compare(EditorModel.panelValue(edited, "opacity", 0), 0.55);
        compare(edited.capabilityResolution.renderer.effectiveTier,
                "procedural2d");
        compare(edited.iconStyleDefinition.id, "plain-original");
    }

    function test_rendererThemeCandidateIsAnIsolatedResolvedCardDraft() {
        let session = EditorModel.load(snapshot("bottom", 7));
        session = EditorModel.setPanelValue(session, "opacity", 0.55);
        const theme = {
            id: "holographic-ring",
            panelStyle: {
                appearance: "futuristic",
                opacity: 0.9
            },
            iconStyle: {
                iconShape: "circle",
                iconSize: 48
            },
            layoutStyle: {
                layout: "ring",
                layoutRadius: 96
            },
            iconStyleRef: {
                id: "dark-orb"
            },
            capabilityResolution: {
                available: true,
                renderer: {
                    requestedTier: "procedural2d",
                    effectiveTier: "procedural2d",
                    fallbackApplied: false,
                    reasonCode: ""
                }
            }
        };

        const candidate = EditorModel.rendererThemeCandidate(session, theme);
        compare(candidate.appearance, "futuristic");
        compare(candidate.opacity, 0.9);
        compare(candidate.iconShape, "circle");
        compare(candidate.iconSize, 48);
        compare(candidate.layout, "ring");
        compare(candidate.layoutRadius, 96);
        compare(candidate.completeThemeId, "holographic-ring");
        compare(candidate.recommendedIconStyleId, "dark-orb");
        compare(candidate.iconStyle, "plain-original");
        compare(candidate.iconStyleDefinition.id, "plain-original");
        compare(candidate.iconThemeId, undefined);
        compare(candidate.capabilityResolution.renderer.effectiveTier,
                "procedural2d");

        candidate.layout = "horizontal";
        candidate.capabilityResolution.renderer.effectiveTier = "true3d";
        compare(EditorModel.panelValue(session, "layout", ""), "adaptive");
        compare(theme.layoutStyle.layout, "ring");
        compare(theme.capabilityResolution.renderer.effectiveTier,
                "procedural2d");
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
            themeDefinition: {
                id: "fixture-projected-skin",
                valid: true
            },
            themeProjectionStatus: "ready",
            themeProjectionError: "",
            iconStyles: [
                {
                    id: "metallic-blue",
                    name: "Metallic Blue"
                }
            ],
            iconStyleDefinition: {
                id: "metallic-blue",
                valid: true
            },
            iconStyleProjectionStatus: "ready",
            iconStyleProjectionError: "",
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
        compare(projected.themeDefinition.id, "fixture-projected-skin");
        compare(projected.themeProjectionStatus, "ready");
        compare(projected.iconStyleDefinition.id, "metallic-blue");
        compare(projected.iconStyles.length, 1);

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

    function test_dynamicGlowFieldFollowsServerCapabilityProjection() {
        let source = snapshot("bottom", 11)
        source.panelValues.glowIntensity = 1
        source.panelFields.push({
            key: "glowIntensity",
            scope: "panel",
            control: "slider",
            capability: "dynamic-glow"
        })
        let session = EditorModel.load(source)
        session = EditorModel.setPanelValue(session, "glowIntensity", 1.6)
        compare(EditorModel.panelCandidate(session).glowIntensity, 1.6)

        const projected = EditorModel.withProjection(session, {
            success: true,
            status: "resolved",
            panelFields: source.panelFields.filter(function(field) {
                return field.key !== "glowIntensity"
            }),
            globalFields: session.globalFields,
            panelValues: {
                visible: true,
                opacity: 0.9,
                layout: "adaptive"
            },
            globalValues: session.globalBaseline,
            themes: [],
            capabilityResolution: { available: true }
        })
        verify(!Object.prototype.hasOwnProperty.call(
            EditorModel.panelCandidate(projected), "glowIntensity"))
    }
}
