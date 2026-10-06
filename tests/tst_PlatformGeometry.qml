import QtQuick
import QtTest
import ArchDock.Rendering 1.0
import "../qml/ArchDock/Rendering/PlatformGeometry.js" as PlatformGeometry

// ADFIX-TASK-002: the generated 3D geometry. A look without a 3D mesh of its
// own stands on a platform shaped exactly like its layout, and every icon
// stands on a pedestal of its own.
TestCase {
    name: "PlatformGeometry"

    readonly property var supportedLayouts: ["circular", "ring", "ellipse", "radial", "polygon",
        "triangle", "square", "pentagon", "hexagon", "octagon"]

    function key(point) {
        return point.map(function(value) { return Math.round(value * 1e5) }).join(",")
    }

    // Every edge of a closed surface is shared by exactly two triangles.
    function boundaryEdges(mesh) {
        const edges = {}
        for (let i = 0; i < mesh.indexes.length; i += 3) {
            const corners = [mesh.indexes[i], mesh.indexes[i + 1], mesh.indexes[i + 2]]
                .map(function(index) { return key(mesh.positions[index]) })
            for (let side = 0; side < 3; ++side) {
                const a = corners[side], b = corners[(side + 1) % 3]
                if (a === b) continue
                const edge = a < b ? a + "|" + b : b + "|" + a
                edges[edge] = (edges[edge] || 0) + 1
            }
        }
        return Object.keys(edges).filter(function(edge) { return edges[edge] !== 2 })
    }

    // A triangle's winding agrees with the normals it was given.
    function misWound(mesh) {
        let wrong = 0
        for (let i = 0; i < mesh.indexes.length; i += 3) {
            const a = mesh.positions[mesh.indexes[i]]
            const b = mesh.positions[mesh.indexes[i + 1]]
            const c = mesh.positions[mesh.indexes[i + 2]]
            const ab = [b[0] - a[0], b[1] - a[1], b[2] - a[2]]
            const ac = [c[0] - a[0], c[1] - a[1], c[2] - a[2]]
            const cross = [ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2],
                           ab[0] * ac[1] - ab[1] * ac[0]]
            const n = mesh.normals[mesh.indexes[i]]
            if (cross[0] * n[0] + cross[1] * n[1] + cross[2] * n[2] <= 0) ++wrong
        }
        return wrong
    }

    function test_pedestalIsAClosedColumnFacingOutward() {
        const mesh = PlatformGeometry.pedestal(24)
        compare(mesh.format, "org.archdock.mesh")
        compare(mesh.indexes.length / 3, 24 * 3, "two side and one top triangle per segment")
        compare(misWound(mesh), 0)
        for (const point of mesh.positions) {
            verify(point[2] >= 0 && point[2] <= 1, "between its foot and its top")
            verify(Math.hypot(point[0], point[1]) <= 1 + 1e-9, "inside its radius")
        }
        verify(mesh.colors === undefined, "a pedestal takes the platform's material")
    }

    function test_layoutsWithAnExactPlatform_data() {
        return supportedLayouts.map(function(layout) { return { tag: layout, layout: layout } })
    }

    function test_layoutsWithAnExactPlatform(data) {
        const shape = PlatformGeometry.shapeForLayout(data.layout, 7)
        verify(shape !== null, data.layout + " has a 3D platform")
        for (const bend of [-1, 0, 0.6]) {
            const mesh = PlatformGeometry.platform({ shape: shape, band: 0.1, bend: bend,
                palette: { top: [0.1, 0.2, 0.3, 1], rim: [0.9, 0.5, 0.2, 1],
                           wall: [0.05, 0.1, 0.15, 1], under: [0, 0, 0, 1] } })
            verify(mesh !== null)
            compare(boundaryEdges(mesh).length, 0, data.layout + " is a closed surface")
            compare(misWound(mesh), 0, data.layout + " faces outward everywhere")
            compare(mesh.colors.length, mesh.positions.length, "one colour per vertex")
            verify(mesh.rimCount > 0 && mesh.rimCount % 3 === 0, "the rim is its own range")
            compare(mesh.rimOffset + mesh.rimCount, mesh.indexes.length, "the rim comes last")
            for (const point of mesh.positions)
                verify(Math.hypot(point[0], point[1]) <= PlatformGeometry.reach + 1e-9,
                       "within the scene's reach: " + Math.hypot(point[0], point[1]))
        }
    }

    // The point of a generated platform: the layout's own icon positions,
    // carried onto it, stand on its flat top.
    function test_everyIconStandsOnThePlatform_data() {
        return supportedLayouts.map(function(layout) { return { tag: layout, layout: layout } })
    }

    function test_everyIconStandsOnThePlatform(data) {
        const count = 9
        const geometry = LayoutEngine.metrics(data.layout, count, 48, 8, 1, 140, 1, 10, false, 0, 7)
        const shape = PlatformGeometry.shapeForLayout(data.layout, 7)
        const mesh = PlatformGeometry.platform({ shape: shape, band: PlatformGeometry.minimumBand, bend: 0 })
        // The flat top's triangles: their normals point straight up.
        const top = []
        for (let i = 0; i < mesh.rimOffset; i += 3) {
            const n = mesh.normals[mesh.indexes[i]]
            if (n[2] > 0.999)
                top.push([mesh.indexes[i], mesh.indexes[i + 1], mesh.indexes[i + 2]]
                    .map(function(index) { return mesh.positions[index] }))
        }
        verify(top.length > 0)
        function onTop(x, y) {
            return top.some(function(t) {
                const d = function(p, q, r) { return (p[0] - r[0]) * (q[1] - r[1]) - (q[0] - r[0]) * (p[1] - r[1]) }
                const a = d([x, y], t[0], t[1]), b = d([x, y], t[1], t[2]), c = d([x, y], t[2], t[0])
                return !((a < -1e-9 || b < -1e-9 || c < -1e-9) && (a > 1e-9 || b > 1e-9 || c > 1e-9))
            })
        }
        for (let index = 0; index < count; ++index) {
            const entry = LayoutEngine.entryGeometry(data.layout, index, count, geometry, 0, 7,
                                                     "upright", "canonical", "free")
            const x = entry.position.x + geometry.iconSize / 2 - geometry.width / 2
            const y = entry.position.y + geometry.iconSize / 2 - geometry.height / 2
            // Screen y grows downward; the platform's y grows upward.
            const stand = PlatformGeometry.standPoint(x, -y, geometry.radius, mesh.track)
            // A pedestal's footprint, not only its middle: an open path's end
            // icons once stood half over its ends.
            for (let step = 0; step <= 8; ++step) {
                const px = stand[0] + (step < 8 ? 0.05 * Math.cos(step * Math.PI / 4) : 0)
                const py = stand[1] + (step < 8 ? 0.05 * Math.sin(step * Math.PI / 4) : 0)
                verify(onTop(px, py), data.layout + " entry " + index + " reaches "
                       + px.toFixed(3) + "," + py.toFixed(3) + " off the platform's top")
            }
        }
    }

    function test_aBendTiltsTheTopAboutTheIconsLine() {
        const shape = PlatformGeometry.shapeForLayout("ring")
        const flat = PlatformGeometry.platform({ shape: shape, band: 0.1, bend: 0 })
        const bent = PlatformGeometry.platform({ shape: shape, band: 0.1, bend: 1 })
        const heights = function(mesh) {
            let inner = Infinity, outer = -Infinity
            for (let i = 0; i < mesh.positions.length; ++i) {
                const p = mesh.positions[i]
                if (mesh.normals[i][2] < 0.3) continue
                const r = Math.hypot(p[0], p[1])
                if (Math.abs(r - (PlatformGeometry.trackRadius - 0.1)) < 1e-6) inner = Math.min(inner, p[2])
                if (Math.abs(r - (PlatformGeometry.trackRadius + 0.1)) < 1e-6) outer = Math.max(outer, p[2])
            }
            return [inner, outer]
        }
        const level = heights(flat), tilted = heights(bent)
        fuzzyCompare(level[0], PlatformGeometry.topHeight, 1e-9)
        fuzzyCompare(level[1], PlatformGeometry.topHeight, 1e-9)
        verify(tilted[1] > PlatformGeometry.topHeight + 0.05, "the outer edge rises")
        verify(tilted[0] < PlatformGeometry.topHeight - 0.05, "the inner edge sinks")
        fuzzyCompare((tilted[0] + tilted[1]) / 2, PlatformGeometry.topHeight, 1e-9)
    }

    function test_layoutsWithoutAnExactPlatform_data() {
        return ["arc", "semicircle", "fan", "horizontal", "vertical", "star", "spiral", ""]
            .map(function(layout) { return { tag: layout || "empty", layout: layout } })
    }

    function test_layoutsWithoutAnExactPlatform(data) {
        compare(PlatformGeometry.shapeForLayout(data.layout, 6), null)
        compare(PlatformGeometry.platform({ shape: null }), null)
    }
}
