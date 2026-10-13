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
        "triangle", "square", "pentagon", "hexagon", "octagon", "star"]

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

    function test_tileIsABoundedSolidWithRealDepthAndBevel() {
        // ADREP-TASK-005, OF-42: a closed volume rather than a textured plane.
        function volume(mesh) {
            let sum = 0
            for (let i = 0; i < mesh.indexes.length; i += 3) {
                const a = mesh.positions[mesh.indexes[i]]
                const b = mesh.positions[mesh.indexes[i + 1]]
                const c = mesh.positions[mesh.indexes[i + 2]]
                sum += a[0] * (b[1]*c[2]-b[2]*c[1])
                    + a[1] * (b[2]*c[0]-b[0]*c[2]) + a[2] * (b[0]*c[1]-b[1]*c[0])
            }
            return sum / 6
        }
        for (const shape of ["rounded", "square", "squircle", "circle", "hexagon", "diamond"]) {
            const thin = PlatformGeometry.tile({shape: shape, thickness: 0.2, bevel: 0, diameter: 0.9})
            const thick = PlatformGeometry.tile({shape: shape, thickness: 0.8, bevel: 0, diameter: 0.9})
            const beveled = PlatformGeometry.tile({shape: shape, thickness: 0.8, bevel: 0.08, diameter: 0.9})
            for (const mesh of [thin, thick, beveled]) {
                compare(mesh.format, "org.archdock.mesh")
                compare(misWound(mesh), 0, shape + " every face winds outward")
                verify(volume(mesh) > 0, shape + " encloses positive volume")
                for (const p of mesh.positions) {
                    verify(p[0] >= -0.9 && p[0] <= 0.9 && p[1] >= -0.9 && p[1] <= 0.9)
                    verify(p[2] >= -0.8 && p[2] <= 0)
                }
                for (const uv of mesh.uv0s) verify(uv[0] >= 0 && uv[0] <= 1 && uv[1] >= 0 && uv[1] <= 1)
                verify(mesh.indexes.length / 3 < 1000, "a tile stays within the shared scene budget")
            }
            fuzzyCompare(volume(thick) / volume(thin), 4, 0.00001)
            verify(volume(beveled) < volume(thick), "a bevel removes the outer corners of the solid")
            verify(beveled.indexes.length > thick.indexes.length, "the bevel has its own faces")
        }
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

    function test_pedestalShapesAreClosedSolidsWithRelief_data() {
        return PlatformGeometry.pedestalShapes.map(function(kind) { return { tag: kind, kind: kind } })
    }

    function test_pedestalShapesAreClosedSolidsWithRelief(data) {
        // The Pedestals page's shapes: each a turned solid that stands on
        // z 0 within its reach, winds outward, carries texture coordinates
        // once round and along its profile, and tangents for its relief map.
        const mesh = PlatformGeometry.pedestalShape(data.kind, 24)
        compare(mesh.format, "org.archdock.mesh")
        compare(misWound(mesh), 0, data.kind + " winds outward")
        // Every stand is seen from a little above now, its foot included.
        compare(boundaryEdges(mesh).length, 0, data.kind + " is a closed surface")
        const top = PlatformGeometry.pedestalShapeHeight(data.kind)
        const reach = PlatformGeometry.pedestalShapeReach(data.kind)
        verify(reach > 0 && reach <= 1.4 + 1e-9, data.kind + " reaches no further than the widest stand")
        verify(PlatformGeometry.pedestalShapeScale(data.kind) > 0, data.kind + " has a width per unit")
        for (const point of mesh.positions) {
            verify(point[2] >= -1e-9 && point[2] <= top + 1e-9, data.kind + " between its foot and its top")
            verify(Math.hypot(point[0], point[1]) <= reach + 1e-9, data.kind + " within its reach")
        }
        // Seen from above, its outline reaches above its foot and a little
        // below it; a thicker one reaches higher.
        const outline = PlatformGeometry.pedestalOutline(data.kind, 1, 20)
        verify(outline.top > 0 && outline.bottom <= 0, data.kind + " has an outline above and below its foot")
        verify(PlatformGeometry.pedestalOutline(data.kind, 3, 20).top > outline.top, data.kind + " grows with thickness")
        for (const uv of mesh.uv0s) verify(uv[0] >= 0 && uv[0] <= 1 && uv[1] >= 0 && uv[1] <= 1)
        compare(mesh.tangents.length, mesh.positions.length, data.kind + " has a tangent per vertex")
        compare(mesh.binormals.length, mesh.positions.length)
        verify(mesh.indexes.length / 3 < 1000, data.kind + " stays within the shared scene budget")
    }

    function test_flatStandLayoutFloatsTheIconOverItsStand() {
        // The flat painter's and a folder opening's measures: nothing for
        // None or a style's own pedestal under Auto; otherwise a stand under
        // the icon, the Lift gap between them, the pair centred on the cell.
        compare(PlatformGeometry.pedestalLayout({}, 52).drawn, false, "new panels have no stands")
        compare(PlatformGeometry.pedestalLayout({ pedestalShape: "auto", iconPedestalEnabled: true }, 52).drawn, false)
        for (const shape of PlatformGeometry.pedestalShapes) {
            const layout = PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalLift: 0.4 }, 52)
            verify(layout.drawn, shape)
            verify(layout.height > 0 && layout.width > 0, shape + " has a size")
            verify(layout.gap > 0, shape + " floats the icon")
            fuzzyCompare(layout.above, layout.below, 0.01, shape + " is centred on the cell with its icon")
            const higher = PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalLift: 1.6 }, 52)
            verify(higher.gap > layout.gap && higher.raise > layout.raise, shape + " floats higher with Lift")
            // On a free panel's path the stand's middle is on the path and
            // the icon floats above it (the owner: "why are they outside
            // the dock").
            const onPath = PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalLift: 0.4 }, 52, true)
            fuzzyCompare(onPath.standCentre, 0, 0.01, shape + " stands on the path")
            verify(onPath.raise > layout.raise, shape + " lifts the icon above its stand on the path")
            // A level stand (Stand facing: Viewer) is seen View angle degrees
            // from above; one that is part of the panel (Panel, the default)
            // is seen straight on, as the flat panel is.
            const steep = PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalFacing: "viewer",
                                                            pedestalViewAngle: 50 }, 52)
            const side = PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalFacing: "viewer",
                                                           pedestalViewAngle: 0 }, 52)
            compare(steep.tilt, 50)
            compare(side.tilt, 0)
            const flat = PlatformGeometry.pedestalLayout({ pedestalShape: shape }, 52)
            compare(flat.tilt, 90, shape + " lies in the flat panel's plane")
            fuzzyCompare(flat.height, flat.width, 1e-6, shape + " seen straight on is as tall as it is wide")
            // A folder of a 3D panel draws its stands at the angle the
            // panel's own are seen at, over its own facing and View angle,
            // up to straight on; without that angle nothing changes.
            const seen = function(angle) {
                return PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalFacing: "viewer",
                                                        pedestalViewAngle: 20, pedestalSeenAngle: angle }, 52)
            }
            compare(seen(34).tilt, 34, shape + " takes the 3D panel's angle")
            compare(seen(75).tilt, 75, shape + " takes an angle past View angle's 60")
            compare(seen(120).tilt, 90)
            compare(seen(-5).tilt, 0)
            compare(seen(null).tilt, 20, shape + " keeps View angle with no 3D angle")
            compare(seen(undefined).tilt, 20)
            compare(seen(NaN).tilt, 20)
        }
    }

    function test_turnedStandExtents() {
        // A stand lying on a panel seen from 30 degrees above its plane
        // reaches nothing below the plane; one standing level in front of a
        // panel seen face-on would reach behind it, which the scene lifts
        // it out of.
        const t = 30 * Math.PI / 180
        const lying = PlatformGeometry.pedestalExtents("plate", 1, [0, Math.cos(t), Math.sin(t)],
                                                       0, Math.cos(t), Math.sin(t))
        verify(Math.abs(lying.lowest) < 1e-6, "a stand on the panel stays on it")
        verify(lying.top > 0 && lying.bottom < 0)
        const v = 20 * Math.PI / 180
        const level = PlatformGeometry.pedestalExtents("plate", 1, [0, Math.cos(v), Math.sin(v)], 0, 0, 1)
        verify(level.lowest < -0.5, "a level stand before a face-on panel would reach into it")
        fuzzyCompare(level.topX, 0, 1e-9, "a stand without a lean has its top over its foot")
        const leaning = PlatformGeometry.pedestalExtents("plate", 1, [0.3, 0.9, 0.3], 0, 0, 1)
        verify(leaning.topX > 0, "a leaning stand's top moves across the screen")
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

    // A star with any number of points, from three to twelve, is one closed
    // band within the scene's reach, its tips ending in points.
    function test_starsOfEveryPointCount() {
        for (let points = 3; points <= 12; ++points) {
            const shape = PlatformGeometry.shapeForLayout("star", points)
            compare(shape.points, points)
            const mesh = PlatformGeometry.platform({ shape: shape, band: 0.1, bend: 0 })
            compare(boundaryEdges(mesh).length, 0, points + " points: a closed surface")
            compare(misWound(mesh), 0, points + " points: faces outward everywhere")
            let farthest = 0
            for (const point of mesh.positions)
                farthest = Math.max(farthest, Math.hypot(point[0], point[1]))
            verify(farthest <= PlatformGeometry.reach + 1e-9, points + " points: within reach")
            verify(farthest > PlatformGeometry.reach - 1e-6, points + " points: the tips reach the edge")
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

    function test_rearHalfBendKeepsTheClosedMeshAndItsAnchors() {
        for (const layout of supportedLayouts) {
            const flat = PlatformGeometry.platform({shape: PlatformGeometry.shapeForLayout(layout, 7), band: 0.1})
            for (const bend of [-1, -0.5, 0.5, 1]) {
                const folded = PlatformGeometry.foldMesh(flat, bend, PlatformGeometry.topHeight)
                compare(folded.indexes.length, flat.indexes.length)
                compare(boundaryEdges(folded).length, 0, layout + " remains closed")
                compare(misWound(folded), 0, layout + " faces its deformed normals")
                compare(folded.rimOffset, flat.rimOffset)
                compare(folded.rimCount, flat.rimCount)
                // As far as the rear icon stands from the hinge: a star's
                // track lies further in than a circle's, to leave room for
                // its points.
                const foot = PlatformGeometry.foldPoint([0, flat.track, PlatformGeometry.topHeight], bend)
                verify(Math.abs(foot[2] - PlatformGeometry.topHeight) > 0.5 * flat.track / PlatformGeometry.trackRadius,
                    "the rear icon's foot follows the folded half")
                const front = [0, -flat.track, PlatformGeometry.topHeight]
                compare(PlatformGeometry.foldPoint(front, bend), front, "the front half stays still")
                // Polygon bands have shorter y reach than a circular band.
                verify(folded.positions.some(p => Math.abs(p[2] - PlatformGeometry.topHeight) > 0.25),
                    layout + " has a substantial fold, not only a rim tilt")
            }
            compare(PlatformGeometry.foldMesh(flat, 0), flat, "zero bend preserves the original mesh")
        }
    }

    function test_layoutsWithoutAnExactPlatform_data() {
        return ["arc", "semicircle", "fan", "horizontal", "vertical", "spiral", ""]
            .map(function(layout) { return { tag: layout || "empty", layout: layout } })
    }

    function test_layoutsWithoutAnExactPlatform(data) {
        compare(PlatformGeometry.shapeForLayout(data.layout, 6), null)
        compare(PlatformGeometry.platform({ shape: null }), null)
    }
}
