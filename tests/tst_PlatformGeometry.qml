import QtQuick
import QtTest
import ArchDock.Rendering 1.0
import "../qml/ArchDock/Rendering/PlatformGeometry.js" as PlatformGeometry

// The generated 3D geometry. A look without a 3D mesh of its
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
        // A closed volume rather than a textured plane.
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
            // the icon floats above it.
            const onPath = PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalLift: 0.4 }, 52, true)
            fuzzyCompare(onPath.standCentre, 0, 0.01, shape + " stands on the path")
            verify(onPath.raise > layout.raise, shape + " lifts the icon above its stand on the path")
            // A flat panel's stand lies level, seen View angle degrees from
            // above, whatever its facing (which turns a 3D panel's stands):
            // near horizontal by default, straight from above at 90.
            const steep = PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalFacing: "viewer",
                                                            pedestalViewAngle: 50 }, 52)
            const side = PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalFacing: "viewer",
                                                           pedestalViewAngle: 0 }, 52)
            compare(steep.tilt, 50)
            compare(side.tilt, 0)
            const flat = PlatformGeometry.pedestalLayout({ pedestalShape: shape }, 52)
            compare(flat.tilt, 20, shape + " lies near horizontal by default")
            const above = PlatformGeometry.pedestalLayout({ pedestalShape: shape, pedestalViewAngle: 90 }, 52)
            compare(above.tilt, 90)
            verify(Math.abs(above.height - flat.height) > 1e-6, shape + " is seen otherwise from straight above")
            if (!PlatformGeometry.pedestalSpec(shape).rounded)
                fuzzyCompare(above.height, above.width, 1e-6, shape + " seen straight from above is as tall as it is wide")
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
        return supportedLayouts.concat(["mobius"]).map(function(layout) { return { tag: layout, layout: layout } })
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

    // A Möbius strip platform: one band along the layout's ∞ with one half
    // twist, closed on itself where a lap ends turned over (no gap, no
    // seam); its strands pass the crossing apart in height, the second over
    // the first; it twists, standing taller than its own thickness; and
    // every icon of the layout stands on its middle line.
    function test_mobiusStripPlatform() {
        const shape = PlatformGeometry.shapeForLayout("mobius", 6)
        compare(shape.kind, "mobius")
        const mesh = PlatformGeometry.platform({ shape: shape, band: 0.1, bend: 0 })
        compare(mesh.shape, "mobius")
        compare(boundaryEdges(mesh).length, 0, "closed on itself")
        compare(misWound(mesh), 0, "faces outward everywhere")
        fuzzyCompare(PlatformGeometry.standLift("mobius", 0), -PlatformGeometry.mobiusLift, 1e-9)
        fuzzyCompare(PlatformGeometry.standLift("mobius", 0.5), PlatformGeometry.mobiusLift, 1e-9)
        compare(PlatformGeometry.standLift("ring", 0.3), 0)
        let top = -Infinity, bottom = Infinity
        for (const point of mesh.positions) {
            top = Math.max(top, point[2])
            bottom = Math.min(bottom, point[2])
        }
        verify(top - bottom > 2 * PlatformGeometry.mobiusLift + 0.1, "it twists: " + (top - bottom))
        const count = 8
        const geometry = LayoutEngine.metrics("mobius", count, 48, 8, 1, 140, 1, 10, false, 0, 6)
        const reachOut = mesh.track * LayoutEngine.mobiusReach
        const line = []
        for (let step = 0; step < 720; ++step) {
            const at = LayoutEngine.mobiusPoint(step / 720, reachOut)
            line.push([at.x, -at.y])
        }
        for (let index = 0; index < count; ++index) {
            const entry = LayoutEngine.entryGeometry("mobius", index, count, geometry, 0, 6,
                                                     "upright", "canonical", "free")
            const x = entry.position.x + geometry.iconSize / 2 - geometry.width / 2
            const y = entry.position.y + geometry.iconSize / 2 - geometry.height / 2
            const stand = PlatformGeometry.standPoint(x, -y, geometry.radius, mesh.track)
            let nearest = Infinity
            for (const point of line)
                nearest = Math.min(nearest, Math.hypot(point[0] - stand[0], point[1] - stand[1]))
            verify(nearest < 0.01, "entry " + index + " stands on the band's middle line: " + nearest)
        }
    }

    // A picture's relief: the platform is cut finely enough for the
    // picture's bumps to rise from it; it stays one closed surface facing
    // outward, every point two runs share rises with both, no edge is drawn
    // on its own (no rim range), and an open end does not rise. Without a
    // relief the platform is as it was.
    function test_pictureReliefMesh_data() {
        return supportedLayouts.concat(["mobius"]).map(function(layout) { return { tag: layout, layout: layout } })
    }

    function test_pictureReliefMesh(data) {
        const shape = PlatformGeometry.shapeForLayout(data.layout, 7)
        const spec = { shape: shape, band: 0.1, bend: 0.3,
            palette: { top: [0.1, 0.2, 0.3, 1], rim: [0.9, 0.5, 0.2, 1],
                       wall: [0.05, 0.1, 0.15, 1], under: [0, 0, 0, 1] } }
        const plain = PlatformGeometry.platform(spec)
        compare(JSON.stringify(PlatformGeometry.platform(Object.assign({ relief: false }, spec))),
                JSON.stringify(plain), "no relief, no change")
        verify(plain.uv1s === undefined && plain.sectionLength === undefined)
        const mesh = PlatformGeometry.platform(Object.assign({ relief: true }, spec))
        compare(boundaryEdges(mesh).length, 0, data.layout + " is a closed surface")
        compare(misWound(mesh), 0, data.layout + " faces outward everywhere")
        verify(mesh.positions.length > 2 * plain.positions.length, "cut finely: " + mesh.positions.length)
        verify(mesh.positions.length <= PlatformGeometry.reliefPoints * 1.2, "within the limit: " + mesh.positions.length)
        compare(mesh.uv1s.length, mesh.positions.length, "every point says how far it rises")
        compare(mesh.tangents.length, mesh.positions.length)
        verify(mesh.textureRepeats >= 1)
        verify(mesh.sectionLength > 0 && mesh.alongLength > 0)
        compare(mesh.rimCount, 0, "no edge drawn on its own")
        compare(mesh.rimOffset, mesh.indexes.length)
        const rises = {}
        let highest = 0
        for (let index = 0; index < mesh.positions.length; ++index) {
            const k = key(mesh.positions[index])
            rises[k] = (rises[k] || []).concat([mesh.uv1s[index][0]])
            highest = Math.max(highest, mesh.uv1s[index][0])
        }
        for (const k of Object.keys(rises))
            if (rises[k].length > 1)
                verify(rises[k].every(function(rise) { return rise === rises[k][0] }),
                       "a point shared by faces rises with them all: " + rises[k].join(" "))
        compare(highest, 1, "the band rises all the way")
        // A polygon's or a star's corners rise with their sides.
        if (["triangle", "square", "pentagon", "hexagon", "octagon", "polygon", "star"].indexOf(data.layout) >= 0)
            verify(mesh.uv1s.every(function(rise) { return rise[0] === 1 }), "nothing fades at the corners")
        for (const point of mesh.positions)
            verify(Math.hypot(point[0], point[1]) <= PlatformGeometry.reach + 1e-9)
        // A folded relief keeps its light but does not rise.
        const folded = PlatformGeometry.foldMesh(mesh, 0.5, PlatformGeometry.topHeight)
        compare(folded.uv1s.length, folded.positions.length)
        verify(folded.uv1s.every(function(rise) { return rise[0] === 0 }))
        compare(folded.binormals.length, folded.positions.length)
    }

    // A theme's own mesh cut for a picture's relief: every triangle cut
    // alike, so neighbours share every point of their edge, within the
    // triangle limit; every point moves all the way, and a point two faces
    // share moves the one way in both, between their faces' ways, so the
    // faces stay joined.
    // A stand wearing a picture is cut finely for its relief, within a
    // smaller limit than the platform's, and stays a closed solid.
    function test_pedestalShapesCutForRelief_data() {
        return PlatformGeometry.pedestalShapes.map(function(kind) { return { tag: kind, kind: kind } })
    }

    function test_pedestalShapesCutForRelief(data) {
        const stand = PlatformGeometry.pedestalShape(data.kind, 24)
        const mesh = PlatformGeometry.reliefMesh(stand, 0.05, 6000)
        verify(mesh !== null, data.kind + " is cut")
        verify(mesh.indexes.length > stand.indexes.length, data.kind + " is cut finely")
        verify(mesh.indexes.length / 3 <= 6000, data.kind + " within the stand's limit")
        compare(mesh.uv1s.length, mesh.positions.length)
        verify(mesh.uv1s.every(function(rise) { return rise[0] === 1 }), data.kind + ": every point moves")
        verify(mesh.uv1s.every(function(rise) { return [0, 1, 2].indexOf(rise[1]) >= 0 }), data.kind + ": a side per point")
        compare(boundaryEdges(mesh).length, 0, data.kind + " stays closed")
        compare(misWound(mesh), 0, data.kind + " still faces outward")
        verify(mesh.meshStep > 0)
    }

    function test_themeMeshCutForRelief() {
        const shape = PlatformGeometry.shapeForLayout("octagon", 8)
        const theme = PlatformGeometry.platform({ shape: shape, band: 0.12, bend: 0 })
        let t0 = Date.now()
        const mesh = PlatformGeometry.reliefMesh(theme, 0.015)
        verify(Date.now() - t0 < 3000, "cut in a moment: " + (Date.now() - t0) + " ms")
        verify(mesh !== null)
        verify(mesh.indexes.length / 3 > 4 * theme.indexes.length / 3, "cut finely")
        verify(mesh.indexes.length / 3 <= PlatformGeometry.reliefTriangles, "within the limit")
        compare(mesh.uv1s.length, mesh.positions.length)
        verify(mesh.uv1s.every(function(rise) { return rise[0] === 1 }), "every point moves all the way")
        compare(boundaryEdges(mesh).length, 0, "still closed")
        compare(misWound(mesh), 0, "still faces outward")
        verify(mesh.meshStep > 0)
        // Each point says which plane its face takes a picture from, the
        // top's points from across z, every point of a face the same.
        verify(mesh.uv1s.every(function(rise) { return [0, 1, 2].indexOf(rise[1]) >= 0 }), "a side per point")
        for (let t = 0; t < mesh.indexes.length; t += 3) {
            const sides = [0, 1, 2].map(function(c) { return mesh.uv1s[mesh.indexes[t + c]][1] })
            verify(sides[0] === sides[1] && sides[1] === sides[2], "one side per face")
            const a = mesh.positions[mesh.indexes[t]], b = mesh.positions[mesh.indexes[t + 1]], c = mesh.positions[mesh.indexes[t + 2]]
            if (Math.abs(a[2] - b[2]) < 1e-9 && Math.abs(a[2] - c[2]) < 1e-9)
                compare(sides[0], 0, "a level face takes its picture from across z")
        }
        const ways = {}
        let edgeWays = 0
        for (let index = 0; index < mesh.positions.length; ++index) {
            const k = key(mesh.positions[index])
            const way = key(mesh.normals[index])
            if (ways[k] !== undefined)
                compare(way, ways[k], "a shared point moves the one way")
            ways[k] = way
        }
        // A corner of the plain platform, where faces meet at an angle,
        // moves between their ways: not along any one face's.
        for (let index = 0; index < theme.positions.length; ++index) {
            const n = mesh.normals[mesh.positions.findIndex(function(p) { return key(p) === key(theme.positions[index]) })]
            const own = theme.normals[index]
            if (Math.abs(n[0] * own[0] + n[1] * own[1] + n[2] * own[2]) < 0.999)
                ++edgeWays
        }
        verify(edgeWays > 0, "the edges move between their faces")
    }

    // The star is the six-pointed Star of David whatever corner count a
    // panel carries: one closed band within the scene's reach, its six tips
    // ending in points at the edge, its notches a third of the way in.
    function test_starIsTheSixPointedStar() {
        for (const corners of [3, 6, 8, 12]) {
            const shape = PlatformGeometry.shapeForLayout("star", corners)
            compare(shape.points, 6)
            const mesh = PlatformGeometry.platform({ shape: shape, band: 0.1, bend: 0 })
            compare(boundaryEdges(mesh).length, 0, "a closed surface")
            compare(misWound(mesh), 0, "faces outward everywhere")
            let farthest = 0
            for (const point of mesh.positions)
                farthest = Math.max(farthest, Math.hypot(point[0], point[1]))
            verify(farthest <= PlatformGeometry.reach + 1e-9, "within reach")
            verify(farthest > PlatformGeometry.reach - 1e-6, "the tips reach the edge")
            const notch = PlatformGeometry.starCorners(6)[1].c
            fuzzyCompare(Math.hypot(notch[0], notch[1]), 1 / Math.sqrt(3), 1e-9, "the notches where two triangles' edges cross")
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

    // An edge panel's straight platform (PanelScene3D's edge panels, never a
    // free panel's layout): one closed solid along its edge, lying or
    // standing, its band centred on the panel's middle line and as long as
    // asked.
    function test_edgePanelBarPlatform_data() {
        return [{ tag: "lying", vertical: false }, { tag: "standing", vertical: true }]
    }

    function test_edgePanelBarPlatform(data) {
        const length = 3.5
        const mesh = PlatformGeometry.platform({ shape: { kind: "bar", vertical: data.vertical, length: length },
            band: PlatformGeometry.maximumBand, bend: 0,
            palette: { top: [0.1, 0.2, 0.3, 1], rim: [0.9, 0.5, 0.2, 1],
                       wall: [0.05, 0.1, 0.15, 1], under: [0, 0, 0, 1] } })
        verify(mesh !== null)
        compare(mesh.shape, "bar")
        compare(boundaryEdges(mesh).length, 0, "a closed surface")
        compare(misWound(mesh), 0, "faces outward everywhere")
        const across = PlatformGeometry.maximumBand + (PlatformGeometry.innerBevel + PlatformGeometry.outerBevel) / 2
        let lowAcross = Infinity, highAcross = -Infinity, lowAlong = Infinity, highAlong = -Infinity
        for (const point of mesh.positions) {
            const a = data.vertical ? point[0] : point[1], b = data.vertical ? point[1] : point[0]
            lowAcross = Math.min(lowAcross, a)
            highAcross = Math.max(highAcross, a)
            lowAlong = Math.min(lowAlong, b)
            highAlong = Math.max(highAlong, b)
        }
        fuzzyCompare(lowAcross, -across, 1e-9)
        fuzzyCompare(highAcross, across, 1e-9)
        fuzzyCompare(lowAlong, -length, 1e-9)
        fuzzyCompare(highAlong, length, 1e-9)
    }

    function test_bubbleIsADomeOverEveryShape() {
        for (const shape of ["rounded", "circle", "square", "squircle", "hexagon", "diamond"]) {
            const outline = PlatformGeometry.tileOutline(shape)
            const mesh = PlatformGeometry.bubble({shape: shape, height: 0.9, diameter: 0.92, rings: 10})
            compare(mesh.positions.length, 2 * (outline.length * 10 + 1), shape + ": a point per outline point per ring, and the top, each way")
            let lowest = 1e9, highest = -1e9
            for (const p of mesh.positions) { lowest = Math.min(lowest, p[2]); highest = Math.max(highest, p[2]) }
            verify(Math.abs(lowest + 0.9) < 1e-9, shape + ": the bottom at the height behind")
            verify(Math.abs(highest - 0.9) < 1e-9, shape + ": the top at the height")
            for (const n of mesh.normals) verify(Math.abs(Math.hypot(n[0], n[1], n[2]) - 1) < 1e-6, shape + ": unit normals")
            // Every triangle faces outward: its own normal agrees with its
            // first corner's.
            for (let at = 0; at < mesh.indexes.length; at += 3) {
                const a = mesh.positions[mesh.indexes[at]], b = mesh.positions[mesh.indexes[at + 1]], c = mesh.positions[mesh.indexes[at + 2]]
                const n = mesh.normals[mesh.indexes[at]]
                const ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2]
                const face = [uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx]
                verify(face[0] * n[0] + face[1] * n[1] + face[2] * n[2] > -1e-9, shape + ": a triangle faces outward")
            }
            // The rim's normals lie nearly flat, the top's points up, the
            // bottom's down.
            const rim = mesh.normals[0]
            verify(Math.abs(rim[2]) < 0.35, shape + ": the rim's normal lies nearly flat: " + rim[2].toFixed(2))
            compare(mesh.normals[outline.length * 10][2], 1)
            compare(mesh.normals[mesh.normals.length - 1][2], -1)
        }
    }

    function test_aBubbleGrowsToHoldTheIconWhole() {
        // The icon's picture is a square 0.72 either way of the centre, in
        // a frame where the tile's half width is 1. A circle must be wider
        // than its diagonal, a diamond wider still, a rounded square and a
        // square hold it within the tile's own width.
        const diagonal = 0.72 * Math.SQRT2
        verify(Math.abs(PlatformGeometry.containingScale("circle", 0.72, 0, 0) - diagonal) < 1e-6, "a circle: the diagonal")
        verify(Math.abs(PlatformGeometry.containingScale("square", 0.72, 0, 0) - 0.72) < 1e-6, "a square: the side")
        verify(PlatformGeometry.containingScale("rounded", 0.72, 0, 0) < 0.92, "a rounded square holds it as it is")
        verify(PlatformGeometry.containingScale("hexagon", 0.72, 0, 0) > 0.92, "a hexagon has to grow")
        verify(Math.abs(PlatformGeometry.containingScale("diamond", 0.72, 0, 0) - 1.44) < 1e-6, "a diamond: twice the half side")
        // A picture moved aside needs more on that side.
        verify(PlatformGeometry.containingScale("circle", 0.72, 0.2, 0) > diagonal, "an offset picture needs more")
        verify(PlatformGeometry.outlineReach(PlatformGeometry.tileOutline("circle"), 1.1) > 0.99, "a circle reaches one every way")
        // A bubble and a plate asked to be wider than the cell are.
        let widest = 0
        for (const p of PlatformGeometry.bubble({shape: "circle", height: 0.9, diameter: 1.1}).positions) widest = Math.max(widest, Math.abs(p[0]))
        verify(Math.abs(widest - 1.1) < 1e-6, "a bubble wider than the cell: " + widest)
        widest = 0
        for (const p of PlatformGeometry.tile({shape: "circle", thickness: 0.3, bevel: 0, diameter: 1.1}).positions) widest = Math.max(widest, Math.abs(p[0]))
        verify(Math.abs(widest - 1.1) < 1e-6, "a plate wider than the cell: " + widest)
    }

    function test_hologramLightLeavesEveryStandsMiddle() {
        // Every stand has a lens in the middle of its top, as wide as the
        // top's flat part allows; through a ring or a halo it is the panel
        // inside it; a projector's is its lens.
        for (const shape of PlatformGeometry.pedestalShapes) {
            const spot = PlatformGeometry.pedestalOpening(shape)
            verify(spot.radius > 0, shape + " has a lens")
            verify(spot.radius < PlatformGeometry.pedestalShapeReach(shape), shape + "'s lens lies inside it")
            verify(spot.height >= 0 && spot.height <= PlatformGeometry.pedestalShapeHeight(shape) + 1e-9,
                   shape + "'s lens lies on its top")
        }
        verify(PlatformGeometry.pedestalOpening("ring").hole && PlatformGeometry.pedestalOpening("ring").height === 0,
               "a ring's light leaves the panel inside it")
        verify(PlatformGeometry.pedestalOpening("halo").hole)
        fuzzyCompare(PlatformGeometry.pedestalOpening("star").radius, 0.58 * 0.72, 1e-9, "a star's middle")
        compare(PlatformGeometry.pedestalOpening("projector").radius, 0.62)
        compare(PlatformGeometry.pedestalFinish("projector").look, "metallic")
        compare(PlatformGeometry.pedestalFinish("plate"), null, "other stands wear the panel's look")
    }

    function test_standsStandAtTheScreenEdgesSide() {
        compare(PlatformGeometry.standDirection("bottom", false).name, "up")
        compare(PlatformGeometry.standDirection("top", false).turn, 180)
        compare(PlatformGeometry.standDirection("left", false).name, "right")
        compare(PlatformGeometry.standDirection("right", false).turn, -90)
        compare(PlatformGeometry.standDirection("top", true).name, "up", "a free panel's path keeps its icons up")
    }

    function test_hologramIconFloatsInItsLight() {
        compare(PlatformGeometry.hologramLayout({}, 52, false, "bottom").on, false, "no hologram by default")
        compare(PlatformGeometry.hologramLayout({ hologramIcons: true, hologramBeam: false }, 52, false, "bottom").on,
                false, "a hologram without its light stays in its place")
        for (const edge of ["bottom", "top", "left", "right"]) {
            for (const shape of ["none", "plate", "star", "ring", "projector", "column"]) {
                const values = { hologramIcons: true, pedestalShape: shape }
                const layout = PlatformGeometry.hologramLayout(values, 52, false, edge)
                verify(layout.on, edge + " " + shape)
                verify(layout.raise > 0, edge + " " + shape + ": the icon lies away from the screen edge")
                verify(layout.wraps, "an edge panel's light wraps its icon")
                fuzzyCompare(layout.beam.length, layout.lift + 52 * 1.05, 1e-6, "it reaches on past the icon")
                verify(layout.beam.endHalf > layout.beam.startHalf, "the light widens as it rises")
                if (shape !== "none") {
                    verify(layout.opening.y >= layout.standTop - 1e-6
                           && layout.opening.y <= layout.standTop + layout.stand.height + 1e-6,
                           edge + " " + shape + ": its lens lies on its stand")
                    fuzzyCompare(layout.standTop + layout.stand.height / 2, 0, 1e-6, "its stand stands on the panel")
                }
                verify(layout.above > 0, "it reaches past its cell away from the edge")
                const higher = PlatformGeometry.hologramLayout(Object.assign({ hologramBeamHeight: 2 }, values), 52, false, edge)
                verify(higher.raise > layout.raise, "Light height lifts the icon higher")
            }
        }
        // A free panel's light rises tall, the icon high in its wide end.
        const free = PlatformGeometry.hologramLayout({ hologramIcons: true }, 52, true, "")
        verify(!free.wraps)
        fuzzyCompare(free.lift, 52 * 0.95, 1e-6)
        fuzzyCompare(free.opening.y, 0, 1e-6, "its lens lies on the path")
    }
}
