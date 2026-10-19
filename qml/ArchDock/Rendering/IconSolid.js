.pragma library

// Real 3D icons: an icon's own picture as a solid block. The block's
// outline is the picture's own silhouette, cut from its transparency; the
// picture lies on the block's front, the back wears it mirrored, and the
// sides take the picture's edge colours, so the icon stays the one the
// user has, only given depth.
//
// From a small picture of the glyph (RGBA bytes): a mask of the opaque
// pixels, the mask's outlines as closed loops of pixel edges (each loop
// consistently wound, so outlines and holes tell apart by the sign of
// their area), each outline simplified (Douglas-Peucker) and triangulated
// (ear clipping), and the caps and walls of each outline's block built as
// a mesh in the glyph's own frame: a hundred units square, a hundred
// units deep (the scene scales the depth), x to the right, y up, z toward
// the viewer.

function maskFromImageData(data, width, height, threshold) {
    const mask = new Uint8Array(width * height)
    const limit = threshold === undefined ? 96 : threshold
    for (let index = 0; index < width * height; ++index)
        mask[index] = data[index * 4 + 3] > limit ? 1 : 0
    return mask
}

// The outlines of the mask's filled pixels: closed loops of pixel-corner
// points, in picture coordinates (x right, y down). Every filled pixel
// contributes the edges toward its empty neighbours, each edge wound
// clockwise round the pixel, so a loop round filled pixels has positive
// area and a loop round a hole negative. Returns the outer loops only, the
// largest first; a hole in a picture is filled in its block.
function outlines(mask, width, height) {
    const filled = function(x, y) {
        return x >= 0 && y >= 0 && x < width && y < height && mask[y * width + x] === 1
    }
    // Edges keyed by their start corner; a corner may start two edges where
    // two filled regions touch at that corner only.
    const starts = new Map()
    const add = function(x0, y0, x1, y1) {
        const key = x0 + "," + y0
        if (!starts.has(key)) starts.set(key, [])
        starts.get(key).push([x0, y0, x1, y1])
    }
    for (let y = 0; y < height; ++y)
        for (let x = 0; x < width; ++x) {
            if (!filled(x, y)) continue
            if (!filled(x, y - 1)) add(x, y, x + 1, y)
            if (!filled(x + 1, y)) add(x + 1, y, x + 1, y + 1)
            if (!filled(x, y + 1)) add(x + 1, y + 1, x, y + 1)
            if (!filled(x - 1, y)) add(x, y + 1, x, y)
        }
    const loops = []
    for (const [, edges] of starts) {
        while (edges.length) {
            const first = edges.pop()
            const loop = [[first[0], first[1]]]
            let current = first
            let guard = 0
            while (guard++ < width * height * 4) {
                const key = current[2] + "," + current[3]
                const candidates = starts.get(key)
                if (!candidates || !candidates.length) break
                // Where two edges start, take the one turning right (the
                // tighter loop), so regions touching at a corner stay apart.
                let pick = 0
                if (candidates.length > 1) {
                    const dx = current[2] - current[0], dy = current[3] - current[1]
                    for (let index = 1; index < candidates.length; ++index) {
                        const turn = dx * (candidates[index][3] - candidates[index][1]) - dy * (candidates[index][2] - candidates[index][0])
                        const best = dx * (candidates[pick][3] - candidates[pick][1]) - dy * (candidates[pick][2] - candidates[pick][0])
                        if (turn > best) pick = index
                    }
                }
                const next = candidates.splice(pick, 1)[0]
                if (next[2] === first[0] && next[3] === first[1]) {
                    loop.push([next[0], next[1]])
                    break
                }
                loop.push([next[0], next[1]])
                current = next
            }
            if (loop.length >= 4)
                loops.push(loop)
        }
    }
    const outer = loops.filter(function(loop) { return signedArea(loop) > 0 })
    outer.sort(function(a, b) { return signedArea(b) - signedArea(a) })
    return outer
}

function signedArea(points) {
    let area = 0
    for (let index = 0; index < points.length; ++index) {
        const a = points[index], b = points[(index + 1) % points.length]
        area += a[0] * b[1] - b[0] * a[1]
    }
    return area / 2
}

