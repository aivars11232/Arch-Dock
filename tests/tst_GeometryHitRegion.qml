import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// TASK-0033 Phase C: the active input region of a free radial scene is the
// band the surface draws plus the entries themselves. The empty interior and
// the corners of the rectangle pass through.
TestCase {
    name: "GeometryHitRegion"

    Component {
        id: regionComponent

        GeometryHitRegion {}
    }

    function ringGeometry() {
        return LayoutEngine.metrics(
            "ring", 4, 40, 8, 1, 120, 2, 12, false, 0, 6)
    }

    function makeRegion(properties) {
        const region = createTemporaryObject(regionComponent, this,
                                             properties || ({}))
        verify(region, "region created")
        return region
    }

    function test_ringBandAndEntriesAcceptWhileTheInteriorPassesThrough() {
        const geometry = ringGeometry()
        const center = { x: geometry.width / 2, y: geometry.height / 2 }
        const region = makeRegion({
            layout: "ring", geometry: geometry, angle: 0, polygonSides: 6,
            bandWidth: 48, entryMargin: 4,
            entryRects: [{ x: center.x - 20, y: 12, width: 40, height: 40 }]
        })

        compare(region.contains(Qt.point(center.x, center.y)), false,
                "ring interior passes through")
        compare(region.contains(Qt.point(1, 1)), false, "corner passes through")
        compare(region.contains(Qt.point(center.x + geometry.radius, center.y)),
                true, "a point on the ring accepts")
        compare(region.contains(Qt.point(center.x + geometry.radius + 20,
                                         center.y)),
                true, "a point inside the band accepts")
        compare(region.contains(Qt.point(center.x + geometry.radius + 40,
                                         center.y)),
                false, "a point beyond the band passes through")
        compare(region.contains(Qt.point(center.x, 30)), true,
                "an entry accepts")
        compare(region.contains(Qt.point(center.x - 22, 10)), true,
                "the entry margin accepts")
        compare(region.contains(Qt.point(NaN, 10)), false,
                "a non-finite point never accepts")
    }

    function test_regionTurnsWithTheAngle() {
        const geometry = ringGeometry()
        const center = { x: geometry.width / 2, y: geometry.height / 2 }
        const arcGeometry = LayoutEngine.rotationEnvelope(LayoutEngine.metrics(
            "arc", 4, 40, 8, 1, 120, 2, 12, false, 0, 6))
        const arcCenter = { x: arcGeometry.width / 2, y: arcGeometry.height / 2 }
        const upright = makeRegion({ layout: "arc", geometry: arcGeometry,
                                     angle: 0, bandWidth: 48, entryRects: [] })
        const turned = makeRegion({ layout: "arc", geometry: arcGeometry,
                                    angle: 180, bandWidth: 48, entryRects: [] })
        // At rest the arc curves over the top of its circle like a rainbow, so
        // its apex sits above the centre; turned by 180 degrees it hangs below.
        const low = Qt.point(arcCenter.x, arcCenter.y + arcGeometry.radius)
        const high = Qt.point(arcCenter.x, arcCenter.y - arcGeometry.radius)
        compare(upright.contains(high), true, "the apex is on the band at rest")
        compare(upright.contains(low), false, "nothing is drawn below the centre at rest")
        compare(turned.contains(high), false, "turned by 180, the band moved away")
        compare(turned.contains(low), true, "and now hangs below the centre")
        verify(center.x > 0)
    }

    // ---- ADREP-TASK-002: hits follow travelled entries -------------------

    Component {
        id: sceneComponent

        PanelScene {
            orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                return { id: "entry-" + index, displayName: "Entry " + index }
            })
            panelDefinition: ({ edge: "free", layout: "arc", layoutRadius: 120, iconSize: 40,
                                spacing: 8, layoutPadding: 12, rendererTier: "procedural2d",
                                panelRotationMode: "none", panelMotionTarget: "items" })
            runtimeState: ({ hovered: false, hoveredEntry: -1, editMode: false,
                             dragInProgress: false, rendererFallback: "" })
            hostCapabilities: ({ available: true,
                                 renderer: { effectiveTier: "procedural2d", fallbackApplied: false },
                                 rotation: { available: true, support: "arbitrary" } })
            entryDelegateContext: ({ hostKind: "free" })
            rotationAnimationEnabled: false
        }
    }

    function centreOf(rect) {
        return Qt.point(rect.x + rect.width / 2, rect.y + rect.height / 2)
    }

    // The region is given the travelled entries' rectangles, as the scene
    // builds them: each entry takes its hits where it went, and the rest of
    // the band still takes the wheel.
    function test_travelledEntryRectanglesTakeTheirHits() {
        const geometry = LayoutEngine.rotationEnvelope(LayoutEngine.metrics(
            "arc", 6, 40, 8, 1, 120, 2, 12, false, 0, 6))
        function rects(travel) {
            const travelled = Object.assign({}, geometry, { travel: travel })
            const result = []
            for (let index = 0; index < 6; ++index) {
                const entry = LayoutEngine.entryGeometry("arc", index, 6, travelled, 0, 6,
                                                         "upright", "live")
                result.push(entry.onTrack ? entry.entryBounds
                                          : { x: -100000, y: -100000, width: 0, height: 0 })
            }
            return result
        }
        const resting = rects(0)
        const moved = rects(1)
        const region = makeRegion({ layout: "arc", geometry: geometry, angle: 0,
                                    bandWidth: 1, entryMargin: 0, entryRects: moved })
        for (let index = 0; index < 5; ++index) {
            const centre = centreOf(moved[index])
            compare(region.contains(centre), true, "entry " + index + " takes its hits where it went")
            const neighbour = centreOf(resting[index + 1])
            fuzzyCompare(centre.x, neighbour.x, 0.01)
            fuzzyCompare(centre.y, neighbour.y, 0.01)
        }
        compare(moved[5].width, 0, "the entry that left the end has no rectangle")
        // Only the entry rectangles take hits here (a one-pixel band): the
        // slot the entries moved out of is free.
        compare(region.contains(Qt.point(centreOf(resting[0]).x - 15, centreOf(resting[0]).y - 15)),
                false, "the vacated first slot passes through")
    }

    // The scene itself: entries hidden by travel take no input, and the
    // travelled ones take it where they are drawn.
    function test_sceneHitsFollowTravel() {
        const scene = createTemporaryObject(sceneComponent, this)
        verify(scene, "scene created")
        compare(scene.activeInputRegionKind, "geometry-band")
        compare(scene.trackWindow.loop, 7, "six entries that fit travel round a loop of seven")
        scene.wheelTravel = 1
        let hidden = 0
        for (let index = 0; index < scene.entryCount; ++index) {
            const entry = scene.entryGeometryAt(index)
            const rect = scene.entryRects[index]
            const item = scene.entryItemAt(index)
            if (!entry.onTrack) {
                ++hidden
                compare(rect.width, 0, "a hidden entry has no rectangle")
                compare(item.enabled, false, "a hidden entry takes no input")
                compare(item.visible, false, "a hidden entry is not drawn")
                continue
            }
            const centre = centreOf(entry.entryBounds)
            verify(scene.containsInputPoint(centre), "entry " + index + " takes input where it is")
            verify(centre.x >= rect.x && centre.x <= rect.x + rect.width
                   && centre.y >= rect.y && centre.y <= rect.y + rect.height)
            compare(item.enabled, true)
        }
        compare(hidden, 1, "one entry waits off the path")
    }

    function test_disabledRegionAcceptsEverything() {
        const region = makeRegion({ layout: "ring", geometry: ringGeometry(),
                                    enabled: false })
        compare(region.contains(Qt.point(0, 0)), true)
        compare(region.contains(Qt.point(-50, -50)), true)
    }

    function test_degenerateGeometryDoesNotThrow() {
        const region = makeRegion({ layout: "spiral", geometry: ({}),
                                    angle: NaN, bandWidth: NaN, entryRects: null })
        const result = region.contains(Qt.point(10, 10))
        verify(result === true || result === false)
    }
}
