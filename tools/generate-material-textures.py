#!/usr/bin/env python3
"""Original Arch Dock material textures; GPL-3.0-or-later.

Two stages, both deterministic and authored for this project, with no
reference pixels, downloaded artwork or fonts:

1. The ADREP-TASK-004 vector swatches (`<look>.svg`): SVG primitives that
   define each look's colours and pattern.
2. The raster textures the renderers use (`<look>.png`, 512 px): each
   swatch rendered and given real surface detail - brushed streaks, grass
   blades, glowing plasma gas, marble veins, stone speckle, frost or a soft
   shimmer - from tileable value noise and strokes on fixed seeds, and a
   relief map beside it (`<look>-normal.png`, a tangent-space normal map
   from the texture's brightness) that the 3D renderer lights as a real
   surface. This stage needs PySide6 (QtSvg renders the swatch); run it
   with `--raster`, or `--all` for both stages. Theme packages keep their
   vector surfaces.

A third stage, `--real <folder>`, makes the looks of real things - the
river and the magma - from real texture sets rather than drawn patterns:
ambientCG's CC0 1.0 sets (https://ambientcg.com, `<Id>_1K-JPG.zip` unpacked
into `<folder>/<Id>/`): Lava003, Lava002, Lava005 and Lava004 for the magma
and its heats, Gravel021 and Gravel020 as river beds. The water over the bed
is composed here: its ripples are a wave spectrum (Tessendorf, "Simulating
Ocean Water", 2001: the Phillips spectrum of wind-driven waves), the bed
seen through water that absorbs light with depth, the light the ripples
focus on the bed, and the sky mirrored where the ripples tilt. This stage
needs PySide6 or PyQt6 to read the sets.

A fourth stage, `--decorations <folder>`, makes the panel decorations' real
materials from ambientCG's CC0 sets in the same folder: for each, the set's
colour (`deco-<name>.jpg`, 512 px; a PNG where it has its own outline) and
its relief as a grey height map (`deco-<name>-height.png`, 256 px, from the
set's displacement). The flat
renderer shades the height map into the colour; the 3D platform raises the
band by it. The Alienware-style metals take their relief from a hexagonal
paving set, which gives them a real honeycomb. The same stage makes the
realistic decorations' materials: barks for branches, trunks and woody
vines, straw and rope for a broom, rubber for a hilt's grip, old building
stone, worn paint for starship hulls, glossy plastic, river pebbles, and
the end grain of sawn logs (`deco-log-end*.png`, cut from inside the disc
so it fills a log's end).

A fifth stage, `--nebulae <folder> [out]`, makes the galactic decorations'
strips from the James Webb Space Telescope's pictures (NASA, ESA, CSA,
STScI, released by STScI for free use as in the public domain, with credit
requested): a band through each picture, `deco-<name>.png` 2048 x 256, made
to repeat along a panel by blending its end into its start, and its
brightness smoothed as the height under a glassy surface
(`deco-<name>-height.png`, 512 x 64). Pictures are written as JPEG. The pictures are the 2000-pixel
renditions from science.nasa.gov: cosmic-cliffs.png, pillars-of-creation.png,
southern-ring.png, tarantula.png and first-deep-field.png.
"""
import math
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "qml/ArchDock/Rendering/materials"


def rect(x, y, w, h, fill, opacity=1):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" opacity="{opacity}"/>'


def path(d, stroke, width=1, opacity=1):
    return f'<path d="{d}" fill="none" stroke="{stroke}" stroke-width="{width}" opacity="{opacity}"/>'


def swatch(name):
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">',
             '<!-- Original Arch Dock vector material. GPL-3.0-or-later. -->',
             '<defs><linearGradient id="sheen" x2="0.8" y2="1">'
             '<stop stop-color="#f8f8f8"/><stop offset=".36" stop-color="#777"/>'
             '<stop offset=".62" stop-color="#d8d8d8"/><stop offset="1" stop-color="#4d4d4d"/>'
             '</linearGradient></defs>']
    if name in ("glass", "floating-glass"):
        # Glass refracts light; the metallic swatch keeps the darker sheen.
        parts.append('<defs><linearGradient id="glass-sheen" x2="0.8" y2="1">'
                     '<stop stop-color="#fff"/><stop offset=".3" stop-color="#b5b5b5"/>'
                     '<stop offset=".6" stop-color="#f8f8f8"/><stop offset="1" stop-color="#d0d0d0"/>'
                     '</linearGradient></defs>')
        parts.append(rect(0, 0, 128, 128, "url(#glass-sheen)"))
        spacing = 17 if name == "glass" else 29
        for i in range(-128, 256, spacing):
            parts.append(path(f"M{i} 0 l128 128", "#fff", .7, .08))
        # Fine frost has structure, without sparkle or opaque glitter.
        for i in range(96):
            x, y = (i * 37) % 128, (i * 61) % 128
            parts.append(rect(x, y, 1.5, 1.5, "#fff", .15))
        if name == "floating-glass":
            parts.append(path("M0 27 H128 M0 31 H128", "#fff", 2, .65))
    elif name == "crystal":
        parts.append(rect(0, 0, 128, 128, "#a5a5a5"))
        for row in range(4):
            for col in range(4):
                x, y = col * 32, row * 32
                shade = 100 + ((row * 3 + col * 7) % 8) * 20
                parts.append(f'<path d="M{x} {y} h32 l-16 32 Z" fill="rgb({shade},{shade},{shade})" stroke="#eee" stroke-width=".8"/>')
    elif name == "metallic":
        parts.append(rect(0, 0, 128, 128, "url(#sheen)"))
        for y in range(0, 128, 2):
            parts.append(path(f"M0 {y} H128", "#fff" if y % 6 else "#222", .7, .4))
    elif name == "futuristic":
        parts.append(rect(0, 0, 128, 128, "#454545"))
        for y in range(0, 128, 32):
            parts.append(path(f"M0 {y} H128 M{y} 0 V128", "#242424", 4))
            parts.append(path(f"M0 {y+8} h20 l8 8 h36 l8 -8 h56", "#ececec", 2))
            parts.append(rect(19, y+6, 5, 5, "#fff"))
        parts.append(path("M80 0 V40 L104 64 V128", "#aaa", 1))
    elif name == "aurora":
        parts.append(rect(0, 0, 128, 128, "#202020"))
        for x in range(0, 128, 6):
            parts.append(path(f"M{x} 96 C{x+2} 70 {x-2} 48 {x+1} 24", "#bbb", 3, .5))
    elif name in ("neon", "plasma", "lightning", "lime"):
        parts.append(rect(0, 0, 128, 128, "#555" if name == "neon" else "#343434"))
        for y in range(0, 160, 24 if name == "neon" else 17):
            if name == "neon":
                d = f"M0 {y} H128"
                parts.extend([path(d, "#999", 9), path(d, "#eee", 4), path(d, "#fff", 1)])
            elif name in ("plasma", "lightning"):
                d = f"M0 {y} C32 {y-20} 64 {y+24} 96 {y} S128 {y-20} 160 {y}"
                parts.extend([path(d, "#888", 8), path(d, "#eee", 2)])
            else:
                for x in range(0, 128, 20):
                    parts.append(path(f"M{x} {y} l10 -7 10 7 -10 7 Z", "#ccc", 1.5))
    elif name == "organic":
        parts.append(rect(0, 0, 128, 128, "#999"))
        for y in range(-16, 144, 7):
            parts.append(path(f"M0 {y} C32 {y+15} 58 {y-15} 128 {y+4}", "#ddd" if y % 3 else "#555", 2, .6))
    elif name == "minimal":
        parts.append(rect(0, 0, 128, 128, "#b3b3b3"))
        for i in range(0, 128, 8):
            parts.append(path(f"M{i} 0 l-32 128", "#888", 1, .6))
    elif name == "platform":
        parts.append(rect(0, 0, 128, 128, "#818181"))
        for y in range(0, 128, 32):
            for x in range(0, 128, 32):
                parts.append(rect(x+2, y+2, 28, 28, "#bababa"))
                parts.append(path(f"M{x+4} {y+25} V{y+4} H{x+25}", "#eee", 2))
    elif name == "plate":
        parts.append(rect(0, 0, 128, 128, "url(#sheen)"))
        for x in range(8, 128, 32):
            parts.append(path(f"M{x} 0 V128", "#333", 3))
            for y in range(8, 128, 32):
                parts.append(f'<circle cx="{x+4}" cy="{y}" r="2" fill="#eee"/>')
    elif name == "pedestal":
        parts.append(rect(0, 0, 128, 128, "#555"))
        for i in range(0, 128, 16):
            parts.append(rect(i, 0, 8, 128, "#aaa"))
            parts.append(path(f"M{i} 0 V128", "#eee", 2))
    else:
        raise ValueError(name)
    parts.append("</svg>\n")
    return "\n".join(parts)


MATERIALS = ("glass", "crystal", "neon", "minimal", "plasma", "lime",
             "floating-glass", "metallic", "futuristic", "organic",
             "platform", "plate", "pedestal", "lightning", "aurora")
SIZE = 512
REPO = Path(__file__).resolve().parents[1]


def write_swatches():
    ROOT.mkdir(parents=True, exist_ok=True)
    for material in MATERIALS:
        (ROOT / f"{material}.svg").write_text(swatch(material), encoding="utf-8")


