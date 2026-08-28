import QtQuick 2.15
import QtTest 1.3
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "PanelScene"
    when: windowShown
    width: 360
    height: 320

    Component {
        id: sceneComponent

        PanelScene {}
    }

    function definition(overrides) {
        const result = {
            schemaVersion: 2,
            id: "free-test",
            edge: "bottom",
            layout: "horizontal",
            layoutScale: 1,
            layoutAngle: 0,
            layoutRadius: 100,
            layoutRows: 2,
            layoutPadding: 12,
            pathSides: 6,
            pathOrientation: "upright",
            rendererTier: "procedural2d",
            panelThemeId: "",
            appearance: "glass",
            color: "#33566d",
            opacity: 0.9,
            iconSize: 36,
            spacing: 6,
            iconShape: "rounded",
            revealZone: 8,
            revealHandle: "edge-strip"
        }
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            result[key] = additions[key]
        return result
    }

    function entries() {
        return [
            { id: "org.example.one", displayName: "One", active: false },
            { id: "org.example.two", displayName: "Two", active: true },
            {
                id: "org.example.three",
                displayName: "Three",
                active: false,
                minimized: true
            }
        ]
    }

    function createScene(properties) {
        const values = {
            panelDefinition: definition(),
            runtimeState: {
                hovered: false,
                hoveredEntry: 1,
                rendererFallback: ""
            },
            orderedEntries: entries(),
            hostCapabilities: {
                available: true,
                renderer: {
                    effectiveTier: "procedural2d",
                    fallbackApplied: false
                }
            },
            themeDefinition: ({}),
            iconStyleDefinition: ({}),
            animationProfiles: ({ reducedMotion: true }),
            screenBounds: { x: 0, y: 0, width: 1280, height: 720 },
            availableBounds: { x: 0, y: 0, width: 1280, height: 680 }
        }
        const additions = properties || ({})
        for (const key of Object.keys(additions))
            values[key] = additions[key]
        const scene = createTemporaryObject(sceneComponent, testCase, values)
        verify(scene !== null)
        wait(0)
        return scene
    }

    function test_deterministicFixedData() {
        const first = createScene()
        const second = createScene()
        compare(JSON.stringify(first.layoutGeometry),
                JSON.stringify(second.layoutGeometry))
        compare(JSON.stringify(first.contentBounds),
                JSON.stringify(second.contentBounds))
        compare(JSON.stringify(first.effectBounds),
                JSON.stringify(second.effectBounds))
        compare(JSON.stringify(first.inputRegion),
                JSON.stringify(second.inputRegion))
        compare(JSON.stringify(first.revealHandle),
                JSON.stringify(second.revealHandle))
        compare(JSON.stringify(first.popupAnchors),
                JSON.stringify(second.popupAnchors))
        for (let index = 0; index < first.entryCount; ++index) {
            compare(JSON.stringify(first.entryGeometryAt(index)),
                    JSON.stringify(second.entryGeometryAt(index)))
        }
    }

    function test_invalidAndMissingThemesUseSafeFallback() {
        const missing = createScene({
            panelDefinition: definition({
                rendererTier: "skinned2d",
                panelThemeId: "missing-theme"
            }),
            hostCapabilities: ({ available: true }),
            themeDefinition: ({})
        })
        compare(missing.effectiveRendererTier, "procedural2d")
        compare(missing.fallbackApplied, true)
        compare(missing.fallbackReason, "theme-unavailable")
        verify(missing.visualPanel !== null)
        compare(missing.visualPanel.rendererReady, true)

        const invalid = createScene({
            panelDefinition: definition({
                rendererTier: "skinned2d",
                panelThemeId: "broken-theme"
            }),
            hostCapabilities: ({ available: true }),
            themeDefinition: ({
                id: "broken-theme",
                valid: false,
                status: "invalid"
            })
        })
        compare(invalid.effectiveRendererTier, "procedural2d")
        compare(invalid.fallbackApplied, true)
        compare(invalid.fallbackReason, "theme-unavailable")

        const unavailableRenderer = createScene({
            panelDefinition: definition({
                rendererTier: "skinned2d",
                panelThemeId: "valid-theme"
            }),
            hostCapabilities: ({ available: true }),
            themeDefinition: ({ id: "valid-theme", valid: true })
        })
        compare(unavailableRenderer.effectiveRendererTier, "procedural2d")
        compare(unavailableRenderer.fallbackApplied, true)
        compare(unavailableRenderer.fallbackReason, "renderer-unavailable")

        const missingLegacyAsset = createScene({
            panelDefinition: definition({
                themeAsset: "file:///missing/arch-dock-theme.svg"
            }),
            themeDefinition: ({})
        })
        compare(missingLegacyAsset.effectiveRendererTier, "procedural2d")
        compare(missingLegacyAsset.fallbackApplied, true)
        compare(missingLegacyAsset.fallbackReason, "theme-unavailable")
    }

    function test_nestedDefinitionContractIsAccepted() {
        const scene = createScene({
            panelDefinition: {
                layout: {
                    pathType: "ring",
                    scale: 1,
                    angle: 0,
                    radius: 90,
                    rows: 2,
                    padding: 10,
                    polygonSides: 6,
                    orientation: "tangent"
                },
                placement: { edge: "right" },
                visibility: { revealZone: 7 },
                presentation: { revealHandle: "bar" },
                surface: {
                    rendererTier: "procedural2d",
                    appearance: "minimal",
                    opacity: 0.8
                },
                iconStyle: { size: 32, spacing: 5, shape: "circle" }
            }
        })
        compare(scene.layoutPath, "ring")
        compare(scene.layoutRadius, 90)
        compare(scene.pathOrientation, "tangent")
        compare(scene.iconSize, 32)
        compare(scene.iconSpacing, 5)
        compare(scene.iconShape, "circle")
        compare(scene.appearance, "minimal")
        compare(scene.revealHandle.edge, "right")
        compare(scene.revealHandle.mode, "bar")
        compare(scene.revealHandle.width, 7)
    }

    function test_procedural2DProducesVisiblePixels() {
        const scene = createScene()
        wait(20)
        const image = grabImage(scene)
        compare(image.width, scene.width)
        compare(image.height, scene.height)
        const first = scene.entryGeometryAt(0)
        const centerX = Math.round(first.position.x + scene.iconSize / 2)
        const centerY = Math.round(first.position.y + scene.iconSize / 2)
        verify(image.alpha(centerX, centerY) > 0,
               "the fixed procedural entry was not rendered")
        const trackGapX = Math.round(
            scene.layoutPadding + scene.iconSize + scene.iconSpacing / 2)
        verify(image.alpha(trackGapX, Math.round(scene.height / 2)) > 0,
               "the procedural panel surface was not rendered")
        verify(scene.visualPanel.rendererReady)
    }

    function test_publicInputsAndOutputs() {
        const scene = createScene()
        compare(scene.panelDefinition.schemaVersion, 2)
        compare(scene.runtimeState.hoveredEntry, 1)
        compare(scene.orderedEntries.length, 3)
        compare(scene.hostCapabilities.available, true)
        compare(scene.animationProfiles.reducedMotion, true)
        compare(scene.screenBounds.width, 1280)
        compare(scene.availableBounds.height, 680)

        compare(scene.entryCount, 3)
        compare(scene.width, scene.layoutGeometry.width)
        compare(scene.height, scene.layoutGeometry.height)
        compare(scene.visualBounds.width, scene.contentBounds.width)
        verify(scene.effectBounds.width > scene.contentBounds.width)
        compare(scene.inputRegion.width, scene.contentBounds.width)
        compare(scene.revealHandle.edge, "bottom")
        compare(scene.revealHandle.y,
                scene.height - scene.revealHandle.height)
        compare(scene.popupAnchors.entries.length, 3)
        compare(scene.popupAnchors.primary.index, 1)
        compare(scene.previewAnchors.entries.length, 3)
        compare(scene.effectiveRendererTier, "procedural2d")
        compare(scene.fallbackApplied, false)
        compare(scene.fallbackReason, "")
        compare(scene.runtimeCapabilityStatus.available, true)
        compare(scene.runtimeCapabilityStatus.effectiveRendererTier,
                "procedural2d")
        verify(scene.visualPanel !== null)
        compare(scene.iconDelegates.count, 3)
        verify(scene.entryItemAt(0) !== null)
    }
}
