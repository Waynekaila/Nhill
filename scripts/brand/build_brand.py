#!/usr/bin/env python3
"""Builds every Nhill brand asset from code (reproducible, no manual export).

The symbol is a monogram "N" whose right stem rises higher than the left one —
a climb towards a summit — with a gold pennant planted at its top (the summit
reached, the goal).
It is a nod to Napoleon Hill ("Think and Grow Rich": growth, aiming higher) and
to the word "hill". Drawn with a single stroke and round joins so it stays
readable from 16 px to print.

Outputs:
  assets/brand/  nhill-mark.svg, nhill-logo{,-reverse,-mono,-document}.svg (+ .png previews),
                 nhill-icon-{business,console,pos}.svg, nhill-adaptive-foreground.svg
  assets/icons/  nhill-{app}-{16..1024}.png, nhill-{app}.ico
  client/android/res/  launcher icons (Nhill Business)

The wordmark is converted to outlines from IBM Plex Sans SemiBold (OFL), so the
SVG files do not depend on installed fonts.

Usage: python3 scripts/brand/build_brand.py  (needs fontTools, Pillow, rsvg-convert)
"""
import subprocess
import sys
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
FONT = ROOT / "assets/fonts/IBMPlexSans-SemiBold.ttf"
BRAND = ROOT / "assets/brand"
ICONS = ROOT / "assets/icons"
ANDROID = ROOT / "client/android/res"

# ---------------------------------------------------------------- palette
NIGHT_900 = "#0E2236"   # sidebar, dark surfaces
NIGHT_700 = "#173F5F"   # primary ("Nhill blue")
NIGHT_500 = "#2D6A8F"
NIGHT_300 = "#9BC2DA"
GOLD_600 = "#B7862A"    # accent: the summit
GOLD_400 = "#D9A945"
INK_900 = "#14181F"
INK_600 = "#56606B"
PAPER = "#F7F5F0"
WHITE = "#FFFFFF"

# Geometry on a 256 grid: one stroke "M left-bottom V left-top L right-bottom V right-top".
STROKE = 34
LEFT_X, RIGHT_X = 58, 162
BOTTOM, LEFT_TOP, RIGHT_TOP = 214, 120, 42
# Pennant attached to the right side of the right stem, near its top.
FLAG = [(RIGHT_X + STROKE / 2 - 2, RIGHT_TOP - 8), (RIGHT_X + STROKE / 2 + 52, RIGHT_TOP + 14),
        (RIGHT_X + STROKE / 2 - 2, RIGHT_TOP + 38)]


def mark(fg, summit):
    d = f"M{LEFT_X} {BOTTOM} V{LEFT_TOP} L{RIGHT_X} {BOTTOM} V{RIGHT_TOP}"
    pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in FLAG)
    return (f'<path d="{d}" fill="none" stroke="{fg}" stroke-width="{STROKE}" '
            f'stroke-linecap="round" stroke-linejoin="round"/>\n    '
            f'<polygon points="{pts}" fill="{summit}" stroke="{summit}" stroke-width="6" stroke-linejoin="round"/>')


# Bounds of the drawn mark (stroke included) for cropping in the horizontal logo.
MARK_X0 = LEFT_X - STROKE / 2
MARK_X1 = FLAG[1][0] + 3
MARK_Y0 = min(RIGHT_TOP - STROKE / 2, FLAG[0][1] - 3)
MARK_Y1 = BOTTOM + STROKE / 2


def wordmark(text, colour, cap_height_px, x0, baseline, tracking_em=-0.01):
    """SVG path of the wordmark (outlined glyphs) and its width."""
    font = TTFont(str(FONT))
    gs = font.getGlyphSet()
    cmap = font.getBestCmap()
    upm = font["head"].unitsPerEm
    cap = font["OS/2"].sCapHeight or 698
    scale = cap_height_px / cap
    tracking = tracking_em * upm
    x = 0.0
    parts = []
    for ch in text:
        gname = cmap[ord(ch)]
        pen = SVGPathPen(gs)
        gs[gname].draw(TransformPen(pen, (scale, 0, 0, -scale, x0 + x * scale, baseline)))
        parts.append(pen.getCommands())
        x += font["hmtx"][gname][0] + tracking
    return f'<path fill="{colour}" d="{" ".join(parts)}"/>', (x - tracking) * scale


def svg(width, height, body, title):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width:.1f} {height:.1f}" '
            f'width="{width:.0f}" height="{height:.0f}" role="img" aria-label="{title}">\n'
            f'  <title>{title}</title>\n  {body}\n</svg>\n')


def horizontal(variant, suffix=""):
    c = {
        "colour": dict(fg=NIGHT_700, summit=GOLD_600, word=INK_900, sub=INK_600),
        "reverse": dict(fg=WHITE, summit=GOLD_400, word=WHITE, sub=NIGHT_300),
        "mono": dict(fg=INK_900, summit=INK_900, word=INK_900, sub=INK_900),
        "document": dict(fg=INK_600, summit=INK_600, word=INK_600, sub=INK_600),
    }[variant]
    h = 96.0
    cap = 40.0
    mark_h = cap * 1.75           # the mark rises above the wordmark like a summit
    s = mark_h / (MARK_Y1 - MARK_Y0)
    mark_w = (MARK_X1 - MARK_X0) * s
    gap = 8.0
    baseline = (h + cap) / 2 + 6
    top = baseline - mark_h + 2   # mark bottom sits on the baseline
    m = (f'<g transform="translate(0 {top:.2f}) scale({s:.4f}) translate({-MARK_X0:.2f} {-MARK_Y0:.2f})">\n    '
         + mark(c["fg"], c["summit"]) + "\n  </g>")
    words, ww = wordmark("Nhill", c["word"], cap, mark_w + gap, baseline)
    body = m + "\n  " + words
    width = mark_w + gap + ww + 2
    if suffix:
        sub, sw = wordmark(suffix, c["sub"], cap * 0.62, width + 12, baseline, tracking_em=0.02)
        body += "\n  " + sub
        width += 12 + sw + 2
    return svg(width, h, body, "Nhill" + (" " + suffix if suffix else ""))


