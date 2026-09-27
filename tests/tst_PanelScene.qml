import QtQuick 2.15
import QtQuick.Window
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

    function test_inheritedSegmentPreservesScene() {
        const ordinary = createTemporaryObject(sceneComponent, testCase, {
            panelDefinition: definition(), orderedEntries: entries()
        })
        const inherited = createTemporaryObject(sceneComponent, testCase, {
            panelDefinition: definition({ segments: [{ id: "main", source: "inherited",
                padding: -1, spacing: -1, background: "inherited", corners: "inherited",
                presentation: "open", motionProfile: "" }] }), orderedEntries: entries()
        })
        verify(ordinary && inherited)
        verify(!inherited.segmentedScene)
        compare(inherited.width, ordinary.width)
        compare(inherited.height, ordinary.height)
        compare(JSON.stringify(inherited.entryRects), JSON.stringify(ordinary.entryRects))
        compare(JSON.stringify(inherited.popupAnchors), JSON.stringify(ordinary.popupAnchors))
        wait(50)
        verify(grabImage(inherited).equals(grabImage(ordinary)))
    }

    function test_segmentMotionChangesPixelsAndHonorsReducedMotion() {
        const items = entries().slice(0, 1)
        items[0].segmentId = "main"
        const config = definition({ segments: [{ id: "main", order: 0,
            background: "solid", color: "#226688", padding: 20,
            spacing: -1, corners: "rounded", presentation: "open", motionProfile: "pulse" }] })
        const window = createTemporaryObject(motionWindowComponent, testCase)
        verify(waitForRendering(window.contentItem))
        const scene = createTemporaryObject(sceneComponent, window.contentItem, {
            x: 70, y: 80, panelDefinition: config, orderedEntries: items,
            animationProfiles: { animationProfiles: [{ id: "pulse", target: "icon", trigger: "hover-hold",
                reducedMotion: { mode: "none" }, tracks: [{ id: "scale", property: "scale",
                    from: 1, to: 1.16, duration: 250, easing: "linear" }] }] }
        })
        verify(scene)
        mouseMove(window.contentItem, 350, 300)
        wait(30)
        const before = grabImage(scene)
        const segment = scene.segmentSurfaces.itemAt(0)
        mouseMove(segment, segment.width / 2, segment.height / 2)
        tryVerify(function() { return segment.visualMotion.scale > 1.05 })
        wait(30)
        verify(!grabImage(scene).equals(before), "segment motion must change the rendered frame")
        scene.panelDefinition = Object.assign({}, config, { reducedMotion: true })
        tryCompare(scene.segmentSurfaces.itemAt(0), "reducedMotion", true)
        tryVerify(function() { return scene.segmentSurfaces.itemAt(0).visualMotion.scale === 1 })
        wait(30)
        const still = grabImage(scene)
        wait(150)
        verify(grabImage(scene).equals(still))
    }

    Component {
        id: motionWindowComponent
        Window { width: 360; height: 320; visible: true }
    }

    Component {
        id: pointerEntryComponent
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            enabled: parent.sceneInputEnabled
            onEntered: parent.sceneContext.scene.runtimeState = ({ hoveredEntry: parent.sceneIndex })
            onExited: parent.sceneContext.scene.runtimeState = ({ hoveredEntry: -1 })
        }
    }

    function test_closedSegmentKeepsEntryHover() {
        const window = createTemporaryObject(motionWindowComponent, testCase)
        verify(waitForRendering(window.contentItem))
        const items = entries().slice(0, 1)
        items[0].segmentId = "main"
        const scene = createTemporaryObject(sceneComponent, window.contentItem, {
            x: 70, y: 80, orderedEntries: items, entryDelegate: pointerEntryComponent,
            panelDefinition: definition({ reducedMotion: false, segments: [{ id: "main", order: 0,
                background: "solid", color: "#226688", padding: 20, spacing: -1,
                corners: "rounded", presentation: "closed", motionProfile: "" }] })
        })
        scene.entryDelegateContext = { scene: scene }
        const segment = scene.segmentSurfaces.itemAt(0)
        mouseMove(window.contentItem, 350, 300)
        tryCompare(segment, "expanded", false)
        mouseMove(segment, segment.width / 2, segment.height / 2)
        tryVerify(function() { return scene.runtimeState.hoveredEntry === 0 })
        wait(200)
        compare(segment.expanded, true)
        compare(scene.entryItemAt(0).sceneInputEnabled, true)
        mouseMove(window.contentItem, 350, 300)
        tryCompare(segment, "expanded", false)
    }

    function test_contentRefreshPreservesHoveredSegment_data() {
        return [{ tag: "animated", reducedMotion: false },
                { tag: "reduced-motion", reducedMotion: true }]
    }

    function test_contentRefreshPreservesHoveredSegment(data) {
        failOnWarning(/Binding loop detected/)
        const window = createTemporaryObject(motionWindowComponent, testCase)
        verify(waitForRendering(window.contentItem))
        mouseMove(window.contentItem, 350, 300)
        const items = entries().slice(0, 1)
        items[0].segmentId = "main"
        const config = definition({ reducedMotion: data.reducedMotion, segments: [{
            id: "main", order: 0, background: "solid", color: "#226688",
            padding: 20, spacing: -1, corners: "rounded", presentation: "closed"
        }] })
        const scene = createTemporaryObject(sceneComponent, window.contentItem, {
            x: 70, y: 80, orderedEntries: items, entryDelegate: pointerEntryComponent,
            panelDefinition: config
        })
        scene.entryDelegateContext = { scene: scene }
        const segment = scene.segmentSurfaces.itemAt(0)
        compare(segment.expanded, false)
        mouseMove(segment, segment.width / 2, segment.height / 2)
        tryVerify(function() { return scene.runtimeState.hoveredEntry === 0 })
        compare(segment.expanded, true)
        let surfaceChanges = 0
        segment.expandedChanged.connect(function() { ++surfaceChanges })

        // Content snapshots recompute segment geometry/definition without
        // changing the requested presentation. They must not reset it and
        // hide the hovered entry while its visibility binding is evaluating.
        scene.orderedEntries = JSON.parse(JSON.stringify(items))
        scene.panelDefinition = JSON.parse(JSON.stringify(config))
        compare(surfaceChanges, 0)
        compare(scene.runtimeState.hoveredEntry, 0)
        compare(segment.expanded, true)

        // Real preference changes still use the controller's interaction
        // guards, then settle closed once the pointer leaves.
        const open = JSON.parse(JSON.stringify(config))
        open.segments[0].presentation = "open"
        scene.panelDefinition = open
        scene.panelDefinition = config
        compare(surfaceChanges, 0)
        compare(segment.expanded, true)
        mouseMove(window.contentItem, 350, 300)
        tryCompare(segment, "expanded", false)
    }

    function test_independentSegmentsOwnSurfacesAndInput() {
        const items = entries()
        items[0].segmentId = "launchers"
        items[1].segmentId = "tasks"
        items[2].segmentId = "tasks"
        const scene = createTemporaryObject(sceneComponent, testCase, {
            panelDefinition: definition({ reducedMotion: true, segments: [
                { id: "launchers", order: 0, padding: 8, spacing: 2,
                  background: "solid", color: "#226688", corners: "square", presentation: "open" },
                { id: "tasks", order: 1, padding: 20, spacing: 14,
                  background: "solid", color: "#883322", corners: "capsule", presentation: "closed" }
            ] }), orderedEntries: items, entryDelegate: hostEntryComponent
        })
        verify(scene !== null)
        verify(scene.segmentedScene)
        compare(scene.segmentSurfaces.count, 2)
        const first = scene.segmentSurfaces.itemAt(0)
        const second = scene.segmentSurfaces.itemAt(1)
        compare(first.cornerRadius, 0)
        compare(second.cornerRadius, second.height / 2)
        verify(second.x >= first.x + first.width)
        tryVerify(function() { return !second.expanded })
        verify(scene.entryItemAt(0).sceneInputEnabled)
        verify(!scene.entryItemAt(1).sceneInputEnabled)
        verify(!scene.entryItemAt(2).visible)
        second.forceOpen = true
        tryVerify(function() { return second.expanded })
        tryVerify(function() { return scene.entryItemAt(1).sceneInputEnabled })
        const owner = scene.entryItemAt(1)
        const updated = JSON.parse(JSON.stringify(items))
        updated[1].active = false
        scene.orderedEntries = updated
        compare(scene.entryItemAt(1), owner)
        compare(scene.segmentSurfaces.itemAt(1), second)
        scene.runtimeState = { popupOpen: true }
        second.forceOpen = false
        verify(second.expanded)
        scene.runtimeState = {}
        tryVerify(function() { return !second.expanded })
        compare(scene.entryGeometryAt(1).segmentId, "tasks")
        compare(scene.entryGeometryAt(0).segmentId, "launchers")
    }

    Component {
        id: hostEntryComponent

        Rectangle {
            property var bridgedEntry: parent.sceneEntry
            property int bridgedIndex: parent.sceneIndex
            property bool bridgedInputEnabled: parent.sceneInputEnabled

            objectName: "host-entry-" + bridgedIndex
            anchors.fill: parent
            color: bridgedInputEnabled ? "#44ff88" : "#ff8844"
        }
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

    function chassisThemeDefinition() {
        const packageRoot = Qt.resolvedUrl(
            "../assets/themes/sci-fi-chassis-dark/")
        const sourceRect = { x: 0, y: 0, width: 1200, height: 160 }
        return {
            format: "org.archdock.theme",
            version: 2,
            id: "sci-fi-chassis-dark",
            valid: true,
            loadable: true,
            status: "valid",
            assets: [
                {
                    id: "surface",
                    naturalSize: { width: 1200, height: 160 }
                },
                {
                    id: "glow",
                    naturalSize: { width: 1200, height: 160 }
                },
                {
                    id: "input-mask",
                    naturalSize: { width: 1200, height: 160 }
                }
            ],
            layers: [{
                id: "glow-open",
                asset: "glow",
                role: "glow",
                sourceRect: sourceRect,
                opacity: 1,
                blendMode: "source-over"
            }],
            states: [{ id: "open", layers: ["glow-open"] }],
            slices: [{
                id: "open-horizontal",
                asset: "surface",
                state: "open",
                orientation: "horizontal",
                sourceRect: sourceRect,
                fixedStart: 152,
                fixedEnd: 152,
                centerMode: "stretch"
            }],
            contentRegions: [{
                id: "open-content",
                state: "open",
                orientation: "horizontal",
                shape: "rect",
                rect: { x: 168, y: 36, width: 864, height: 88 },
                baseline: 72
            }],
            inputMasks: [{
                id: "open-input",
                asset: "input-mask",
                state: "open",
                orientation: "horizontal",
                threshold: 0.5
            }],
            effectMargins: { left: 14, top: 12, right: 14, bottom: 14 },
            assetPaths: {
                surface: packageRoot + "assets/surface.svg",
                glow: packageRoot + "assets/glow.svg",
                "input-mask": packageRoot + "masks/input.svg"
            }
        }
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

        const incompleteTheme = createScene({
            panelDefinition: definition({
                rendererTier: "skinned2d",
                panelThemeId: "valid-theme"
            }),
            hostCapabilities: ({ available: true }),
            themeDefinition: ({ id: "valid-theme", valid: true })
        })
        compare(incompleteTheme.effectiveRendererTier, "procedural2d")
        compare(incompleteTheme.fallbackApplied, true)
        compare(incompleteTheme.fallbackReason, "theme-contract-invalid")

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

    function test_skinned2DWrapsLayoutInSafeThemeGeometry() {
        const scene = createScene({
            panelDefinition: definition({
                rendererTier: "skinned2d",
                panelThemeId: "sci-fi-chassis-dark",
                color: "#44ddea",
                glowIntensity: 1.35
            }),
            runtimeState: {
                hovered: true,
                hoveredEntry: 1,
                rendererFallback: "",
                presentationState: "open",
                transitionState: "closing",
                presentationProgress: 0.4
            },
            hostCapabilities: {
                available: true,
                renderer: {
                    effectiveTier: "skinned2d",
                    fallbackApplied: false
                }
            },
            themeDefinition: chassisThemeDefinition(),
            animationProfiles: ({ reducedMotion: false })
        })
        tryVerify(function() {
            return scene.effectiveRendererTier === "skinned2d"
        }, 3000)

        compare(scene.fallbackApplied, false)
        compare(scene.fallbackReason, "")
        compare(scene.presentationState, "open")
        compare(scene.contentBounds.width, scene.layoutGeometry.width)
        compare(scene.contentBounds.height, scene.layoutGeometry.height)
        verify(scene.contentBounds.x > 0)
        verify(scene.contentBounds.y > 0)
        verify(scene.width > scene.contentBounds.width)
        verify(scene.height > scene.contentBounds.height)
        compare(scene.visualBounds.width, scene.width)
        compare(scene.visualBounds.height, scene.height)
        verify(scene.effectBounds.width > scene.visualBounds.width)
        verify(scene.effectBounds.height > scene.visualBounds.height)

        const first = scene.entryGeometryAt(0)
        verify(first.position.x >= scene.contentBounds.x)
        verify(first.position.y >= scene.contentBounds.y)
        verify(scene.popupAnchors.entries[0].x >= scene.contentBounds.x)
        verify(scene.contains(Qt.point(scene.width / 2, scene.height / 2)))
        verify(!scene.contains(Qt.point(0, 0)))
        verify(scene.visualPanel.rendererReady)
        compare(scene.visualPanel.hovered, true)
        compare(scene.visualPanel.transitionState, "closing")
        compare(scene.visualPanel.presentationProgress, 0.4)
        compare(scene.visualPanel.tintColor, "#44ddea")
        compare(scene.visualPanel.glowIntensity, 1.35)
        compare(scene.visualPanel.reducedMotion, false)
    }

    function test_true3DFallbackKeepsSkinContentGeometry() {
        const theme = chassisThemeDefinition();
        theme.capabilities = {rendererTiers: ["true3d", "skinned2d", "procedural2d"],
            fallbackRendererTiers: ["skinned2d", "procedural2d"]};
        const direct = createScene({
            panelDefinition: definition({rendererTier: "skinned2d", panelThemeId: theme.id}),
            themeDefinition: theme,
            hostCapabilities: {available: true, renderer: {effectiveTier: "skinned2d"}}
        });
        tryCompare(direct, "effectiveRendererTier", "skinned2d");
        const fallback = createScene({
            panelDefinition: definition({rendererTier: "true3d", panelThemeId: theme.id}),
            themeDefinition: theme,
            hostCapabilities: {available: true, renderer: {effectiveTier: "true3d"}}
        });
        tryCompare(fallback, "effectiveRendererTier", "skinned2d");
        verify(fallback.fallbackApplied);
        compare(JSON.stringify(fallback.contentBounds), JSON.stringify(direct.contentBounds));
        compare(JSON.stringify(fallback.entryRects), JSON.stringify(direct.entryRects));
        compare(fallback.width, direct.width);
        compare(fallback.height, direct.height);
    }

    function test_chassisSkinWorksForHorizontalFreeAndRejectsVertical() {
        const rendererCapabilities = {
            available: true,
            renderer: {
                effectiveTier: "skinned2d",
                fallbackApplied: false
            }
        }
        const freeScene = createScene({
            panelDefinition: definition({
                id: "free-chassis",
                edge: "free",
                layout: "horizontal",
                rendererTier: "skinned2d",
                panelThemeId: "sci-fi-chassis-dark"
            }),
            runtimeState: {
                hovered: false,
                hoveredEntry: 1,
                rendererFallback: "",
                presentationState: "open"
            },
            hostCapabilities: rendererCapabilities,
            themeDefinition: chassisThemeDefinition(),
            geometryCompatibilityProfile: "live",
            entryDelegateContext: { hostKind: "free" }
        })
        tryCompare(freeScene, "effectiveRendererTier", "skinned2d", 3000)
        compare(freeScene.fallbackApplied, false)
        compare(freeScene.layoutPath, "horizontal")
        compare(freeScene.themeOrientation, "horizontal")
        compare(freeScene.geometryCompatibilityProfile, "live")
        verify(freeScene.visualPanel.rendererReady)

        const verticalScene = createScene({
            panelDefinition: definition({
                id: "vertical-chassis",
                edge: "left",
                layout: "vertical",
                rendererTier: "skinned2d",
                panelThemeId: "sci-fi-chassis-dark"
            }),
            runtimeState: {
                hovered: false,
                hoveredEntry: 1,
                rendererFallback: "",
                presentationState: "open"
            },
            hostCapabilities: rendererCapabilities,
            themeDefinition: chassisThemeDefinition()
        })
        tryCompare(verticalScene, "fallbackReason",
                   "theme-orientation-unavailable", 3000)
        compare(verticalScene.effectiveRendererTier, "procedural2d")
        compare(verticalScene.fallbackApplied, true)
        verify(verticalScene.visualPanel.rendererReady)
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

    function test_hostEntryDelegateReceivesSceneContext() {
        const scene = createScene({
            panelDefinition: definition({
                layout: "circular",
                pathOrientation: "tangent"
            }),
            entryDelegate: hostEntryComponent,
            entryInteractionEnabled: false,
            geometryCompatibilityProfile: "live"
        })

        compare(scene.geometryCompatibilityProfile, "live")
        const slot = scene.entryItemAt(1)
        verify(slot !== null)
        verify(slot.delegateItem !== null)
        compare(slot.sceneEntry.id, "org.example.two")
        compare(slot.sceneIndex, 1)
        compare(slot.sceneInputEnabled, false)
        compare(slot.delegateItem.objectName, "host-entry-1")
        compare(slot.delegateItem.bridgedEntry.id, "org.example.two")
        compare(slot.delegateItem.bridgedIndex, 1)
        compare(slot.delegateItem.bridgedInputEnabled, false)
        compare(slot.width, scene.layoutGeometry.iconSize)
        compare(slot.height, scene.layoutGeometry.iconSize)
        compare(slot.rotation, 0)
    }

    function test_windowStateRefreshPreservesInteractionOwner() {
        const scene = createTemporaryObject(sceneComponent, testCase, {
            panelDefinition: definition(), orderedEntries: entries(),
            entryDelegate: hostEntryComponent
        })
        verify(scene !== null)
        const owner = scene.entryItemAt(1).delegateItem
        const changed = entries()
        changed[1].active = false
        changed[1].minimized = true
        changed[1].displayName = "Renamed"
        scene.orderedEntries = changed
        compare(scene.entryItemAt(1).delegateItem, owner,
                "window updates must not destroy an open menu's owner")
        compare(owner.bridgedEntry.displayName, "Renamed")
        compare(owner.bridgedEntry.active, false)
        compare(owner.bridgedEntry.minimized, true)
        scene.orderedEntries = [changed[0], changed[2]]
        compare(scene.entryCount, 2)
        compare(scene.entryItemAt(1).sceneEntry.id, "org.example.three")
        compare(scene.entryItemAt(2), null)
    }

    // A panel that is not on the baked tier must not pick up any of its
    // geometry. Track metrics are the marker: they exist only when the scene
    // is actually laid out on a theme's anchor path.
    function test_nonBakedScenesReportNoTrackGeometry_data() {
        return [
            { tag: "procedural-horizontal", tier: "procedural2d",
              layout: "horizontal", edge: "bottom" },
            { tag: "procedural-ring", tier: "procedural2d",
              layout: "ring", edge: "free" },
            { tag: "skinned-horizontal", tier: "skinned2d",
              layout: "horizontal", edge: "bottom" }
        ]
    }

    function test_nonBakedScenesReportNoTrackGeometry(data) {
        const scene = createTemporaryObject(sceneComponent, testCase, {
            panelDefinition: definition({
                rendererTier: data.tier,
                layout: data.layout,
                edge: data.edge
            }),
            orderedEntries: entries(),
            hostCapabilities: ({
                available: true,
                renderer: {
                    requestedTier: data.tier,
                    effectiveTier: data.tier,
                    fallbackApplied: false
                }
            }),
            entryDelegateContext: ({
                hostKind: data.edge === "free" ? "free" : "native"
            })
        })
        verify(scene !== null)
        verify(!scene.bakedTierRequested, "the baked tier is not requested")
        verify(!scene.bakedMetadataUsable, "no baked metadata is usable")
        compare(scene.activeTrackMetrics, null, "no track geometry is used")
        verify(scene.activeInputRegionKind !== "platform-mask",
               "the platform mask is not claimed")
        verify(scene.width > 0 && scene.height > 0, "the scene has a size")
    }
}
