import QtQuick 2.15
import QtTest 1.3
import "../qml/runtime/CapabilityModel.js" as CapabilityModel

TestCase {
    name: "CapabilityModel"

    QtObject {
        id: nativeSequences
        property list<string> tiers: ["true3d", "procedural2d"]
    }

    function test_nativeSequencesPreserveCapabilityDecisions() {
        const resolution = { rendererChoices: [{ tier: "true3d", available: true }] }
        const theme = { valid: true, scene3D: {},
            scene3DResources: { mesh: {}, iconMesh: {}, material: {} },
            capabilities: { rendererTiers: nativeSequences.tiers } }
        verify(CapabilityModel.scene3DControlsAvailable(resolution, theme, { rendererAvailable: true }))
        compare(CapabilityModel.normalized(nativeSequences.tiers), ["true3d", "procedural2d"])
    }

    function test_sceneControlsRequireAllThreeAuthorities() {
        const resolution = { rendererChoices: [{ tier: "true3d", available: true }] }
        const theme = { valid: true, scene3D: { mesh: "mesh" },
            scene3DResources: { mesh: {}, iconMesh: {}, material: {} },
            capabilities: { rendererTiers: ["true3d", "procedural2d"] } }
        const consumer = { rendererAvailable: true }
        verify(CapabilityModel.scene3DControlsAvailable(resolution, theme, consumer))
        verify(!CapabilityModel.scene3DControlsAvailable({}, theme, consumer))
        verify(!CapabilityModel.scene3DControlsAvailable(resolution, {}, consumer))
        verify(!CapabilityModel.scene3DControlsAvailable(resolution, theme, {}))
        for (const missing of ["valid", "scene3D", "scene3DResources", "capabilities"]) {
            const partial = JSON.parse(JSON.stringify(theme))
            delete partial[missing]
            verify(!CapabilityModel.scene3DControlsAvailable(resolution, partial, consumer), missing)
        }
        // A saved setting cannot replace the consumer's successful probe.
        verify(!CapabilityModel.scene3DControlsAvailable(resolution,
            { rendererTier: "true3d", scene3DQuality: "high" }, { rendererAvailable: false }))
        // ADFIX-TASK-002: a look without meshes of its own qualifies when the
        // renderer generates its platform, and only then.
        const generated = { valid: true, scene3D: { generic: true, generated: {} },
            scene3DResources: { material: {} },
            capabilities: { rendererTiers: ["true3d", "baked2.5d"] } }
        verify(CapabilityModel.scene3DControlsAvailable(resolution, generated, consumer))
        const meshless = JSON.parse(JSON.stringify(generated))
        delete meshless.scene3D.generated
        verify(!CapabilityModel.scene3DControlsAvailable(resolution, meshless, consumer))
        verify(!CapabilityModel.scene3DControlsAvailable(resolution, generated, {}))
    }

    // ADFIX AUD-03: the 3D page names what actually blocks 3D.
    function test_scene3DBlockerNamesTheActualBlocker() {
        const resolution = { rendererChoices: [{ tier: "true3d", available: true },
                                               { tier: "baked2.5d", available: true }] }
        const ready = { rendererAvailable: true }
        const generated = { valid: true, scene3D: { generic: true, generated: {} },
            scene3DResources: { material: {} },
            capabilities: { rendererTiers: ["true3d", "baked2.5d"], layouts: ["ring", "circular"] } }
        compare(CapabilityModel.scene3DBlocker(resolution, generated, ready, { layout: "ring" }), null)
        compare(CapabilityModel.scene3DBlocker(resolution, generated, ready,
            { nativePanel: true, layout: "ring" }).code, "host")
        const noRenderer = CapabilityModel.scene3DBlocker(resolution, generated,
            { rendererAvailable: false, reasonCode: "renderer-platform-unsupported" }, { layout: "ring" })
        compare(noRenderer.code, "renderer")
        verify(noRenderer.text.indexOf(CapabilityModel.reasonLabel("renderer-platform-unsupported")) >= 0)
        compare(CapabilityModel.scene3DBlocker(resolution, { valid: false }, ready, { layout: "ring" }).code, "look")
        // Without a 3D form: imported artwork, a straight skin and a curve
        // that has no platform are three different answers.
        const flat = { valid: true, capabilities: { rendererTiers: ["baked2.5d"], layouts: ["ring", "arc"] } }
        compare(CapabilityModel.scene3DBlocker(resolution, flat, ready,
            { layout: "ring", importedArtwork: true }).code, "look")
        const skin = { valid: true, capabilities: { rendererTiers: ["skinned2d"], layouts: ["horizontal"] } }
        const skinBlocker = CapabilityModel.scene3DBlocker(resolution, skin, ready, { layout: "horizontal" })
        compare(skinBlocker.code, "look")
        verify(skinBlocker.text.indexOf("straight") >= 0, skinBlocker.text)
        compare(CapabilityModel.scene3DBlocker(resolution, flat, ready, { layout: "arc" }).code, "layout")
        // A 3D form whose resources are missing, or a renderer the resolver refuses.
        const broken = JSON.parse(JSON.stringify(generated))
        delete broken.scene3D.generated
        compare(CapabilityModel.scene3DBlocker(resolution, broken, ready, { layout: "ring" }).code, "resources")
        const refused = { rendererChoices: [{ tier: "true3d", available: false, reasonCode: "renderer-host-unsupported" },
                                            { tier: "baked2.5d", available: true }] }
        const refusal = CapabilityModel.scene3DBlocker(refused, generated, ready, { layout: "ring" })
        compare(refusal.code, "renderer-choice")
        verify(refusal.text.indexOf(CapabilityModel.reasonLabel("renderer-host-unsupported")) >= 0)
        const noReturn = { rendererChoices: [{ tier: "true3d", available: true }] }
        compare(CapabilityModel.scene3DBlocker(noReturn, generated, ready, { layout: "ring" }).code, "off-tier")
    }

    function test_threeDOffUsesOnlyDeclaredAvailableSurface() {
        const theme = {capabilities: {rendererTiers:
            ["true3d", "baked2.5d", "skinned2d", "procedural2d"]}}
        const resolution = {rendererChoices: [
            {tier: "baked2.5d", available: true},
            {tier: "skinned2d", available: true},
            {tier: "procedural2d", available: true}]}
        compare(CapabilityModel.scene3DOffTier(resolution, theme), "baked2.5d")
        resolution.rendererChoices[0].available = false
        compare(CapabilityModel.scene3DOffTier(resolution, theme), "skinned2d")
        theme.capabilities.rendererTiers = ["true3d", "procedural2d"]
        compare(CapabilityModel.scene3DOffTier(resolution, theme), "procedural2d")
        resolution.rendererChoices[2].available = false
        compare(CapabilityModel.scene3DOffTier(resolution, theme), "")
        compare(CapabilityModel.scene3DOffTier({}, theme), "")
        compare(CapabilityModel.scene3DOffTier(resolution, {}), "")
    }

    function test_runtimeFallbackHidesOnlyMeshPartMechanisms() {
        const resolution = {presentationMechanisms: [
            {id: "open", available: true},
            {id: "collapse-radial", available: true},
            {id: "collapse-horizontal", available: true},
            {id: "shutter", available: false}]}
        const theme = {scene3D: {parts: [{mechanism: "collapse-radial"}]}}
        compare(CapabilityModel.scenePresentationMechanisms(resolution, theme, "true3d"),
            ["open", "collapse-radial", "collapse-horizontal"])
        for (const tier of ["baked2.5d", "skinned2d", "procedural2d", ""]) {
            compare(CapabilityModel.scenePresentationMechanisms(resolution, theme, tier),
                ["open", "collapse-horizontal"])
        }
        compare(CapabilityModel.scenePresentationMechanisms({}, theme, "true3d"), [])
    }

    function test_normalizesNestedValueWrappers() {
        const source = {
            value: {
                layouts: {
                    value: [{
                        value: {
                            id: { value: "ring" },
                            available: { value: true },
                            reasonCode: { value: "available" }
                        }
                    }]
                }
            }
        }

        const decision = CapabilityModel.decision(source, "layouts", "ring")
        compare(decision.id, "ring")
        compare(decision.available, true)
        compare(decision.reasonCode, "available")
    }

    function test_filtersOptionsOnlyByBackendDecision() {
        const options = [
            { label: "Adaptive", value: "adaptive" },
            { label: "Ring", value: "ring" }
        ]
        const resolution = {
            layouts: [
                { id: "adaptive", available: false,
                  reasonCode: "host-layout-unsupported" },
                { id: "ring", available: true, reasonCode: "available" }
            ]
        }

        const available = CapabilityModel.availableOptions(
            options, resolution, "layouts")
        compare(available.length, 1)
        compare(available[0].value, "ring")
    }

    function test_distinguishesRendererNotInstalledAndDisabled() {
        const resolution = {
            rendererChoices: [
                { tier: "baked2.5d", available: false,
                  reasonCode: "renderer-disabled" },
                { tier: "true3d", available: false,
                  reasonCode: "renderer-not-installed" }
            ]
        }

        const baked = CapabilityModel.rendererChoice(resolution, "baked2.5d")
        const threeD = CapabilityModel.rendererChoice(resolution, "true3d")
        compare(CapabilityModel.reasonText(baked), "renderer-disabled")
        compare(CapabilityModel.reasonText(threeD), "renderer-not-installed")
    }

    function test_missingDecisionFailsClosed() {
        const missing = CapabilityModel.decision({}, "controls", "unknown")
        compare(missing.available, false)
        compare(missing.reasonCode, "invalid-capability-result")
        compare(CapabilityModel.availableOptions(
            [{ label: "Unknown", value: "unknown" }], {}, "controls").length, 0)
    }

    function test_unavailableThreeDOptionsRemainHidden_data() {
        return ["renderer-not-installed", "renderer-disabled",
                "renderer-import-unavailable", "renderer-backend-unsupported",
                "renderer-scene-unavailable", "theme-capability-undeclared"]
            .map(function(reason) { return { tag: reason, reason: reason } })
    }

    function test_unavailableThreeDOptionsRemainHidden(data) {
        const resolution = {
            rendererChoices: [
                { tier: "procedural2d", available: true },
                { tier: "true3d", available: false, reasonCode: data.reason }
            ]
        }
        const options = CapabilityModel.availableOptions([
            { label: "2D", value: "procedural2d" },
            { label: "3D", value: "true3d" }
        ], resolution, "rendererChoices")
        compare(options.length, 1)
        compare(options[0].value, "procedural2d")
        compare(CapabilityModel.reasonText(
            CapabilityModel.rendererChoice(resolution, "true3d")), data.reason)
    }

    function test_filtersBackendResolvedItemsWithoutNameRules() {
        const themes = [
            { id: "arbitrary-denied-name", available: false },
            { id: "arbitrary-allowed-name", available: true }
        ]
        const available = CapabilityModel.availableItems({ value: themes })
        compare(available.length, 1)
        compare(available[0].id, "arbitrary-allowed-name")
    }

    function test_rendererSummaryReportsFallback() {
        compare(CapabilityModel.rendererSummary({
            renderer: {
                requestedTier: "skinned2d",
                effectiveTier: "procedural2d",
                fallbackApplied: true,
                reasonCode: "renderer-host-unsupported"
            }
        }), "skinned2d -> procedural2d (renderer-host-unsupported)")
        compare(CapabilityModel.rendererSummary({}),
                "invalid-capability-result")
    }

    // Every code the capability resolver can report has a sentence a person
    // can read, and no label repeats the code it explains.
    function test_reasonLabelsArePlainLanguage() {
        const codes = [
            "invalid-capability-input", "platform-unsupported",
            "host-capability-unavailable", "host-layout-unsupported",
            "theme-capability-undeclared", "theme-host-unsupported",
            "theme-layout-unsupported", "renderer-not-installed",
            "renderer-disabled", "renderer-scene-unavailable",
            "renderer-host-unsupported", "renderer-platform-unsupported",
            "presentation-mechanism-unavailable", "rotation-range-incompatible",
            "renderer-rotation-unavailable", "no-safe-renderer-fallback",
            "theme-not-found", "theme-package-unavailable",
            "required-capability-unavailable", "icon-style-unavailable",
            "motion-profile-unavailable", "scene3d-mesh-unavailable"
        ]
        const unknown = CapabilityModel.reasonLabel("a-code-from-a-later-release")
        for (const code of codes) {
            const label = CapabilityModel.reasonLabel(code)
            verify(label.length > 0 && label.indexOf(code) < 0, code + ": " + label)
            verify(label !== unknown, code + " has its own sentence")
        }
        verify(unknown.indexOf("a-code-from-a-later-release") < 0, unknown)
        compare(CapabilityModel.reasonLabel(""), "no reason was reported")
        compare(CapabilityModel.rendererLabel("procedural2d"), "2D")
        compare(CapabilityModel.rendererLabel("baked2.5d"), "baked 2.5D")
        compare(CapabilityModel.rendererLabel("true3d"), "3D")
    }
}
