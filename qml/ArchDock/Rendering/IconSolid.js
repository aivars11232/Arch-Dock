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
    const step = settings.step === undefined ? 0 : settings.step
    const contrast = settings.contrast === undefined ? 30 : settings.contrast
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
    // Touching groups whose colours meet softly along their shared edge
    // (a shaded part grown in bands) are one part; a hard edge keeps
    // two colours apart however close their means.
    const owner = groups.map(function(group, index) { return index })
    const find = function(index) {
        while (owner[index] !== index) { owner[index] = owner[owner[index]]; index = owner[index] }
        return index
    }
    const edges = new Map()
    for (let index = 0; index < width * height; ++index) {
        if (labels[index] < 0) continue
        const x = index % width
        for (const next of [x < width - 1 ? index + 1 : -1, index + width < width * height ? index + width : -1]) {
            if (next < 0 || labels[next] < 0 || labels[next] === labels[index]) continue
            const key = Math.min(labels[index], labels[next]) + ":" + Math.max(labels[index], labels[next])
            const difference = Math.abs(data[next * 4] - data[index * 4]) + Math.abs(data[next * 4 + 1] - data[index * 4 + 1])
                + Math.abs(data[next * 4 + 2] - data[index * 4 + 2])
            const edge = edges.get(key) || { sum: 0, count: 0 }
            edge.sum += difference; ++edge.count
            edges.set(key, edge)
        }
    }
    for (const [key, edge] of edges) {
        if (edge.sum / edge.count > contrast) continue
        const [first, second] = key.split(":").map(Number)
        const a = find(first), b = find(second)
        if (a !== b) owner[b] = a
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
    const bump = settings.bump === undefined ? 2 : settings.bump
    const step = settings.step === undefined ? 12 : settings.step
    const shapes = settings.shapes || []
    const unit = 100 / width
    const whole = distanceInside(function(index) { return labels[index] >= 0 }, width, height)
    let reach = 1
    for (let index = 0; index < width * height; ++index) reach = Math.max(reach, whole[index])
    // A round outline is a globe: the whole rises as a ball and the parts
    // ride on it; any other outline rises a little as a cushion.
    const dome = settings.globe ? reach * unit * bump / 2 : (settings.dome === undefined ? 12 : settings.dome)
    const ride = settings.globe ? 0.5 : 1
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
        const shape = shapes[part] ? shapes[part].shape : "blob"
        // A part with corners is a prism: a flat top with a bevel a
        // quarter of its radius wide; a rounded part is a half round.
        const bevel = Math.max(1, radius * 0.25), top = bump * unit * radius * 0.6
        for (let index = 0; index < width * height; ++index) {
            if (labels[index] !== part) continue
            const d = own[index]
            heights[index] += ride * (shape === "polygon" ? top * Math.min(1, d / bevel)
                                                          : bump * unit * Math.sqrt(Math.max(0, d * (2 * radius - d))))
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
    const read = iconShapes(data, width, height, settings)
    const parted = read
    const layers = partLayers(parted.labels, parted.parts, width, height)
    // A round outline, nearly a circle, filled: a globe.
    const outline = []
    for (let index = 0; index < width * height; ++index) if (parted.labels[index] >= 0) outline.push(index)
    const whole = fitEllipse(outline, width)
    const kind = reliefKind(parted.labels.map(function(label) { return label >= 0 ? 1 : 0 }), width, height)
    const globe = settings.globe !== false && whole !== null && kind.hullFill >= 0.88 && whole.b / whole.a >= 0.8
        && outline.length / (Math.PI * whole.a * whole.b) > 0.85
    const heights = reliefHeights(parted.labels, parted.parts, layers, width, height,
                                  Object.assign({}, settings, { shapes: read.shapes, globe: globe }))
    const columns = width + 1, rows = height + 1
    const unit = 100 / width
    const cell = function(x, y) { return x >= 0 && y >= 0 && x < width && y < height && parted.labels[y * width + x] >= 0 ? y * width + x : -1 }
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
    // The outline drawn smooth: each corner on the picture's edge moved
    // halfway toward the middle of its two neighbours along the edge,
    // twice, so the pixels' steps become the slants and curves they stand
    // for. A corner where the edge meets itself stays.
    const solid = function(x, y) { return cell(x, y) >= 0 }
    const placeX = new Float32Array(columns * rows), placeY = new Float32Array(columns * rows)
    const rim = []
    for (let y = 0; y < rows; ++y)
        for (let x = 0; x < columns; ++x) {
            const corner = y * columns + x
            placeX[corner] = x; placeY[corner] = y
            if (!used[corner] || (solid(x - 1, y - 1) && solid(x, y - 1) && solid(x - 1, y) && solid(x, y))) continue
            const next = []
            if (solid(x, y - 1) !== solid(x, y)) next.push(corner + 1)
            if (solid(x - 1, y - 1) !== solid(x - 1, y)) next.push(corner - 1)
            if (solid(x - 1, y) !== solid(x, y)) next.push(corner + columns)
            if (solid(x - 1, y - 1) !== solid(x, y - 1)) next.push(corner - columns)
            if (next.length === 2) rim.push([corner, next[0], next[1]])
        }
    for (let pass = 0; pass < 2; ++pass) {
        const moved = rim.map(function(entry) {
            return [0.5 * placeX[entry[0]] + 0.25 * (placeX[entry[1]] + placeX[entry[2]]),
                    0.5 * placeY[entry[0]] + 0.25 * (placeY[entry[1]] + placeY[entry[2]])]
        })
        rim.forEach(function(entry, index) { placeX[entry[0]] = moved[index][0]; placeY[entry[0]] = moved[index][1] })
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
            const px = (placeX[corner] / width - 0.5) * 100, py = (0.5 - placeY[corner] / height) * 100
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
    // The rings, as real rings through the picture's plane; they may
    // reach behind the body, which stands with its back on the plane.
    let reach = depth
    for (const ring of read.rings) {
        const torus = torusMesh(ring.ellipse, ring.band, ring.front, width, height, settings)
        const base = positions.length
        for (let index = 0; index < torus.positions.length; ++index) {
            positions.push(torus.positions[index]); normals.push(torus.normals[index]); uv0s.push(torus.uv0s[index])
        }
        for (const index of torus.indexes) indexes.push(base + index)
        reach = Math.max(reach, torus.reach)
    }
    return { positions: positions, normals: normals, uv0s: uv0s, indexes: indexes,
             parts: parted.parts.length, layers: top + 1, depth: depth, reach: reach, rings: read.rings.length, globe: globe }
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
// "auto" (the picture's own kind). The mesh tells which kind it is. With
// `globe: false` a round picture rises as parts, not as a ball (inside a
// bubble, a ball would be a bubble in the bubble).
function iconMesh(data, width, height, relief, options) {
    const settings = options || ({})
    let kind = relief
    if (kind !== "rounded" && kind !== "block") {
        const mask = maskFromImageData(data, width, height, settings.threshold)
        kind = reliefKind(mask, width, height, settings).kind
    }
    const mesh = kind === "block" ? solidMesh(data, width, height, settings) : reliefMesh(data, width, height, settings)
    mesh.kind = kind
    mesh.colour = meanColour(data, width, height, settings.threshold)
    return mesh
}

// How far a 3D scene stretches a solid's depth to show it at Icon depth
// `depth`: a modelled solid (IconModel) is made at its own depth, so by
// the ratio of the two; the solids made here, a hundred units deep, by
// Icon depth itself.
function depthStretch(mesh, depth) {
    const built = mesh ? Number(mesh.builtDepth) : 0
    return built > 0 ? depth / built : depth
}

// The picture's mean colour, its clear pixels left out: what a glow round
// its solid shines in when it takes the picture's own colours, a solid's
// glow being its shape and never a flat copy of the picture.
function meanColour(data, width, height, threshold) {
    const limit = threshold === undefined ? 96 : threshold
    let red = 0, green = 0, blue = 0, weight = 0
    for (let index = 0; index < width * height; ++index) {
        const alpha = data[index * 4 + 3]
        if (alpha <= limit) continue
        red += data[index * 4] * alpha; green += data[index * 4 + 1] * alpha; blue += data[index * 4 + 2] * alpha
        weight += alpha
    }
    return weight > 0 ? [red / weight / 255, green / weight / 255, blue / weight / 255, 1] : [1, 1, 1, 1]
}

// ---------------------------------------------------------------------
// Shapes in the picture. A logo is drawn from shapes: a ring round a
// figure, a disc, a triangle or a letter. Each part of one colour is
// read for the shape it is, and the volume follows the shape: a thin
// closed loop that fits an ellipse is a ring seen at a tilt and becomes
// a real ring (a torus) through the icon; a part with few corners rises
// as a bevelled prism; a round outline is a globe the parts ride on.

// The ellipse a set of pixels fits: centre, semi-axes (the longer first)
// and the longer axis's angle, from the pixels' second moments.
function fitEllipse(indexes, width) {
    const count = indexes.length
    if (!count) return null
    let cx = 0, cy = 0
    for (const index of indexes) { cx += index % width + 0.5; cy += Math.floor(index / width) + 0.5 }
    cx /= count; cy /= count
    let mxx = 0, myy = 0, mxy = 0
    for (const index of indexes) {
        const dx = index % width + 0.5 - cx, dy = Math.floor(index / width) + 0.5 - cy
        mxx += dx * dx; myy += dy * dy; mxy += dx * dy
    }
    mxx /= count; myy /= count; mxy /= count
    const trace = mxx + myy, det = mxx * myy - mxy * mxy
    const root = Math.sqrt(Math.max(0, trace * trace / 4 - det))
    const large = trace / 2 + root, small = Math.max(1e-6, trace / 2 - root)
    // A filled ellipse's second moment along an axis is a quarter of
    // that semi-axis squared.
    return { cx: cx, cy: cy, a: 2 * Math.sqrt(large), b: 2 * Math.sqrt(small),
             angle: axisAngle(0.5 * Math.atan2(2 * mxy, mxx - myy)), count: count }
}

// The pixels of a part together with the holes it closes round: the
// part's pixels plus every pixel not reachable from the picture's edge
// without crossing the part.
function filledPixels(labels, part, width, height) {
    const reached = new Uint8Array(width * height)
    const queue = new Int32Array(width * height)
    let head = 0, tail = 0
    const push = function(index) { if (!reached[index] && labels[index] !== part) { reached[index] = 1; queue[tail++] = index } }
    for (let x = 0; x < width; ++x) { push(x); push((height - 1) * width + x) }
    for (let y = 0; y < height; ++y) { push(y * width); push(y * width + width - 1) }
    while (head < tail) {
        const index = queue[head++]
        const x = index % width, y = (index - x) / width
        if (x > 0) push(index - 1)
        if (x < width - 1) push(index + 1)
        if (y > 0) push(index - width)
        if (y < height - 1) push(index + width)
    }
    const own = [], holes = []
    for (let index = 0; index < width * height; ++index) {
        if (labels[index] === part) own.push(index)
        else if (!reached[index]) holes.push(index)
    }
    return { own: own, holes: holes }
}

// How many corners a part's outline keeps when simplified coarsely (a
// tenth of its size), for telling a triangle or a letter from a blob.
function partCorners(labels, part, width, height) {
    const mask = new Uint8Array(width * height)
    let left = width, right = -1, top = height, bottom = -1
    for (let index = 0; index < width * height; ++index) {
        if (labels[index] !== part) continue
        mask[index] = 1
        const x = index % width, y = (index - x) / width
        left = Math.min(left, x); right = Math.max(right, x); top = Math.min(top, y); bottom = Math.max(bottom, y)
    }
    const loops = outlines(mask, width, height)
    if (!loops.length) return 0
    const size = Math.max(right - left + 1, bottom - top + 1)
    return simplify(loops[0], Math.max(1, size * 0.1)).length
}

// The ellipse a thin band's pixels lie on, by least squares on the conic
// A x^2 + B x y + C y^2 + D x + E y = 1: centre, semi-axes (the longer
// first) and the longer axis's angle; null when the pixels fit no ellipse.
function fitConic(indexes, width) {
    if (indexes.length < 6) return null
    const m = [], v = [0, 0, 0, 0, 0]
    for (let i = 0; i < 5; ++i) m.push([0, 0, 0, 0, 0])
    for (const index of indexes) {
        const x = index % width + 0.5, y = Math.floor(index / width) + 0.5
        const row = [x * x, x * y, y * y, x, y]
        for (let i = 0; i < 5; ++i) { v[i] += row[i]; for (let j = 0; j < 5; ++j) m[i][j] += row[i] * row[j] }
    }
    // Solve m w = v by elimination.
    for (let i = 0; i < 5; ++i) {
        let pivot = i
        for (let k = i + 1; k < 5; ++k) if (Math.abs(m[k][i]) > Math.abs(m[pivot][i])) pivot = k
        if (Math.abs(m[pivot][i]) < 1e-12) return null
        if (pivot !== i) { const t = m[i]; m[i] = m[pivot]; m[pivot] = t; const tv = v[i]; v[i] = v[pivot]; v[pivot] = tv }
        for (let k = i + 1; k < 5; ++k) {
            const f = m[k][i] / m[i][i]
            for (let j = i; j < 5; ++j) m[k][j] -= f * m[i][j]
            v[k] -= f * v[i]
        }
    }
    const w = [0, 0, 0, 0, 0]
    for (let i = 4; i >= 0; --i) {
        let sum = v[i]
        for (let j = i + 1; j < 5; ++j) sum -= m[i][j] * w[j]
        w[i] = sum / m[i][i]
    }
    const [A, B, C, D, E] = w
    const det = 4 * A * C - B * B
    if (det <= 1e-12) return null
    const cx = (B * E - 2 * C * D) / det, cy = (B * D - 2 * A * E) / det
    const constant = A * cx * cx + B * cx * cy + C * cy * cy + D * cx + E * cy - 1
    const half = (A + C) / 2, spread = Math.sqrt(((A - C) / 2) ** 2 + (B / 2) ** 2)
    const small = half - spread, large = half + spread
    // An ellipse: both curvatures of one sign, the centre's value of the
    // other (the normalisation to 1 may have turned every sign).
    if (!(small > 0 && large > 0 && constant < 0) && !(small < 0 && large < 0 && constant > 0)) return null
    // Each curvature's direction; the longer axis is the one with the
    // smaller curvature in size.
    const axisOf = function(curvature) {
        if (Math.abs(B) < 1e-9) return Math.abs(curvature - A) < Math.abs(curvature - C) ? 0 : Math.PI / 2
        return Math.atan2(curvature - A, B / 2)
    }
    const first = Math.sqrt(-constant / small), second = Math.sqrt(-constant / large)
    if (first >= second) return { cx: cx, cy: cy, a: first, b: second, angle: axisAngle(axisOf(small)), count: indexes.length }
    return { cx: cx, cy: cy, a: second, b: first, angle: axisAngle(axisOf(large)), count: indexes.length }
}

// An axis angle kept between a quarter turn either way, so the
// direction across the longer axis always points down the picture.
function axisAngle(angle) {
    let result = angle
    while (result > Math.PI / 2) result -= Math.PI
    while (result <= -Math.PI / 2) result += Math.PI
    return result
}

// A pixel's angle round an ellipse, in the ellipse's own frame.
function ellipseAngle(ellipse, x, y) {
    const dx = x - ellipse.cx, dy = y - ellipse.cy
    const along = dx * Math.cos(ellipse.angle) + dy * Math.sin(ellipse.angle)
    const across = -dx * Math.sin(ellipse.angle) + dy * Math.cos(ellipse.angle)
    return Math.atan2(across / ellipse.b, along / ellipse.a)
}

// What each part is: "disc" (a filled ellipse), "polygon" (few corners)
// or "blob". Rings are read from the outline's thin strokes, not here.
function partShapes(labels, parts, width, height) {
    const shapes = []
    for (let part = 0; part < parts.length; ++part) {
        const own = []
        for (let index = 0; index < width * height; ++index) if (labels[index] === part) own.push(index)
        const points = own.map(function(index) { return [index % width + 0.5, Math.floor(index / width) + 0.5] })
        const hull = convexHull(points)
        const hullArea = hull.length >= 3 ? Math.abs(signedArea(hull)) : own.length
        const hullFill = own.length / Math.max(1, hullArea)
        const ellipse = fitEllipse(own, width)
        let shape = "blob"
        const round = ellipse ? own.length / (Math.PI * ellipse.a * ellipse.b) : 0
        if (hullFill > 0.85 && round > 0.85 && round < 1.2) shape = "disc"
        else {
            const corners = partCorners(labels, part, width, height)
            if (corners >= 3 && corners <= 6) shape = "polygon"
        }
        shapes.push({ shape: shape, ellipse: ellipse, hullFill: hullFill })
    }
    return shapes
}

// The rings in the outline: its thin strokes (what an opening by
// `radius`, an erosion then a dilation, takes away), in touching runs,
// each run fitted with an ellipse; runs on one ellipse are one ring. A
// ring has to lie on its ellipse within its own band's width, go at least
// half the way round, and, when it goes nearly all the way, its hull has
// to be the ellipse's and not a frame's with corners. Each ring tells its
// ellipse, its band's width, its pixels and the angle of the gap where
// another part hides it.
function thinRings(mask, data, width, height, options) {
    const settings = options || ({})
    const radius = settings.strokeRadius === undefined ? Math.max(2.5, width * 0.045) : settings.strokeRadius
    const inside = distanceInside(function(index) { return mask[index] === 1 }, width, height)
    const core = new Uint8Array(width * height)
    for (let index = 0; index < width * height; ++index) if (inside[index] > radius) core[index] = 1
    const toCore = distanceInside(function(index) { return !core[index] }, width, height)
    const thin = new Uint8Array(width * height)
    for (let index = 0; index < width * height; ++index) if (mask[index] && toCore[index] > radius) thin[index] = 1
    const seen = new Uint8Array(width * height)
    const runs = []
    for (let start = 0; start < width * height; ++start) {
        if (!thin[start] || seen[start]) continue
        const run = [start]; seen[start] = 1
        for (let head = 0; head < run.length; ++head) {
            const index = run[head], x = index % width, y = (index - x) / width
            for (const side of [x > 0 ? index - 1 : -1, x < width - 1 ? index + 1 : -1, y > 0 ? index - width : -1, y < height - 1 ? index + width : -1])
                if (side >= 0 && thin[side] && !seen[side]) { seen[side] = 1; run.push(side) }
        }
        if (run.length >= Math.max(8, width * height * 0.003)) runs.push(run)
    }
    const fits = function(conic, pixels, stroke) {
        let near = 0
        const bins = new Uint8Array(36)
        for (const index of pixels) {
            const x = index % width + 0.5, y = Math.floor(index / width) + 0.5
            const angle = ellipseAngle(conic, x, y)
            const rx = Math.cos(angle) * conic.a, ry = Math.sin(angle) * conic.b
            const px = conic.cx + rx * Math.cos(conic.angle) - ry * Math.sin(conic.angle)
            const py = conic.cy + rx * Math.sin(conic.angle) + ry * Math.cos(conic.angle)
            if (Math.hypot(x - px, y - py) <= Math.max(1.5, stroke * 0.75)) ++near
            bins[Math.floor(((angle + Math.PI) / (2 * Math.PI)) * 36) % 36] = 1
        }
        let covered = 0
        for (const bin of bins) covered += bin
        return { near: near / pixels.length, covered: covered, bins: bins }
    }
    // A band's width from its pixels' distances to its edge: they run
    // from one to half the width, so the mean is a quarter of the width
    // and a half.
    const strokeOf = function(pixels) {
        let sum = 0
        for (const index of pixels) sum += inside[index]
        return Math.max(1, 4 * sum / pixels.length - 2)
    }
    // Runs are grouped greedily, the largest first: a run joins a group
    // when the group and the run together lie on one ellipse, so the arcs
    // of a ring cut by the figure it crosses find each other.
    runs.sort(function(a, b) { return b.length - a.length })
    const rings = []
    const taken = runs.map(function() { return false })
    for (let first = 0; first < runs.length; ++first) {
        if (taken[first]) continue
        let pixels = runs[first].slice()
        let conic = fitConic(pixels, width)
        if (!conic || conic.b / conic.a < 0.08 || conic.a > width) continue
        taken[first] = true
        let grew = true
        while (grew) {
            grew = false
            for (let other = 0; other < runs.length; ++other) {
                if (taken[other]) continue
                const joint = pixels.concat(runs[other])
                const fit = fitConic(joint, width)
                if (!fit || fit.b < 2 || fit.b / fit.a < 0.12 || fit.a > width) continue
                const stroke = strokeOf(joint)
                if (stroke > fit.a * 0.3 || fits(fit, joint, stroke).near < 0.8) continue
                pixels = joint; conic = fit; taken[other] = true; grew = true
            }
        }
        rings.push({ conic: conic, pixels: pixels })
    }
    const result = []
    for (const ring of rings) {
        const whole = fitConic(ring.pixels, width) || ring.conic
        const stroke = strokeOf(ring.pixels)
        const fit = fits(whole, ring.pixels, stroke)
        if (fit.near < 0.8 || fit.covered < 18) continue
        if (fit.covered >= 30) {
            const hull = convexHull(ring.pixels.map(function(index) { return [index % width + 0.5, Math.floor(index / width) + 0.5] }))
            const hullRound = Math.abs(signedArea(hull)) / (Math.PI * (whole.a + stroke / 2) * (whole.b + stroke / 2))
            if (hullRound > 1.1) continue
        }
        // The whole band along the ellipse: every pixel within the band's
        // half width. Where the ring crosses in front of the figure its
        // edges show as sharp changes of colour across the band; where the
        // figure hides it the band lies on the figure's smooth surface.
        // The widest stretch without the ring's edges is the gap.
        const band = []
        const edgy = new Float32Array(36), total = new Float32Array(36)
        const read = function(index) { return index >= 0 && index < width * height ? [data[index * 4], data[index * 4 + 1], data[index * 4 + 2]] : null }
        for (let index = 0; index < width * height; ++index) {
            if (!mask[index]) continue
            const x = index % width + 0.5, y = Math.floor(index / width) + 0.5
            const angle = ellipseAngle(whole, x, y)
            const rx = Math.cos(angle) * whole.a, ry = Math.sin(angle) * whole.b
            const px = whole.cx + rx * Math.cos(whole.angle) - ry * Math.sin(whole.angle)
            const py = whole.cy + rx * Math.sin(whole.angle) + ry * Math.cos(whole.angle)
            // Half the band and a pixel of softened edge.
            if (Math.hypot(x - px, y - py) > Math.max(2, stroke * 0.5 + 1.5)) continue
            band.push(index)
            const bin = Math.floor(((angle + Math.PI) / (2 * Math.PI)) * 36) % 36
            ++total[bin]
            const here = read(index)
            let sharpest = 0
            const ix = index % width
            for (const side of [ix > 0 ? index - 1 : -1, ix < width - 1 ? index + 1 : -1, index - width, index + width]) {
                const there = read(side)
                if (!there) continue
                sharpest = Math.max(sharpest, Math.abs(here[0] - there[0]) + Math.abs(here[1] - there[1]) + Math.abs(here[2] - there[2]))
            }
            if (sharpest > 60) ++edgy[bin]
        }
        const shown = function(bin) { return total[bin] > 0 && (fit.bins[bin] || edgy[bin] / total[bin] >= 0.2) }
        let gap = null, bestStart = -1, bestLength = 0
        for (let start = 0; start < 36; ++start) {
            if (shown(start)) continue
            let length = 0
            while (length < 36 && !shown((start + length) % 36)) ++length
            if (length > bestLength) { bestLength = length; bestStart = start }
        }
        if (bestLength > 0 && bestLength < 36) gap = ((bestStart + bestLength / 2) / 36) * 2 * Math.PI - Math.PI
        result.push({ ellipse: whole, band: stroke, pixels: band, gap: gap })
    }
    return result
}

// A ring as a torus in the glyph's frame, tilted so that it projects to
// the ellipse it was drawn as; `front` is the angle, in the ellipse's
// frame, of the side that comes toward the viewer. Texture coordinates
// follow the projection, so the ring wears its own colours.
function torusMesh(ellipse, band, front, width, height, options) {
    const settings = options || ({})
    const around = settings.around || 48, across = settings.across || 12
    const unit = 100 / width
    const R = ellipse.a * unit
    const r = Math.max(1.5, band / 2 * unit)
    // The tilt about the longer axis lifts one side of the ring toward
    // the viewer: the side the front angle points to.
    let tilt = Math.acos(Math.max(0, Math.min(1, ellipse.b / ellipse.a)))
    if (Math.sin(front) < 0) tilt = -tilt
    const cosT = Math.cos(tilt), sinT = Math.sin(tilt)
    const cosA = Math.cos(ellipse.angle), sinA = Math.sin(ellipse.angle)
    const cx = (ellipse.cx / width - 0.5) * 100, cy = (0.5 - ellipse.cy / height) * 100
    const positions = [], normals = [], uv0s = [], indexes = []
    // In the ring's own frame: X along the longer axis, Y along the
    // shorter one, Z toward the viewer; then the tilt about X, the turn
    // about Z and the move to the centre. Picture y runs down, so the
    // frame's y is the picture's negated.
    const place = function(x, y, z) {
        const ty = y * cosT - z * sinT, tz = y * sinT + z * cosT
        const rx = x * cosA - ty * sinA, ry = x * sinA + ty * cosA
        return [rx, -ry, tz]
    }
    for (let i = 0; i < around; ++i) {
        const u = i / around * 2 * Math.PI
        for (let j = 0; j < across; ++j) {
            const v = j / across * 2 * Math.PI
            const p = place((R + r * Math.cos(v)) * Math.cos(u), (R + r * Math.cos(v)) * Math.sin(u), r * Math.sin(v))
            const n = place(Math.cos(v) * Math.cos(u), Math.cos(v) * Math.sin(u), Math.sin(v))
            positions.push([cx + p[0], cy + p[1], p[2]])
            normals.push(n)
            uv0s.push([(cx + p[0]) / 100 + 0.5, (cy + p[1]) / 100 + 0.5])
        }
    }
    for (let i = 0; i < around; ++i)
        for (let j = 0; j < across; ++j) {
            const a = i * across + j, b = ((i + 1) % around) * across + j
            const c = i * across + (j + 1) % across, d = ((i + 1) % around) * across + (j + 1) % across
            indexes.push(a, b, d, a, d, c)
        }
    return { positions: positions, normals: normals, uv0s: uv0s, indexes: indexes, reach: R * Math.abs(sinT) + r }
}

// Which side of a ring comes toward the viewer: the side opposite the
// gap where another part hides it; a ring hidden nowhere shows its lower
// half nearer, as a ring seen from a little above does. Given as the
// angle, in the ellipse's frame, of the nearest point.
function ringFront(shape) {
    if (shape.gap !== null && shape.gap !== undefined) return shape.gap + Math.PI
    // Straight down in the picture (y grows downward) in the frame.
    return ellipseAngle(shape.ellipse, shape.ellipse.cx, shape.ellipse.cy + shape.ellipse.b)
}

function insideHull(hull, point) {
    if (hull.length < 3) return false
    for (let i = 0; i < hull.length; ++i)
        if (cross(hull[i], hull[(i + 1) % hull.length], point) < 0) return false
    return true
}

// The rings taken out of the parts: a ring's pixels inside the other
// pixels' convex hull go to the nearest part (so a figure the ring
// crosses in front of is whole again), the rest become clear; parts of
// one colour that now touch are one part.
function bridgeRings(labels, parts, rings, width, height) {
    if (!rings.length) return { labels: labels, parts: parts, rings: [] }
    const ringPixels = new Uint8Array(width * height)
    for (const ring of rings) for (const index of ring.pixels) ringPixels[index] = 1
    const others = []
    for (let index = 0; index < width * height; ++index)
        if (labels[index] >= 0 && !ringPixels[index]) others.push([index % width + 0.5, Math.floor(index / width) + 0.5])
    const hull = convexHull(others)
    const next = new Int32Array(labels)
    const pending = new Set()
    for (let index = 0; index < width * height; ++index) {
        if (!ringPixels[index]) continue
        next[index] = -1
        if (insideHull(hull, [index % width + 0.5, Math.floor(index / width) + 0.5])) pending.add(index)
    }
    // Grow the other parts into the ring's pixels within the hull.
    let guard = 0
    while (pending.size && guard++ < width + height) {
        const settled = []
        for (const index of pending) {
            const x = index % width, y = (index - x) / width
            const sides = [x > 0 ? index - 1 : -1, x < width - 1 ? index + 1 : -1, y > 0 ? index - width : -1, y < height - 1 ? index + width : -1]
            for (const side of sides)
                if (side >= 0 && next[side] >= 0 && !pending.has(side)) { settled.push([index, next[side]]); break }
        }
        if (!settled.length) break
        for (const [index, part] of settled) { next[index] = part; pending.delete(index) }
    }
    // Renumber, then merge touching parts of one colour.
    const owner = parts.map(function(part, index) { return index })
    const find = function(index) { while (owner[index] !== index) index = owner[index]; return index }
    const close = function(a, b) {
        return Math.abs(parts[a].colour[0] - parts[b].colour[0]) + Math.abs(parts[a].colour[1] - parts[b].colour[1])
            + Math.abs(parts[a].colour[2] - parts[b].colour[2]) <= 90
    }
    for (let index = 0; index < width * height; ++index) {
        if (next[index] < 0) continue
        const x = index % width
        for (const side of [x < width - 1 ? index + 1 : -1, index + width < width * height ? index + width : -1]) {
            if (side < 0 || next[side] < 0 || next[side] === next[index]) continue
            const a = find(next[index]), b = find(next[side])
            if (a !== b && close(a, b)) owner[b] = a
        }
    }
    const merged = relabel(next, parts, owner.map(function(_, index) { return find(index) }), width, height)
    const folded = foldTiny(merged.labels, merged.parts, width, height, Math.max(4, width * height * 0.01))
    return { labels: folded.labels, parts: folded.parts,
             rings: rings.map(function(ring) { return { ellipse: ring.ellipse, band: ring.band, front: ringFront(ring) } }) }
}

// Parts smaller than `minimum` pixels go to the neighbour they touch
// most, or away when they touch none.
function foldTiny(labels, parts, width, height, minimum) {
    const small = parts.map(function(part) { return part.count < minimum })
    if (!small.some(function(flag) { return flag })) return { labels: labels, parts: parts }
    const contact = parts.map(function() { return new Map() })
    for (let index = 0; index < width * height; ++index) {
        const label = labels[index]
        if (label < 0) continue
        const x = index % width
        for (const next of [x < width - 1 ? index + 1 : -1, index + width < width * height ? index + width : -1]) {
            if (next < 0 || labels[next] < 0 || labels[next] === label) continue
            contact[label].set(labels[next], (contact[label].get(labels[next]) || 0) + 1)
            contact[labels[next]].set(label, (contact[labels[next]].get(label) || 0) + 1)
        }
    }
    const target = parts.map(function(part, index) { return index })
    for (let index = 0; index < parts.length; ++index) {
        if (!small[index]) continue
        let best = -1, most = 0
        for (const [other, shared] of contact[index]) if (!small[other] && shared > most) { most = shared; best = other }
        target[index] = best
    }
    const next = new Int32Array(labels)
    for (let index = 0; index < width * height; ++index)
        if (next[index] >= 0 && target[next[index]] !== next[index]) next[index] = target[next[index]]
    const owner = parts.map(function(part, index) { return target[index] < 0 ? index : target[index] })
    return relabel(next, parts, owner, width, height)
}

// The picture read as shapes: its parts, with rings taken out as rings.
function iconShapes(data, width, height, options) {
    const settings = options || ({})
    const mask = maskFromImageData(data, width, height, settings.threshold)
    const grouped = colourParts(data, mask, width, height, settings)
    const rings = thinRings(mask, data, width, height, settings)
    const bridged = bridgeRings(grouped.labels, grouped.parts, rings, width, height)
    const shapes = partShapes(bridged.labels, bridged.parts, width, height)
    return { mask: mask, labels: bridged.labels, parts: bridged.parts, shapes: shapes, rings: bridged.rings }
}
