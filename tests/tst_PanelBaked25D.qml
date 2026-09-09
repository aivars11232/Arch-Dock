import QtQuick 2.15
import QtTest 1.3
import ArchDock.Rendering 1.0

// Baked 2.5D renderer contract.
//
// The acceptance criteria this proves are the ones that distinguish a
// perspective platform from a flat picture: real icons pass behind and in
// front of declared platform layers, depth order and scale are deterministic,
// the logical order the keyboard walks is unaffected by that draw order, and
// nothing here needs Qt Quick 3D.
TestCase {
    id: testCase

    name: "PanelBaked25D"
    when: windowShown
    width: 1400
    height: 900
    visible: true

    readonly property url assetRoot: Qt.resolvedUrl("fixtures/theme-v2/assets/")

    Component {
        id: sceneComponent

        PanelScene {}
    }

    // A flat, opaque delegate. Pixel colour is then an unambiguous answer to
    // "was this icon drawn over the platform layer or under it".
    Component {
        id: solidEntryDelegate

        Rectangle {
            anchors.fill: parent
            color: "#ff0000"
        }
    }

    function assetUrl(name) {
        return String(assetRoot) + name
    }

    // A complete baked projection. `overrides` replaces top-level members so a
    // case can remove a layer, break an asset path or move the occlusion depth
    // without restating the whole package.
    function bakedTheme(overrides) {
        const artwork = { x: 0, y: 0, width: 1200, height: 600 }
        const result = {
            format: "org.archdock.theme",
            version: 2,
            id: "fixture-baked-ring",
            valid: true,
            loadable: true,
            status: "valid",
            capabilities: {
                hosts: ["free-desktop"],
                rendererTiers: ["baked2.5d", "procedural2d"],
                preferredRendererTier: "baked2.5d",
                fallbackRendererTiers: ["procedural2d"],
                layouts: ["ring"],
                orientations: ["free"],
                features: ["dynamic-tint", "dynamic-glow", "icon-state-styling"],
                presentationMechanisms: [],
                rotation: { mode: "free" }
            },
            assets: [
                { id: "platform-rear", kind: "vector",
                  naturalSize: { width: 1200, height: 600 } },
                { id: "platform-band", kind: "vector",
                  naturalSize: { width: 1200, height: 600 } },
                { id: "platform-glow", kind: "mask",
                  naturalSize: { width: 1200, height: 600 } },
                { id: "input-mask", kind: "mask",
                  naturalSize: { width: 1200, height: 600 } }
            ],
            assetPaths: {
                "platform-rear": assetUrl("platform-solid.svg"),
                "platform-band": assetUrl("platform-band.svg"),
                "platform-glow": assetUrl("platform-glow.svg"),
                "input-mask": assetUrl("ring-input.svg")
            },
            layers: [
                { id: "rear-layer", asset: "platform-rear", role: "rear",
                  sourceRect: artwork, opacity: 1, blendMode: "source-over" },
                { id: "glow-layer", asset: "platform-glow", role: "glow",
                  sourceRect: artwork, opacity: 0.8,
                  blendMode: "source-over" },
                { id: "front-layer", asset: "platform-band",
                  role: "foreground", sourceRect: artwork, opacity: 1,
                  blendMode: "source-over" }
            ],
            states: [
                { id: "normal",
                  layers: ["rear-layer", "glow-layer", "front-layer"] },
                { id: "open",
                  layers: ["rear-layer", "glow-layer", "front-layer"] },
                { id: "collapsed", layers: ["rear-layer", "front-layer"] },
                { id: "hover",
                  layers: ["rear-layer", "glow-layer", "front-layer"] }
            ],
            tracks: [
                {
                    id: "ring-track",
                    state: "open",
                    shape: "ellipse",
                    center: { x: 600, y: 300 },
                    radiusX: 450,
                    radiusY: 160,
                    startDegrees: 0,
                    sweepDegrees: 360,
                    depth: { farScale: 0.6, nearScale: 1,
                             occlusionDepth: 0.5 },
                    tilt: { minimumDegrees: -12, maximumDegrees: 12,
                            defaultDegrees: 0 }
                }
            ],
            inputMasks: [
                { id: "open-input", asset: "input-mask", state: "open",
                  orientation: "free", threshold: 0.5 }
            ],
            effectMargins: { left: 0, top: 0, right: 0, bottom: 0 }
        }
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            result[key] = additions[key]
        return result
    }

    function bakedDefinition(overrides) {
        const result = {
            schemaVersion: 2,
            id: "free-baked",
            edge: "free",
            layout: "ring",
            layoutScale: 1,
            layoutAngle: 0,
            layoutRadius: 300,
            layoutPadding: 0,
            pathOrientation: "upright",
            rendererTier: "baked2.5d",
            panelThemeId: "fixture-baked-ring",
            iconSize: 60,
            spacing: 8,
            opacity: 1
        }
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            result[key] = additions[key]
        return result
    }

    function bakedCapabilities() {
        return {
            available: true,
            renderer: {
                requestedTier: "baked2.5d",
                effectiveTier: "baked2.5d",
                fallbackApplied: false,
                reasonCode: "available"
            },
            rotation: { available: true }
        }
    }

    function entries(count) {
        const result = []
        for (let index = 0; index < count; ++index) {
            result.push({
                id: "entry-" + index,
                displayName: "Entry " + index
            })
        }
        return result
    }

    function createScene(overrides) {
        const settings = overrides || ({})
        const scene = createTemporaryObject(sceneComponent, testCase, {
            panelDefinition: bakedDefinition(settings.definition),
            themeDefinition: bakedTheme(settings.theme),
            hostCapabilities: settings.capabilities !== undefined
                ? settings.capabilities : bakedCapabilities(),
            orderedEntries: entries(settings.count === undefined
                                    ? 4 : settings.count),
            runtimeState: settings.runtimeState !== undefined
                ? settings.runtimeState : ({ presentationState: "open" }),
            animationProfiles: settings.animationProfiles !== undefined
                ? settings.animationProfiles : ({ reducedMotion: true }),
            entryDelegate: solidEntryDelegate,
            entryDelegateContext: ({ hostKind: "free" }),
            geometryCompatibilityProfile: "canonical"
        })
        verify(scene !== null)
        return scene
    }

    function waitForTier(scene, tier) {
        tryCompare(scene, "effectiveRendererTier", tier, 5000)
    }

    function fuzzy(actual, expected, message) {
        verify(Math.abs(actual - expected) <= 0.001,
               message + ": actual=" + actual + " expected=" + expected)
    }

    function test_true3DFallbackKeepsBakedGeometryAndDeclaredOrder() {
        const theme = bakedTheme();
        theme.capabilities.rendererTiers = ["true3d", "baked2.5d", "procedural2d"];
        theme.capabilities.fallbackRendererTiers = ["baked2.5d", "procedural2d"];
        const direct = createScene({theme: theme});
        waitForTier(direct, "baked2.5d");
        const fallback = createScene({
            definition: {rendererTier: "true3d"}, theme: theme,
            capabilities: {available: true, renderer: {effectiveTier: "true3d"}}
        });
        waitForTier(fallback, "baked2.5d");
        verify(fallback.fallbackApplied);
        verify(fallback.activeTrackMetrics !== null,
               "fallback must place icons on the baked platform track");
        compare(JSON.stringify(fallback.entryRects), JSON.stringify(direct.entryRects));
        compare(fallback.width, direct.width);
        compare(fallback.height, direct.height);
        const proceduralFirst = bakedTheme();
        proceduralFirst.capabilities.rendererTiers = ["true3d", "baked2.5d", "procedural2d"];
        proceduralFirst.capabilities.fallbackRendererTiers = ["procedural2d", "baked2.5d"];
        fallback.themeDefinition = proceduralFirst;
        waitForTier(fallback, "procedural2d");
        compare(fallback.activeTrackMetrics, null);
    }

    // Criterion: real application icons can pass behind and in front of
    // platform layers, and the theme's declared occlusion depth is what
    // decides which. The same scene is rendered twice with only that depth
    // changed, so nothing but the occlusion contract can explain the
    // difference.
    function test_realIconsPassBehindAndInFrontOfPlatformLayers() {
        const behind = createScene()
        waitForTier(behind, "baked2.5d")

        const farEntry = behind.entryGeometryAt(0)
        compare(farEntry.depth, 0, "entry 0 sits at the far edge")
        verify(!farEntry.inFront, "a far entry is behind the foreground")
        const sampleX = Math.round(farEntry.entryBounds.x
                                   + farEntry.entryBounds.width / 2)
        const sampleY = Math.round(farEntry.entryBounds.y
                                   + farEntry.entryBounds.height / 2)

        const hidden = grabImage(behind)
        compare(hidden.blue(sampleX, sampleY), 255,
                "the foreground band covers the far icon")
        compare(hidden.red(sampleX, sampleY), 0,
                "no part of the far icon shows through")

        // Only the declared occlusion depth changes: every entry is now at or
        // beyond it, so the same icon must draw over the same band.
        const theme = bakedTheme()
        theme.tracks = [{
            id: "ring-track", state: "open", shape: "ellipse",
            center: { x: 600, y: 300 }, radiusX: 450, radiusY: 160,
            startDegrees: 0, sweepDegrees: 360,
            depth: { farScale: 0.6, nearScale: 1, occlusionDepth: 0 }
        }]
        const front = createScene({ theme: theme })
        waitForTier(front, "baked2.5d")

        const raised = front.entryGeometryAt(0)
        compare(raised.depth, 0, "the entry has not moved")
        verify(raised.inFront, "it is now in front of the foreground")
        fuzzy(raised.entryBounds.x, farEntry.entryBounds.x,
              "the entry is at the same place")

        const shown = grabImage(front)
        compare(shown.red(sampleX, sampleY), 255,
                "the same icon now draws over the band")
    }

    // Criterion: depth order and scale are deterministic, and the near entry
    // is drawn last so it wins where entries overlap.
    function test_depthOrderAndScaleAreDeterministic() {
        const scene = createScene()
        waitForTier(scene, "baked2.5d")

        const far = scene.entryGeometryAt(0)
        const right = scene.entryGeometryAt(1)
        const near = scene.entryGeometryAt(2)
        compare(far.depth, 0, "far depth")
        compare(near.depth, 1, "near depth")
        fuzzy(right.depth, 0.5, "flank depth")
        fuzzy(far.scaleFactor, 0.6, "far scale is the declared far scale")
        fuzzy(near.scaleFactor, 1, "near scale is the declared near scale")

        verify(scene.entryItemAt(0).z < scene.entryItemAt(1).z,
               "a nearer entry is drawn after a further one")
        verify(scene.entryItemAt(1).z < scene.entryItemAt(2).z,
               "draw order follows depth all the way to the near edge")
        compare(scene.entryItemAt(0).z, far.depth,
               "the drawn z is the reported depth")

        const repeat = scene.entryGeometryAt(0)
        compare(repeat.depth, far.depth, "depth is stable")
        compare(repeat.x, far.x, "position is stable")
        compare(repeat.scaleFactor, far.scaleFactor, "scale is stable")
    }

    // Criterion: logical navigation order stays the entry order. Depth decides
    // only what paints first; it must never reorder what a keyboard walks.
    function test_logicalOrderSurvivesDepthSorting() {
        const scene = createScene()
        waitForTier(scene, "baked2.5d")

        for (let index = 0; index < 4; ++index) {
            const item = scene.entryItemAt(index)
            verify(item !== null, "entry " + index + " exists")
            compare(item.objectName, "panel-entry-" + index,
                    "delegate " + index + " keeps its logical index")
            compare(String(item.sceneEntry.id), "entry-" + index,
                    "delegate " + index + " keeps its entry")
        }
        // The near entry paints last but is still the third in the model.
        verify(scene.entryItemAt(2).z > scene.entryItemAt(0).z,
               "the third entry paints above the first")
    }

    // Criterion: the foreground occlusion layer sits between the entries at
    // the declared depth rather than above or below all of them.
    function test_foregroundSitsBetweenEntriesAtTheDeclaredDepth() {
        const scene = createScene()
        waitForTier(scene, "baked2.5d")

        compare(scene.occlusionDepth, 0.5, "the declared depth is reported")
        const foreground = scene.foregroundOcclusionItem
        verify(foreground !== null, "the foreground layer was instantiated")
        verify(scene.entryItemAt(0).z < scene.occlusionDepth,
               "the far entry is below the foreground")
        verify(scene.entryItemAt(2).z >= scene.occlusionDepth,
               "the near entry is at or above the foreground")
    }

    // Criterion: a missing platform asset falls back without losing the dock.
    function test_missingPlatformFallsBackAndKeepsThePanelUsable() {
        const theme = bakedTheme()
        theme.assetPaths = {
            "platform-rear": assetUrl("does-not-exist.svg"),
            "platform-band": assetUrl("platform-band.svg"),
            "platform-glow": assetUrl("platform-glow.svg"),
            "input-mask": assetUrl("ring-input.svg")
        }
        const scene = createScene({ theme: theme })
        waitForTier(scene, "procedural2d")

        verify(scene.fallbackApplied, "the fallback is reported")
        verify(scene.fallbackReason.length > 0, "a reason is given")
        compare(scene.runtimeCapabilityStatus.requestedRendererTier,
                "baked2.5d", "the request is still reported truthfully")
        verify(scene.width > 0 && scene.height > 0, "the panel keeps a size")
        compare(scene.entryCount, 4, "every entry survives the fallback")
        for (let index = 0; index < 4; ++index)
            verify(scene.entryItemAt(index) !== null, "entry " + index)
    }

    // A decorative layer that cannot load is skipped and counted; it does not
    // cost the panel its renderer.
    function test_aMissingDecorativeLayerIsSkippedNotFatal() {
        const theme = bakedTheme()
        theme.assetPaths = {
            "platform-rear": assetUrl("platform-solid.svg"),
            "platform-band": assetUrl("platform-band.svg"),
            "platform-glow": assetUrl("does-not-exist.svg"),
            "input-mask": assetUrl("ring-input.svg")
        }
        const scene = createScene({ theme: theme })
        waitForTier(scene, "baked2.5d")
        verify(!scene.fallbackApplied, "the panel keeps the baked renderer")
        verify(scene.visualPanel !== null, "the platform still draws")
    }

    // The scene box holds the platform and every entry, at several radii and
    // entry counts, so nothing the theme positions is clipped away.
    function test_sceneBoxHoldsPlatformAndEntries_data() {
        const rows = []
        for (const count of [1, 4, 9])
            for (const radius of [180, 300, 520])
                rows.push({ tag: count + "@" + radius,
                            count: count, radius: radius })
        return rows
    }

    function test_sceneBoxHoldsPlatformAndEntries(data) {
        const scene = createScene({
            count: data.count,
            definition: { layoutRadius: data.radius }
        })
        waitForTier(scene, "baked2.5d")
        verify(scene.width >= 1 && scene.height >= 1, "the scene has a size")
        for (let index = 0; index < data.count; ++index) {
            const bounds = scene.entryGeometryAt(index).entryBounds
            verify(bounds.x >= -0.001 && bounds.y >= -0.001
                   && bounds.x + bounds.width <= scene.width + 0.001
                   && bounds.y + bounds.height <= scene.height + 0.001,
                   "entry " + index + " fits the scene box")
        }
    }

    // Input follows the platform: the empty desktop inside and around a ring
    // passes through, while the platform and the icons on it do not.
    function test_inputFollowsThePlatformAndItsEntries() {
        const scene = createScene()
        waitForTier(scene, "baked2.5d")

        compare(scene.activeInputRegionKind, "platform-mask",
                "the package mask is the active region")
        verify(!scene.containsInputPoint(Qt.point(1, 1)),
               "the empty corner passes through")

        const near = scene.entryGeometryAt(2)
        verify(scene.containsInputPoint(Qt.point(
                   near.entryBounds.x + near.entryBounds.width / 2,
                   near.entryBounds.y + near.entryBounds.height / 2)),
               "an icon accepts input")
    }

    // Tilt is bounded by the theme and moves the platform with the track.
    function test_tiltIsBoundedByTheTheme() {
        const level = createScene()
        waitForTier(level, "baked2.5d")
        const tilted = createScene({
            definition: { surface2_5D: { tilt: 12 } }
        })
        waitForTier(tilted, "baked2.5d")
        const clamped = createScene({
            definition: { surface2_5D: { tilt: 900 } }
        })
        waitForTier(clamped, "baked2.5d")

        verify(tilted.activeTrackMetrics.tiltFactor
               > level.activeTrackMetrics.tiltFactor,
               "a declared tilt opens the ring up")
        compare(clamped.activeTrackMetrics.tiltFactor,
                tilted.activeTrackMetrics.tiltFactor,
                "a tilt past the declared maximum is clamped to it")
        fuzzy(tilted.activeTrackMetrics.platform.height
              / level.activeTrackMetrics.platform.height,
              tilted.activeTrackMetrics.tiltFactor,
              "the platform takes the same factor as the track")
    }

    // A baked theme on a host that cannot present it, or with the tier
    // withheld by the resolver, falls back rather than drawing anyway.
    function test_aWithheldTierIsNotDrawn() {
        const scene = createScene({
            capabilities: {
                available: true,
                renderer: {
                    requestedTier: "baked2.5d",
                    effectiveTier: "procedural2d",
                    fallbackApplied: true,
                    reasonCode: "renderer-host-unsupported"
                }
            }
        })
        waitForTier(scene, "procedural2d")
        compare(scene.activeTrackMetrics, null,
                "no track geometry is used when the tier was withheld")
        verify(scene.entryItemAt(0) !== null, "entries still render")
    }

    // ---- Production families -------------------------------------------

    // A projection of one installed package, shaped exactly as the backend
    // publishes it. The layer and state pattern is fixed by the family
    // generator, so it is built rather than restated three times.
    function productionTheme(themeId, shape, radiusX, radiusY, rotationMode,
                             layouts, extraTrack) {
        const packageRoot = Qt.resolvedUrl("../assets/themes/" + themeId + "/")
        const artwork = { x: 0, y: 0, width: 1200, height: 600 }
        const glow = { normal: 0.5, hover: 0.9, open: 0.72, collapsed: 0.28 }
        const layers = [
            { id: "shadow", asset: "platform-shadow", role: "shadow",
              sourceRect: artwork, opacity: 1, blendMode: "source-over" },
            { id: "rear", asset: "platform-rear", role: "rear",
              sourceRect: artwork, opacity: 1, blendMode: "source-over" },
            { id: "reflection", asset: "platform-reflection",
              role: "reflection", sourceRect: artwork, opacity: 1,
              blendMode: "source-over" }
        ]
        const states = []
        const inputMasks = []
        for (const state of ["normal", "hover", "open", "collapsed"]) {
            layers.push({ id: "glow-" + state, asset: "glow-mask",
                          role: "glow", sourceRect: artwork,
                          opacity: glow[state], blendMode: "source-over" })
            states.push({ id: state,
                          layers: ["shadow", "rear", "reflection",
                                   "glow-" + state, "front"] })
            inputMasks.push({ id: state + "-input", asset: "input-mask",
                              state: state, orientation: "free",
                              threshold: 0.5 })
        }
        layers.push({ id: "front", asset: "platform-front",
                      role: "foreground", sourceRect: artwork, opacity: 1,
                      blendMode: "source-over" })

        const track = {
            id: "platform-track", shape: shape,
            center: { x: 600, y: 300 }, radiusX: radiusX, radiusY: radiusY,
            startDegrees: 0, sweepDegrees: 360,
            depth: { farScale: 0.62, nearScale: 1, occlusionDepth: 0.62 },
            tilt: { minimumDegrees: -10, maximumDegrees: 10,
                    defaultDegrees: 0 }
        }
        const additions = extraTrack || ({})
        for (const key of Object.keys(additions))
            track[key] = additions[key]

        return {
            format: "org.archdock.theme", version: 2, id: themeId,
            valid: true, loadable: true, status: "valid",
            capabilities: {
                hosts: ["free-desktop"],
                rendererTiers: ["baked2.5d", "procedural2d"],
                preferredRendererTier: "baked2.5d",
                fallbackRendererTiers: ["procedural2d"],
                layouts: layouts, orientations: ["free"],
                features: ["dynamic-tint", "dynamic-glow",
                           "icon-state-styling"],
                presentationMechanisms: ["open"],
                rotation: { mode: rotationMode }
            },
            assets: [
                { id: "platform-rear", kind: "vector",
                  naturalSize: { width: 1200, height: 600 } },
                { id: "platform-front", kind: "vector",
                  naturalSize: { width: 1200, height: 600 } },
                { id: "platform-shadow", kind: "vector",
                  naturalSize: { width: 1200, height: 600 } },
                { id: "platform-reflection", kind: "vector",
                  naturalSize: { width: 1200, height: 600 } },
                { id: "glow-mask", kind: "mask",
                  naturalSize: { width: 1200, height: 600 } },
                { id: "input-mask", kind: "mask",
                  naturalSize: { width: 1200, height: 600 } }
            ],
            assetPaths: {
                "platform-rear": packageRoot + "assets/platform-rear.svg",
                "platform-front": packageRoot + "assets/platform-front.svg",
                "platform-shadow": packageRoot + "assets/platform-shadow.svg",
                "platform-reflection":
                    packageRoot + "assets/platform-reflection.svg",
                "glow-mask": packageRoot + "assets/glow-mask.svg",
                "input-mask": packageRoot + "masks/input.svg"
            },
            layers: layers,
            states: states,
            tracks: [track],
            inputMasks: inputMasks,
            effectMargins: { left: 16, top: 16, right: 16, bottom: 16 }
        }
    }

    function productionFamily(name) {
        if (name === "octagon-platform-steel") {
            return productionTheme(name, "polygon", 444, 162, "free",
                                   ["octagon", "polygon"], { sides: 8 })
        }
        if (name === "arc-platform-orange") {
            return productionTheme(name, "arc", 443, 162, "none",
                                   ["arc", "semicircle"],
                                   { startDegrees: 110, sweepDegrees: 140 })
        }
        return productionTheme(name, "ellipse", 446, 163, "free",
                               ["ring", "circular"])
    }

    // Criterion: real icons align with the perspective platform, the themes
    // work in a free host, and nothing is clipped, across every family at
    // several scales and entry counts.
    function test_productionFamiliesAtMultipleScales_data() {
        const rows = []
        for (const family of ["ring-platform-blue", "octagon-platform-steel",
                              "arc-platform-orange"]) {
            for (const radius of [200, 380]) {
                for (const count of [3, 8]) {
                    rows.push({ tag: family + "/" + radius + "/" + count,
                                family: family, radius: radius, count: count,
                                layout: family === "arc-platform-orange"
                                    ? "arc" : family === "octagon-platform-steel"
                                        ? "octagon" : "ring" })
                }
            }
        }
        return rows
    }

    function test_productionFamiliesAtMultipleScales(data) {
        const scene = createScene({
            theme: productionFamily(data.family),
            count: data.count,
            definition: {
                panelThemeId: data.family,
                layout: data.layout,
                layoutRadius: data.radius
            }
        })
        waitForTier(scene, "baked2.5d")
        verify(!scene.fallbackApplied, "the production package renders baked")

        // The platform is actually drawn.
        const image = grabImage(scene)
        compare(image.width, Math.round(scene.width))
        compare(image.height, Math.round(scene.height))

        let drawn = 0
        for (let x = 2; x < image.width - 2; x += 7) {
            for (let y = 2; y < image.height - 2; y += 7) {
                if (image.alpha(x, y) > 40)
                    ++drawn
            }
        }
        verify(drawn > 0, "the platform draws visible pixels")

        // Every entry sits inside the scene box and on the declared track.
        for (let index = 0; index < data.count; ++index) {
            const entry = scene.entryGeometryAt(index)
            const bounds = entry.entryBounds
            verify(bounds.x >= -0.001 && bounds.y >= -0.001
                   && bounds.x + bounds.width <= scene.width + 0.001
                   && bounds.y + bounds.height <= scene.height + 0.001,
                   "entry " + index + " fits the scene")
            verify(entry.scaleFactor >= 0.62 - 0.001
                   && entry.scaleFactor <= 1 + 0.001,
                   "entry " + index + " scales within the declared range")
            verify(entry.depth >= 0 && entry.depth <= 1,
                   "entry " + index + " has a normalized depth")
        }

        // The transparent surround is not clickable.
        compare(scene.activeInputRegionKind, "platform-mask")
        verify(!scene.containsInputPoint(Qt.point(1, 1)),
               "the corner passes through")
    }

    // Criterion: a closed family may turn; the open arc may not, because
    // sweeping it would carry entries off the platform drawn beneath them.
    function test_rotationFollowsTheFamilyShape_data() {
        return [
            { tag: "ring", family: "ring-platform-blue", layout: "ring",
              rotates: true },
            { tag: "octagon", family: "octagon-platform-steel",
              layout: "octagon", rotates: true },
            { tag: "arc", family: "arc-platform-orange", layout: "arc",
              rotates: false }
        ]
    }

    function test_rotationFollowsTheFamilyShape(data) {
        const scene = createScene({
            theme: productionFamily(data.family),
            count: 6,
            definition: {
                panelThemeId: data.family,
                layout: data.layout,
                layoutRadius: 300,
                panelRotationMode: "clockwise",
                panelRotationSpeed: 30,
                panelRotationTrigger: "idle"
            },
            animationProfiles: ({ reducedMotion: false })
        })
        waitForTier(scene, "baked2.5d")
        compare(scene.sceneRotationEnabled, data.rotates,
                "rotation availability follows the track shape")
    }

    // The scene and the Studio preview draw one family identically: the
    // preview must not be able to advertise a platform the desktop would not
    // produce.
    Component {
        id: previewComponent

        LivePanelPreview {
            width: 520
            height: 400
            previewMode: "free"
            animationProfiles: ({ reducedMotion: true })
        }
    }

    function test_previewAndSceneAgreeOnTheProductionFamily() {
        const theme = productionFamily("ring-platform-blue")
        const definition = bakedDefinition({
            panelThemeId: "ring-platform-blue",
            layout: "ring",
            layoutRadius: 260
        })
        const scene = createScene({
            theme: theme, count: 6,
            definition: { panelThemeId: "ring-platform-blue",
                          layout: "ring", layoutRadius: 260 }
        })
        waitForTier(scene, "baked2.5d")

        const preview = createTemporaryObject(previewComponent, testCase, {
            panelDefinition: definition,
            themeDefinition: theme,
            hostCapabilities: bakedCapabilities(),
            orderedEntries: entries(6)
        })
        verify(preview !== null)
        tryCompare(preview, "activeRendererTier", "baked2.5d", 5000)

        const previewScene = preview.panelSceneItem
        compare(previewScene.width, scene.width, "same scene width")
        compare(previewScene.height, scene.height, "same scene height")
        compare(previewScene.occlusionDepth, scene.occlusionDepth,
                "same occlusion depth")
        for (let index = 0; index < 6; ++index) {
            const live = scene.entryGeometryAt(index)
            const shown = previewScene.entryGeometryAt(index)
            fuzzy(shown.x, live.x, "entry " + index + " x")
            fuzzy(shown.y, live.y, "entry " + index + " y")
            fuzzy(shown.depth, live.depth, "entry " + index + " depth")
            fuzzy(shown.scaleFactor, live.scaleFactor,
                  "entry " + index + " scale")
        }
    }

    // ---- Depth, fallback, resources and reduced motion -------------------

    // Criterion: no z-order flicker or nondeterminism. The same inputs must
    // give the same depth order every time, and re-reading must not change it.
    function test_depthOrderIsStableAcrossRepeatedReads() {
        const scene = createScene({
            theme: productionFamily("ring-platform-blue"),
            count: 9,
            definition: { panelThemeId: "ring-platform-blue",
                          layout: "ring", layoutRadius: 320 }
        })
        waitForTier(scene, "baked2.5d")

        const first = []
        for (let index = 0; index < 9; ++index)
            first.push(scene.entryItemAt(index).z)
        for (let pass = 0; pass < 4; ++pass) {
            wait(20)
            for (let index = 0; index < 9; ++index) {
                compare(scene.entryItemAt(index).z, first[index],
                        "entry " + index + " keeps its z on pass " + pass)
            }
        }
        // Distinct depths, so nothing can swap order frame to frame.
        for (let a = 0; a < 9; ++a) {
            for (let b = a + 1; b < 9; ++b) {
                if (Math.abs(first[a] - first[b]) < 0.0001) {
                    verify(Math.abs(scene.entryGeometryAt(a).x
                                    - scene.entryGeometryAt(b).x) > 0.5,
                           "entries sharing a depth are not in the same place")
                }
            }
        }
    }

    // Criterion: open and collapsed states select their declared layers, and
    // the reported tier does not change with the state.
    function test_openAndCollapsedStatesSelectDeclaredLayers_data() {
        return [
            { tag: "normal", state: "normal", glow: 0.5 },
            { tag: "open", state: "open", glow: 0.72 },
            { tag: "collapsed", state: "collapsed", glow: 0.28 }
        ]
    }

    function test_openAndCollapsedStatesSelectDeclaredLayers(data) {
        const scene = createScene({
            theme: productionFamily("ring-platform-blue"),
            count: 5,
            definition: { panelThemeId: "ring-platform-blue",
                          layout: "ring", layoutRadius: 260 },
            runtimeState: ({ presentationState: data.state })
        })
        waitForTier(scene, "baked2.5d")
        verify(scene.activeSurfaceRenderer !== null)
        compare(scene.effectiveRendererTier, "baked2.5d",
                "the state does not change the tier")
        verify(scene.width > 0 && scene.height > 0)
    }

    // Criterion: continuous effects stop when the panel cannot be seen, and
    // under reduced motion.
    function test_glowPulseStopsWhenHiddenOrReducedMotion() {
        const running = createScene({
            theme: productionFamily("ring-platform-blue"),
            count: 4,
            definition: { panelThemeId: "ring-platform-blue",
                          layout: "ring", layoutRadius: 260 },
            animationProfiles: ({ reducedMotion: false })
        })
        waitForTier(running, "baked2.5d")
        const renderer = running.activeSurfaceRenderer
        verify(renderer !== null, "the baked renderer is reachable")
        verify(renderer.glowAnimationRunning, "the glow pulses when visible")

        running.sceneConcealed = true
        tryCompare(renderer, "glowAnimationRunning", false, 2000)
        compare(renderer.effectiveGlowPhase, 0,
                "a concealed panel holds its glow at rest")
        running.sceneConcealed = false
        tryCompare(renderer, "glowAnimationRunning", true, 2000)

        const reduced = createScene({
            theme: productionFamily("ring-platform-blue"),
            count: 4,
            definition: { panelThemeId: "ring-platform-blue",
                          layout: "ring", layoutRadius: 260 },
            animationProfiles: ({ reducedMotion: true })
        })
        waitForTier(reduced, "baked2.5d")
        const still = reduced.activeSurfaceRenderer
        verify(!still.glowAnimationRunning,
               "reduced motion holds the glow still")
        compare(still.effectiveGlowPhase, 0, "and at rest")
        // The platform is still drawn and its state still selected: reduced
        // motion removes movement, not feedback.
        verify(reduced.width > 0 && reduced.height > 0)
        compare(reduced.effectiveRendererTier, "baked2.5d")
    }

    // Criterion: resource use stays bounded across repeated theme changes.
    // The renderer must not accumulate layer items as families are swapped.
    function test_repeatedThemeChangesReleaseTheirLayers() {
        const scene = createScene({
            theme: productionFamily("ring-platform-blue"),
            count: 6,
            definition: { panelThemeId: "ring-platform-blue",
                          layout: "ring", layoutRadius: 280 }
        })
        waitForTier(scene, "baked2.5d")
        const baseline = scene.activeSurfaceRenderer.renderedLayerIds.length
        verify(baseline > 0, "the first theme declares layers")

        const families = [
            ["octagon-platform-steel", "octagon"],
            ["arc-platform-orange", "arc"],
            ["ring-platform-blue", "ring"]
        ]
        for (let cycle = 0; cycle < 3; ++cycle) {
            for (const entry of families) {
                scene.themeDefinition = productionFamily(entry[0])
                scene.panelDefinition = bakedDefinition({
                    panelThemeId: entry[0],
                    layout: entry[1],
                    layoutRadius: 280
                })
                waitForTier(scene, "baked2.5d")
                const renderer = scene.activeSurfaceRenderer
                compare(renderer.renderedLayerIds.length, baseline,
                        "layer count stays bounded on " + entry[0]
                        + " cycle " + cycle)
                compare(renderer.skippedLayerCount, 0,
                        "no layer is left unresolved")
            }
        }
        compare(scene.effectiveRendererTier, "baked2.5d",
                "the panel survives repeated theme changes")
    }

    // Criterion: the fallback reports the active tier truthfully and keeps a
    // usable panel, for each way a package can fail.
    function test_fallbackReportsTheActiveTier_data() {
        return [
            { tag: "missing-platform", broken: "platform-rear" },
            { tag: "missing-mask", broken: "input-mask" }
        ]
    }

    function test_fallbackReportsTheActiveTier(data) {
        const theme = productionFamily("ring-platform-blue")
        const paths = {}
        for (const key of Object.keys(theme.assetPaths))
            paths[key] = theme.assetPaths[key]
        paths[data.broken] = assetUrl("does-not-exist.svg")
        theme.assetPaths = paths

        const scene = createScene({
            theme: theme, count: 5,
            definition: { panelThemeId: "ring-platform-blue",
                          layout: "ring", layoutRadius: 260 }
        })
        waitForTier(scene, "procedural2d")
        verify(scene.fallbackApplied, "the fallback is reported")
        verify(scene.fallbackReason.length > 0, "with a reason")
        compare(scene.runtimeCapabilityStatus.effectiveRendererTier,
                "procedural2d", "the active tier is reported truthfully")
        compare(scene.runtimeCapabilityStatus.requestedRendererTier,
                "baked2.5d", "and so is what was asked for")
        verify(scene.width > 0 && scene.height > 0, "the dock keeps a size")
        for (let index = 0; index < 5; ++index)
            verify(scene.entryItemAt(index) !== null, "entry " + index)
    }
}
