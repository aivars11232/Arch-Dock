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