class Noise:
    """Tileable value noise: a periodic random lattice, smoothly interpolated."""

    def __init__(self, period, seed):
        rng = random.Random(seed)
        self.period = period
        self.grid = [[rng.random() for _ in range(period)] for _ in range(period)]

    def at(self, x, y):
        p = self.period
        x0 = int(math.floor(x)) % p
        y0 = int(math.floor(y)) % p
        fx = x - math.floor(x)
        fy = y - math.floor(y)
        sx = fx * fx * (3 - 2 * fx)
        sy = fy * fy * (3 - 2 * fy)
        g = self.grid
        x1 = (x0 + 1) % p
        y1 = (y0 + 1) % p
        top = g[y0][x0] + (g[y0][x1] - g[y0][x0]) * sx
        bottom = g[y1][x0] + (g[y1][x1] - g[y1][x0]) * sx
        return top + (bottom - top) * sy


def octaves(seed, periods):
    return [Noise(period, seed + index * 101) for index, period in enumerate(periods)]


def brushed(seed, direction=0.0, grain=0.35):
    """Fine streaks along `direction` (radians) with a soft large-scale variation."""
    fine = octaves(seed, [256, 128])
    broad = octaves(seed + 7, [4, 8])
    c, s = math.cos(direction), math.sin(direction)

    def detail(u, v):
        along = u * c + v * s
        across = -u * s + v * c
        # Streaks: high frequency across the brushing, low along it.
        streak = fine[0].at(across * 256, along * 6) * 0.7 + fine[1].at(across * 128, along * 3) * 0.3
        soft = broad[0].at(u * 4, v * 4) * 0.6 + broad[1].at(u * 8, v * 8) * 0.4
        return 0.5 + (streak - 0.5) * grain + (soft - 0.5) * 0.25
    return detail


def wood(seed, rings=9.0, grain=0.5):
    """Wood: rings warped by noise, with a fine fibre."""
    warp = octaves(seed, [4, 8, 16])
    fibre = octaves(seed + 3, [256, 128])

    def detail(u, v):
        w = (warp[0].at(u * 4, v * 4) - 0.5) * 0.35 + (warp[1].at(u * 8, v * 8) - 0.5) * 0.15 \
            + (warp[2].at(u * 16, v * 16) - 0.5) * 0.06
        ring = math.sin((v + w) * rings * 2 * math.pi) * 0.5 + 0.5
        ring = ring ** 2.2
        f = fibre[0].at(u * 256, v * 24) * 0.6 + fibre[1].at(u * 128, v * 12) * 0.4
        return 0.5 + (ring - 0.5) * grain + (f - 0.5) * 0.22
    return detail


def stone(seed, grain=0.45):
    """Stone or concrete: speckle plus mottling."""
    speckle = octaves(seed, [256, 128, 64])
    mottle = octaves(seed + 5, [6, 12])

    def detail(u, v):
        s = speckle[0].at(u * 256, v * 256) * 0.5 + speckle[1].at(u * 128, v * 128) * 0.3 \
            + speckle[2].at(u * 64, v * 64) * 0.2
        m = mottle[0].at(u * 6, v * 6) * 0.6 + mottle[1].at(u * 12, v * 12) * 0.4
        return 0.5 + (s - 0.5) * grain + (m - 0.5) * 0.3
    return detail


def frosted(seed, grain=0.18):
    """Glass and crystal: fine frost with soft highlights."""
    frost = octaves(seed, [128, 64])
    light = octaves(seed + 9, [3, 6])

    def detail(u, v):
        f = frost[0].at(u * 128, v * 128) * 0.6 + frost[1].at(u * 64, v * 64) * 0.4
        l = light[0].at(u * 3, v * 3) * 0.7 + light[1].at(u * 6, v * 6) * 0.3
        return 0.5 + (f - 0.5) * grain + (l - 0.5) * 0.3
    return detail


def shimmer(seed, grain=0.22):
    """Energy looks: soft cells of light with a fine sparkle."""
    cells = octaves(seed, [8, 16, 32])
    spark = octaves(seed + 11, [256])

    def detail(u, v):
        c = cells[0].at(u * 8, v * 8) * 0.5 + cells[1].at(u * 16, v * 16) * 0.3 + cells[2].at(u * 32, v * 32) * 0.2
        s = spark[0].at(u * 256, v * 256)
        return 0.5 + (c - 0.5) * 0.5 + (s - 0.5) * grain
    return detail


def painted(paint):
    """A texture painted with QPainter on top of a detail field: `paint(painter,
    size, rng)` draws into the base image after the field was applied."""
    def detail(u, v):
        return 0.5
    detail.paint = paint
    return detail


def grass(painter, size, rng):
    """Grass seen from above: thousands of short blades in varied greens over
    dark soil, lighter at their tips; strokes wrap at the edges to tile."""
    from PySide6.QtCore import QPointF
    from PySide6.QtGui import QColor, QPainterPath, QPen
    painter.fillRect(0, 0, size, size, QColor(34, 62, 22))
    painter.setRenderHint(painter.RenderHint.Antialiasing, True)
    for index in range(size * size // 26):
        x, y = rng.uniform(0, size), rng.uniform(0, size)
        length = rng.uniform(size / 42, size / 17)
        angle = -math.pi / 2 + rng.uniform(-0.65, 0.65)
        bend = rng.uniform(-0.35, 0.35)
        hue = rng.uniform(78, 112)
        light = rng.uniform(0.28, 0.62)
        colour = QColor.fromHslF(hue / 360, rng.uniform(0.45, 0.7), light)
        tip = QColor.fromHslF(hue / 360, 0.6, min(0.85, light + 0.22))
        pen = QPen(colour, rng.uniform(1.0, 2.2)); pen.setCapStyle(pen.capStyle().RoundCap)
        for dx, dy in ((0, 0), (-size, 0), (size, 0), (0, -size), (0, size)):
            path = QPainterPath(QPointF(x + dx, y + dy))
            path.quadTo(QPointF(x + dx + math.cos(angle + bend) * length * 0.6, y + dy + math.sin(angle + bend) * length * 0.6),
                        QPointF(x + dx + math.cos(angle) * length, y + dy + math.sin(angle) * length))
            painter.setPen(pen); painter.drawPath(path)
            pen2 = QPen(tip, max(0.8, pen.widthF() * 0.6)); pen2.setCapStyle(pen.capStyle().RoundCap)
            painter.setPen(pen2)
            painter.drawLine(QPointF(x + dx + math.cos(angle) * length * 0.75, y + dy + math.sin(angle) * length * 0.75),
                             QPointF(x + dx + math.cos(angle) * length, y + dy + math.sin(angle) * length))


def lightning_field(size, seed):
    """Lightning that looks like lightning (the owner, 2026-10-13: "not some
    randomly drawn zigzag lines"): bolts grown the way a discharge crackles,
    by splitting each stroke at its middle and pushing the middle aside,
    again and again, so the path wanders at every scale; thinner forks leave
    the bolts and die away. Each stroke is a white-hot core inside a blue and
    violet glow, the light adding up where strokes meet, on a clear ground
    (the panel draws the background, see below), fading out toward the
    band's edges. The bolts run left to right and
    meet themselves at the edges, so the texture wraps round a ring without
    a seam. Returns rows of RGBA bytes; needs no Qt."""
    rng = random.Random(seed)
    core = [0.0] * (size * size)
    glow = [0.0] * (size * size)

    def bolt(x0, y0, x1, y1, roughness, depth):
        points = [(x0, y0), (x1, y1)]
        spread = roughness * math.hypot(x1 - x0, y1 - y0)
        for _ in range(depth):
            grown = [points[0]]
            for (ax, ay), (bx, by) in zip(points, points[1:]):
                mx, my = (ax + bx) / 2, (ay + by) / 2
                length = math.hypot(bx - ax, by - ay) or 1
                offset = rng.gauss(0, spread)
                # Pushed across the stroke, not along it.
                grown.append((mx - (by - ay) / length * offset, my + (bx - ax) / length * offset))
                grown.append((bx, by))
            points = grown
            spread *= 0.55
        return points

    def stroke(points, width, light):
        reach = width * 7
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            dx, dy = bx - ax, by - ay
            length2 = dx * dx + dy * dy or 1e-9
            left, right = int(math.floor(min(ax, bx) - reach)), int(math.ceil(max(ax, bx) + reach))
            top, bottom = int(math.floor(min(ay, by) - reach)), int(math.ceil(max(ay, by) + reach))
            for py in range(max(0, top), min(size, bottom + 1)):
                for px in range(left, right + 1):
                    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length2))
                    ex, ey = px - (ax + dx * t), py - (ay + dy * t)
                    d2 = ex * ex + ey * ey
                    if d2 > reach * reach:
                        continue
                    index = py * size + (px % size)
                    core[index] += light * math.exp(-d2 / (width * width * 0.5))
                    glow[index] += light * 0.22 * math.exp(-math.sqrt(d2) / (width * 2.2))

    for index in range(5):
        y = size * rng.uniform(0.26, 0.74)
        # Wraps: the bolt ends where it starts, one width further on.
        main = bolt(0, y, size, y, 0.11, 8)
        brightness = rng.uniform(0.75, 1.0) if index < 3 else rng.uniform(0.35, 0.55)
        width = rng.uniform(1.1, 1.6) if index < 3 else rng.uniform(0.7, 1.0)
        stroke(main, width, brightness)
        # Forks leave the bolt at a slant and die away.
        for _ in range(rng.randint(4, 7)):
            at = rng.randrange(8, len(main) - 8)
            ax, ay = main[at]
            heading = rng.choice((-1, 1)) * rng.uniform(0.35, 1.0)
            reach = size * rng.uniform(0.06, 0.16)
            bx, by = ax + reach, ay + heading * reach
            fork = bolt(ax, ay, bx, by, 0.16, 6)
            stroke(fork, width * 0.55, brightness * 0.55)
            if rng.random() < 0.5:
                at = rng.randrange(4, len(fork) - 4)
                cx, cy = fork[at]
                twig = bolt(cx, cy, cx + reach * 0.5, cy + heading * reach * rng.uniform(0.2, 0.8), 0.18, 5)
                stroke(twig, width * 0.35, brightness * 0.35)
    return core, glow