// Douglas-Peucker on a closed loop: split at the two points farthest apart,
// simplify each half, and drop every point within `tolerance` of the line
// between its neighbours.
function simplify(points, tolerance) {
    if (points.length <= 4) return points.slice()
    let farA = 0, farB = 0, farthest = -1
    for (let i = 0; i < points.length; ++i)
        for (let j = i + 1; j < points.length; ++j) {
            const d = (points[i][0] - points[j][0]) ** 2 + (points[i][1] - points[j][1]) ** 2
            if (d > farthest) { farthest = d; farA = i; farB = j }
        }
    const half = function(from, to) {
        const run = []
        for (let i = from; ; i = (i + 1) % points.length) {
            run.push(points[i])
            if (i === to) break
        }
        return run
    }
    const first = reduce(half(farA, farB), tolerance), second = reduce(half(farB, farA), tolerance)
    const result = first.slice(0, first.length - 1).concat(second.slice(0, second.length - 1))
    return result.length >= 3 ? result : points.slice()
}

function reduce(run, tolerance) {
    if (run.length <= 2) return run
    const a = run[0], b = run[run.length - 1]
    const dx = b[0] - a[0], dy = b[1] - a[1]
    const length = Math.hypot(dx, dy) || 1e-9
    let farthest = -1, at = 0
    for (let i = 1; i < run.length - 1; ++i) {
        const distance = Math.abs(dx * (run[i][1] - a[1]) - dy * (run[i][0] - a[0])) / length
        if (distance > farthest) { farthest = distance; at = i }
    }
    if (farthest <= tolerance) return [a, b]
    const left = reduce(run.slice(0, at + 1), tolerance), right = reduce(run.slice(at), tolerance)
    return left.slice(0, left.length - 1).concat(right)
}

function cross(a, b, c) {
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
}

function inTriangle(p, a, b, c) {
    const s1 = cross(a, b, p), s2 = cross(b, c, p), s3 = cross(c, a, p)
    return (s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0)
}

// Ear clipping of a simple polygon wound counter-clockwise: triples of
// point indexes.
function triangulate(points) {
    const count = points.length
    if (count < 3) return []
    const order = []
    for (let i = 0; i < count; ++i) order.push(i)
    const triangles = []
    let guard = 0
    while (order.length > 3 && guard++ < count * count) {
        let clipped = false
        for (let i = 0; i < order.length; ++i) {
            const i0 = order[(i + order.length - 1) % order.length], i1 = order[i], i2 = order[(i + 1) % order.length]
            const a = points[i0], b = points[i1], c = points[i2]
            if (cross(a, b, c) <= 1e-9) continue
            let blocked = false
            for (const k of order) {
                if (k === i0 || k === i1 || k === i2) continue
                if (inTriangle(points[k], a, b, c)) { blocked = true; break }
            }
            if (blocked) continue
            triangles.push([i0, i1, i2])
            order.splice(i, 1)
            clipped = true
            break
        }
        // A polygon no ear fits (a twisted outline): take any convex corner.
        if (!clipped) {
            let taken = false
            for (let i = 0; i < order.length; ++i) {
                const i0 = order[(i + order.length - 1) % order.length], i1 = order[i], i2 = order[(i + 1) % order.length]
                if (cross(points[i0], points[i1], points[i2]) > 0) {
                    triangles.push([i0, i1, i2]); order.splice(i, 1); taken = true; break
                }
            }
            if (!taken) order.splice(0, 1)
        }
    }
    if (order.length === 3) triangles.push([order[0], order[1], order[2]])
    return triangles
}

