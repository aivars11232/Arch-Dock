#!/usr/bin/env python3
"""Original ADREP-TASK-004 vector material swatches; GPL-3.0-or-later.

No reference pixels, downloaded artwork, fonts, filters or random seeds. SVG
primitives stay sharp at the renderer's requested resolution and also serve
as the optional Quick3D base-colour maps and icon-tile materials.
"""
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


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    for material in ("glass", "crystal", "neon", "minimal", "plasma", "lime",
                     "floating-glass", "metallic", "futuristic", "organic",
                     "platform", "plate", "pedestal"):
        (ROOT / f"{material}.svg").write_text(swatch(material), encoding="utf-8")
