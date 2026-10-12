#!/usr/bin/env python3
"""Original Arch Dock material textures; GPL-3.0-or-later.

Two stages, both deterministic and authored for this project, with no
reference pixels, downloaded artwork or fonts:

1. The ADREP-TASK-004 vector swatches (`<look>.svg`): SVG primitives that
   define each look's colours and pattern.
2. The raster textures the renderers use (`<look>.png`, 512 px): each
   swatch rendered and given real surface detail - brushed streaks, grass
   blades, flowing plasma, marble veins, stone speckle, frost or a soft
   shimmer - from tileable value noise and strokes on fixed seeds, and a
   relief map beside it (`<look>-normal.png`, a tangent-space normal map
   from the texture's brightness) that the 3D renderer lights as a real
   surface. This stage needs PySide6 (QtSvg renders the swatch); run it
   with `--raster`, or `--all` for both stages. Theme packages keep their
   vector surfaces.
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
    elif name in ("neon", "plasma", "lime"):
        parts.append(rect(0, 0, 128, 128, "#555" if name == "neon" else "#343434"))
        for y in range(0, 160, 24 if name == "neon" else 17):
            if name == "neon":
                d = f"M0 {y} H128"
                parts.extend([path(d, "#999", 9), path(d, "#eee", 4), path(d, "#fff", 1)])
            elif name == "plasma":
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
             "platform", "plate", "pedestal")
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


def plasma(painter, size, rng):
    """Contained plasma: bright filaments flowing along the band inside dark
    borders, each with a wide violet glow and a white-pink core; the drawing
    wraps at its left and right edges so it runs round a ring without a seam."""
    from PySide6.QtCore import QPointF
    from PySide6.QtGui import QColor, QLinearGradient, QPainterPath, QPen
    painter.fillRect(0, 0, size, size, QColor(10, 3, 20))
    band = QLinearGradient(0, 0, 0, size)
    band.setColorAt(0, QColor(10, 3, 20)); band.setColorAt(0.18, QColor(52, 14, 92)); band.setColorAt(0.5, QColor(90, 28, 150))
    band.setColorAt(0.82, QColor(52, 14, 92)); band.setColorAt(1, QColor(10, 3, 20))
    painter.fillRect(0, 0, size, size, band)
    painter.setRenderHint(painter.RenderHint.Antialiasing, True)
    wobble = octaves(77, [8, 16])
    for index in range(26):
        lane = size * rng.uniform(0.16, 0.84)
        sway = size * rng.uniform(0.04, 0.11)
        points = []
        steps = 64
        for step in range(steps + 1):
            x = step / steps * size
            # Periodic in x: the same offset at both ends.
            phase = step / steps * 2 * math.pi
            y = lane + sway * (math.sin(phase * rng.choice((1, 2, 3)) + index) * 0.5
                               + (wobble[0].at(step / 5 + index * 3, index) - 0.5) * 1.6
                               + (wobble[1].at(step / 2.5 + index * 7, index) - 0.5) * 0.8)
            points.append((x, y))
        # Close the wrap: ease the last points back onto the first.
        for step in range(steps - 6, steps + 1):
            share = (step - (steps - 6)) / 6
            points[step] = (points[step][0], points[step][1] * (1 - share) + points[0][1] * share)
        for dx in (-size, 0, size):
            path = QPainterPath(QPointF(points[0][0] + dx, points[0][1]))
            for x, y in points[1:]:
                path.lineTo(QPointF(x + dx, y))
            for width, colour in ((16, QColor(140, 60, 220, 60)), (7, QColor(190, 110, 255, 120)), (2.2, QColor(255, 225, 255, 235))):
                pen = QPen(colour, width); pen.setCapStyle(pen.capStyle().RoundCap); pen.setJoinStyle(pen.joinStyle().RoundJoin)
                painter.setPen(pen); painter.drawPath(path)
    # Dark borders: the plasma stays within the band.
    edge = QLinearGradient(0, 0, 0, size)
    edge.setColorAt(0, QColor(10, 3, 20, 255)); edge.setColorAt(0.1, QColor(10, 3, 20, 0))
    edge.setColorAt(0.9, QColor(10, 3, 20, 0)); edge.setColorAt(1, QColor(10, 3, 20, 255))
    painter.fillRect(0, 0, size, size, edge)


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
    "lime": (peel(11), 0.5),
    "neon": (shimmer(12, 0.3), 0.5),
    "plasma": (painted(plasma), 0),
}



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
    "lime": 0.8, "neon": 0.6, "plasma": 0.5,
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
        target = ROOT / f"{name}.png"
        image = build(render_svg(ROOT / f"{name}.svg"), detail, strength, seed=100 + index)
        image.save(str(target), "PNG")
        bumps = ROOT / f"{name}-normal.png"
        relief(image, RELIEF.get(name, 0.5)).save(str(bumps), "PNG")
        print(name, target.stat().st_size, "bytes; relief", bumps.stat().st_size, "bytes")
    del app


if __name__ == "__main__":
    stage = sys.argv[1] if len(sys.argv) > 1 else "--svg"
    if stage in ("--svg", "--all"):
        write_swatches()
    if stage in ("--raster", "--all"):
        write_rasters()
