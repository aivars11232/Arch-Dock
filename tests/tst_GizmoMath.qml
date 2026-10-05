import QtQuick
import QtTest
import "../qml/ArchDock/Rendering/GizmoMath.js" as GizmoMath

// AD3D-TASK-002: the desktop gizmo's pointer arithmetic.
TestCase {
    name: "GizmoMath"

    function test_axisTravelFollowsTheDrawnAxis() {
        const origin = Qt.point(100, 100)
        // An axis drawn 50 pixels to the right stands for 10 scene units.
        const right = Qt.point(150, 100)
        fuzzyCompare(GizmoMath.axisTravel(origin, right, 10, Qt.point(25, 0)), 5, 1e-9)
        fuzzyCompare(GizmoMath.axisTravel(origin, right, 10, Qt.point(-50, 0)), -10, 1e-9)
        // Movement across the axis does nothing.
        fuzzyCompare(GizmoMath.axisTravel(origin, right, 10, Qt.point(0, 40)), 0, 1e-9)
        // A diagonal axis takes only the component along it.
        const diagonal = Qt.point(130, 140)
        fuzzyCompare(GizmoMath.axisTravel(origin, diagonal, 1, Qt.point(30, 40)), 1, 1e-9)
        // Seen end-on, an axis has no direction to follow.
        compare(GizmoMath.axisTravel(origin, Qt.point(101, 101), 10, Qt.point(30, 0)), 0)
    }

    function test_sweptDegreesIsSignedAndShort() {
        const centre = Qt.point(0, 0)
        fuzzyCompare(GizmoMath.sweptDegrees(centre, Qt.point(10, 0), Qt.point(0, 10)), 90, 1e-9)
        fuzzyCompare(GizmoMath.sweptDegrees(centre, Qt.point(0, 10), Qt.point(10, 0)), -90, 1e-9)
        // Across the left side the short way round, not 350 degrees.
        fuzzyCompare(GizmoMath.sweptDegrees(centre, Qt.point(-10, 1), Qt.point(-10, -1)),
                     2 * Math.atan2(1, 10) * 180 / Math.PI, 1e-6)
    }

    function test_scaleRatioMeasuresFromTheCentre() {
        const centre = Qt.point(50, 50)
        fuzzyCompare(GizmoMath.scaleRatio(centre, Qt.point(70, 50), Qt.point(90, 50)), 2, 1e-9)
        fuzzyCompare(GizmoMath.scaleRatio(centre, Qt.point(70, 50), Qt.point(50, 60)), 0.5, 1e-9)
        compare(GizmoMath.scaleRatio(centre, Qt.point(51, 50), Qt.point(90, 50)), 1)
    }

    function test_modifiersFollowBlenderConventions() {
        compare(GizmoMath.modifierSettings(Qt.NoModifier).snap, false)
        compare(GizmoMath.modifierSettings(Qt.NoModifier).precision, 1)
        compare(GizmoMath.modifierSettings(Qt.ControlModifier).snap, true)
        compare(GizmoMath.modifierSettings(Qt.ShiftModifier).precision, 0.1)
        fuzzyCompare(GizmoMath.snapped(0.37, 0.05), 0.35, 1e-9)
        fuzzyCompare(GizmoMath.snapped(22, 15), 15, 1e-9)
        compare(GizmoMath.snapped(0.37, 0), 0.37)
        compare(GizmoMath.clamp(3, 0, 1), 1)
    }
}