APPS = {
    # app: (background, mark, summit)
    "business": (NIGHT_700, WHITE, GOLD_400),
    "console": (INK_900, WHITE, GOLD_400),
    "pos": (GOLD_600, NIGHT_900, WHITE),
}


def icon(app):
    bg, fg, summit = APPS[app]
    # Mark slightly reduced and centred inside the rounded square.
    s = 0.70
    cx, cy = (MARK_X0 + MARK_X1) / 2, (MARK_Y0 + MARK_Y1) / 2
    body = (f'<rect width="256" height="256" rx="58" fill="{bg}"/>\n  '
            f'<g transform="translate({128 - s * cx:.2f} {128 - s * cy:.2f}) scale({s})">\n    ' + mark(fg, summit) + "\n  </g>")
    return svg(256, 256, body, f"Nhill {app.capitalize() if app != 'pos' else 'POS'}")


def adaptive_foreground():
    s = 66 / 108 * 0.84
    cx, cy = (MARK_X0 + MARK_X1) / 2, (MARK_Y0 + MARK_Y1) / 2
    body = (f'<g transform="translate({128 - s * cx:.2f} {128 - s * cy:.2f}) scale({s:.4f})">\n    '
            + mark(WHITE, GOLD_400) + "\n  </g>")
    return svg(256, 256, body, "Nhill Business")


def render(svg_path, png_path, w, h=None):
    png_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = ["rsvg-convert", "-w", str(w)] + (["-h", str(h)] if h else []) + ["-o", str(png_path), str(svg_path)]
    subprocess.run(cmd, check=True)


def main():
    BRAND.mkdir(parents=True, exist_ok=True)
    ICONS.mkdir(parents=True, exist_ok=True)
    for old in list(BRAND.glob("*")) + list(ICONS.glob("*")):
        old.unlink()
    files = {
        "nhill-mark.svg": svg(256, 256, mark(NIGHT_700, GOLD_600), "Nhill"),
        "nhill-logo.svg": horizontal("colour"),
        "nhill-logo-reverse.svg": horizontal("reverse"),
        "nhill-logo-mono.svg": horizontal("mono"),
        "nhill-logo-document.svg": horizontal("document"),
        "nhill-logo-business.svg": horizontal("colour", "Business"),
        "nhill-logo-console.svg": horizontal("colour", "Console"),
        "nhill-logo-pos.svg": horizontal("colour", "POS"),
        "nhill-logo-business-reverse.svg": horizontal("reverse", "Business"),
        "nhill-logo-console-reverse.svg": horizontal("reverse", "Console"),
        "nhill-logo-pos-reverse.svg": horizontal("reverse", "POS"),
        "nhill-adaptive-foreground.svg": adaptive_foreground(),
    }
    for app in APPS:
        files[f"nhill-icon-{app}.svg"] = icon(app)
    for name, content in files.items():
        (BRAND / name).write_text(content, encoding="utf-8")

    for app in APPS:
        src = BRAND / f"nhill-icon-{app}.svg"
        for size in (16, 24, 32, 48, 64, 128, 256, 512, 1024):
            render(src, ICONS / f"nhill-{app}-{size}.png", size)
        Image.open(ICONS / f"nhill-{app}-256.png").save(
            ICONS / f"nhill-{app}.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    for name in ("nhill-logo", "nhill-logo-reverse", "nhill-logo-mono", "nhill-logo-business", "nhill-logo-pos",
                 "nhill-logo-console"):
        subprocess.run(["rsvg-convert", "-h", "240", "-o", str(BRAND / f"{name}.png"), str(BRAND / f"{name}.svg")],
                       check=True)

    # Android launcher icons (Nhill Business is the Android application)
    densities = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
    for d, px in densities.items():
        render(BRAND / "nhill-icon-business.svg", ANDROID / f"mipmap-{d}/ic_launcher.png", px)
        render(BRAND / "nhill-adaptive-foreground.svg", ANDROID / f"mipmap-{d}/ic_launcher_foreground.png",
               int(px * 108 / 48))
    (ANDROID / "mipmap-anydpi-v26").mkdir(parents=True, exist_ok=True)
    (ANDROID / "mipmap-anydpi-v26/ic_launcher.xml").write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
        '    <background android:drawable="@color/ic_launcher_background"/>\n'
        '    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>\n'
        '</adaptive-icon>\n', encoding="utf-8")
    (ANDROID / "values").mkdir(parents=True, exist_ok=True)
    (ANDROID / "values/ic_launcher_background.xml").write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n'
        f'    <color name="ic_launcher_background">{NIGHT_700}</color>\n</resources>\n', encoding="utf-8")
    print("brand assets written to", BRAND, ICONS, ANDROID)


if __name__ == "__main__":
    sys.exit(main())