// The solid of a picture's mask: caps and walls for every outline. Fifty
// units thick (a quarter either way of the glyph's frame); the scene
// scales it by Icon depth.
function solidMesh(data, width, height, options) {
    const settings = options || ({})
    const mask = maskFromImageData(data, width, height, settings.threshold)
    const loops = outlines(mask, width, height)
    const total = loops.reduce(function(sum, loop) { return sum + signedArea(loop) }, 0)
    const kept = loops.filter(function(loop) { return signedArea(loop) >= total * (settings.minimumShare || 0.01) })
    const positions = [], normals = [], uv0s = [], indexes = []
    const tolerance = settings.tolerance === undefined ? 0.75 : settings.tolerance
    for (const loop of kept) {
        // Picture coordinates to the glyph's frame: y up, a hundred units
        // square, wound counter-clockwise seen from the front.
        const polygon = simplify(loop, tolerance).map(function(point) {
            return [(point[0] / width - 0.5) * 100, (0.5 - point[1] / height) * 100, point[0] / width, 1 - point[1] / height]
        })
        if (signedArea(polygon) < 0) polygon.reverse()
        if (polygon.length < 3) continue
        const front = positions.length
        for (const point of polygon) {
            positions.push([point[0], point[1], 25]); normals.push([0, 0, 1]); uv0s.push([point[2], point[3]])
        }
        const back = positions.length
        for (const point of polygon) {
            positions.push([point[0], point[1], -25]); normals.push([0, 0, -1]); uv0s.push([point[2], point[3]])
        }
        for (const triangle of triangulate(polygon)) {
            indexes.push(front + triangle[0], front + triangle[1], front + triangle[2])
            indexes.push(back + triangle[2], back + triangle[1], back + triangle[0])
        }
        // The walls: one quad per edge, its normal outward (the loop runs
        // counter-clockwise, so the outside is on the right of each edge),
        // its colour the picture's just inside the edge: read on the edge
        // itself the picture blends with the clear outside.
        const inset = (settings.wallInset === undefined ? 1 : settings.wallInset) / 100
        for (let i = 0; i < polygon.length; ++i) {
            const a = polygon[i], b = polygon[(i + 1) % polygon.length]
            const dx = b[0] - a[0], dy = b[1] - a[1]
            const length = Math.hypot(dx, dy) || 1e-9
            const normal = [dy / length, -dx / length, 0]
            const inward = [-dy / length * inset, dx / length * inset]
            // Along the edge as well, so a corner's walls stay off the
            // neighbouring edge.
            const slide = Math.min(inset, length / 200)
            const along = [dx / length * slide, dy / length * slide]
            const base = positions.length
            positions.push([a[0], a[1], 25], [b[0], b[1], 25], [b[0], b[1], -25], [a[0], a[1], -25])
            normals.push(normal, normal, normal, normal)
            const atA = [a[2] + inward[0] + along[0], a[3] + inward[1] + along[1]]
            const atB = [b[2] + inward[0] - along[0], b[3] + inward[1] - along[1]]
            uv0s.push(atA, atB, atB, atA)
            indexes.push(base, base + 2, base + 1, base, base + 3, base + 2)
        }
    }
    return { positions: positions, normals: normals, uv0s: uv0s, indexes: indexes, loops: kept.length, depth: 25 }
}

// ---------------------------------------------------------------------
// The relief of a picture: its colour parts inflated into rounded volumes.
// The picture as a whole rises like a cushion from its rim; each part of
// one colour rises again from its own edge, a thin part a little and a
// broad part more; a part inside another stands a step above it. The
// light then rounds every part, as it rounds a modelled logo.