# Lightning in colour (the owner, 2026-10-13: "not just mere colors but
# lightning naturally colors, even if in nature we don't have such colors"):
# each palette is a white-hot core and the colour of the glow round it.
LIGHTNING_PALETTES = {
    "storm": ((1.0, 0.95, 1.0), (0.53, 0.32, 1.0)),
    "ice": ((0.92, 1.0, 1.0), (0.22, 0.78, 1.0)),
    "crimson": ((1.0, 0.92, 0.9), (1.0, 0.16, 0.2)),
    "toxic": ((0.95, 1.0, 0.9), (0.36, 1.0, 0.22)),
    "solar": ((1.0, 0.98, 0.86), (1.0, 0.6, 0.1)),
    "arcane": ((1.0, 0.92, 1.0), (1.0, 0.24, 0.88)),
    "void": ((0.88, 0.78, 1.0), (0.46, 0.1, 1.0)),
    "aurora": ((0.9, 1.0, 0.97), (0.18, 1.0, 0.72)),
    # Every colour along the band (the owner, 2026-10-13: rainbow wherever
    # it can be practically applied).
    "rainbow": None,
}


def lightning_rows(fields, palette):
    """The bolts in a palette's colours on a clear ground: the light they give
    is the colour, its strongest part how much of the pixel they cover, so
    the storm behind is clear and the panel draws its own background under
    them at the strength chosen (the owner, 2026-10-13: "make the background
    for lightning totally transparent, or with option to change
    transparency"). Over black it is exactly the light of the bolts."""
    core, glow = fields
    colours = LIGHTNING_PALETTES[palette]
    size = int(math.isqrt(len(core)))

    def smooth(edge0, edge1, value):
        t = min(1.0, max(0.0, (value - edge0) / (edge1 - edge0)))
        return t * t * (3 - 2 * t)

    rows = []
    for py in range(size):
        envelope = smooth(0.0, 0.16, py / size) * smooth(1.0, 0.84, py / size)
        row = bytearray()
        for px in range(size):
            index = py * size + px
            heat, halo = core[index] * envelope, glow[index] * envelope
            if colours is None:
                gr, gg, gb = rainbow(px / size + 0.06 * math.sin(2 * math.pi * px / size))
                cr, cg, cb = 0.85 + 0.15 * gr, 0.85 + 0.15 * gg, 0.85 + 0.15 * gb
            else:
                (cr, cg, cb), (gr, gg, gb) = colours
            light = (255 * (1 - math.exp(-(heat * 1.15 * cr + halo * 1.7 * gr))),
                     255 * (1 - math.exp(-(heat * 1.15 * cg + halo * 1.7 * gg))),
                     255 * (1 - math.exp(-(heat * 1.15 * cb + halo * 1.7 * gb))))
            cover = max(light) / 255
            if cover < 1 / 255:
                row += bytes((0, 0, 0, 0))
                continue
            row += bytes((clamp(light[0] / cover), clamp(light[1] / cover), clamp(light[2] / cover),
                          clamp(cover * 255)))
        rows.append(bytes(row))
    return rows


def write_lightning(directory=ROOT, seed=233):
    """lightning.png (Storm) and lightning-<palette>.png, one bolt drawing in
    every palette, and their one relief map."""
    fields = lightning_field(SIZE, seed)
    for palette in LIGHTNING_PALETTES:
        rows = lightning_rows(fields, palette)
        name = "lightning.png" if palette == "storm" else f"lightning-{palette}.png"
        write_png(Path(directory) / name, rows)
        if palette == "storm":
            write_png(Path(directory) / "lightning-normal.png", relief_rows(rows, RELIEF["lightning"]))
    print("lightning written to", directory)


# Aurora's colours: the bright lower edge of its curtains and the crown they
# fade into, from the polar lights to a few no sky has shown.
AURORA_PALETTES = {
    "borealis": ((0.25, 1.0, 0.55), (0.55, 0.3, 1.0)),
    "emerald": ((0.15, 1.0, 0.45), (0.15, 0.8, 1.0)),
    "australis": ((1.0, 0.3, 0.55), (0.6, 0.25, 1.0)),
    "cosmic": ((0.45, 0.45, 1.0), (1.0, 0.3, 0.85)),
    "solar": ((1.0, 0.8, 0.25), (1.0, 0.3, 0.2)),
    # Every colour along the band, shifting as the curtains rise (the
    # owner, 2026-10-13: "for aurora, include rainbow color too").
    "rainbow": None,
}


def rainbow(hue):
    """A bright spectral colour for a hue from 0 to 1, round the wheel."""
    import colorsys
    return colorsys.hsv_to_rgb(hue % 1.0, 0.85, 1.0)


def aurora_rows(size, seed, palette):
    """Aurora (the owner, 2026-10-13: a look of its own, like lightning and
    plasma): curtains of light hanging from a wavy lower edge, folding as
    they run along the band, streaked by rays rising from the edge and fading
    with height from the edge's colour to the crown's - two curtains, one
    behind the other - on a clear ground like lightning's, fading toward the
    band's edges. The waves and noise repeat across the width, so it wraps
    round a ring without a seam. Returns rows of RGBA bytes; needs no Qt."""
    rng = random.Random(seed)
    colours = AURORA_PALETTES[palette]
    curtains = []
    for index in range(2):
        curtains.append({
            "base": rng.uniform(0.6, 0.7) - index * 0.08,
            "waves": [(rng.choice((1, 2)), rng.uniform(0.04, 0.07), rng.random()),
                      (rng.choice((3, 4, 5)), rng.uniform(0.015, 0.03), rng.random())],
            "drift": Noise(6, seed + 11 + index * 101),
            "fold": Noise(5, seed + 23 + index * 101),
            "rays": Noise(96, seed + 37 + index * 101),
            "height": rng.uniform(0.32, 0.42) - index * 0.06,
            "weight": 1.0 if index == 0 else 0.6,
        })

    def smooth(edge0, edge1, value):
        t = min(1.0, max(0.0, (value - edge0) / (edge1 - edge0)))
        return t * t * (3 - 2 * t)

    rows = []
    for py in range(size):
        v = py / size
        envelope = smooth(0.0, 0.12, v) * smooth(1.0, 0.88, v)
        row = bytearray()
        for px in range(size):
            u = px / size
            if colours is None:
                # Once round the wheel across the width, so the edges meet.
                hue = u + 0.06 * math.sin(2 * math.pi * u)
                (er, eg, eb), (cr, cg, cb) = rainbow(hue), rainbow(hue + 0.18)
            else:
                (er, eg, eb), (cr, cg, cb) = colours
            lr = lg = lb = 0.0
            for curtain in curtains:
                edge = curtain["base"] + (curtain["drift"].at(u * 6, 0.5) - 0.5) * 0.06
                for count, amplitude, phase in curtain["waves"]:
                    edge += amplitude * math.sin(2 * math.pi * (count * u + phase))
                above = edge - v
                height = curtain["height"]
                if above < 0:
                    light = math.exp(above / 0.018)
                else:
                    light = math.exp(-above / (height * 0.45)) * (1 - smooth(height * 0.75, height, above))
                if light < 1e-3:
                    continue
                # Rays rise from the edge: streaks across the width, leaning a
                # little with height; folds brighten parts of the curtain.
                ray = curtain["rays"].at(u * 96 + above * 9, 0.5)
                rays = 0.35 + 0.65 * ray * ray
                fold = 0.55 + 0.45 * curtain["fold"].at(u * 5, 1.5)
                strength = light * rays * fold * curtain["weight"]
                t = min(1.0, max(0.0, above / height)) ** 0.8
                lr += strength * (er + (cr - er) * t)
                lg += strength * (eg + (cg - eg) * t)
                lb += strength * (eb + (cb - eb) * t)
            light = (255 * (1 - math.exp(-lr * 1.7 * envelope)), 255 * (1 - math.exp(-lg * 1.7 * envelope)),
                     255 * (1 - math.exp(-lb * 1.7 * envelope)))
            cover = max(light) / 255
            if cover < 1 / 255:
                row += bytes((0, 0, 0, 0))
                continue
            row += bytes((clamp(light[0] / cover), clamp(light[1] / cover), clamp(light[2] / cover),
                          clamp(cover * 255)))
        rows.append(bytes(row))
    return rows


def write_aurora(directory=ROOT, seed=307):
    """aurora.png (Borealis) and aurora-<palette>.png, and one relief map."""
    for palette in AURORA_PALETTES:
        rows = aurora_rows(SIZE, seed, palette)
        name = "aurora.png" if palette == "borealis" else f"aurora-{palette}.png"
        write_png(Path(directory) / name, rows)
        if palette == "borealis":
            write_png(Path(directory) / "aurora-normal.png", relief_rows(rows, RELIEF["aurora"]))
    print("aurora written to", directory)


