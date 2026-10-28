#!/usr/bin/env python3
"""Import the real application screenshots into the website.

The screenshots come from the automatic tours of the compiled applications
(scripts/qa/ui-screenshots.sh, CI artefact "nhill-ci-logs-and-screenshots",
demo business with fictitious data). This tool:

1. converts each expected capture to WebP (assets/images/<name>.webp);
2. sets the width and height attributes of every <img> that shows it in
   src/pages/**, so the declared size always matches the file.

    python3 tools/import_screenshots.py SCREENSHOT_DIR [--check]

--check only reports what is missing or out of date, without writing anything.
The site must then be rebuilt (tools/build_site.py).
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from PIL import Image

SITE = Path(__file__).resolve().parent.parent
IMAGES = SITE / "assets" / "images"
PAGES = SITE / "src" / "pages"

# Website image -> tour capture (first file found wins).
MAP: dict[str, list[str]] = {
    "capture-tableau-de-bord": ["business-1366x768-05-tableau-de-bord.png"],
    "capture-facture": ["business-1366x768-07-facture.png"],
    "capture-stock": ["business-1366x768-11-stock.png"],
    "capture-rapports": ["business-1366x768-15-rapports.png"],
    "capture-journal": ["business-1366x768-15b-journal-activite.png"],
    "capture-equipe": ["business-1366x768-19-equipe.png"],
    "capture-telephone-tableau-de-bord": ["business-telephone-05-tableau-de-bord.png"],
    "capture-rendu-monnaie": ["pos-1366x768-p05-rendu-monnaie.png"],
    "capture-caisse-ticket": ["pos-1366x768-p07-ticket.png"],
    "capture-session-caisse": ["pos-1366x768-p10-session-cloturee.png"],
}

IMG_TAG = re.compile(r'<img\b[^>]*>')


def set_size(tag: str, w: int, h: int) -> str:
    for attr, val in (("width", w), ("height", h)):
        if re.search(rf'\b{attr}="\d+"', tag):
            tag = re.sub(rf'\b{attr}="\d+"', f'{attr}="{val}"', tag)
        else:
            tag = tag.replace("<img", f'<img {attr}="{val}"', 1)
    return tag


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", type=Path, help="directory holding the tour captures (PNG)")
    ap.add_argument("--check", action="store_true", help="report only, write nothing")
    args = ap.parse_args()

    sizes: dict[str, tuple[int, int]] = {}
    missing = []
    for name, candidates in MAP.items():
        src = next((args.source / c for c in candidates if (args.source / c).is_file()), None)
        if src is None:
            missing.append(f"{name}: none of {', '.join(candidates)}")
            continue
        with Image.open(src) as im:
            im = im.convert("RGB")
            sizes[name] = im.size
            out = IMAGES / f"{name}.webp"
            print(f"{src.name} -> {out.relative_to(SITE)} {im.size[0]}x{im.size[1]}")
            if not args.check:
                im.save(out, "WEBP", quality=86, method=6)

    changed = 0
    for page in sorted(PAGES.rglob("*.html")):
        text = page.read_text(encoding="utf-8")

        def fix(m: re.Match) -> str:
            tag = m.group(0)
            src = re.search(r'src="[^"]*/assets/images/([^"/]+)\.webp"', tag)
            if not src or src.group(1) not in sizes:
                return tag
            return set_size(tag, *sizes[src.group(1)])

        new = IMG_TAG.sub(fix, text)
        if new != text:
            changed += 1
            print(f"size attributes updated: {page.relative_to(SITE)}")
            if not args.check:
                page.write_text(new, encoding="utf-8")

    for m in missing:
        print(f"MISSING {m}", file=sys.stderr)
    print(f"{len(sizes)} image(s), {changed} page(s) to update{' (check only)' if args.check else ''}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