// The picture's parts of one colour: each pixel of the mask grouped with
// the neighbours whose colour is within `tolerance` (the sum of the three
// channel differences) of the group's first pixel, or within `step` of
// the pixel itself, so a shaded part stays whole while a hard edge parts
// two colours; groups of nearly the same mean colour that touch are then
// one part; parts too small to matter go into the neighbour they touch
// most. Returns the part of every pixel (-1 where clear) and the parts.
function colourParts(data, mask, width, height, options) {
    const settings = options || ({})
    const tolerance = settings.tolerance === undefined ? 90 : settings.tolerance
    const step = settings.step === undefined ? 30 : settings.step
    const minimumShare = settings.minimumShare === undefined ? 0.01 : settings.minimumShare
    const labels = new Int32Array(width * height).fill(-1)
    const groups = []
    const queue = new Int32Array(width * height)
    for (let start = 0; start < width * height; ++start) {
        if (!mask[start] || labels[start] >= 0) continue
        const group = groups.length
        const seed = [data[start * 4], data[start * 4 + 1], data[start * 4 + 2]]
        const sum = [0, 0, 0]
        let count = 0, head = 0, tail = 0
        queue[tail++] = start
        labels[start] = group
        while (head < tail) {
            const index = queue[head++]
            sum[0] += data[index * 4]; sum[1] += data[index * 4 + 1]; sum[2] += data[index * 4 + 2]
            ++count
            const x = index % width, y = (index - x) / width
            const neighbours = [index - 1, index + 1, index - width, index + width]
            for (let side = 0; side < 4; ++side) {
                if (side === 0 && x === 0) continue
                if (side === 1 && x === width - 1) continue
                if (side === 2 && y === 0) continue
                if (side === 3 && y === height - 1) continue
                const next = neighbours[side]
                if (!mask[next] || labels[next] >= 0) continue
                const fromSeed = Math.abs(data[next * 4] - seed[0]) + Math.abs(data[next * 4 + 1] - seed[1])
                    + Math.abs(data[next * 4 + 2] - seed[2])
                const fromHere = Math.abs(data[next * 4] - data[index * 4]) + Math.abs(data[next * 4 + 1] - data[index * 4 + 1])
                    + Math.abs(data[next * 4 + 2] - data[index * 4 + 2])
                if (fromSeed > tolerance && fromHere > step) continue
                labels[next] = group
                queue[tail++] = next
            }
        }
        groups.push({ count: count, colour: [sum[0] / count, sum[1] / count, sum[2] / count] })
    }
    // Touching groups of nearly the same colour are one part.
    const owner = groups.map(function(group, index) { return index })
    const find = function(index) {
        while (owner[index] !== index) { owner[index] = owner[owner[index]]; index = owner[index] }
        return index
    }
    const touching = function(a, b) {
        return Math.abs(groups[a].colour[0] - groups[b].colour[0]) + Math.abs(groups[a].colour[1] - groups[b].colour[1])
            + Math.abs(groups[a].colour[2] - groups[b].colour[2]) <= tolerance
    }
    for (let index = 0; index < width * height; ++index) {
        if (labels[index] < 0) continue
        const x = index % width
        for (const next of [x < width - 1 ? index + 1 : -1, index + width < width * height ? index + width : -1]) {
            if (next < 0 || labels[next] < 0 || labels[next] === labels[index]) continue
            const a = find(labels[index]), b = find(labels[next])
            if (a !== b && touching(a, b)) owner[b] = a
        }
    }
    const merged = relabel(labels, groups, owner.map(function(_, index) { return find(index) }), width, height)
    // Specks go into the neighbour they touch most, until none is left.
    const total = merged.parts.reduce(function(sum, part) { return sum + part.count }, 0)
    for (let round = 0; round < 8; ++round) {
        const small = merged.parts.map(function(part) { return part.count < total * minimumShare })
        if (!small.some(function(flag) { return flag })) break
        const contact = merged.parts.map(function() { return new Map() })
        for (let index = 0; index < width * height; ++index) {
            const label = merged.labels[index]
            if (label < 0) continue
            const x = index % width
            for (const next of [x < width - 1 ? index + 1 : -1, index + width < width * height ? index + width : -1]) {
                if (next < 0 || merged.labels[next] < 0 || merged.labels[next] === label) continue
                const other = merged.labels[next]
                contact[label].set(other, (contact[label].get(other) || 0) + 1)
                contact[other].set(label, (contact[other].get(label) || 0) + 1)
            }
        }
        const target = merged.parts.map(function(part, index) { return index })
        for (let index = 0; index < merged.parts.length; ++index) {
            if (!small[index]) continue
            let best = -1, most = 0
            for (const [other, shared] of contact[index])
                if (!small[other] && shared > most) { most = shared; best = other }
            if (best < 0)
                for (const [other, shared] of contact[index])
                    if (shared > most) { most = shared; best = other }
            if (best >= 0) target[index] = best
        }
        const settled = target.map(function(to, index) {
            let guard = 0
            while (target[to] !== to && guard++ < 16) to = target[to]
            return to
        })
        const next = relabel(merged.labels, merged.parts, settled, width, height)
        merged.labels = next.labels; merged.parts = next.parts
    }
    return merged
}

