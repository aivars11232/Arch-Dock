import QtQuick 2.15
import QtTest 1.3
import "../qml/runtime/DockGeometry.js" as DockGeometry

TestCase {
    name: "DockGeometry"

    function test_polygonPathUsesRadiusAndTangentOrientation() {
        const geometry = DockGeometry.metrics(
            "polygon", 4, 40, 8, 1, 120, 2, 12, false, 0, 4);
        compare(geometry.width, 304);
        compare(geometry.height, 304);

        const first = DockGeometry.position("polygon", 0, 4, geometry, 0, 4, "tangent");
        compare(Math.round(first.x), 132);
        compare(Math.round(first.y), 12);
        compare(Math.round(first.rotation), 45);
    }

    function test_pathAnchorAlignsGeometryInsideLargerSurface() {
        const geometry = DockGeometry.metrics(
            "polygon", 4, 40, 8, 1, 120, 2, 12, false, 0, 4);
        const offset = DockGeometry.anchorOffset("bottom-right", 500, 420, geometry);

        compare(offset.x, 196);
        compare(offset.y, 116);
    }
}