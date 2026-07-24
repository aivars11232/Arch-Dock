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

    function test_freePanelShapeFamiliesProduceDistinctPaths() {
        const ellipse = DockGeometry.metrics(
            "ellipse", 8, 40, 8, 1, 120, 2, 12, false, 0, 6);
        const ellipseTop = DockGeometry.position("ellipse", 0, 8, ellipse, 0, 6, "upright");
        const ellipseSide = DockGeometry.position("ellipse", 2, 8, ellipse, 0, 6, "upright");
        verify(Math.abs(ellipseSide.x - ellipse.width / 2) > 80);
        verify(Math.abs(ellipseTop.y - ellipse.height / 2) < 100);

        const triangle = DockGeometry.metrics(
            "triangle", 6, 40, 8, 1, 120, 2, 12, false, 0, 6);
        const first = DockGeometry.position("triangle", 0, 6, triangle, 0, 6, "tangent");
        const second = DockGeometry.position("triangle", 1, 6, triangle, 0, 6, "tangent");
        verify(first.rotation !== second.rotation || first.x !== second.x);

        const spiral = DockGeometry.metrics(
            "spiral", 8, 40, 8, 1, 120, 2, 12, false, 0, 6);
        const inner = DockGeometry.position("spiral", 0, 8, spiral, 0, 6, "upright");
        const outer = DockGeometry.position("spiral", 7, 8, spiral, 0, 6, "upright");
        verify(Math.abs(outer.x - spiral.width / 2) > Math.abs(inner.x - spiral.width / 2));
    }

    function test_pathAnchorAlignsGeometryInsideLargerSurface() {
        const geometry = DockGeometry.metrics(
            "polygon", 4, 40, 8, 1, 120, 2, 12, false, 0, 4);
        const offset = DockGeometry.anchorOffset("bottom-right", 500, 420, geometry);

        compare(offset.x, 196);
        compare(offset.y, 116);
    }
}