def plasma_field(size, seed, warp=0.85, heat=1.0, palette="original"):
    """Plasma as glowing gas, not lines (the owner, 2026-10-13: the old
    filaments "don't really look like plasma, more like static lightning"):
    tileable value noise warped through itself twice, so it curls and flows
    like ionized gas, with soft bright tendrils where a ridged layer peaks,
    coloured from deep violet through magenta to a pink-white heart, and
    kept inside the band by dark borders above and below. It wraps left to
    right, so it runs round a ring without a seam. Returns rows of RGBA
    bytes; needs no Qt."""
    def layer(offset, periods):
        return [Noise(period, seed + offset + index * 101) for index, period in enumerate(periods)]
    warp_a, warp_b = layer(1, (3, 6, 12)), layer(2, (3, 6, 12))
    warp_c, warp_d = layer(3, (4, 8, 16)), layer(4, (4, 8, 16))
    body, tendril = layer(5, (4, 8, 16, 32)), layer(6, (3, 6, 12, 24))
    tint = layer(7, (2, 4))

    def fbm(noises, x, y):
        total, amplitude, weight = 0.0, 1.0, 0.0
        for noise in noises:
            total += amplitude * noise.at(x * noise.period, y * noise.period)
            weight += amplitude
            amplitude *= 0.5
        return total / weight

    def smooth(edge0, edge1, value):
        t = min(1.0, max(0.0, (value - edge0) / (edge1 - edge0)))
        return t * t * (3 - 2 * t)

    stops = ((0.0, (6, 1, 16)), (0.28, (40, 8, 86)), (0.5, (104, 26, 184)),
             (0.68, (176, 58, 246)), (0.82, (236, 112, 255)), (0.93, (255, 188, 252)),
             (1.0, (255, 240, 255)))

    def colour(t):
        t = min(1.0, max(0.0, t))
        for (a, ca), (b, cb) in zip(stops, stops[1:]):
            if t <= b:
                s = (t - a) / (b - a)
                return [ca[i] + (cb[i] - ca[i]) * s for i in range(3)]
        return list(stops[-1][1])

    rows = []
    for py in range(size):
        v = py / size
        # Dark above and below, brightest along the middle of the band.
        envelope = smooth(0.02, 0.24, v) * smooth(0.98, 0.76, v)
        row = bytearray()
        for px in range(size):
            u = px / size
            # The gas flows along the band: stretched across it.
            y = v * 1.5
            qx, qy = fbm(warp_a, u, y), fbm(warp_b, u + 0.37, y + 0.11)
            rx = fbm(warp_c, u + warp * qx, y + warp * qy)
            ry = fbm(warp_d, u + warp * qx + 0.17, y + warp * qy + 0.29)
            gas = fbm(body, u + 0.7 * rx, y + 0.7 * ry)
            ridge = 1 - abs(2 * fbm(tendril, u + 0.55 * rx, y + 0.55 * ry) - 1)
            glow = ridge ** 5
            t = (0.18 + 0.62 * smooth(0.32, 0.78, gas) + 0.55 * glow) * heat * envelope
            if palette == "rainbow":
                # The gas in every colour along the band, its hottest parts
                # paling toward white.
                hr, hg, hb = rainbow(u + 0.06 * math.sin(2 * math.pi * u) + 0.12 * gas)
                pale = smooth(0.7, 1.0, t)
                level = 255 * min(1.0, max(0.0, t) * 1.15)
                r, g, b = ((hr + (1 - hr) * pale) * level, (hg + (1 - hg) * pale) * level,
                           (hb + (1 - hb) * pale) * level)
            else:
                r, g, b = colour(t)
                # A cooler blue through some of the gas, as plasma shifts colour.
                cool = smooth(0.55, 0.8, fbm(tint, u, y)) * 0.35 * envelope
                r, g, b = r * (1 - cool) + 70 * cool, g * (1 - cool) + 80 * cool, b * (1 - cool) + 255 * cool
            row += bytes((clamp(r), clamp(g), clamp(b), 255))
        rows.append(bytes(row))
    return rows


def relief_rows(rows, strength):
    """relief() for raw RGBA rows: the same tangent-space normal map."""
    size = len(rows)
    height = [[(row[x * 4] * 0.3 + row[x * 4 + 1] * 0.59 + row[x * 4 + 2] * 0.11) / 255
               for x in range(size)] for row in rows]
    scale = 2.5 * strength
    out = []
    for y in range(size):
        up, down, row = height[(y - 1) % size], height[(y + 1) % size], height[y]
        line = bytearray()
        for x in range(size):
            dx = (row[(x + 1) % size] - row[(x - 1) % size]) * scale
            dy = (down[x] - up[x]) * scale
            length = math.sqrt(dx * dx + dy * dy + 1)
            line += bytes((clamp((-dx / length * 0.5 + 0.5) * 255), clamp((-dy / length * 0.5 + 0.5) * 255),
                           clamp((1 / length * 0.5 + 0.5) * 255), 255))
        out.append(bytes(line))
    return out


