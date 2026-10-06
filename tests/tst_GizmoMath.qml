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

    // ADFIX UF-07: dragging the platform's body turns and tilts it.
    function test_orbitTurnsAcrossAndTiltsUpAndDown() {
        let turned = GizmoMath.orbit(25, 10, Qt.point(40, 0), false)
        fuzzyCompare(turned.yaw, 20, 1e-9)
        fuzzyCompare(turned.pitch, 25, 1e-9)
        turned = GizmoMath.orbit(25, 10, Qt.point(0, -40), false)
        fuzzyCompare(turned.pitch, 35, 1e-9, "dragging up tilts the far side down")
        // Ctrl snaps both to 15 degrees.
        turned = GizmoMath.orbit(25, 10, Qt.point(13, 9), true)
        compare(turned.yaw % 15, 0)
        compare(turned.pitch % 15, 0)
        // Pitch stays within its range; yaw wraps the short way round.
        compare(GizmoMath.orbit(25, 0, Qt.point(0, -400), false).pitch, 60)
        fuzzyCompare(GizmoMath.orbit(25, 170, Qt.point(80, 0), false).yaw, -170, 1e-9)
    }

    // ADFIX AUD-04: an arrow seen end-on is not a silent handle.
    function test_anEndOnArrowMovesWithUpAndDown() {
        verify(GizmoMath.axisEndOn(Qt.point(100, 100), Qt.point(104, 103)))
        verify(!GizmoMath.axisEndOn(Qt.point(100, 100), Qt.point(130, 100)))
        // Where the ordinary rule gives nothing ...
        compare(GizmoMath.axisTravel(Qt.point(100, 100), Qt.point(102, 101), 10, Qt.point(0, -60)), 0)
        // ... up moves along the axis and down back, one length per 120 pixels.
        fuzzyCompare(GizmoMath.endOnTravel(Qt.point(0, -60), 10, 120), 5, 1e-9)
        fuzzyCompare(GizmoMath.endOnTravel(Qt.point(25, 120), 10, 120), -10, 1e-9)
    }
}
