import QtQuick 2.15
import QtTest 1.3
import ArchDock.Rendering 1.0

TestCase {
    name: "DockGeometry"

    function test_segmentGeometryKeepsIndependentBounds() {
        const definitions = [
            { id: "second", order: 1, padding: 20, spacing: 14 },
            { id: "first", order: 0, padding: 4, spacing: 2 }
        ]
        const entries = [{ segmentId: "first" }, { segmentId: "second" }, { segmentId: "second" }]
        for (const vertical of [false, true]) {
            const layout = vertical ? "vertical" : "horizontal"
            const result = LayoutEngine.segmentGeometry(definitions, entries,
                layout, 40, 8, 1, 12, vertical, 0, "upright", "canonical", "bottom")
            compare(result.segments[0].id, "first")
            compare(result.segments[1].id, "second")
            compare(result.entries[0].segmentId, "first")
            compare(result.entries[1].segmentId, "second")
            const axis = vertical ? "y" : "x"
            const extent = vertical ? "height" : "width"
            compare(result.segments[0][extent], 48)
            compare(result.segments[1][extent], 134)
            compare(result.segments[1][axis], 56)
            compare(result.geometry[extent], 190)
            compare(result.entries[2].position[axis] - result.entries[1].position[axis], 54)
            for (const entry of result.entries) {
                verify(entry.x >= entry.panelBounds.x)
                verify(entry.y >= entry.panelBounds.y)
                verify(entry.x + 40 <= entry.panelBounds.x + entry.panelBounds.width)
                verify(entry.y + 40 <= entry.panelBounds.y + entry.panelBounds.height)
            }
        }
    }

    function test_folderExpansionBoundsAndFallback() {
        for (const layout of ["fan", "grid", "stack", "arc", "ring"]) {
            for (const count of [0, 1, 8, 48]) {
                const value = LayoutEngine.expansionGeometry(layout, count, 48, 8, 120, 6)
                compare(value.count, count)
                compare(value.layout, layout)
                verify(!value.fallbackApplied)
                compare(JSON.stringify(value), JSON.stringify(
                    LayoutEngine.expansionGeometry(layout, count, 48, 8, 120, 6)))
                for (const point of value.entries)
                    verifyPointInside(point, value, layout + "/" + count)
                if (layout === "stack" && count > 1) {
                    verify(value.entries[1].x - value.entries[0].x >= 24,
                           "each stacked child retains an exposed pointer target")
                }
            }
        }
        const invalid = LayoutEngine.expansionGeometry("legacy", Infinity, NaN, -5, Infinity, 0)
        compare(invalid.layout, "fan")
        verify(invalid.fallbackApplied)
        compare(invalid.fallbackReason, "unsupported-folder-layout")
        verify(isFinite(invalid.width) && isFinite(invalid.height))
        const bounded = LayoutEngine.expansionGeometry("grid", 500, 999, 999, -5, NaN)
        compare(bounded.count, 48)
        compare(bounded.iconSize, 128)
        for (const point of bounded.entries)
            verifyPointInside(point, bounded, "bounded")
        compare(JSON.stringify(LayoutEngine.expansionOffset("ring", 2, 8, 48, 8, 120, 3)),
                JSON.stringify(LayoutEngine.expansionOffset("circular", 2, 8, 48, 8, 120, 3)))
    }

    function test_folderNamesAndDensePagesFitOneScrollingAxis() {
        for (const layout of ["fan", "grid"]) {
            for (const maximumWidth of [160, 620]) {
                const value = LayoutEngine.expansionGeometry(layout, 48, 56, 12, 140, 7,
                    { maximumWidth: maximumWidth, labelWidth: 112, labelHeight: 36 })
                verify(value.width <= maximumWidth)
                compare(value.iconSize, 56, "glyphs retain their full size")
                verify(value.height > 420)
                for (const point of value.entries) {
                    verify(point.x >= 0 && point.x + value.cellWidth <= value.width)
                    verify(point.y >= 0 && point.y + value.cellHeight <= value.height)
                }
                if (layout === "fan") {
                    for (let i = 1; i < value.entries.length; ++i)
                        verify(value.entries[i].y - value.entries[i - 1].y >= value.cellHeight)
                }
            }
        }
    }

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

    // ---- Baked 2.5D anchor tracks -------------------------------------

    function ringTrack(overrides) {
        const result = {
            id: "ring-track",
            shape: "ellipse",
            center: { x: 600, y: 300 },
            radiusX: 450,
            radiusY: 160,
            startDegrees: 0,
            sweepDegrees: 360,
            sides: 8,
            depth: { farScale: 0.6, nearScale: 1.0, occlusionDepth: 0.5 },
            tilt: { minimumDegrees: -12, maximumDegrees: 12, defaultDegrees: 0 }
        }
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            result[key] = additions[key]
        return result
    }

    function trackMetrics(track, count, radius, tilt) {
        return LayoutEngine.trackMetrics(
            track, 1200, 600, count, 48, 10,
            radius === undefined ? 450 : radius, tilt)
    }

    // Criterion: depth scale and order are deterministic, and an icon's size
    // follows how far down the perspective path it sits.
    function test_trackDepthScaleAndOrderAreDeterministic() {
        const track = ringTrack()
        const count = 8
        const metrics = trackMetrics(track, count)
        let previousDepth = -1
        const first = LayoutEngine.trackEntryGeometry(
            track, 0, count, metrics, 0, "upright")
        compare(first.depth, 0, "the first entry sits at the far edge")
        fuzzy(first.scaleFactor, 0.6, "far entries take the declared far scale")

        for (let index = 0; index < count; ++index) {
            const entry = LayoutEngine.trackEntryGeometry(
                track, index, count, metrics, 0, "upright")
            const repeat = LayoutEngine.trackEntryGeometry(
                track, index, count, metrics, 0, "upright")
            compare(entry.depth, repeat.depth, "depth is repeatable")
            compare(entry.x, repeat.x, "position is repeatable")
            verify(entry.depth >= 0 && entry.depth <= 1, "depth stays normalized")
            verify(entry.scaleFactor >= 0.6 - 0.0001
                   && entry.scaleFactor <= 1 + 0.0001,
                   "scale stays inside the declared range")
            compare(entry.depthOrder, entry.depth, "depth drives draw order")
            compare(entry.inFront, entry.depth >= 0.5,
                    "occlusion depth decides which side an entry passes")
            compare(entry.rotation, 0, "an upright entry has no rotation")
            if (index <= count / 2)
                verify(entry.depth >= previousDepth, "depth advances downwards")
            previousDepth = entry.depth
        }

        const nearest = LayoutEngine.trackEntryGeometry(
            track, count / 2, count, metrics, 0, "upright")
        compare(nearest.depth, 1, "the opposite entry sits at the near edge")
        fuzzy(nearest.scaleFactor, 1, "near entries take the declared near scale")
        verify(nearest.entryBounds.width > first.entryBounds.width,
               "a near icon is drawn larger than a far one")
    }

    // Criterion: every entry and the platform fit inside the reported box, at
    // several radii and entry counts.
    function test_trackSceneBoxContainsPlatformAndEveryEntry_data() {
        const rows = []
        for (const count of [0, 1, 3, 12, 24]) {
            for (const radius of [120, 450, 900]) {
                rows.push({ tag: count + "@" + radius,
                            count: count, radius: radius })
            }
        }
        return rows
    }

    function test_trackSceneBoxContainsPlatformAndEveryEntry(data) {
        const track = ringTrack()
        const metrics = trackMetrics(track, data.count, data.radius)
        verify(metrics.width >= 1 && metrics.height >= 1, "the box has a size")
        verify(metrics.platform.x >= -0.0001
               && metrics.platform.y >= -0.0001
               && metrics.platform.x + metrics.platform.width
                   <= metrics.width + 0.0001
               && metrics.platform.y + metrics.platform.height
                   <= metrics.height + 0.0001,
               "the drawn platform fits the box")
        for (let index = 0; index < data.count; ++index) {
            const entry = LayoutEngine.trackEntryGeometry(
                track, index, data.count, metrics, 0, "upright")
            const bounds = entry.entryBounds
            verify(bounds.x >= -0.0001 && bounds.y >= -0.0001
                   && bounds.x + bounds.width <= metrics.width + 0.0001
                   && bounds.y + bounds.height <= metrics.height + 0.0001,
                   "entry " + index + " fits the box")
        }
    }

    // Criterion: hostile inputs never produce non-finite geometry.
    function test_trackDegenerateInputsStayFinite_data() {
        return [
            { tag: "nan", track: { shape: "ellipse", center: { x: NaN, y: NaN },
                                   radiusX: NaN, radiusY: NaN,
                                   startDegrees: NaN, sweepDegrees: NaN,
                                   sides: NaN,
                                   depth: { farScale: NaN, nearScale: NaN,
                                            occlusionDepth: NaN } },
              count: 5, radius: NaN },
            { tag: "infinite", track: { shape: "arc", center: { x: 0, y: 0 },
                                        radiusX: Infinity, radiusY: Infinity,
                                        startDegrees: Infinity,
                                        sweepDegrees: Infinity, sides: 8,
                                        depth: { farScale: 0.5, nearScale: 1,
                                                 occlusionDepth: 0.5 } },
              count: 3, radius: Infinity },
            { tag: "zero-negative", track: { shape: "polygon",
                                             center: { x: -10, y: -10 },
                                             radiusX: 0, radiusY: -5,
                                             startDegrees: 0, sweepDegrees: 0,
                                             sides: 1,
                                             depth: { farScale: -1,
                                                      nearScale: -2,
                                                      occlusionDepth: -3 } },
              count: 4, radius: -50 },
            { tag: "strings", track: { shape: "wobble", center: { x: "600", y: "300" },
                                       radiusX: "450", radiusY: "160",
                                       startDegrees: "x", sweepDegrees: "y",
                                       sides: "8",
                                       depth: { farScale: "a", nearScale: "b",
                                                occlusionDepth: "c" } },
              count: 6, radius: "450" },
            { tag: "empty", track: {}, count: 2, radius: 300 }
        ]
    }

    function test_trackDegenerateInputsStayFinite(data) {
        const metrics = trackMetrics(data.track, data.count, data.radius)
        const problems = []
        collectNonFinite(metrics, "metrics", problems)
        compare(problems.join(","), "", "metrics carry a non-finite value")
        verify(metrics.width >= 1 && metrics.height >= 1, "the box has a size")
        for (let index = 0; index < data.count; ++index) {
            const entry = LayoutEngine.trackEntryGeometry(
                data.track, index, data.count, metrics, 0, "upright")
            collectNonFinite(entry, "entry[" + index + "]", problems)
            compare(problems.join(","), "", "entry carries a non-finite value")
            verify(entry.pathProgress >= 0 && entry.pathProgress <= 1,
                   "progress stays in [0, 1]")
            fuzzy(Math.hypot(entry.outwardNormal.x, entry.outwardNormal.y), 1,
                  "outward normal is a unit vector")
            verify(entry.scaleFactor > 0, "scale stays positive")
        }
    }

    // Criterion: a closed track turns with the scene; an open one does not,
    // because sweeping an arc would carry entries off the drawn platform.
    function test_closedTracksRotateAndOpenTracksDoNot() {
        const ring = ringTrack()
        verify(LayoutEngine.trackSupportsRotation(ring), "a ring turns")
        verify(LayoutEngine.trackSupportsRotation(
                   ringTrack({ shape: "polygon" })), "a polygon turns")
        verify(!LayoutEngine.trackSupportsRotation(
                   ringTrack({ shape: "arc", sweepDegrees: 140 })),
               "an arc does not turn")

        const metrics = trackMetrics(ring, 6)
        const resting = LayoutEngine.trackEntryGeometry(
            ring, 0, 6, metrics, 0, "upright")
        const turned = LayoutEngine.trackEntryGeometry(
            ring, 0, 6, metrics, 90, "upright")
        verify(Math.abs(turned.x - resting.x) > 1, "a turned ring moves")
        const full = LayoutEngine.trackEntryGeometry(
            ring, 0, 6, metrics, 360, "upright")
        fuzzy(full.x, resting.x, "a full turn returns to the start")

        const arc = ringTrack({ shape: "arc", sweepDegrees: 140 })
        const arcMetrics = trackMetrics(arc, 6)
        const arcResting = LayoutEngine.trackEntryGeometry(
            arc, 0, 6, arcMetrics, 0, "upright")
        const arcTurned = LayoutEngine.trackEntryGeometry(
            arc, 0, 6, arcMetrics, 90, "upright")
        compare(arcTurned.x, arcResting.x, "an open track ignores rotation")
    }

    // Criterion: tilt is bounded by the theme and moves the platform and the
    // track together, so icons keep sitting on the artwork.
    function test_tiltIsClampedAndMovesPlatformWithTrack() {
        const track = ringTrack()
        const level = trackMetrics(track, 6, 450, 0)
        const tilted = trackMetrics(track, 6, 450, 12)
        const beyond = trackMetrics(track, 6, 450, 900)
        const flat = trackMetrics(track, 6, 450, -900)

        verify(tilted.tiltFactor > level.tiltFactor, "a positive tilt opens up")
        compare(beyond.tiltFactor, tilted.tiltFactor,
                "a tilt beyond the declared maximum is clamped to it")
        verify(flat.tiltFactor < level.tiltFactor,
               "a negative tilt flattens within the declared minimum")
        verify(flat.tiltFactor > 0, "a clamped tilt stays usable")

        fuzzy(tilted.radiusY / level.radiusY, tilted.tiltFactor,
              "the track radius takes the tilt factor")
        fuzzy(tilted.platform.height / level.platform.height,
              tilted.tiltFactor, "the platform takes the same tilt factor")
        compare(tilted.radiusX, level.radiusX, "tilt leaves the width alone")

        const untiltable = ringTrack({ tilt: undefined })
        compare(trackMetrics(untiltable, 6, 450, 30).tiltFactor, 1,
                "a theme that declares no tilt does not tilt")
    }

    // Criterion: the logical order the accessibility tree walks is the entry
    // order, whatever the depth order draws first.
    function test_logicalOrderIsIndependentOfDepthOrder() {
        const track = ringTrack()
        const count = 8
        const metrics = trackMetrics(track, count)
        const progress = []
        for (let index = 0; index < count; ++index) {
            const entry = LayoutEngine.trackEntryGeometry(
                track, index, count, metrics, 0, "upright")
            progress.push(entry.pathProgress)
        }
        for (let index = 1; index < progress.length; ++index) {
            verify(progress[index] > progress[index - 1],
                   "entry " + index + " follows its predecessor on the path")
        }
    }

    // ---- Tracks, spacing and overcrowded curves ------------------------

    // The Blue Ring platform as shipped: artwork 1200 x 600, track radius
    // 446 x 163. A panel draws it at radius 300 with icon 52 and padding 18.
    function platformRingTrack() {
        return ringTrack({
            radiusX: 446, radiusY: 163,
            depth: { farScale: 0.62, nearScale: 1.0, occlusionDepth: 0.62 },
            tilt: { minimumDegrees: -10, maximumDegrees: 10, defaultDegrees: 0 }
        })
    }

    function entryCentre(entry) {
        return { x: entry.entryBounds.x + entry.entryBounds.width / 2,
                 y: entry.entryBounds.y + entry.entryBounds.height / 2 }
    }

    // Criterion: an entry stands on the track the platform is drawn for. The
    // scene box offset is applied once, whatever padding the panel has.
    function test_trackEntriesStandOnThePlatformTrack_data() {
        return [
            { tag: "ring-300-padding-18", padding: 18, radius: 300, count: 6 },
            { tag: "no-padding", padding: 0, radius: 300, count: 6 },
            { tag: "enlarged-padding-40", padding: 40, radius: 520, count: 9 }
        ]
    }

    function test_trackEntriesStandOnThePlatformTrack(data) {
        const track = platformRingTrack()
        const metrics = LayoutEngine.trackMetrics(
            track, 1200, 600, data.count, 52, data.padding, data.radius)
        const scale = data.radius / 446
        fuzzy(metrics.center.x - metrics.platform.x, 600 * scale,
              "the track centre sits on the artwork's own centre in x")
        fuzzy(metrics.center.y - metrics.platform.y, 300 * scale,
              "the track centre sits on the artwork's own centre in y")
        for (let index = 0; index < data.count; ++index) {
            const entry = LayoutEngine.trackEntryGeometry(
                track, index, data.count, metrics, 0, "upright")
            const centre = entryCentre(entry)
            const radians = (index / data.count * 360 - 90) * Math.PI / 180
            fuzzy(centre.x, metrics.center.x + Math.cos(radians) * metrics.radiusX,
                  "entry " + index + " x is on the platform track")
            fuzzy(centre.y, metrics.center.y + Math.sin(radians) * metrics.radiusY,
                  "entry " + index + " y is on the platform track")
            fuzzy(entry.x + metrics.iconSize / 2, centre.x,
                  "entry " + index + " logical and drawn centres agree in x")
            fuzzy(entry.y + metrics.iconSize / 2, centre.y,
                  "entry " + index + " logical and drawn centres agree in y")
        }
    }

    function curveStep(layout, count, spacing, index) {
        const value = LayoutEngine.metrics(
            layout, count, 52, spacing, 1, 300, 2, 18, false, 0, 6)
        const first = LayoutEngine.entryGeometry(
            layout, index, count, value, 0, 6, "upright", "live")
        const second = LayoutEngine.entryGeometry(
            layout, index + 1, count, value, 0, 6, "upright", "live")
        return second.pathProgress - first.pathProgress
    }

    // Criterion: the canonical spacing control regulates separation along a
    // curved track. At its default or above the entries keep the even spread
    // every curved layout has always had; below it they close up about the
    // middle of the track, down to touching at zero.
    function test_spacingRegulatesSeparationAlongCurvedTracks_data() {
        return [
            { tag: "ring", layout: "ring", closed: true,
              length: 2 * Math.PI * 300 },
            { tag: "circular", layout: "circular", closed: true,
              length: 2 * Math.PI * 300 },
            { tag: "octagon", layout: "octagon", closed: true,
              length: 8 * 2 * 300 * Math.sin(Math.PI / 8) },
            { tag: "semicircle", layout: "semicircle", closed: false,
              length: Math.PI * 300 },
            { tag: "arc", layout: "arc", closed: false,
              length: 130 * Math.PI / 180 * 300 },
            { tag: "fan", layout: "fan", closed: false,
              length: 116 * Math.PI / 180 * 300 }
        ]
    }

    function test_spacingRegulatesSeparationAlongCurvedTracks(data) {
        const count = 6
        const spans = data.closed ? count : count - 1
        const evenPitch = data.length / spans
        for (const spacing of [8, 10, 48]) {
            fuzzy(curveStep(data.layout, count, spacing, 2), 1 / spans,
                  "spacing " + spacing + " keeps the even spread")
        }
        let previous = -1
        for (const spacing of [0, 2, 4, 6, 8]) {
            const pitch = curveStep(data.layout, count, spacing, 2) * data.length
            verify(pitch > previous, "separation grows at spacing " + spacing)
            previous = pitch
            fuzzy(pitch, 52 + (evenPitch - 52) * spacing / 8,
                  "spacing " + spacing + " scales the even gap")
        }
        fuzzy(curveStep(data.layout, count, 0, 2) * data.length, 52,
              "zero spacing leaves the icons touching, never overlapping")

        // The group closes up about the middle of the track, in order.
        const tight = LayoutEngine.metrics(
            data.layout, count, 52, 2, 1, 300, 2, 18, false, 0, 6)
        const evenLast = data.closed ? (count - 1) / count : 1
        const first = LayoutEngine.entryGeometry(
            data.layout, 0, count, tight, 0, 6, "upright", "live")
        const last = LayoutEngine.entryGeometry(
            data.layout, count - 1, count, tight, 0, 6, "upright", "live")
        verify(first.pathProgress > 0 && last.pathProgress < evenLast,
               "both ends move towards the middle of the track")
        fuzzy((first.pathProgress - 0.5) / (0 - 0.5),
              (last.pathProgress - 0.5) / (evenLast - 0.5),
              "both ends contract by the same factor")
        verify(first.onTrack !== false && last.onTrack !== false,
               "a track that holds its entries hides none of them")
    }

    // The same rule on a baked track: the icons close up towards the near
    // side of the platform, which is where a ring's front is.
    function test_spacingRegulatesBakedTracks() {
        const track = platformRingTrack()
        const count = 6
        function placed(placement) {
            const metrics = LayoutEngine.trackMetrics(
                track, 1200, 600, count, 52, 18, 300, undefined, false,
                placement)
            const result = []
            for (let index = 0; index < count; ++index) {
                result.push(LayoutEngine.trackEntryGeometry(
                    track, index, count, metrics, 0, "upright"))
            }
            return result
        }
        const legacy = placed(undefined)
        const even = placed({ spacing: 10, spacingReference: 8 })
        const tight = placed({ spacing: 2, spacingReference: 8 })
        const touching = placed({ spacing: 0, spacingReference: 8 })
        for (let index = 0; index < count; ++index) {
            fuzzy(even[index].pathProgress, legacy[index].pathProgress,
                  "entry " + index + " keeps the even spread at the default")
            fuzzy(even[index].x, legacy[index].x,
                  "entry " + index + " keeps its position at the default")
            verify(tight[index].depth >= 0 && tight[index].depth <= 1,
                   "depth stays normalized")
            verify(tight[index].onTrack !== false, "a ring hides no entry")
        }
        const length = 2 * Math.PI * 300
        fuzzy((touching[3].pathProgress - touching[2].pathProgress) * length,
              52, "zero spacing touches on the platform's own circle")
        verify(tight[3].pathProgress - tight[2].pathProgress
               < even[3].pathProgress - even[2].pathProgress,
               "a smaller spacing brings neighbours together")
        fuzzy(tight[3].pathProgress, 0.5, "the entry at the front stays there")
        verify(tight[0].depth > even[0].depth,
               "the far entry comes towards the front")
    }

    // Criterion: an open curve that cannot hold its entries side by side
    // keeps them icon + spacing apart and shows a window of them. A transient
    // offset moves the window in stable order; nothing leaves the curve.
    function test_overcrowdedOpenCurvesShowAWindow_data() {
        return [
            { tag: "semicircle", layout: "semicircle", sweep: 180 },
            { tag: "arc", layout: "arc", sweep: 130 },
            { tag: "fan", layout: "fan", sweep: 116 }
        ]
    }

    function test_overcrowdedOpenCurvesShowAWindow(data) {
        const radius = 150
        const icon = 52
        const spacing = 8
        const count = 14
        const length = data.sweep * Math.PI / 180 * radius
        const capacity = Math.floor(length / (icon + spacing)) + 1
        function value(entries) {
            return LayoutEngine.metrics(data.layout, entries, icon, spacing, 1,
                                        radius, 2, 18, false, 0, 6)
        }
        function placed(offset) {
            const geometry = value(count)
            geometry.browseOffset = offset
            const result = []
            for (let index = 0; index < count; ++index) {
                result.push(LayoutEngine.entryGeometry(
                    data.layout, index, count, geometry, 0, 6, "upright",
                    "live"))
            }
            return result
        }

        const window = LayoutEngine.pathWindow(data.layout, count, value(count))
        compare(window.windowed, true, "fourteen icons do not fit")
        compare(window.capacity, capacity)
        compare(window.maximumOffset, count - capacity)

        for (const offset of [0, 3, count - capacity]) {
            const entries = placed(offset)
            let visible = 0
            for (let index = 0; index < count; ++index) {
                const inside = index >= offset && index < offset + capacity
                compare(entries[index].onTrack, inside,
                        "entry " + index + " at offset " + offset)
                verify(entries[index].pathProgress >= 0
                       && entries[index].pathProgress <= 1,
                       "entry " + index + " never leaves the curve")
                if (inside)
                    ++visible
            }
            compare(visible, capacity)
            for (let index = offset + 1; index < offset + capacity; ++index) {
                fuzzy((entries[index].pathProgress
                       - entries[index - 1].pathProgress) * length,
                      icon + spacing, "neighbours keep icon + spacing")
            }
            fuzzy(entries[offset].pathProgress
                  + entries[offset + capacity - 1].pathProgress, 1,
                  "the window is centred on the curve")
        }

        // The order is stable: one step moves every entry one slot.
        const before = placed(2)
        const after = placed(3)
        for (let index = 3; index < 2 + capacity; ++index) {
            fuzzy(after[index].pathProgress, before[index - 1].pathProgress,
                  "entry " + index + " takes its predecessor's slot")
        }

        // A curve that holds its entries is unchanged: they span all of it.
        const few = value(5)
        compare(LayoutEngine.pathWindow(data.layout, 5, few).windowed, false)
        compare(LayoutEngine.entryGeometry(
                    data.layout, 0, 5, few, 0, 6, "upright", "live")
                    .pathProgress, 0)
        compare(LayoutEngine.entryGeometry(
                    data.layout, 4, 5, few, 0, 6, "upright", "live")
                    .pathProgress, 1)

        // A closed ring is never windowed; the wheel turns it instead.
        compare(LayoutEngine.pathWindow(
                    "ring", 40, LayoutEngine.metrics(
                        "ring", 40, icon, spacing, 1, radius, 2, 18, false,
                        0, 6)).windowed, false)
    }

    // Criterion: compact Fan and Arc folder contents sit on an exact half
    // circle and larger folders move along it. The reference folder holds 43
    // children; a named cell is 108 wide.
    function test_folderContentsFollowAnExactHalfCircle_data() {
        const rows = []
        for (const layout of ["arc", "fan"]) {
            rows.push({ tag: layout + "/names", layout: layout,
                        labelWidth: 108, labelHeight: 36.65625, capacity: 4 })
            rows.push({ tag: layout + "/icons", layout: layout,
                        labelWidth: 0, labelHeight: 0, capacity: 9 })
        }
        return rows
    }

    function folderPath(layout, count, labelWidth, labelHeight, height) {
        return LayoutEngine.expansionGeometry(layout, count, 48, 6, 140, 7, {
            maximumWidth: 620, maximumHeight: height === undefined ? 400 : height,
            labelWidth: labelWidth, labelHeight: labelHeight, compactPath: true
        })
    }

    function test_folderContentsFollowAnExactHalfCircle(data) {
        const count = 43
        const value = folderPath(data.layout, count, data.labelWidth,
                                 data.labelHeight)
        verify(value.followsPath)
        fuzzy(value.pathRadiusX, 140, "the nominal radius is kept")
        fuzzy(value.pathRadiusY, 140, "a circle, not an ellipse, when it fits")
        const path = LayoutEngine.expansionPath(value, value.height)
        compare(path.windowed, true)
        compare(path.capacity, data.capacity)
        compare(path.maximumOffset, count - data.capacity)
        fuzzy(path.step * (path.capacity - 1), Math.PI,
              "the entries on the path span exactly 180 degrees")

        const centre = { x: value.cellWidth / 2,
                         y: path.radiusY + value.cellHeight / 2 }
        for (const offset of [0, 1, 7.5, count - data.capacity]) {
            const whole = Math.abs(offset - Math.round(offset)) < 0.0001
            let onPath = 0
            let previousAngle = -Infinity
            for (let index = 0; index < count; ++index) {
                const point = LayoutEngine.expansionPathPoint(
                    value, path, index, offset)
                if (point.visibility <= 0)
                    continue
                const x = point.x + value.cellWidth / 2 - centre.x
                const y = point.y + value.cellHeight / 2 - centre.y
                fuzzy(Math.hypot(x, y), 140,
                      "entry " + index + " is on the circle at offset " + offset)
                if (!point.onPath)
                    continue
                ++onPath
                verify(x >= -0.0001, "entry " + index + " is on the half circle")
                verify(point.x >= -0.0001
                       && point.x + value.cellWidth <= value.width + 0.0001,
                       "entry " + index + " fits the popup width")
                verify(point.y >= -0.0001
                       && point.y + value.cellHeight <= value.height + 0.0001,
                       "entry " + index + " fits the popup height")
                const angle = Math.atan2(y, x)
                verify(angle > previousAngle,
                       "entry " + index + " follows its predecessor on the path")
                previousAngle = angle
            }
            compare(onPath, whole ? data.capacity : data.capacity - 1,
                    "entries on the path at offset " + offset)
        }

        // At rest the first and last entries on the path stand on the two
        // ends of the diameter: a half circle, with no straight run.
        const top = LayoutEngine.expansionPathPoint(value, path, 5, 5)
        const bottom = LayoutEngine.expansionPathPoint(
            value, path, 5 + data.capacity - 1, 5)
        fuzzy(top.x, 0, "the first entry is on the diameter")
        fuzzy(top.y, 0, "the first entry is at the top end")
        fuzzy(bottom.x, 0, "the last entry is on the diameter")
        fuzzy(bottom.y, 2 * path.radiusY, "the last entry is at the bottom end")
        // Stable order: one step moves every entry one slot along the curve.
        for (let index = 6; index < 5 + data.capacity; ++index) {
            const moved = LayoutEngine.expansionPathPoint(value, path, index, 6)
            const former = LayoutEngine.expansionPathPoint(
                value, path, index - 1, 5)
            fuzzy(moved.x, former.x, "entry " + index + " takes the next slot x")
            fuzzy(moved.y, former.y, "entry " + index + " takes the next slot y")
        }
        // An entry beyond either end is not on the path and cannot be hit.
        compare(LayoutEngine.expansionPathPoint(value, path, 4, 5).onPath, false)
        compare(LayoutEngine.expansionPathPoint(
                    value, path, 5 + data.capacity, 5).onPath, false)
    }

    // A folder that fits shrinks its circle instead of leaving gaps, and a
    // popup with too little room becomes a half ellipse, never a straight run.
    function test_folderHalfCircleFitsSmallFoldersAndShortPopups() {
        const single = folderPath("arc", 1, 108, 36.65625)
        compare(single.width, single.cellWidth)
        compare(single.height, Math.ceil(single.cellHeight))
        const point = LayoutEngine.expansionPathPoint(
            single, LayoutEngine.expansionPath(single, single.height), 0, 0)
        fuzzy(point.x, 0, "a single child needs no curve")
        fuzzy(point.y, 0, "a single child needs no curve")

        for (const count of [2, 3, 4]) {
            const value = folderPath("arc", count, 108, 36.65625)
            const path = LayoutEngine.expansionPath(value, value.height)
            compare(path.windowed, false, count + " named children fit")
            verify(value.pathRadiusX <= 140 && value.pathRadiusX >= 48 * 1.2)
            const first = LayoutEngine.expansionPathPoint(value, path, 0, 0)
            const last = LayoutEngine.expansionPathPoint(
                value, path, count - 1, 0)
            fuzzy(first.x, 0, "the first child is on the diameter")
            fuzzy(first.y, 0, "the first child is at the top end")
            fuzzy(last.x, 0, "the last child is on the diameter")
            fuzzy(last.y, 2 * path.radiusY, "the last child is at the bottom end")
            for (let index = 0; index < count; ++index) {
                const child = LayoutEngine.expansionPathPoint(
                    value, path, index, 0)
                compare(child.onPath, true)
                verify(child.x >= -0.0001
                       && child.x + value.cellWidth <= value.width + 0.0001)
            }
        }

        // Only 200 pixels of height: the vertical radius gives way.
        const value = folderPath("fan", 43, 108, 36.65625)
        const squeezed = LayoutEngine.expansionPath(value, 200)
        fuzzy(squeezed.radiusY, (200 - value.cellHeight) / 2,
              "the vertical radius follows the room")
        fuzzy(squeezed.radiusX, 140, "the horizontal radius is kept")
        verify(squeezed.capacity >= 2)
        fuzzy(squeezed.step * (squeezed.capacity - 1), Math.PI,
              "a half ellipse still spans exactly 180 degrees")
        for (let index = 0; index < squeezed.capacity; ++index) {
            const child = LayoutEngine.expansionPathPoint(
                value, squeezed, index, 0)
            compare(child.onPath, true)
            verify(child.y >= -0.0001
                   && child.y + value.cellHeight <= 200 + 0.0001,
                   "child " + index + " stays inside the short popup")
        }
    }

    // A folder opens on the side of the clicked icon that faces away from
    // the dock. `anchor` is where the icon stands: on the edge nearest it.
    function test_folderContentsOpenAwayFromTheFolder_data() {
        const rows = []
        for (const side of ["right", "left", "top", "bottom"])
            for (const layout of ["fan", "arc", "grid", "stack", "ring"])
                rows.push({ tag: side + "/" + layout, side: side, layout: layout })
        return rows
    }

    function test_folderContentsOpenAwayFromTheFolder(data) {
        const value = LayoutEngine.expansionGeometry(data.layout, 6, 48, 6, 140, 3, {
            maximumWidth: 620, maximumHeight: 400, labelWidth: 108,
            labelHeight: 36.65625, compactPath: true, side: data.side })
        compare(value.side, data.side)
        const vertical = data.side === "top" || data.side === "bottom"
        // Unit vector away from the folder, and the near edge's coordinate.
        const out = { right: [1, 0], left: [-1, 0], top: [0, -1], bottom: [0, 1] }[data.side]
        const near = { right: 0, left: value.width, top: value.height, bottom: 0 }[data.side]
        fuzzy(vertical ? value.anchor.y : value.anchor.x, near, "the folder stands on the near edge")
        const centres = []
        if (value.followsPath) {
            const path = LayoutEngine.expansionPath(value, value.height)
            for (let index = 0; index < path.capacity; ++index) {
                const point = LayoutEngine.expansionPathPoint(value, path, index, 0)
                centres.push({ x: point.x + value.cellWidth / 2, y: point.y + value.cellHeight / 2,
                               left: point.x, top: point.y })
            }
        } else {
            for (const point of value.entries)
                centres.push({ x: point.x + value.cellWidth / 2, y: point.y + value.cellHeight / 2,
                               left: point.x, top: point.y })
        }
        const depth = c => (c.x - value.anchor.x) * out[0] + (c.y - value.anchor.y) * out[1]
        for (const c of centres) {
            verify(c.left >= -0.001 && c.left + value.cellWidth <= value.width + 0.001
                   && c.top >= -0.001 && c.top + value.cellHeight <= value.height + 0.001,
                   "every child fits the popup")
            verify(depth(c) > 0, data.tag + ": no child stands behind the folder")
        }
        if (value.followsPath) {
            // Both ends of the half circle stand beside the folder, on the
            // near edge's cells; the middle child is the farthest out.
            const first = centres[0], last = centres[centres.length - 1]
            fuzzy(depth(first), depth(last), "the ends share the near edge")
            const middle = centres[Math.floor((centres.length - 1) / 2)]
            verify(depth(middle) > depth(first) + 100, data.tag + ": the curve bulges outward")
        }
        if (data.layout === "stack" || data.layout === "ring") {
            // Both start beside the folder.
            const distance = c => Math.hypot(c.x - value.anchor.x, c.y - value.anchor.y)
            const nearest = centres.reduce((best, c, index) =>
                distance(c) < distance(centres[best]) ? index : best, 0)
            compare(nearest, 0, data.tag + ": the first child is the nearest")
        }
        if (data.layout === "stack") {
            for (let index = 1; index < centres.length; ++index)
                verify(depth(centres[index]) > depth(centres[index - 1]), "a stack rises outward")
        }

        // Leaning along "across", the folder's anchor moves to that side's
        // start so the contents grow toward the lean.
        const leaning = LayoutEngine.expansionGeometry(data.layout, 6, 48, 6, 140, 3, {
            maximumWidth: 620, maximumHeight: 400, labelWidth: 108, labelHeight: 36.65625,
            compactPath: true, side: data.side, lean: 0.7 })
        const across = a => vertical ? a.x : a.y
        if (data.layout !== "stack")
            verify(across(leaning.anchor) < across(value.anchor) - 10, data.tag + ": the lean moves the folder's anchor")
    }

    function test_folderWithoutASideKeepsItsFrame() {
        const options = { maximumWidth: 620, maximumHeight: 400, labelWidth: 108,
                          labelHeight: 36.65625, compactPath: true }
        for (const layout of ["fan", "grid", "stack", "arc", "ring"]) {
            const before = LayoutEngine.expansionGeometry(layout, 9, 48, 6, 140, 3, options)
            const right = LayoutEngine.expansionGeometry(layout, 9, 48, 6, 140, 3,
                Object.assign({ side: "right" }, options))
            if (layout === "fan" || layout === "arc" || layout === "grid")
                compare(JSON.stringify(right.entries), JSON.stringify(before.entries),
                        layout + ": opening to the right is the original frame")
            compare(before.side, "right")
        }
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