def write_png(target, rows):
    """A plain RGBA PNG from rows of bytes, without Qt."""
    import struct
    import zlib

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))
    width, height = len(rows[0]) // 4, len(rows)
    raw = b"".join(b"\x00" + row for row in rows)
    Path(target).write_bytes(b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def write_plasma(directory=ROOT, seed=171):
    rows = plasma_field(SIZE, seed)
    write_png(Path(directory) / "plasma.png", rows)
    write_png(Path(directory) / "plasma-normal.png", relief_rows(rows, RELIEF["plasma"]))
    write_png(Path(directory) / "plasma-rainbow.png", plasma_field(SIZE, seed, palette="rainbow"))
    print("plasma written to", directory)


def read_png(source):
    """Rows of RGBA bytes from an 8-bit, non-interlaced RGB or RGBA PNG."""
    import struct
    import zlib
    data = Path(source).read_bytes()
    position, compressed = 8, b""
    while position < len(data):
        length = struct.unpack(">I", data[position:position + 4])[0]
        tag, body = data[position + 4:position + 8], data[position + 8:position + 8 + length]
        if tag == b"IHDR":
            width, height, depth, kind = struct.unpack(">IIBB", body[:10])
            assert depth == 8 and kind in (2, 6) and body[12] == 0, "unsupported PNG"
        elif tag == b"IDAT":
            compressed += body
        position += 12 + length
    raw = zlib.decompress(compressed)
    step = 4 if kind == 6 else 3
    stride = width * step
    rows, previous, offset = [], bytearray(stride), 0
    for _ in range(height):
        kind_of_filter = raw[offset]
        line = bytearray(raw[offset + 1:offset + 1 + stride])
        offset += 1 + stride
        for x in range(stride):
            left = line[x - step] if x >= step else 0
            up = previous[x]
            corner = previous[x - step] if x >= step else 0
            if kind_of_filter == 1:
                line[x] = (line[x] + left) & 255
            elif kind_of_filter == 2:
                line[x] = (line[x] + up) & 255
            elif kind_of_filter == 3:
                line[x] = (line[x] + (left + up) // 2) & 255
            elif kind_of_filter == 4:
                guess = left + up - corner
                pa, pb, pc = abs(guess - left), abs(guess - up), abs(guess - corner)
                line[x] = (line[x] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
        previous = line
        if step == 3:
            rows.append(b"".join(bytes(line[i:i + 3]) + b"\xff" for i in range(0, stride, 3)))
        else:
            rows.append(bytes(line))
    return rows


def rainbow_rows(rows):
    """A look in every colour along the band: each pixel's light kept, its
    colour turned to the rainbow's at its place across the width, its
    brightest parts paling toward white; it still wraps without a seam."""
    size = len(rows[0]) // 4
    out = []
    for row in rows:
        line = bytearray()
        for x in range(size):
            r, g, b, a = row[x * 4], row[x * 4 + 1], row[x * 4 + 2], row[x * 4 + 3]
            light = (r * 0.3 + g * 0.59 + b * 0.11) / 255
            hr, hg, hb = rainbow(x / size + 0.06 * math.sin(2 * math.pi * x / size))
            pale = max(0.0, min(1.0, (light - 0.8) / 0.2)) * 0.5
            level = 255 * min(1.0, light * 1.25)
            line += bytes((clamp((hr + (1 - hr) * pale) * level), clamp((hg + (1 - hg) * pale) * level),
                           clamp((hb + (1 - hb) * pale) * level), a))
        out.append(bytes(line))
    return out


def lime_rows(size, seed):
    """A lime's peel (the owner, 2026-10-13: "we named lime color after fruit
    called lime, and there should be also the texture of lime not just
    color"): a juicy citrus green, deeper in patches and yellowing in
    others, its skin finely grained and dimpled all over by small oil
    glands, each a soft hollow lit from one side, from cellular noise that
    repeats across the tile. Returns rows of RGBA bytes; needs no Qt."""
    rng = random.Random(seed)
    cells = 92
    points = [[(rng.random(), rng.random()) for _ in range(cells)] for _ in range(cells)]
    tone = [Noise(period, seed + 7 + index * 101) for index, period in enumerate((3, 6, 12))]
    blush = [Noise(period, seed + 19 + index * 101) for index, period in enumerate((2, 4))]
    grain = Noise(128, seed + 29)
    rows = []
    for py in range(size):
        row = bytearray()
        for px in range(size):
            u, v = px / size, py / size
            # The nearest oil gland, round the tile's edges as well, and the
            # side of the gland this point lies on.
            cx, cy = u * cells, v * cells
            ix, iy = int(cx), int(cy)
            nearest, toward = 9.0, (0.0, 0.0)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    fx, fy = points[(iy + dy) % cells][(ix + dx) % cells]
                    ox, oy = cx - (ix + dx + fx), cy - (iy + dy + fy)
                    distance = math.hypot(ox, oy)
                    if distance < nearest:
                        nearest, toward = distance, (ox, oy)
            hollow = math.exp(-(nearest / 0.22) ** 2)
            # A hollow is dark on the side away from the light, bright on the
            # side facing it (light from the upper left).
            facing = (-(toward[0]) - toward[1]) / max(1e-6, nearest) if nearest > 1e-6 else 0
            relief = -0.16 * hollow + 0.1 * hollow * facing
            shade = sum(noise.at(u * noise.period, v * noise.period) * weight
                        for noise, weight in zip(tone, (0.55, 0.3, 0.15)))
            yellow = sum(noise.at(u * noise.period, v * noise.period) * weight
                         for noise, weight in zip(blush, (0.65, 0.35)))
            mix = min(1.0, max(0.0, (shade - 0.3) * 1.5))
            ripe = max(0.0, yellow - 0.5) * 2
            r = 70 + 70 * mix + 70 * ripe
            g = 150 + 60 * mix + 25 * ripe
            b = 24 + 26 * mix
            light = 1.0 + relief + (grain.at(u * 128, v * 128) - 0.5) * 0.08
            row += bytes((clamp(r * light), clamp(g * light), clamp(b * light), 255))
        rows.append(bytes(row))
    return rows


def write_lime(directory=ROOT, seed=401):
    rows = lime_rows(SIZE, seed)
    write_png(Path(directory) / "lime.png", rows)
    write_png(Path(directory) / "lime-normal.png", relief_rows(rows, RELIEF["lime"]))
    print("lime written to", directory)




def chameleon_rows(rows, seed, strength=0.0):
    """Polished, heat-tinted metal (the owner, 2026-10-13: "metal can have
    faint rainbow colors ... we usually call that chameleon"): the metal's
    own grain and shine, with a faint thin-film sheen drifting through
    purple, blue, gold and green across it, a little stronger in the light.
    Repeats across the tile like the metal itself."""
    size = len(rows[0]) // 4
    film = [Noise(period, seed + index * 101) for index, period in enumerate((2, 4, 8))]
    out = []
    for py, row in enumerate(rows):
        line = bytearray()
        v = py / size
        for x in range(size):
            u = x / size
            r, g, b, a = row[x * 4], row[x * 4 + 1], row[x * 4 + 2], row[x * 4 + 3]
            light = (r * 0.3 + g * 0.59 + b * 0.11) / 255
            drift = sum(noise.at(u * noise.period, v * noise.period) * weight
                        for noise, weight in zip(film, (0.6, 0.28, 0.12)))
            hue = 0.7 + 0.75 * drift + 0.05 * math.sin(2 * math.pi * (u + v))
            hr, hg, hb = rainbow(hue)
            # Faint: a sheen over the metal, not paint; Iridescent is the
            # stronger film the owner ticked as well.
            share = (0.08 + 0.12 * light) * (1 - strength) + (0.22 + 0.2 * light) * strength
            line += bytes((clamp(r * (1 - share) + hr * light * 255 * share),
                           clamp(g * (1 - share) + hg * light * 255 * share),
                           clamp(b * (1 - share) + hb * light * 255 * share), a))
        out.append(bytes(line))
    return out


def write_rainbows(directory=ROOT):
    """neon and futuristic in every colour, and the metals' chameleon
    finish, from their own textures."""
    for name in ("neon", "futuristic"):
        write_png(Path(directory) / f"{name}-rainbow.png", rainbow_rows(read_png(ROOT / f"{name}.png")))
    for index, name in enumerate(("metallic", "plate")):
        source = read_png(ROOT / f"{name}.png")
        write_png(Path(directory) / f"{name}-chameleon.png", chameleon_rows(source, 509 + index * 17))
        write_png(Path(directory) / f"{name}-iridescent.png", chameleon_rows(source, 509 + index * 17, 1.0))
    print("rainbow and chameleon looks written to", directory)


def marble(seed):
    """Marble: a light stone with dark veins where a warped noise field crosses
    its middle, and a soft mottling."""
    field = octaves(seed, [4, 8, 16, 32])
    mottle = octaves(seed + 3, [6, 12])

    def detail(u, v):
        f = field[0].at(u * 4, v * 4) * 0.5 + field[1].at(u * 8, v * 8) * 0.25 + field[2].at(u * 16, v * 16) * 0.15 + field[3].at(u * 32, v * 32) * 0.1
        vein = math.exp(-((f - 0.5) ** 2) / 0.0012)
        second = math.exp(-((f - 0.42) ** 2) / 0.0006)
        m = mottle[0].at(u * 6, v * 6) * 0.6 + mottle[1].at(u * 12, v * 12) * 0.4
        return 0.5 - vein * 0.32 - second * 0.14 + (m - 0.5) * 0.18
    return detail


def peel(seed):
    """A lime's peel: dense soft dimples."""
    pores = octaves(seed, [96, 192])
    soft = octaves(seed + 4, [6])

    def detail(u, v):
        p = pores[0].at(u * 96, v * 96) * 0.6 + pores[1].at(u * 192, v * 192) * 0.4
        return 0.5 + (p - 0.5) * 0.5 + (soft[0].at(u * 6, v * 6) - 0.5) * 0.2
    return detail


# ambientCG texture sets (CC0 1.0, https://ambientcg.com/view?id=<Id>) the
# real looks are made from: the magma's heats, and the beds of the river's
# moods.
LAVA_SETS = {"magma": "Lava003", "ember": "Lava002", "blaze": "Lava005", "molten": "Lava004"}
WATER = {
    # the bed, the water's own colour, how strongly it absorbs the bed's
    # light with depth, the sky it mirrors, and how much of that sky
    "river": ("Gravel021", (0.16, 0.46, 0.54), 0.8, (0.72, 0.82, 0.92), 0.35),
    "clear": ("Gravel020", (0.24, 0.52, 0.56), 0.45, (0.80, 0.88, 0.95), 0.30),
    "deep": ("Gravel021", (0.02, 0.10, 0.22), 2.2, (0.55, 0.65, 0.80), 0.40),
    "jade": ("Gravel021", (0.08, 0.40, 0.30), 1.2, (0.75, 0.88, 0.84), 0.35),
    "sunset": ("Gravel020", (0.24, 0.20, 0.36), 1.0, (1.0, 0.62, 0.34), 0.5),
}
_qt_app = None


def qt_modules():
    """Qt's image classes from PySide6 or PyQt6, whichever is installed."""
    import os
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    try:
        from PySide6 import QtCore, QtGui
    except ImportError:
        from PyQt6 import QtCore, QtGui
    global _qt_app
    if _qt_app is None:
        _qt_app = QtGui.QGuiApplication(sys.argv[:1])
    return QtGui, QtCore


def load_rows(path, size=SIZE):
    """A picture's pixels, scaled to size, as rows of (r, g, b) in 0..1."""
    QtGui, QtCore = qt_modules()
    image = QtGui.QImage(str(path))
    if image.isNull():
        raise SystemExit(f"cannot read {path}")
    image = image.scaled(size, size, QtCore.Qt.AspectRatioMode.IgnoreAspectRatio,
                         QtCore.Qt.TransformationMode.SmoothTransformation)
    image = image.convertToFormat(QtGui.QImage.Format.Format_RGBA8888)
    bits = image.constBits()
    # PyQt6 hands out a pointer that must be read by its size; PySide6 a
    # memoryview.
    data = bits.asstring(image.sizeInBytes()) if hasattr(bits, "asstring") else bytes(bits)
    stride = image.bytesPerLine()
    return [[(data[y * stride + x * 4] / 255, data[y * stride + x * 4 + 1] / 255,
              data[y * stride + x * 4 + 2] / 255) for x in range(size)] for y in range(size)]


def ripples(size, seed, components=96, wind=(1.0, 0.3)):
    """A water surface's small waves as a height field that tiles: wave
    components on the texture's own lattice (each repeats exactly across
    it), their amplitudes from the Phillips spectrum of wind-driven waves
    (Tessendorf 2001: the waves the wind builds most are the largest, the
    shortest are damped, and waves run with the wind), their phases
    random. The strongest components and a weighted handful of the rest.
    Returns rows of heights in -1..1."""
    rng = random.Random(seed)
    wx, wy = wind
    length = math.hypot(wx, wy)
    wx, wy = wx / length, wy / length
    # The waves the wind builds most are a twelfth of the texture long
    # (Phillips' L is a wavelength over 2 pi); waves under a few texels are
    # damped.
    largest = size / 75.0
    cutoff = size / 640.0
    candidates = []
    for n in range(-40, 41):
        for m in range(-40, 41):
            if n == 0 and m == 0:
                continue
            kx, ky = 2 * math.pi * n / size, 2 * math.pi * m / size
            k = math.hypot(kx, ky)
            along = (kx * wx + ky * wy) / k
            if along <= 0:
                continue
            # Phillips spreads waves by the square of their alignment with
            # the wind; a river's surface is choppier, so by the alignment.
            power = math.exp(-1 / (k * largest) ** 2) / k ** 4 * along * math.exp(-(k * cutoff) ** 2)
            candidates.append((power, kx, ky))
    candidates.sort(reverse=True)
    strongest = candidates[:components // 4]
    rest = candidates[len(strongest):]
    picks = strongest + rng.choices(rest, weights=[c[0] for c in rest], k=components - len(strongest))
    waves = [(math.sqrt(power), kx, ky, rng.uniform(0, 2 * math.pi)) for power, kx, ky in picks]
    # cos(kx x + ky y + phase) = cos(kx x + phase) cos(ky y) - sin(kx x + phase) sin(ky y)
    cos_x = [[math.cos(kx * x + phase) for x in range(size)] for _, kx, _, phase in waves]
    sin_x = [[math.sin(kx * x + phase) for x in range(size)] for _, kx, _, phase in waves]
    rows = []
    for y in range(size):
        row = [0.0] * size
        for index, (amplitude, _, ky, _) in enumerate(waves):
            cy, sy = math.cos(ky * y) * amplitude, math.sin(ky * y) * amplitude
            row = [r + c * cy - s * sy for r, c, s in zip(row, cos_x[index], sin_x[index])]
        rows.append(row)
    top = max(abs(h) for row in rows for h in row) or 1.0
    return [[h / top for h in row] for row in rows]


def surface_normals(heights, strength):
    """Unit normals (x, y, z) of a tiling height field, and how much light
    its surface focuses at each point (the negative of its curvature)."""
    size = len(heights)
    normals, focus = [], []
    for y in range(size):
        up, down, row = heights[(y - 1) % size], heights[(y + 1) % size], heights[y]
        normal_row, focus_row = [], []
        for x in range(size):
            left, right = row[(x - 1) % size], row[(x + 1) % size]
            dx, dy = (right - left) * strength, (down[x] - up[x]) * strength
            length = math.sqrt(dx * dx + dy * dy + 1)
            normal_row.append((-dx / length, -dy / length, 1 / length))
            focus_row.append(-(left + right + up[x] + down[x] - 4 * row[x]))
        normals.append(normal_row)
        focus.append(focus_row)
    top = max(abs(f) for row in focus for f in row) or 1.0
    return normals, [[f / top for f in row] for row in focus]


def normal_rows(normals):
    """A tangent-space normal map (OpenGL convention) as rows of RGBA bytes."""
    return [bytes(b for nx, ny, nz in row for b in (clamp((nx * 0.5 + 0.5) * 255), clamp((ny * 0.5 + 0.5) * 255),
                                                    clamp((nz * 0.5 + 0.5) * 255), 255)) for row in normals]


def water_rows(bed, normals, focus, water, absorb, sky, mirror, seed):
    """A river seen from above: its bed through water of one depth (deeper
    and shallower along it; the 3D renderer lightens the banks itself, so
    the texture tiles any way round), the bed's light absorbed with depth
    (red first, blue last) and the water's own colour scattered back
    instead, the light the ripples focus on the bed, the sky mirrored where
    a ripple tilts the surface toward the viewer, and the sun's glitter on
    the ripples. Returns rows of RGBA bytes."""
    size = len(bed)
    absorption = (1.7, 0.95, 0.55)
    light = (-0.55, -0.55, 0.63)
    along = Noise(3, seed)
    rows = []
    for y in range(size):
        v = y / size
        profile = 0.8
        row = bytearray()
        for x in range(size):
            u = x / size
            depth = profile * absorb * (0.75 + 0.5 * along.at(u * 3, v * 3))
            through = [math.exp(-k * depth) for k in absorption]
            caustic = max(0.0, focus[y][x]) * 1.3
            nx, ny, nz = normals[y][x]
            tilt = min(1.0, math.sqrt(nx * nx + ny * ny) / 0.45)
            fresnel = 0.03 + mirror * tilt * tilt * (3 - 2 * tilt)
            glint = max(0.0, nx * light[0] + ny * light[1] + nz * light[2]) ** 60 * 1.1
            for c in range(3):
                colour = bed[y][x][c] * (1 + caustic) * through[c] + water[c] * (1 - through[c])
                row.append(clamp((colour * (1 - fresnel) + sky[c] * fresnel + glint * sky[c]) * 255))
            row.append(255)
        rows.append(bytes(row))
    return rows


def lava_rows(colour, emission):
    """Cooling lava: the set's colour with its own light screened over it, so
    the glow in the cracks is in the picture the flat renderer draws."""
    return [bytes(b for c, e in zip(colour_row, emission_row)
                  for b in (clamp((1 - (1 - c[0]) * (1 - e[0] * 0.9)) * 255), clamp((1 - (1 - c[1]) * (1 - e[1] * 0.9)) * 255),
                            clamp((1 - (1 - c[2]) * (1 - e[2] * 0.9)) * 255), 255))
            for colour_row, emission_row in zip(colour, emission)]


def write_real(assets, directory=ROOT):
    """The river's moods and the magma's heats from the real sets in
    `assets`, with their relief maps."""
    heights = ripples(SIZE, 701)
    normals, focus = surface_normals(heights, 1.6)
    write_png(Path(directory) / "river-normal.png", normal_rows(normals))
    beds = {}
    for palette, (bed_set, water, absorb, sky, mirror) in WATER.items():
        if bed_set not in beds:
            beds[bed_set] = load_rows(assets / bed_set / f"{bed_set}_1K-JPG_Color.jpg")
        name = "river.png" if palette == "river" else f"river-{palette}.png"
        write_png(Path(directory) / name, water_rows(beds[bed_set], normals, focus, water, absorb, sky, mirror, 707))
        print(name)
    for palette, lava_set in LAVA_SETS.items():
        colour = load_rows(assets / lava_set / f"{lava_set}_1K-JPG_Color.jpg")
        emission = load_rows(assets / lava_set / f"{lava_set}_1K-JPG_Emission.jpg")
        name = "magma.png" if palette == "magma" else f"magma-{palette}.png"
        write_png(Path(directory) / name, lava_rows(colour, emission))
        print(name)
    crust = load_rows(assets / LAVA_SETS["magma"] / f"{LAVA_SETS['magma']}_1K-JPG_NormalGL.jpg")
    write_png(Path(directory) / "magma-normal.png",
              [bytes(b for n in row for b in (clamp(n[0] * 255), clamp(n[1] * 255), clamp(n[2] * 255), 255)) for row in crust])
    print("real looks written to", directory)


RECIPES = {
    "glass": (frosted(1), 0.45),
    "floating-glass": (frosted(2, 0.14), 0.45),
    "crystal": (frosted(3, 0.3), 0.6),
    "metallic": (brushed(4, 0.0, 0.5), 0.55),
    "plate": (brushed(5, math.pi / 2, 0.4), 0.55),
    "futuristic": (brushed(6, math.pi / 4, 0.3), 0.5),
    "minimal": (brushed(7, 0.0, 0.12), 0.3),
    "platform": (stone(8, 0.35), 0.5),
    "pedestal": (marble(9), 0.9),
    "organic": (painted(grass), 0),
    # Computed, not painted: lime_rows(), written by write_lime().
    "lime": (None, 0),
    "neon": (shimmer(12, 0.3), 0.5),
    # Computed, not painted: plasma_field(), written by write_plasma().
    "plasma": (None, 0),
    "lightning": (None, 0),
    "aurora": (None, 0),
}



# The panel decorations' materials: name -> (colour set, height set). The
# sets are ambientCG's (CC0 1.0), unpacked into <folder>/<Id>/.
DECORATION_MATERIALS = {
    "alien-metal": ("Metal046A", "PavingStones075"),
    "alien-plastic": ("Plastic010", "PavingStones075"),
    "diamond-plate": ("DiamondPlate009", "DiamondPlate009"),
    "corrugated": ("CorrugatedSteel009", "CorrugatedSteel009"),
    "hull-metal": ("Metal032", "Metal032"),
    "console-plastic": ("Plastic013A", "Plastic013A"),
    "gold": ("Metal048A", "Metal048A"),
    "copper": ("Metal035", "Metal035"),
    "walnut": ("Wood051", "Wood051"),
    "warm-wood": ("Wood066", "Wood066"),
    "leather-brown": ("Leather030", "Leather030"),
    "leather-red": ("Leather037", "Leather037"),
    "parchment": ("Paper005", "Paper005"),
    "dark-rock": ("Rock035", "Rock035"),
    "castle-stone": ("Bricks076A", "Bricks076A"),
    "marble": ("Marble012", "Marble012"),
    "bamboo": ("Bamboo001A", "Bamboo001A"),
    "velvet": ("Fabric062", "Fabric062"),
    "ice-crystal": ("Ice002", "Ice002"),
    "agate": ("Onyx006", "Onyx006"),
    "obsidian": ("Onyx013", "Onyx013"),
    "iridescent": ("ChristmasTreeOrnament019", "ChristmasTreeOrnament019"),
    "gilded-porcelain": ("ChristmasTreeOrnament013", "ChristmasTreeOrnament013"),
    "carbon": ("Fabric004", "Fabric004"),
    "perforated": ("MetalWalkway014", "MetalWalkway014"),
    "battle-plates": ("MetalPlates013", "MetalPlates013"),
    "serpent": ("Leather008", "Leather008"),
    "volcanic": ("Lava001", "Lava001"),
    "moss": ("Moss002", "Moss002"),
    "mosaic": ("Tiles131", "Tiles131"),
    "azure-tiles": ("Tiles019", "Tiles019"),
}
# More of them: precious metals and gemstones, the banded stones as they
# are, and two neutral ones, the real grain in grey, which a decoration tints
# with an alloy's colour (every gold from one gold) or a gem's (every cut
# stone from one crystal). "facets" is the relief of a cut stone, computed.
DECORATION_MATERIALS.update({
    "gold-polished": ("Metal007", "Metal007"),
    "gold-bright": ("Metal034", "Metal034"),
    "gold-foil": ("Foil002", "Foil002"),
    "silver": ("Metal049A", "Metal049A"),
    "platinum": ("Metal009", "Metal009"),
    "carnelian": ("Onyx002", "Onyx002"),
    "amber-band": ("Onyx007", "Onyx007"),
    "opal": ("Onyx009", "Onyx009"),
    "red-jasper": ("Onyx010", "Onyx010"),
    "malachite": ("Onyx011", "Onyx011"),
    "moss-agate": ("Onyx014", "Onyx014"),
    "black-gold-vein": ("Marble006", "Marble006"),
    "black-marble": ("Marble016", "Marble016"),
})
NEUTRAL_MATERIALS = {
    "gold-neutral": ("Metal048A", "Metal048A"),
    "crystal-neutral": ("Ice001", "facets"),
}
HEIGHT_SIZE = 256


def write_jpeg(target, rows, quality=88):
    """Rows of RGBA bytes as a JPEG (Qt's encoder): a photograph's colours
    in a fraction of a PNG's size."""
    QtGui, _ = qt_modules()
    width, height = len(rows[0]) // 4, len(rows)
    data = b"".join(rows)
    image = QtGui.QImage(data, width, height, width * 4, QtGui.QImage.Format.Format_RGBA8888).copy()
    if not image.save(str(target), "JPG", quality):
        raise SystemExit(f"cannot write {target}")


def write_grey_png(target, rows):
    """Rows of RGBA bytes whose channels are equal, as an 8-bit grey PNG."""
    import struct
    import zlib

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))
    width, height = len(rows[0]) // 4, len(rows)
    raw = b"".join(b"\x00" + row[0::4] for row in rows)
    Path(target).write_bytes(b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def colour_bytes(rows):
    """Rows of (r, g, b) in 0..1 as rows of RGBA bytes."""
    return [bytes(b for r, g, b_ in row for b in (clamp(r * 255), clamp(g * 255), clamp(b_ * 255), 255)) for row in rows]


def height_bytes(rows):
    """A displacement map's rows as a grey height map stretched to its full
    range, as rows of RGBA bytes."""
    values = [[(r + g + b) / 3 for r, g, b in row] for row in rows]
    low = min(min(row) for row in values)
    high = max(max(row) for row in values)
    span = max(1e-6, high - low)
    return [bytes(b for v in row for b in (clamp((v - low) / span * 255),) * 3 + (255,)) for row in values]


def neutral_bytes(rows):
    """A texture's own grain in grey, its brightness spread over the upper
    two thirds so a tint keeps it bright: rows of RGBA bytes."""
    values = [[0.299 * r + 0.587 * g + 0.114 * b for r, g, b in row] for row in rows]
    low = min(min(row) for row in values)
    high = max(max(row) for row in values)
    span = max(1e-6, high - low)
    return [bytes(b for v in row for b in (clamp((0.35 + 0.65 * (v - low) / span) * 255),) * 3 + (255,)) for row in values]


def facet_heights(size, seed, cells=36):
    """A cut stone's relief, tiling: Worley's cellular basis (1996), each
    cell a flat facet tilted its own way, so the creases between facets
    catch the light as a cut stone's edges do. Rows of heights in 0..1."""
    rng = random.Random(seed)
    sites = [(rng.random(), rng.random(), rng.uniform(-1.4, 1.4), rng.uniform(-1.4, 1.4), rng.random() * 0.3)
             for _ in range(cells)]
    rows = []
    for y in range(size):
        row = []
        for x in range(size):
            u, v = x / size, y / size
            best, value = 9.0, 0.0
            for cx, cy, gx, gy, h0 in sites:
                dx = (u - cx + 0.5) % 1.0 - 0.5
                dy = (v - cy + 0.5) % 1.0 - 0.5
                d = dx * dx + dy * dy
                if d < best:
                    best, value = d, h0 + gx * dx + gy * dy
            row.append(value)
        rows.append(row)
    low = min(min(r) for r in rows)
    high = max(max(r) for r in rows)
    return [[(h - low) / max(1e-6, high - low) for h in r] for r in rows]


def write_decorations(assets, directory=ROOT, only=None):
    """The panel decorations' materials from the real sets in `assets`
    (`only`: just these names)."""
    for name, (colour_set, height_set) in DECORATION_MATERIALS.items():
        if only and name not in only:
            continue
        colour = load_rows(assets / colour_set / f"{colour_set}_1K-JPG_Color.jpg")
        height = load_rows(assets / height_set / f"{height_set}_1K-JPG_Displacement.jpg", HEIGHT_SIZE)
        write_jpeg(Path(directory) / f"deco-{name}.jpg", colour_bytes(colour))
        write_grey_png(Path(directory) / f"deco-{name}-height.png", height_bytes(height))
        print(f"deco-{name}")
    for name, (colour_set, height_set) in NEUTRAL_MATERIALS.items():
        if only and name not in only:
            continue
        colour = load_rows(assets / colour_set / f"{colour_set}_1K-JPG_Color.jpg")
        write_jpeg(Path(directory) / f"deco-{name}.jpg", neutral_bytes(colour))
        if height_set == "facets":
            heights = facet_heights(HEIGHT_SIZE, 911)
            write_grey_png(Path(directory) / f"deco-{name}-height.png",
                           [bytes(b for h in row for b in (clamp(h * 255),) * 3 + (255,)) for row in heights])
        else:
            height = load_rows(assets / height_set / f"{height_set}_1K-JPG_Displacement.jpg", HEIGHT_SIZE)
            write_grey_png(Path(directory) / f"deco-{name}-height.png", height_bytes(height))
        print(f"deco-{name}")
    if not only or any(name in only for name in LOG_ENDS):
        write_log_ends(assets, directory)
    print("decoration materials written to", directory)


# The realistic decorations' materials, ambientCG's CC0 sets as above.
DECORATION_MATERIALS.update({
    "bark-oak": ("Bark001", "Bark001"),
    "bark-grey": ("Bark004", "Bark004"),
    "bark-mossy": ("Bark006", "Bark006"),
    "bark-birch": ("Bark007", "Bark007"),
    "bark-plane": ("Bark009", "Bark009"),
    "bark-cedar": ("Bark012", "Bark012"),
    "bark-brown": ("Bark014", "Bark014"),
    "straw": ("ThatchedRoof001A", "ThatchedRoof001A"),
    "straw-grey": ("ThatchedRoof002A", "ThatchedRoof002A"),
    "rope": ("Rope001", "Rope001"),
    "steel-rope": ("Rope002", "Rope002"),
    "rubber": ("Rubber004", "Rubber004"),
    "rubber-grain": ("Rubber001", "Rubber001"),
    "travertine": ("Travertine009", "Travertine009"),
    "travertine-banded": ("Travertine001", "Travertine001"),
    "travertine-blue": ("Travertine013", "Travertine013"),
    "concrete": ("Concrete034", "Concrete034"),
    "plaster": ("Plaster003", "Plaster003"),
    "marble-polished": ("Marble021", "Marble021"),
    "paint-hazard": ("PaintedMetal016", "PaintedMetal016"),
    "paint-white": ("PaintedMetal011", "PaintedMetal011"),
    "paint-red": ("PaintedMetal004", "PaintedMetal004"),
    "paint-green": ("PaintedMetal006", "PaintedMetal006"),
    "plastic-black": ("Plastic006", "Plastic006"),
    "plastic-yellow": ("Plastic016A", "Plastic016A"),
    "pebbles": ("Rocks023", "Rocks023"),
    "gravel": ("Gravel023", "Gravel023"),
    "metal-black": ("Metal027", "Metal027"),
    "wood-pine": ("Wood092", "Wood092"),
    "paper-aged": ("Paper006", "Paper006"),
})
# Sawn log ends: the end grain's whole disc (its box x, y, w, h in the 1K
# set) with its own ragged outline from the set's opacity, so a log's end
# shows its growth rings and its bark round them.
LOG_ENDS = {
    "log-end": ("TreeEnd004", (60, 60, 888, 896)),
    "log-end-pale": ("TreeEnd001", (28, 8, 972, 996)),
}
# The nebulae: picture, the band through it (x, y, width, height in the
# 2000-pixel rendition), whether the picture is turned a quarter first
# (the Pillars of Creation stand upright; turned, they run along a panel),
# and the columns of a bright star's long diffraction spike crossing the
# band, painted out from the pixels either side of them.
NEBULAE = {
    "nebula-carina": ("cosmic-cliffs.png", (0, 205, 2000, 250), False),
    "nebula-pillars": ("pillars-of-creation.png", (0, 360, 2000, 250), True),
    "nebula-southern-ring": ("southern-ring.png", (0, 805, 2000, 250), False),
    "nebula-tarantula": ("tarantula.png", (0, 455, 2000, 250), False),
    "deep-field": ("first-deep-field.png", (0, 900, 1960, 245), False, [(838, 849), (299, 307), (1718, 1726)]),
}
NEBULA_SIZE = (2048, 256)
NEBULA_BLEND = 256


def load_box(path, box, width, height, turn=False, clean=()):
    """A box (x, y, w, h) of a picture, turned a quarter first if asked,
    with the `clean` column ranges (in the box) painted out row by row from
    the pixels just outside them, scaled to width x height, as rows of
    (r, g, b) in 0..1."""
    QtGui, QtCore = qt_modules()
    image = QtGui.QImage(str(path))
    if image.isNull():
        raise SystemExit(f"cannot read {path}")
    if turn:
        image = image.transformed(QtGui.QTransform().rotate(90))
    x, y, w, h = box
    image = image.copy(x, y, w, h)
    for first, last in clean:
        for row in range(h):
            a = image.pixelColor(max(0, first - 2), row)
            b = image.pixelColor(min(w - 1, last + 2), row)
            for column in range(first, last + 1):
                u = (column - first + 1) / (last - first + 2)
                image.setPixelColor(column, row, QtGui.QColor.fromRgbF(
                    a.redF() + (b.redF() - a.redF()) * u, a.greenF() + (b.greenF() - a.greenF()) * u,
                    a.blueF() + (b.blueF() - a.blueF()) * u))
    image = image.scaled(width, height, QtCore.Qt.AspectRatioMode.IgnoreAspectRatio,
                                          QtCore.Qt.TransformationMode.SmoothTransformation)
    image = image.convertToFormat(QtGui.QImage.Format.Format_RGBA8888)
    bits = image.constBits()
    data = bits.asstring(image.sizeInBytes()) if hasattr(bits, "asstring") else bytes(bits)
    stride = image.bytesPerLine()
    return [[(data[r * stride + c * 4] / 255, data[r * stride + c * 4 + 1] / 255,
              data[r * stride + c * 4 + 2] / 255) for c in range(width)] for r in range(height)]


def write_log_ends(assets, directory=ROOT):
    for name, (set_id, box) in LOG_ENDS.items():
        colour = load_box(assets / set_id / f"{set_id}_1K-JPG_Color.jpg", box, SIZE, SIZE)
        opacity = load_box(assets / set_id / f"{set_id}_1K-JPG_Opacity.jpg", box, SIZE, SIZE)
        height = load_box(assets / set_id / f"{set_id}_1K-JPG_Displacement.jpg", box, HEIGHT_SIZE, HEIGHT_SIZE)
        write_png(Path(directory) / f"deco-{name}.png",
                  [bytes(b for (r, g, b_), (a, _, _) in zip(row, alpha)
                         for b in (clamp(r * 255), clamp(g * 255), clamp(b_ * 255), clamp(a * 255)))
                   for row, alpha in zip(colour, opacity)])
        write_grey_png(Path(directory) / f"deco-{name}-height.png", height_bytes(height))
        print(f"deco-{name}")


def write_nebulae(source, directory=ROOT):
    """The galactic strips: a band through each Webb picture, its end
    blended into its start (smoothstep over NEBULA_BLEND pixels) so it
    repeats along a panel without a seam, and its brightness, smoothed,
    as a gentle height."""
    width, height = NEBULA_SIZE
    for name, entry in NEBULAE.items():
        picture, box, turn = entry[:3]
        rows = load_box(Path(source) / picture, box, width + NEBULA_BLEND, height, turn, entry[3] if len(entry) > 3 else ())
        strip = []
        for row in rows:
            out = []
            for x in range(width):
                if x < NEBULA_BLEND:
                    w = x / NEBULA_BLEND
                    w = w * w * (3 - 2 * w)
                    a, b = row[x], row[x + width]
                    out.append((a[0] * w + b[0] * (1 - w), a[1] * w + b[1] * (1 - w), a[2] * w + b[2] * (1 - w)))
                else:
                    out.append(row[x])
            strip.append(out)
        write_jpeg(Path(directory) / f"deco-{name}.jpg", colour_bytes(strip), 90)
        # A quarter-size brightness, blurred twice by a 3 x 3 box that wraps
        # along the strip.
        qw, qh = width // 4, height // 4
        bright = [[sum(0.299 * strip[y * 4 + j][x * 4 + i][0] + 0.587 * strip[y * 4 + j][x * 4 + i][1]
                       + 0.114 * strip[y * 4 + j][x * 4 + i][2] for j in range(4) for i in range(4)) / 16
                   for x in range(qw)] for y in range(qh)]
        for _ in range(2):
            bright = [[sum(bright[min(qh - 1, max(0, y + j))][(x + i) % qw] for j in (-1, 0, 1) for i in (-1, 0, 1)) / 9
                       for x in range(qw)] for y in range(qh)]
        low = min(min(r) for r in bright)
        high = max(max(r) for r in bright)
        span = max(1e-6, high - low)
        write_grey_png(Path(directory) / f"deco-{name}-height.png",
                       [bytes(b for v in row for b in (clamp((v - low) / span * 255),) * 3 + (255,)) for row in bright])
        print(f"deco-{name}")
    print("nebulae written to", directory)


def render_svg(path):
    from PySide6.QtCore import QRectF
    from PySide6.QtGui import QColor, QImage, QPainter
    from PySide6.QtSvg import QSvgRenderer
    image = QImage(SIZE, SIZE, QImage.Format.Format_ARGB32)
    image.fill(QColor(0, 0, 0, 0))
    renderer = QSvgRenderer(str(path))
    painter = QPainter(image)
    renderer.render(painter, QRectF(0, 0, SIZE, SIZE))
    painter.end()
    return image


def clamp(value):
    return 0 if value < 0 else 255 if value > 255 else int(value)


def build(base, detail, strength, seed=0):
    """Multiply the base colours by a detail field (1 = unchanged), or let a
    painted recipe draw the texture itself."""
    from PySide6.QtGui import QColor, QImage, QPainter
    out = QImage(SIZE, SIZE, QImage.Format.Format_ARGB32)
    paint = getattr(detail, "paint", None)
    if paint is not None:
        out.fill(QColor(0, 0, 0, 0))
        painter = QPainter(out)
        paint(painter, SIZE, random.Random(seed))
        painter.end()
        return out
    for y in range(SIZE):
        v = y / SIZE
        for x in range(SIZE):
            u = x / SIZE
            factor = 1 + strength * (detail(u, v) - 0.5) * 2
            colour = base.pixelColor(x, y)
            out.setPixelColor(x, y, QColor(clamp(colour.red() * factor), clamp(colour.green() * factor),
                                           clamp(colour.blue() * factor), colour.alpha()))
    return out


# How far each look's surface stands out: the relief map's strength.
RELIEF = {
    "glass": 0.25, "floating-glass": 0.2, "crystal": 0.9, "metallic": 0.5, "plate": 0.9,
    "futuristic": 0.8, "minimal": 0.2, "platform": 1.0, "pedestal": 0.7, "organic": 1.0,
    "lime": 0.8, "neon": 0.6, "plasma": 0.5, "lightning": 0.5, "aurora": 0.35,
}


def relief(image, strength):
    """A tangent-space normal map from the texture's brightness: brighter is
    higher, so grass blades, stone blocks, rivets and veins stand out under
    the scene's lights."""
    from PySide6.QtGui import QColor, QImage
    size = image.width()
    height = [[0.0] * size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            colour = image.pixelColor(x, y)
            height[y][x] = (colour.red() * 0.3 + colour.green() * 0.59 + colour.blue() * 0.11) / 255
    out = QImage(size, size, QImage.Format.Format_ARGB32)
    scale = 2.5 * strength
    for y in range(size):
        up, down = height[(y - 1) % size], height[(y + 1) % size]
        row = height[y]
        for x in range(size):
            left, right = (x - 1) % size, (x + 1) % size
            dx = (row[right] - row[left]) * scale
            dy = (down[x] - up[x]) * scale
            length = math.sqrt(dx * dx + dy * dy + 1)
            nx, ny, nz = -dx / length, -dy / length, 1 / length
            out.setPixelColor(x, y, QColor(clamp((nx * 0.5 + 0.5) * 255), clamp((ny * 0.5 + 0.5) * 255),
                                           clamp((nz * 0.5 + 0.5) * 255), 255))
    return out


def write_rasters():
    from PySide6.QtGui import QGuiApplication
    app = QGuiApplication(sys.argv[:1])
    for index, (name, (detail, strength)) in enumerate(RECIPES.items()):
        if name == "plasma":
            write_plasma()
            continue
        if name == "lightning":
            write_lightning()
            continue
        if name == "aurora":
            write_aurora()
            continue
        if name == "lime":
            write_lime()
            continue
        target = ROOT / f"{name}.png"
        image = build(render_svg(ROOT / f"{name}.svg"), detail, strength, seed=100 + index)
        image.save(str(target), "PNG")
        bumps = ROOT / f"{name}-normal.png"
        relief(image, RELIEF.get(name, 0.5)).save(str(bumps), "PNG")
        print(name, target.stat().st_size, "bytes; relief", bumps.stat().st_size, "bytes")
    # The rainbow versions of the painted looks, from the textures just made.
    write_rainbows()
    del app


if __name__ == "__main__":
    stage = sys.argv[1] if len(sys.argv) > 1 else "--svg"
    if stage in ("--svg", "--all"):
        write_swatches()
    if stage in ("--raster", "--all"):
        write_rasters()
    # Plasma alone, without Qt: `--plasma [directory]`. The raster stage
    # draws it the same way.
    if stage == "--plasma":
        write_plasma(sys.argv[2] if len(sys.argv) > 2 else ROOT)
    if stage == "--lightning":
        write_lightning(sys.argv[2] if len(sys.argv) > 2 else ROOT)
    if stage == "--aurora":
        write_aurora(sys.argv[2] if len(sys.argv) > 2 else ROOT)
    if stage == "--lime":
        write_lime(sys.argv[2] if len(sys.argv) > 2 else ROOT)
    if stage == "--rainbows":
        write_rainbows(sys.argv[2] if len(sys.argv) > 2 else ROOT)
    if stage == "--real":
        write_real(Path(sys.argv[2]), Path(sys.argv[3]) if len(sys.argv) > 3 else ROOT)
    if stage == "--decorations":
        write_decorations(Path(sys.argv[2]), Path(sys.argv[3]) if len(sys.argv) > 3 else ROOT,
                          set(sys.argv[4:]) or None)
    if stage == "--nebulae":
        write_nebulae(Path(sys.argv[2]), Path(sys.argv[3]) if len(sys.argv) > 3 else ROOT)