// Pixels and groups renumbered after `owner` sends every group to the one
// it belongs to: the parts in order of their first pixel, with their
// counts and mean colours.
function relabel(labels, groups, owner, width, height) {
    const numbers = new Map()
    const parts = []
    const result = new Int32Array(width * height).fill(-1)
    for (let index = 0; index < width * height; ++index) {
        if (labels[index] < 0) continue
        const root = owner[labels[index]]
        if (!numbers.has(root)) {
            numbers.set(root, parts.length)
            parts.push({ count: 0, colour: [0, 0, 0] })
        }
        const number = numbers.get(root)
        result[index] = number
        const part = parts[number], group = groups[labels[index]]
        part.colour[0] += group.colour[0]; part.colour[1] += group.colour[1]; part.colour[2] += group.colour[2]
        ++part.count
    }
    for (const part of parts) {
        part.colour[0] /= part.count; part.colour[1] /= part.count; part.colour[2] /= part.count
    }
    return { labels: result, parts: parts }
}

// Which layer each part stands on: the parts at the rim or the picture's
// edge are the ground, the parts touching only those stand one step up,
// and so on inward.
function partLayers(labels, parts, width, height) {
    const touches = parts.map(function() { return new Set() })
    const ground = parts.map(function() { return false })
    for (let index = 0; index < width * height; ++index) {
        const label = labels[index]
        if (label < 0) continue
        const x = index % width, y = (index - x) / width
        const sides = [[x - 1, y], [x + 1, y], [x, y - 1], [x, y + 1]]
        for (const side of sides) {
            if (side[0] < 0 || side[1] < 0 || side[0] >= width || side[1] >= height) { ground[label] = true; continue }
            const other = labels[side[1] * width + side[0]]
            if (other < 0) ground[label] = true
            else if (other !== label) touches[label].add(other)
        }
    }
    const layers = parts.map(function() { return -1 })
    let front = []
    for (let index = 0; index < parts.length; ++index)
        if (ground[index]) { layers[index] = 0; front.push(index) }
    if (!front.length && parts.length) { layers[0] = 0; front.push(0) }
    while (front.length) {
        const next = []
        for (const index of front)
            for (const other of touches[index])
                if (layers[other] < 0) { layers[other] = layers[index] + 1; next.push(other) }
        front = next
    }
    return layers.map(function(layer) { return layer < 0 ? 0 : layer })
}

// The distance, in pixels, from each pixel that `inside` holds for to the
// nearest pixel it does not (the picture's edge counts as outside), by
// the two-pass chamfer method with the 3-4 weights.
function distanceInside(inside, width, height) {
    const far = 3 * (width + height)
    const distance = new Float32Array(width * height)
    for (let index = 0; index < width * height; ++index) distance[index] = inside(index) ? far : 0
    const at = function(x, y) {
        return x < 0 || y < 0 || x >= width || y >= height ? 0 : distance[y * width + x]
    }
    for (let y = 0; y < height; ++y)
        for (let x = 0; x < width; ++x) {
            const index = y * width + x
            if (!distance[index]) continue
            distance[index] = Math.min(distance[index], at(x - 1, y) + 3, at(x, y - 1) + 3, at(x - 1, y - 1) + 4, at(x + 1, y - 1) + 4)
        }
    for (let y = height - 1; y >= 0; --y)
        for (let x = width - 1; x >= 0; --x) {
            const index = y * width + x
            if (!distance[index]) continue
            distance[index] = Math.min(distance[index], at(x + 1, y) + 3, at(x, y + 1) + 3, at(x + 1, y + 1) + 4, at(x - 1, y + 1) + 4)
        }
    for (let index = 0; index < width * height; ++index) distance[index] /= 3
    return distance
}

