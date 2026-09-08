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
}
