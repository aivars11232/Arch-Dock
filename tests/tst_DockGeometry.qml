import QtQuick 2.15
import QtTest 1.3
import ArchDock.Rendering 1.0

TestCase {
    name: "DockGeometry"

    function geometry(layout, count, angle) {
        return LayoutEngine.metrics(
            layout, count, 40, 8, 1, 120, 2, 12, false,
            angle === undefined ? 0 : angle, 6)
    }

    function livePosition(layout, index, count, value, angle, orientation) {
        return LayoutEngine.position(
            layout, index, count, value, angle || 0, 6,
            orientation || "upright", "live")
    }

    function runtimePosition(layout, index, count, value, angle, orientation) {
        return LayoutEngine.position(
            layout, index, count, value, angle || 0, 6,
            orientation || "upright", "runtime")
    }

    function fuzzy(actual, expected, message) {
        verify(Math.abs(actual - expected) <= 0.0001,
               message + ": actual=" + actual + " expected=" + expected)
    }

    function verifyPointInside(point, value, message) {
        fuzzy(Math.max(0, point.x), point.x, message + " x lower bound")
        fuzzy(Math.max(0, point.y), point.y, message + " y lower bound")
        verify(point.x + value.iconSize <= value.width + 0.0001,
               message + " x upper bound")
        verify(point.y + value.iconSize <= value.height + 0.0001,
               message + " y upper bound")
    }

    function test_supportedLayoutsAreDeterministicAndBounded() {
        const layouts = [
            "horizontal", "vertical", "ring", "arc", "polygon", "fan",
            "spiral"
        ]
        for (const layout of layouts) {
            for (const count of [0, 1, 6]) {
                const value = geometry(layout, count)
                compare(JSON.stringify(geometry(layout, count)),
                        JSON.stringify(value), layout + " deterministic metrics")
                verify(value.width >= value.iconSize)
                verify(value.height >= value.iconSize)
                for (let index = 0; index < count; ++index) {
                    const point = livePosition(layout, index, count, value)
                    compare(JSON.stringify(
                                livePosition(layout, index, count, value)),
                            JSON.stringify(point),
                            layout + " deterministic position")
                    verifyPointInside(point, value,
                                      layout + "[" + index + "/" + count + "]")
                }
            }
        }
    }

    function test_frozenLegacyCompatibilityProfiles() {
        const polygon = LayoutEngine.metrics(
            "polygon", 4, 40, 8, 1, 120, 2, 12, false, 0, 4)
        compare(polygon.width, 304)
        compare(polygon.height, 304)

        const liveUpright = LayoutEngine.position(
            "polygon", 0, 4, polygon, 0, 4, "upright", "live")
        const liveTangent = LayoutEngine.position(
            "polygon", 0, 4, polygon, 0, 4, "tangent", "live")
        const runtimeTangent = LayoutEngine.position(
            "polygon", 0, 4, polygon, 0, 4, "tangent", "runtime")
        const runtimeRadial = LayoutEngine.position(
            "polygon", 0, 4, polygon, 0, 4, "radial", "runtime")
        compare(Math.round(liveUpright.x), 132)
        compare(Math.round(liveUpright.y), 12)
        compare(liveTangent.rotation, liveUpright.rotation)
        compare(Math.round(runtimeTangent.rotation), 45)
        compare(Math.round(runtimeRadial.rotation), -90)

        const arc = geometry("arc", 6)
        const liveArc = livePosition("arc", 2, 6, arc)
        const runtimeArc = runtimePosition("arc", 2, 6, arc)
        fuzzy(runtimeArc.y - liveArc.y, -arc.iconSize * 0.06,
              "legacy runtime arc offset")

        const fan = geometry("fan", 6)
        const liveFan = livePosition("fan", 2, 6, fan)
        const runtimeFan = runtimePosition("fan", 2, 6, fan)
        fuzzy(runtimeFan.y - liveFan.y, fan.iconSize * 0.10,
              "legacy runtime fan offset")

        const singleFan = geometry("fan", 1, 17)
        compare(runtimePosition(
                    "fan", 0, 1, singleFan, 17, "tangent").rotation, 17)
        compare(livePosition(
                    "fan", 0, 1, singleFan, 17, "tangent").rotation, 0)
    }

    function test_linearClosedAndSingleEntryBoundaries() {
        const horizontal = geometry("horizontal", 3)
        const horizontalFirst = livePosition(
            "horizontal", 0, 3, horizontal)
        const horizontalLast = livePosition(
            "horizontal", 2, 3, horizontal)
        compare(horizontalFirst.x, horizontal.padding)
        compare(horizontalFirst.y, horizontal.padding)
        compare(horizontalLast.x,
                horizontal.padding
                + 2 * (horizontal.iconSize + horizontal.spacing))

        const vertical = geometry("vertical", 3)
        const verticalFirst = livePosition("vertical", 0, 3, vertical)
        const verticalLast = livePosition("vertical", 2, 3, vertical)
        compare(verticalFirst.x, vertical.padding)
        compare(verticalFirst.y, vertical.padding)
        compare(verticalLast.y,
                vertical.padding + 2 * (vertical.iconSize + vertical.spacing))

        const ring = geometry("ring", 1)
        const only = livePosition("ring", 0, 1, ring)
        fuzzy(only.x + ring.iconSize / 2, ring.width / 2,
              "single ring entry x")
        fuzzy(only.y, ring.padding, "single ring entry y")

        const zero = geometry("horizontal", 0)
        const emptyResult = LayoutEngine.entryGeometry(
            "horizontal", 0, 0, zero, 0, 6, "upright", "canonical")
        compare(emptyResult.pathProgress, 0.5)
        verifyPointInside(emptyResult.position, zero, "zero-entry safe result")
    }

    function test_outputContractIsComplete() {
        const value = geometry("polygon", 4)
        const result = LayoutEngine.entryGeometry(
            "polygon", 1, 4, value, 0, 4, "tangent", "canonical")
        const repeated = LayoutEngine.entryGeometry(
            "polygon", 1, 4, value, 0, 4, "tangent", "canonical")
        compare(JSON.stringify(repeated), JSON.stringify(result))
        compare(result.position.x, result.x)
        compare(result.position.y, result.y)
        verify(isFinite(result.tangentAngle))
        fuzzy(Math.sqrt(result.outwardNormal.x * result.outwardNormal.x
                        + result.outwardNormal.y * result.outwardNormal.y),
              1, "normalized outward normal")
        verify(isFinite(result.outwardNormal.angle))
        compare(result.depthOrder,
                result.position.y + value.iconSize / 2)
        compare(result.scaleFactor, 1)
        compare(result.pathProgress, 0.25)
        compare(result.panelBounds.x, 0)
        compare(result.panelBounds.y, 0)
        compare(result.panelBounds.width, value.width)
        compare(result.panelBounds.height, value.height)
        compare(result.bounds.width, value.width)
        compare(result.safeInputRegion.width, value.width)
        compare(result.entryBounds.x, result.position.x)
        compare(result.entryBounds.y, result.position.y)
        compare(result.position3D, null)
        compare(result.orientation3D, null)
    }

    function test_surfaceAndThemeCompatibilityContract() {
        const hexagon = LayoutEngine.surface(
            "hexagon", geometry("hexagon", 6), 0, 6)
        compare(hexagon.closed, true)
        compare(hexagon.points.length, 6)

        const arc = LayoutEngine.surface("arc", geometry("arc", 6), 0, 6)
        compare(arc.closed, false)
        verify(arc.points.length > 20)

        const glass = LayoutEngine.themeStyle("glass", "", 52)
        const floating = LayoutEngine.themeStyle("floating-glass", "", 52)
        const plate = LayoutEngine.themeStyle("plate", "", 52)
        const pedestal = LayoutEngine.themeStyle("pedestal", "", 52)
        verify(glass.trackVisible)
        verify(floating.trackVisible)
        verify(!plate.trackVisible)
        verify(!pedestal.trackVisible)
        verify(glass.stroke !== floating.stroke)
        verify(glass.lineWidth !== plate.lineWidth)
        compare(LayoutEngine.themeStyle(
                    "glass", "#123456", 52).stroke, "#123456")
    }

    readonly property var pathLayouts: [
        "horizontal", "vertical", "diagonal", "circular", "ring", "ellipse",
        "radial", "polygon", "triangle", "square", "pentagon", "hexagon",
        "octagon", "star", "arc", "semicircle", "fan", "spiral", "ribbon",
        "horizontal-curve", "vertical-curve", "grid", "floating"
    ]

    function collectNonFinite(value, path, problems) {
        if (value === null || value === undefined)
            return
        if (typeof value === "number") {
            if (!isFinite(value))
                problems.push(path)
            return
        }
        if (typeof value === "object") {
            for (const key of Object.keys(value))
                collectNonFinite(value[key], path + "." + key, problems)
        }
    }

    // TASK-0033 Phase B: no layout, entry count or hostile numeric input may
    // yield a non-finite coordinate, a zero-sized panel, a normal that is not
    // a unit vector, or an entry outside its unrotated panel.
    function test_degenerateInputsNeverProduceNonFiniteGeometry_data() {
        const hostile = [
            { tag: "nan", iconSize: NaN, spacing: NaN, scale: NaN, radius: NaN,
              rows: NaN, padding: NaN, angle: NaN, sides: NaN, inside: true },
            { tag: "infinite", iconSize: 40, spacing: 8, scale: 1,
              radius: Infinity, rows: 2, padding: 12, angle: Infinity, sides: 6,
              inside: true },
            { tag: "zero-negative", iconSize: 0, spacing: -4, scale: 0,
              radius: -50, rows: 0, padding: -3, angle: 0, sides: 1,
              inside: true },
            { tag: "huge-rotated", iconSize: 4096, spacing: 500, scale: 2.5,
              radius: 1e6, rows: 99, padding: 240, angle: 1e6, sides: 99,
              inside: false },
            { tag: "strings", iconSize: "48", spacing: undefined, scale: "x",
              radius: "150", rows: "2", padding: null, angle: "12", sides: "6",
              inside: false }
        ]
        const rows = []
        for (const layout of pathLayouts) {
            for (const count of [0, 1, 7, 24]) {
                for (const inputs of hostile) {
                    const row = Object.assign({}, inputs)
                    row.tag = layout + "/" + count + "/" + inputs.tag
                    row.layout = layout
                    row.count = count
                    rows.push(row)
                }
            }
        }
        return rows
    }

    function test_degenerateInputsNeverProduceNonFiniteGeometry(data) {
        const value = LayoutEngine.metrics(
            data.layout, data.count, data.iconSize, data.spacing, data.scale,
            data.radius, data.rows, data.padding, false, data.angle, data.sides)
        const problems = []
        collectNonFinite(value, "metrics", problems)
        compare(problems.join(","), "", "metrics carry a non-finite value")
        verify(value.width >= 1 && value.height >= 1, "panel has a size")
        verify(value.iconSize >= 16, "icon size floor")

        const surface = LayoutEngine.surface(
            data.layout, value, data.angle, data.sides)
        collectNonFinite(surface.points, "surface", problems)
        compare(problems.join(","), "", "surface carries a non-finite point")
        verify(surface.points.length >= 3, "surface has a path")

        for (let index = 0; index < Math.max(1, data.count); ++index) {
            const result = LayoutEngine.entryGeometry(
                data.layout, index, data.count, value, data.angle, data.sides,
                "upright", "canonical")
            collectNonFinite(result, "entry[" + index + "]", problems)
            compare(problems.join(","), "", "entry carries a non-finite value")
            verify(result.pathProgress >= 0 && result.pathProgress <= 1,
                   "progress stays in [0, 1]")
            fuzzy(Math.hypot(result.outwardNormal.x, result.outwardNormal.y), 1,
                  "outward normal is a unit vector")
            compare(result.rotation, 0, "upright entry has no rotation")
            if (data.inside)
                verifyPointInside(result.position, value,
                                  data.layout + "[" + index + "]")
        }
    }

    // Entry order follows index, positions are distinct, open and closed
    // paths advance in one direction, and the same input always yields the
    // same output.
    function test_entryOrderAndPathDirectionAreDeterministic_data() {
        return [
            { tag: "ring", layout: "ring", monotonic: true },
            { tag: "arc", layout: "arc", monotonic: true },
            { tag: "semicircle", layout: "semicircle", monotonic: true },
            { tag: "fan", layout: "fan", monotonic: true },
            { tag: "spiral", layout: "spiral", monotonic: true },
            { tag: "polygon", layout: "polygon", monotonic: false },
            { tag: "star", layout: "star", monotonic: false }
        ]
    }

    function test_entryOrderAndPathDirectionAreDeterministic(data) {
        const count = 6
        const value = geometry(data.layout, count)
        let previousProgress = -1
        let previousAngle = -Infinity
        const seen = []
        for (let index = 0; index < count; ++index) {
            const result = LayoutEngine.entryGeometry(
                data.layout, index, count, value, 0, 6, "upright", "canonical")
            verify(result.pathProgress > previousProgress,
                   data.layout + " progress increases with index")
            previousProgress = result.pathProgress
            const key = result.x.toFixed(3) + "," + result.y.toFixed(3)
            verify(!seen.includes(key), data.layout + " positions are distinct")
            seen.push(key)
            if (data.monotonic) {
                verify(result.outwardNormal.angle > previousAngle,
                       data.layout + " advances in one direction")
                previousAngle = result.outwardNormal.angle
            }
            const again = LayoutEngine.entryGeometry(
                data.layout, index, count, value, 0, 6, "upright", "canonical")
            compare(JSON.stringify(again), JSON.stringify(result))
        }
    }

    // Upright means upright for the canonical and live scenes even on paths
    // that carry a decorative tilt; tangent follows the path exactly.
    function test_configuredUprightIconsHaveNoRotation_data() {
        return [
            { tag: "ring", layout: "ring" },
            { tag: "arc", layout: "arc" },
            { tag: "fan", layout: "fan" },
            { tag: "spiral", layout: "spiral" },
            { tag: "ribbon", layout: "ribbon" },
            { tag: "floating", layout: "floating" },
            { tag: "ellipse", layout: "ellipse" }
        ]
    }

    function test_configuredUprightIconsHaveNoRotation(data) {
        const count = 5
        const value = geometry(data.layout, count, 23)
        for (let index = 0; index < count; ++index) {
            for (const profile of ["canonical", "live"]) {
                const upright = LayoutEngine.entryGeometry(
                    data.layout, index, count, value, 23, 6, "upright", profile)
                compare(upright.rotation, 0,
                        data.layout + "/" + profile + " upright rotation")
            }
            const tangent = LayoutEngine.entryGeometry(
                data.layout, index, count, value, 23, 6, "tangent", "canonical")
            fuzzy(tangent.rotation, tangent.tangentAngle,
                  data.layout + " tangent follows the path")
        }
    }

    // The entry path and the surface drawn under it come from one sweep table,
    // so the first and last entries sit exactly on the surface's end points.
    function test_openPathsShareOneSweepWithTheirSurface_data() {
        return [
            { tag: "arc", layout: "arc" },
            { tag: "semicircle", layout: "semicircle" },
            { tag: "fan", layout: "fan" }
        ]
    }

    function test_openPathsShareOneSweepWithTheirSurface(data) {
        const count = 7
        const value = geometry(data.layout, count)
        const surface = LayoutEngine.surface(data.layout, value, 0, 6)
        const first = LayoutEngine.entryGeometry(
            data.layout, 0, count, value, 0, 6, "upright", "canonical")
        const last = LayoutEngine.entryGeometry(
            data.layout, count - 1, count, value, 0, 6, "upright", "canonical")
        const startPoint = surface.points[0]
        const endPoint = surface.points[surface.points.length - 1]
        fuzzy(first.x + value.iconSize / 2, startPoint.x, data.layout + " start x")
        fuzzy(first.y + value.iconSize / 2, startPoint.y, data.layout + " start y")
        fuzzy(last.x + value.iconSize / 2, endPoint.x, data.layout + " end x")
        fuzzy(last.y + value.iconSize / 2, endPoint.y, data.layout + " end y")
    }

    function test_compatibilityUtilitiesRemainAvailable() {
        const value = geometry("horizontal", 3)
        compare(LayoutEngine.nearestIndex(
                    "horizontal", 3, value, 0,
                    value.padding + value.iconSize + value.spacing
                    + value.iconSize / 2,
                    value.padding + value.iconSize / 2,
                    6, "upright", "runtime"), 1)

        const offset = LayoutEngine.anchorOffset(
            "bottom-right", 500, 420,
            LayoutEngine.metrics(
                "polygon", 4, 40, 8, 1, 120, 2, 12, false, 0, 4))
        compare(offset.x, 196)
        compare(offset.y, 116)

        const expansion = LayoutEngine.expansionOffset(
            "horizontal", 2, 5, 40, 8, 120, 2)
        compare(expansion.x, 0)
        compare(expansion.y, 0)
    }
}