// The height of every pixel of the relief, in the glyph's frame units: a
// rounded rise over each part, the part's step, and a low cushion over
// the whole picture. A round profile: a half circle over the distance to
// the edge, so an edge meets the ground steeply and a middle lies flat;
// the parts carry the volume, each its own rounded body with a crease
// to the next, twice a half circle's height, so at half the Icon depth
// a part is a full half round.
function reliefHeights(labels, parts, layers, width, height, settings) {
    const dome = settings.dome === undefined ? 12 : settings.dome
    const bump = settings.bump === undefined ? 2 : settings.bump
    const step = settings.step === undefined ? 12 : settings.step
    const unit = 100 / width
    const whole = distanceInside(function(index) { return labels[index] >= 0 }, width, height)
    let reach = 1
    for (let index = 0; index < width * height; ++index) reach = Math.max(reach, whole[index])
    const heights = new Float32Array(width * height)
    for (let index = 0; index < width * height; ++index) {
        if (labels[index] < 0) continue
        const d = whole[index]
        heights[index] = dome * Math.sqrt(Math.max(0, d * (2 * reach - d))) / reach + step * layers[labels[index]]
    }
    for (let part = 0; part < parts.length; ++part) {
        const own = distanceInside(function(index) { return labels[index] === part }, width, height)
        let radius = 1
        for (let index = 0; index < width * height; ++index) radius = Math.max(radius, own[index])
        for (let index = 0; index < width * height; ++index) {
            if (labels[index] !== part) continue
            const d = own[index]
            heights[index] += bump * unit * Math.sqrt(Math.max(0, d * (2 * radius - d)))
        }
    }
    return heights
}

// The relief of a picture's mask as a mesh in the glyph's frame: a grid
// over the picture's pixels, each corner at the height of the pixels round
// it, at the rim on the ground; the front rises toward the viewer, the back
// the same way behind, so the icon is a rounded volume from every side.
// The picture is read half a pixel inside the rim, where it is wholly its
// own. Normals follow the slopes, so the light rounds the parts.
function reliefMesh(data, width, height, options) {
    const settings = options || ({})
    const mask = maskFromImageData(data, width, height, settings.threshold)
    const parted = colourParts(data, mask, width, height, settings)
    const layers = partLayers(parted.labels, parted.parts, width, height)
    const heights = reliefHeights(parted.labels, parted.parts, layers, width, height, settings)
    const columns = width + 1, rows = height + 1
    const unit = 100 / width
    const cell = function(x, y) { return x >= 0 && y >= 0 && x < width && y < height && mask[y * width + x] ? y * width + x : -1 }
    // Each corner's height and texture point.
    const z = new Float32Array(columns * rows)
    const used = new Uint8Array(columns * rows)
    const uv = []
    for (let y = 0; y < rows; ++y)
        for (let x = 0; x < columns; ++x) {
            const cells = [cell(x - 1, y - 1), cell(x, y - 1), cell(x - 1, y), cell(x, y)]
            const opaque = cells.filter(function(index) { return index >= 0 })
            const corner = y * columns + x
            if (!opaque.length) { uv.push(null); continue }
            used[corner] = 1
            if (opaque.length === 4) {
                z[corner] = (heights[cells[0]] + heights[cells[1]] + heights[cells[2]] + heights[cells[3]]) / 4
                uv.push([x / width, 1 - y / height])
            } else {
                z[corner] = 0
                // Toward the middle of the opaque pixels round the corner.
                let dx = 0, dy = 0
                for (const index of opaque) { dx += (index % width) + 0.5 - x; dy += (index - index % width) / width + 0.5 - y }
                const length = Math.hypot(dx, dy) || 1
                uv.push([(x + 0.5 * dx / length) / width, 1 - (y + 0.5 * dy / length) / height])
            }
        }
    const positions = [], normals = [], uv0s = [], indexes = []
    const front = new Int32Array(columns * rows).fill(-1), back = new Int32Array(columns * rows).fill(-1)
    const at = function(x, y) { return x < 0 || y < 0 || x >= columns || y >= rows ? 0 : z[y * columns + x] }
    for (let y = 0; y < rows; ++y)
        for (let x = 0; x < columns; ++x) {
            const corner = y * columns + x
            if (!used[corner]) continue
            const slopeX = (at(x + 1, y) - at(x - 1, y)) / (2 * unit)
            const slopeY = (at(x, y - 1) - at(x, y + 1)) / (2 * unit)
            const length = Math.hypot(slopeX, slopeY, 1)
            const normal = [-slopeX / length, -slopeY / length, 1 / length]
            const px = (x / width - 0.5) * 100, py = (0.5 - y / height) * 100
            front[corner] = positions.length
            positions.push([px, py, z[corner]]); normals.push(normal); uv0s.push(uv[corner])
            back[corner] = positions.length
            positions.push([px, py, -z[corner]]); normals.push([normal[0], normal[1], -normal[2]]); uv0s.push(uv[corner])
        }
    for (let y = 0; y < height; ++y)
        for (let x = 0; x < width; ++x) {
            if (cell(x, y) < 0) continue
            const a = y * columns + x, b = a + 1, c = a + columns, d = c + 1
            indexes.push(front[a], front[c], front[d], front[a], front[d], front[b])
            indexes.push(back[a], back[d], back[c], back[a], back[b], back[d])
        }
    let top = 0, depth = 0
    for (const layer of layers) top = Math.max(top, layer)
    for (const corner of z) depth = Math.max(depth, corner)
    return { positions: positions, normals: normals, uv0s: uv0s, indexes: indexes,
             parts: parted.parts.length, layers: top + 1, depth: depth }
}

