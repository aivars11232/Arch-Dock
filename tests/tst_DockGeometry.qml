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
