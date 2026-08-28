import QtQuick
import QtTest
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "RendererParity"
    width: 1000
    height: 620
    when: windowShown

    readonly property var entries: [
        { id: "one", displayName: "One", running: false },
        { id: "two", displayName: "Two", running: true, active: true },
        { id: "three", displayName: "Three", running: true }
    ]
    readonly property var capabilities: ({
        available: true,
        renderer: {
            requestedTier: "procedural2d",
            effectiveTier: "procedural2d",
            fallbackApplied: false,
            reasonCode: ""
        }
    })

    Component {
        id: previewComponent

        LivePanelPreview {
            width: 480
            height: 280
            orderedEntries: testCase.entries
            hostCapabilities: testCase.capabilities
            animationProfiles: ({ reducedMotion: true })
        }
    }

    Component {
        id: liveSceneComponent

        PanelScene {
            orderedEntries: testCase.entries
            hostCapabilities: testCase.capabilities
            animationProfiles: ({ reducedMotion: true })
            geometryCompatibilityProfile: "canonical"
        }
    }

    function caseDefinition(mode) {
        const common = {
            schemaVersion: 2,
            layoutPadding: 12,
            iconSize: 40,
            spacing: 6,
            appearance: "minimal",
            opacity: 0.87,
            rendererTier: "procedural2d"
        }
        if (mode === "vertical") {
            common.layout = "vertical"
            common.edge = "left"
        } else if (mode === "free") {
            common.layout = "ring"
            common.edge = "free"
            common.layoutRadius = 72
        } else {
            common.layout = "horizontal"
            common.edge = "bottom"
        }
        return common
    }

    function contractSnapshot(scene) {
        const entryGeometry = []
        for (let index = 0; index < scene.entryCount; ++index) {
            const geometry = scene.entryGeometryAt(index)
            entryGeometry.push({
                x: geometry.position.x,
                y: geometry.position.y,
                rotation: geometry.rotation,
                scaleFactor: geometry.scaleFactor,
                depthOrder: geometry.depthOrder,
                normalX: geometry.outwardNormal.x,
                normalY: geometry.outwardNormal.y
            })
        }
        return {
            layoutPath: scene.layoutPath,
            verticalLayout: scene.verticalLayout,
            layoutGeometry: scene.layoutGeometry,
            contentBounds: scene.contentBounds,
            effectBounds: scene.effectBounds,
            inputRegion: scene.inputRegion,
            revealHandle: scene.revealHandle,
            popupAnchors: scene.popupAnchors,
            previewAnchors: scene.previewAnchors,
            effectiveRendererTier: scene.effectiveRendererTier,
            fallbackApplied: scene.fallbackApplied,
            fallbackReason: scene.fallbackReason,
            entries: entryGeometry
        }
    }

    function test_sharedPanelSceneContract_data() {
        return [
            { tag: "horizontal-native", mode: "horizontal" },
            { tag: "vertical-native", mode: "vertical" },
            { tag: "free-ring", mode: "free" }
        ]
    }

    function test_sharedPanelSceneContract(data) {
        const definition = caseDefinition(data.mode)
        const preview = createTemporaryObject(previewComponent, testCase, {
            panelDefinition: definition,
            previewMode: data.mode,
            hoveredEntry: 1
        })
        verify(preview !== null)
        const liveScene = createTemporaryObject(liveSceneComponent, testCase, {
            panelDefinition: definition,
            runtimeState: { hoveredEntry: 1 }
        })
        verify(liveScene !== null)
        wait(30)

        compare(JSON.stringify(contractSnapshot(preview.panelSceneItem)),
                JSON.stringify(contractSnapshot(liveScene)))

        const previewSurface = grabImage(
            preview.panelSceneItem.visualPanel)
        const liveSurface = grabImage(liveScene.visualPanel)
        compare(previewSurface.width, liveSurface.width)
        compare(previewSurface.height, liveSurface.height)
        verify(previewSurface.alpha(
            Math.round(previewSurface.width / 2),
            Math.round(previewSurface.height / 2)) > 0)
        verify(previewSurface.equals(liveSurface),
               data.mode + " shared procedural surface diverged")
    }

    function test_capabilityFallbackParity() {
        const definition = caseDefinition("horizontal")
        definition.rendererTier = "skinned2d"
        const fallbackCapabilities = {
            available: true,
            renderer: {
                requestedTier: "skinned2d",
                effectiveTier: "procedural2d",
                fallbackApplied: true,
                reasonCode: "renderer-host-unsupported"
            }
        }
        const preview = createTemporaryObject(previewComponent, testCase, {
            panelDefinition: definition,
            previewMode: "horizontal",
            hostCapabilities: fallbackCapabilities
        })
        verify(preview !== null)
        const liveScene = createTemporaryObject(liveSceneComponent, testCase, {
            panelDefinition: definition,
            hostCapabilities: fallbackCapabilities
        })
        verify(liveScene !== null)
        wait(0)

        compare(preview.activeRendererTier,
                liveScene.effectiveRendererTier)
        compare(preview.fallbackApplied, liveScene.fallbackApplied)
        compare(preview.fallbackReason, liveScene.fallbackReason)
        compare(preview.fallbackReason, "renderer-host-unsupported")
    }
}