// ---------------------------------------------------------------------
// Which relief a picture asks for. A plate, an outline that fills its box
// like a rounded square or a rectangle (a sign, a badge, a tile), gets a
// block: just thickness. Any other outline (a round picture of a thing,
// a figure, a star) gets its parts rounded.

// The convex hull of points, by the monotone chain method, wound
// counter-clockwise in picture coordinates.
function convexHull(points) {
    const sorted = points.slice().sort(function(a, b) { return a[0] - b[0] || a[1] - b[1] })
    if (sorted.length < 3) return sorted
    const lower = [], upper = []
    for (const point of sorted) {
        while (lower.length >= 2 && cross(lower[lower.length - 2], lower[lower.length - 1], point) <= 0) lower.pop()
        lower.push(point)
    }
    for (let index = sorted.length - 1; index >= 0; --index) {
        const point = sorted[index]
        while (upper.length >= 2 && cross(upper[upper.length - 2], upper[upper.length - 1], point) <= 0) upper.pop()
        upper.push(point)
    }
    return lower.slice(0, lower.length - 1).concat(upper.slice(0, upper.length - 1))
}

// How the mask's outline fills its box and its convex hull, and the kind
// of relief that asks for: "block" for a plate, "rounded" otherwise.
function reliefKind(mask, width, height, options) {
    const settings = options || ({})
    const boxFillNeeded = settings.boxFill === undefined ? 0.84 : settings.boxFill
    const hullFillNeeded = settings.hullFill === undefined ? 0.88 : settings.hullFill
    let area = 0, left = width, right = -1, top = height, bottom = -1
    for (let y = 0; y < height; ++y)
        for (let x = 0; x < width; ++x) {
            if (!mask[y * width + x]) continue
            ++area
            left = Math.min(left, x); right = Math.max(right, x); top = Math.min(top, y); bottom = Math.max(bottom, y)
        }
    if (!area) return { kind: "block", boxFill: 0, hullFill: 0 }
    const boxFill = area / ((right - left + 1) * (bottom - top + 1))
    const points = []
    for (const loop of outlines(mask, width, height)) for (const point of loop) points.push(point)
    const hull = convexHull(points)
    const hullArea = hull.length >= 3 ? Math.abs(signedArea(hull)) : area
    const hullFill = Math.min(1, area / Math.max(1, hullArea))
    return { kind: boxFill >= boxFillNeeded && hullFill >= hullFillNeeded ? "block" : "rounded",
             boxFill: boxFill, hullFill: hullFill }
}

// The mesh of a picture for the relief asked for: "rounded", "block", or
// "auto" (the picture's own kind). The mesh tells which kind it is.
function iconMesh(data, width, height, relief, options) {
    const settings = options || ({})
    let kind = relief
    if (kind !== "rounded" && kind !== "block") {
        const mask = maskFromImageData(data, width, height, settings.threshold)
        kind = reliefKind(mask, width, height, settings).kind
    }
    const mesh = kind === "block" ? solidMesh(data, width, height, settings) : reliefMesh(data, width, height, settings)
    mesh.kind = kind
    return mesh
}
