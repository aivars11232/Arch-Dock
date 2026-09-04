import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// TASK-0031: the channel-to-transform mapping is pure data, so it is proved
// here without a window, a renderer or a running animation. Direction,
// composition and bounds are the three things a motion can get wrong.
TestCase {
    id: testCase

    name: "MotionChannels"

    readonly property real epsilon: 0.0001

    function fuzzy(actual, expected, message) {
        verify2(Math.abs(actual - expected) < epsilon,
                (message || "") + " expected " + expected + " got " + actual)
    }

    function verify2(condition, message) {
        if (!condition)
            fail(message)
        verify(true)
    }

    function geometryFor(layout, edge, index, count) {
        const metrics = LayoutEngine.metrics(
            layout, count, 52, 8, 1, 150, 2, 18,
            layout === "vertical", 0, 6)
        return LayoutEngine.entryGeometry(
            layout, index, count, metrics, 0, 6, "upright", "canonical", edge)
    }

    function contextFor(layout, edge) {
        const geometry = geometryFor(layout, edge, 0, 3)
        return {
            size: 60,
            normal: geometry.outwardNormal,
            tangentAngle: geometry.tangentAngle,
            allowance: null
        }
    }

    function test_noChannelsIsACompleteRest() {
        const motion = MotionChannels.motionFor({}, "icon", { size: 60 })
        compare(motion.x, 0)
        compare(motion.y, 0)
        compare(motion.scale, 1)
        compare(motion.rotateY, 0)
        compare(motion.opacity, 1)
        compare(motion.active, false)
    }

    function test_translationIsExpressedInLogicalUnits() {
        const motion = MotionChannels.motionFor(
            { "icon/translate-y": -0.22 }, "icon", { size: 60 })
        fuzzy(motion.y, -13.2, "translate-y")
        compare(motion.x, 0)
        compare(motion.active, true)
    }

    // The decisive jump check: outward is up for a bottom panel, down for a
    // top panel, right for a left panel and left for a right panel.
    function test_jumpFollowsTheOutwardNormalOfEveryNativeEdge() {
        const expected = [
            { edge: "bottom", layout: "horizontal", x: 0, y: -12 },
            { edge: "top", layout: "horizontal", x: 0, y: 12 },
            { edge: "left", layout: "vertical", x: 12, y: 0 },
            { edge: "right", layout: "vertical", x: -12, y: 0 }
        ]
        for (let index = 0; index < expected.length; ++index) {
            const item = expected[index]
            const motion = MotionChannels.motionFor(
                { "icon/translate-normal": 0.2 }, "icon",
                contextFor(item.layout, item.edge))
            fuzzy(motion.x, item.x, item.edge + " jump x")
            fuzzy(motion.y, item.y, item.edge + " jump y")
        }
    }

    // A ring entry jumps away from the ring centre, not towards a screen edge.
    function test_jumpFollowsRadialNormalsOnAClosedPath() {
        const count = 4
        for (let index = 0; index < count; ++index) {
            const geometry = geometryFor("circular", "bottom", index, count)
            const motion = MotionChannels.motionFor(
                { "icon/translate-normal": 0.2 }, "icon", {
                    size: 60,
                    normal: geometry.outwardNormal,
                    tangentAngle: geometry.tangentAngle
                })
            const centreX = geometry.bounds.width / 2
            const centreY = geometry.bounds.height / 2
            const anchorX = geometry.entryBounds.x
                + geometry.entryBounds.width / 2
            const anchorY = geometry.entryBounds.y
                + geometry.entryBounds.height / 2
            const before = Math.hypot(anchorX - centreX, anchorY - centreY)
            const after = Math.hypot(anchorX + motion.x - centreX,
                                     anchorY + motion.y - centreY)
            verify2(after > before + 6,
                    "ring entry " + index + " did not move outward")
        }
    }

    function test_shakeFollowsTheTangentOfTheRun() {
        const horizontal = MotionChannels.motionFor(
            { "icon/translate-tangent": 0.1 }, "icon",
            contextFor("horizontal", "bottom"))
        fuzzy(horizontal.x, 6, "horizontal tangent x")
        fuzzy(horizontal.y, 0, "horizontal tangent y")

        const vertical = MotionChannels.motionFor(
            { "icon/translate-tangent": 0.1 }, "icon",
            contextFor("vertical", "left"))
        fuzzy(vertical.x, 0, "vertical tangent x")
        fuzzy(vertical.y, 6, "vertical tangent y")
    }

    function test_polarOffsetCombinesAngleAndRadius() {
        const right = MotionChannels.motionFor(
            { "icon/orbit": 0, "icon/path-radius": 0.25 }, "icon", { size: 60 })
        fuzzy(right.x, 15, "orbit 0 x")
        fuzzy(right.y, 0, "orbit 0 y")

        const down = MotionChannels.motionFor(
            { "icon/orbit": 90, "icon/path-radius": 0.25 }, "icon", { size: 60 })
        fuzzy(down.x, 0, "orbit 90 x")
        fuzzy(down.y, 15, "orbit 90 y")
    }

    // Every offset is computed from absolute channel values, never accumulated,
    // so a full turn cannot leave the icon away from its anchor.
    function test_orbitAndSpiralReturnExactlyToTheAnchor() {
        for (let turn = 0; turn <= 4; ++turn) {
            const motion = MotionChannels.motionFor({
                "icon/orbit": 360 * turn,
                "icon/path-radius": 0
            }, "icon", { size: 60 })
            fuzzy(motion.x, 0, "orbit turn " + turn + " x")
            fuzzy(motion.y, 0, "orbit turn " + turn + " y")
        }
        const rested = MotionChannels.motionFor({}, "icon", { size: 60 })
        compare(rested.x, 0)
        compare(rested.y, 0)
    }

    function test_contributionsSumBeforeTheyAreClamped() {
        const motion = MotionChannels.motionFor({
            "icon/translate-y": -0.1,
            "icon/translate-normal": 0.1
        }, "icon", contextFor("horizontal", "bottom"))
        fuzzy(motion.y, -12, "summed vertical displacement")
    }

    function test_effectsAreClampedToTheDeclaredAllowance() {
        const context = contextFor("horizontal", "bottom")
        context.allowance = { left: 4, right: 4, top: 5, bottom: 5 }
        const motion = MotionChannels.motionFor({
            "icon/translate-y": -1.5,
            "icon/translate-x": 1.5
        }, "icon", context)
        compare(motion.y, -5)
        compare(motion.x, 4)
        compare(motion.clamped, true)
    }

    function test_zeroAllowanceHoldsTheEntryOnItsAnchor() {
        const context = contextFor("horizontal", "bottom")
        context.allowance = { left: 0, right: 0, top: 0, bottom: 0 }
        const motion = MotionChannels.motionFor(
            { "icon/translate-normal": 2 }, "icon", context)
        compare(motion.x, 0)
        compare(motion.y, 0)
        compare(motion.clamped, true)
    }

    function test_targetsAreIndependent() {
        const channels = {
            "glyph/rotate-y": 40,
            "tile/scale": 1.12
        }
        const glyph = MotionChannels.motionFor(channels, "glyph", { size: 60 })
        const tile = MotionChannels.motionFor(channels, "tile", { size: 60 })
        compare(glyph.rotateY, 40)
        compare(glyph.scale, 1)
        compare(tile.scale, 1.12)
        compare(tile.rotateY, 0)
    }

    // A flat card narrows as it turns and never inverts or collapses.
    function test_turnCompressionNarrowsWithoutInverting() {
        fuzzy(MotionChannels.turnCompression(0), 1, "0 degrees")
        fuzzy(MotionChannels.turnCompression(60), 0.5, "60 degrees")
        verify(MotionChannels.turnCompression(90) > 0)
        verify(MotionChannels.turnCompression(180) > 0)
        for (let angle = 0; angle <= 360; angle += 15)
            verify2(MotionChannels.turnCompression(angle) > 0,
                    "compression collapsed at " + angle)
    }

    function test_highlightFollowsTheTurn() {
        compare(MotionChannels.motionFor(
                    { "glyph/rotate-y": 0 }, "glyph", {}).highlight, 0)
        fuzzy(MotionChannels.motionFor(
                  { "glyph/rotate-y": 90 }, "glyph", {}).highlight, 1,
              "quarter turn highlight")
        fuzzy(MotionChannels.motionFor(
                  { "glyph/rotate-y": -90 }, "glyph", {}).highlight, -1,
              "reverse quarter turn highlight")
    }

    // The turn is a real perspective transform, not a flat Z spin: a resting
    // card is identity, a turned card narrows, and the two halves are no longer
    // mirror images because one edge has receded.
    function test_turnMatrixIsIdentityAtRestAndPerspectiveWhenTurned() {
        // A card lies flat at z = 0, so at rest the transform must leave every
        // point of it exactly where it was.
        const rest = MotionChannels.turnMatrix(0, 60, 60, 144)
        const probes = [[0, 0], [60, 0], [0, 60], [60, 60], [30, 30]]
        for (let index = 0; index < probes.length; ++index) {
            const point = rest.times(
                Qt.vector4d(probes[index][0], probes[index][1], 0, 1))
            fuzzy(point.w, 1, "resting w")
            fuzzy(point.x / point.w, probes[index][0], "resting x")
            fuzzy(point.y / point.w, probes[index][1], "resting y")
        }

        const turned = MotionChannels.turnMatrix(40, 60, 60, 144)
        const left = turned.times(Qt.vector4d(0, 30, 0, 1))
        const right = turned.times(Qt.vector4d(60, 30, 0, 1))
        const leftX = left.x / left.w
        const rightX = right.x / right.w
        verify2(Math.abs(left.w - right.w) > epsilon,
                "no perspective divide: both edges share a depth")
        verify2(rightX - leftX < 60,
                "card did not narrow: width " + (rightX - leftX))
        verify2(rightX - leftX > 0, "card inverted")
        // Perspective, not just compression: the receding half is the shorter
        // one, so the centre is no longer halfway between the edges.
        verify2(Math.abs((30 - leftX) - (rightX - 30)) > epsilon,
                "turn has no perspective foreshortening")
    }

    // --- neighbour influence ---------------------------------------------

    // The default curve is the historical one, so a panel that configures
    // nothing keeps exactly the magnification it had.
    function test_defaultInfluenceIsTheHistoricalCurve() {
        for (let distance = 0; distance <= 4; ++distance) {
            fuzzy(MotionChannels.magnificationInfluence(distance, 2.4, "linear"),
                  Math.max(0, 1 - distance / 2.4),
                  "distance " + distance)
        }
    }

    function test_influenceIsBoundedByItsReachInEveryFalloff() {
        const falloffs = ["linear", "cosine", "gaussian"]
        for (let index = 0; index < falloffs.length; ++index) {
            const falloff = falloffs[index]
            fuzzy(MotionChannels.magnificationInfluence(0, 3, falloff), 1,
                  falloff + " peak")
            compare(MotionChannels.magnificationInfluence(3, 3, falloff), 0)
            compare(MotionChannels.magnificationInfluence(9, 3, falloff), 0)
            // Monotonic: a further neighbour is never more influenced.
            let previous = 2
            for (let distance = 0; distance <= 3; distance += 0.25) {
                const value = MotionChannels.magnificationInfluence(
                    distance, 3, falloff)
                verify2(value <= previous + epsilon,
                        falloff + " grew at distance " + distance)
                verify2(value >= 0, falloff + " went negative")
                previous = value
            }
        }
    }

    // Deterministic: the same inputs give the same answer, every time.
    function test_influenceIsDeterministic() {
        for (let repeat = 0; repeat < 5; ++repeat) {
            fuzzy(MotionChannels.magnificationInfluence(1.5, 2.4, "cosine"),
                  MotionChannels.magnificationInfluence(1.5, 2.4, "cosine"),
                  "cosine repeat")
            fuzzy(MotionChannels.magnificationInfluence(1, 4, "gaussian"),
                  MotionChannels.magnificationInfluence(1, 4, "gaussian"),
                  "gaussian repeat")
        }
        // A wider reach influences a given neighbour at least as much.
        verify(MotionChannels.magnificationInfluence(2, 4, "linear")
               > MotionChannels.magnificationInfluence(2, 2.4, "linear"))
    }

    function test_magnificationScaleRestsAtOne() {
        compare(MotionChannels.magnificationScale(1.7, 0), 1)
        fuzzy(MotionChannels.magnificationScale(1.7, 1), 1.7, "peak")
        fuzzy(MotionChannels.magnificationScale(1.7, 0.5), 1.35, "half")
        // A magnification below 1 cannot shrink an icon.
        compare(MotionChannels.magnificationScale(0.4, 1), 1)
    }

    function test_maximumDisplacementReservesHeadroomForTranslations() {
        const profile = {
            id: "jump",
            tracks: [
                { property: "translate-normal", from: 0, to: -0.3 },
                { property: "rotate-z", from: -20, to: 20 }
            ]
        }
        // A path motion reserves room for its radius too.
        fuzzy(MotionChannels.maximumDisplacement([{
            id: "spiral",
            tracks: [
                { property: "spiral", from: 0, to: 360 },
                { property: "path-radius", from: 0, to: 0.18 }
            ]
        }], 1), 0.18, "path radius headroom")
        fuzzy(MotionChannels.maximumDisplacement([profile], 1), 0.3, "full")
        fuzzy(MotionChannels.maximumDisplacement([profile], 0.5), 0.15, "half")
        compare(MotionChannels.maximumDisplacement([], 1), 0)
        compare(MotionChannels.maximumDisplacement(
                    [{ id: "broken", valid: false,
                       tracks: [{ property: "translate-y", from: 0, to: -9 }] }],
                    1), 0)
    }
}
