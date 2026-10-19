import QtQuick
import QtTest
import "../qml/ArchDock/Rendering/IconSolid.js" as IconSolid

// The solid an icon's picture becomes: its outline from its transparency,
// caps and walls in the glyph's frame.
TestCase {
    name: "IconSolid"

    // RGBA bytes of a picture whose alpha is 255 where `inside(x, y)` holds.
    function picture(width, height, inside) {
        const data = new Uint8ClampedArray(width * height * 4)
        for (let y = 0; y < height; ++y)
            for (let x = 0; x < width; ++x) {
                const at = (y * width + x) * 4
                data[at] = 200; data[at + 1] = 100; data[at + 2] = 50
                data[at + 3] = inside(x, y) ? 255 : 0
            }
        return data
    }

    function test_squareOutlineHasFourCorners() {
        const data = picture(8, 8, function(x, y) { return x >= 2 && x < 6 && y >= 2 && y < 6 })
        const mask = IconSolid.maskFromImageData(data, 8, 8)
        const loops = IconSolid.outlines(mask, 8, 8)
        compare(loops.length, 1, "one outline")
        compare(loops[0].length, 16, "sixteen pixel-edge corners round a four by four square")
        verify(IconSolid.signedArea(loops[0]) > 0, "an outline runs with positive area")
        const corners = IconSolid.simplify(loops[0], 0.75)
        compare(corners.length, 4, "simplified to its four corners")
    }

    function test_solidOfASquare() {
        const data = picture(8, 8, function(x, y) { return x >= 2 && x < 6 && y >= 2 && y < 6 })
        const mesh = IconSolid.solidMesh(data, 8, 8)
        compare(mesh.loops, 1)
        // Two caps of four points and four walls of four points.
        compare(mesh.positions.length, 8 + 16)
        // Two triangles per cap, two per wall.
        compare(mesh.indexes.length, (2 + 2 + 8) * 3)
        compare(mesh.depth, 25)
        for (const point of mesh.positions) {
            verify(Math.abs(point[2]) === 25, "fifty units thick")
            verify(point[0] >= -50 && point[0] <= 50 && point[1] >= -50 && point[1] <= 50, "within the glyph's frame")
        }
        const front = mesh.positions.filter(function(point) { return point[2] === 25 })
        verify(front.length === 4 + 8, "front cap and the walls' front edges")
        for (const normal of mesh.normals)
            verify(Math.abs(Math.hypot(normal[0], normal[1], normal[2]) - 1) < 1e-6, "unit normals")
        for (const uv of mesh.uv0s)
            verify(uv[0] >= 0 && uv[0] <= 1 && uv[1] >= 0 && uv[1] <= 1, "texture coordinates in the picture")
    }

    function test_discOutlineIsRound() {
        const data = picture(32, 32, function(x, y) { return Math.hypot(x + 0.5 - 16, y + 0.5 - 16) < 12 })
        const mesh = IconSolid.solidMesh(data, 32, 32)
        compare(mesh.loops, 1)
        const cap = mesh.positions.filter(function(point, index) { return point[2] === 25 && index < mesh.positions.length / 3 })
        verify(cap.length >= 12, "a disc keeps many corners: " + cap.length)
        verify(mesh.indexes.length >= cap.length * 3, "triangulated")
    }

    function test_twoPartsGiveTwoBlocks() {
        const data = picture(16, 8, function(x, y) { return (x >= 1 && x < 6 && y >= 1 && y < 7) || (x >= 9 && x < 15 && y >= 1 && y < 7) })
        const mesh = IconSolid.solidMesh(data, 16, 8)
        compare(mesh.loops, 2, "two separate parts")
    }

    function test_aHoleIsFilled() {
        const data = picture(16, 16, function(x, y) {
            const inside = x >= 2 && x < 14 && y >= 2 && y < 14
            const hole = x >= 6 && x < 10 && y >= 6 && y < 10
            return inside && !hole
        })
        const mesh = IconSolid.solidMesh(data, 16, 16)
        compare(mesh.loops, 1, "the hole is not an outline of its own")
        compare(mesh.positions.length, 8 + 16, "the outer square alone")
    }

    function test_wallsReadThePictureInsideTheEdge() {
        // The square covers a quarter to three quarters of the picture each
        // way; every wall reads the picture a little inside that, never on
        // the edge or beyond it.
        const data = picture(8, 8, function(x, y) { return x >= 2 && x < 6 && y >= 2 && y < 6 })
        const mesh = IconSolid.solidMesh(data, 8, 8)
        const walls = mesh.uv0s.slice(8)
        compare(walls.length, 16)
        for (const uv of walls) {
            verify(uv[0] > 0.25 + 0.005 && uv[0] < 0.75 - 0.005, "inside across: " + uv[0])
            verify(uv[1] > 0.25 + 0.005 && uv[1] < 0.75 - 0.005, "inside up and down: " + uv[1])
        }
        // The caps read the picture at the edge itself.
        for (const uv of mesh.uv0s.slice(0, 8))
            verify((Math.abs(uv[0] - 0.25) < 1e-9 || Math.abs(uv[0] - 0.75) < 1e-9)
                   && (Math.abs(uv[1] - 0.25) < 1e-9 || Math.abs(uv[1] - 0.75) < 1e-9), "a cap corner on the edge")
    }

    // RGBA bytes of a picture coloured by `colour(x, y)` (null where clear).
    function coloured(width, height, colour) {
        const data = new Uint8ClampedArray(width * height * 4)
        for (let y = 0; y < height; ++y)
            for (let x = 0; x < width; ++x) {
                const at = (y * width + x) * 4, rgb = colour(x, y)
                if (!rgb) continue
                data[at] = rgb[0]; data[at + 1] = rgb[1]; data[at + 2] = rgb[2]; data[at + 3] = 255
            }
        return data
    }

    function test_reliefOfADiscIsACushion() {
        const width = 32, height = 32
        const data = coloured(width, height, function(x, y) { return Math.hypot(x + 0.5 - 16, y + 0.5 - 16) < 13 ? [220, 120, 60] : null })
        const mesh = IconSolid.reliefMesh(data, width, height)
        compare(mesh.parts, 1, "one part")
        compare(mesh.layers, 1, "on the ground")
        verify(mesh.depth > 20, "it rises: " + mesh.depth)
        // Two vertices per corner (front and back), two triangles per
        // opaque pixel on each side.
        let opaque = 0
        for (let at = 3; at < data.length; at += 4) if (data[at] > 96) ++opaque
        compare(mesh.indexes.length, opaque * 2 * 2 * 3)
        let highest = null
        for (const point of mesh.positions) if (!highest || point[2] > highest[2]) highest = point
        verify(Math.hypot(highest[0], highest[1]) < 8, "the top is near the middle")
        for (let index = 0; index < mesh.positions.length; index += 2) {
            const front = mesh.positions[index], back = mesh.positions[index + 1]
            verify(front[2] >= 0 && back[2] === -front[2], "the back mirrors the front")
            const normal = mesh.normals[index]
            verify(Math.abs(Math.hypot(normal[0], normal[1], normal[2]) - 1) < 1e-6, "unit normals")
            verify(normal[2] > 0 && mesh.normals[index + 1][2] < 0, "front normals forward, back normals backward")
        }
        for (const uv of mesh.uv0s)
            verify(uv[0] >= 0 && uv[0] <= 1 && uv[1] >= 0 && uv[1] <= 1, "texture coordinates in the picture")
    }

    function test_reliefTrianglesFaceTheirNormals() {
        const width = 24, height = 24
        const data = coloured(width, height, function(x, y) { return Math.hypot(x + 0.5 - 12, y + 0.5 - 12) < 10 ? [40, 90, 200] : null })
        const mesh = IconSolid.reliefMesh(data, width, height)
        for (let at = 0; at < mesh.indexes.length; at += 3) {
            const a = mesh.positions[mesh.indexes[at]], b = mesh.positions[mesh.indexes[at + 1]], c = mesh.positions[mesh.indexes[at + 2]]
            const n = mesh.normals[mesh.indexes[at]]
            const ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2]
            const vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2]
            const face = [uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx]
            verify(face[0] * n[0] + face[1] * n[1] + face[2] * n[2] > 0, "a triangle winds with its normal")
        }
    }

    function test_aPartInsideAnotherStandsAStepUp() {
        const width = 32, height = 32
        const data = coloured(width, height, function(x, y) {
            if (x < 2 || y < 2 || x >= 30 || y >= 30) return null
            return x >= 12 && x < 20 && y >= 12 && y < 20 ? [30, 200, 60] : [220, 120, 60]
        })
        const mask = IconSolid.maskFromImageData(data, width, height)
        const parted = IconSolid.colourParts(data, mask, width, height)
        compare(parted.parts.length, 2, "two parts")
        const layers = IconSolid.partLayers(parted.labels, parted.parts, width, height)
        const inner = parted.labels[16 * width + 16], outer = parted.labels[4 * width + 4]
        verify(inner !== outer)
        compare(layers[outer], 0, "the outer part on the ground")
        compare(layers[inner], 1, "the inner part a step up")
        const heights = IconSolid.reliefHeights(parted.labels, parted.parts, layers, width, height, {})
        // Just inside the inner part against just outside it: the step shows.
        verify(heights[16 * width + 12] > heights[16 * width + 11], "higher across the inner edge")
    }

    function test_aShadedPartStaysWhole() {
        const width = 32, height = 32
        const data = coloured(width, height, function(x, y) {
            if (x < 2 || y < 2 || x >= 30 || y >= 30) return null
            return [40 + x * 6, 80, 200 - x * 5]
        })
        const mask = IconSolid.maskFromImageData(data, width, height)
        compare(IconSolid.colourParts(data, mask, width, height).parts.length, 1, "a gradient is one part")
    }

    function test_aSpeckJoinsItsNeighbour() {
        const width = 32, height = 32
        const data = coloured(width, height, function(x, y) {
            if (x < 2 || y < 2 || x >= 30 || y >= 30) return null
            return x === 10 && y === 10 ? [255, 255, 255] : [220, 120, 60]
        })
        const mask = IconSolid.maskFromImageData(data, width, height)
        compare(IconSolid.colourParts(data, mask, width, height).parts.length, 1, "a single pixel of another colour is no part")
    }

    function test_distanceInsideASquare() {
        const width = 8, height = 8
        const distance = IconSolid.distanceInside(function(index) {
            const x = index % 8, y = (index - x) / 8
            return x >= 2 && x < 6 && y >= 2 && y < 6
        }, width, height)
        compare(distance[2 * 8 + 2], 1, "an edge pixel is one away")
        compare(distance[3 * 8 + 3], 2, "a middle pixel is two away")
        compare(distance[0], 0, "outside is nothing")
    }

    function test_aPlateIsABlockAndTheRestIsRounded() {
        const width = 32, height = 32
        // A rounded square fills its box: a plate.
        const plate = IconSolid.maskFromImageData(picture(width, height, function(x, y) {
            const inX = x >= 4 && x < 28, inY = y >= 4 && y < 28
            const corner = (x < 7 || x >= 25) && (y < 7 || y >= 25)
            return inX && inY && !(corner && Math.hypot(Math.min(Math.abs(x - 7), Math.abs(x - 24)), Math.min(Math.abs(y - 7), Math.abs(y - 24))) > 3)
        }), width, height)
        compare(IconSolid.reliefKind(plate, width, height).kind, "block", "a rounded square is a plate")
        // A disc leaves its box's corners empty: a round thing.
        const disc = IconSolid.maskFromImageData(picture(width, height, function(x, y) { return Math.hypot(x + 0.5 - 16, y + 0.5 - 16) < 13 }), width, height)
        compare(IconSolid.reliefKind(disc, width, height).kind, "rounded", "a disc is rounded")
        // A star is far from convex.
        const star = IconSolid.maskFromImageData(picture(width, height, function(x, y) {
            const dx = x + 0.5 - 16, dy = y + 0.5 - 16, r = Math.hypot(dx, dy), a = Math.atan2(dy, dx)
            return r < 6 + 8 * Math.max(0, Math.cos(5 * a))
        }), width, height)
        compare(IconSolid.reliefKind(star, width, height).kind, "rounded", "a star is rounded")
        // The one entry point tells the kind it chose.
        const data = picture(width, height, function(x, y) { return x >= 4 && x < 28 && y >= 4 && y < 28 })
        compare(IconSolid.iconMesh(data, width, height, "auto").kind, "block")
        compare(IconSolid.iconMesh(data, width, height, "rounded").kind, "rounded")
        compare(IconSolid.iconMesh(data, width, height, "block").depth, 25)
    }

    function test_wallsFaceOutward() {
        const data = picture(8, 8, function(x, y) { return x >= 2 && x < 6 && y >= 2 && y < 6 })
        const mesh = IconSolid.solidMesh(data, 8, 8)
        // Every wall triangle, seen from outside along its normal, winds
        // counter-clockwise: the triangle's own normal agrees with the
        // vertex normal.
        for (let at = 12; at < mesh.indexes.length; at += 3) {
            const a = mesh.positions[mesh.indexes[at]], b = mesh.positions[mesh.indexes[at + 1]], c = mesh.positions[mesh.indexes[at + 2]]
            const n = mesh.normals[mesh.indexes[at]]
            const ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2]
            const vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2]
            const faceNormal = [uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx]
            verify(faceNormal[0] * n[0] + faceNormal[1] * n[1] + faceNormal[2] * n[2] > 0, "a wall faces outward")
        }
    }
}
