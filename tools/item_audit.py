#!/usr/bin/env python3
"""Render every ITEM_DEFS entry next to its real items.png cell.

This is the sprite audit used to fix mismatched backpack icons. It parses
`source/items.cpp`, reads the sprite index from the last field of each
definition, crops that cell from SPD's `items.png` and writes a labelled
contact sheet.

Usage:
  tools/venv/bin/python tools/item_audit.py \
      [--upstream ../spd-upstream] [--out docs/item_audit.png]
"""

import argparse
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEF_RE = re.compile(
    r'\s*\{"([^"]+)", "([^"]*)", [^,]+, [^,]+, \d+, \d+, (-?\d+)\},?')

# ItemSpriteSheet.java: xy(x, y) = (x-1) + 16*(y-1)
# Keep these ranges next to the audit so regressions are obvious.
GROUPS = {
    "gold/key/amulet": (18, 61),
    "bombs": (80, 95),
    "weapons T1-T5": (96, 143),
    "missiles/darts": (144, 175),
    "armor": (176, 191),
    "wands": (208, 223),
    "rings": (224, 239),
    "scrolls": (304, 319),
    "stones": (336, 351),
    "potions": (352, 383),
    "seeds": (384, 399),
    "food": (432, 447),
    "quest": (464, 479),
    "bags": (480, 495),
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--upstream", default=os.path.join(ROOT, "..", "spd-upstream"),
                    help="path to a shattered-pixel-dungeon checkout")
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "item_audit.png"))
    ap.add_argument("--scale", type=int, default=4)
    args = ap.parse_args()

    items = []
    with open(os.path.join(ROOT, "source", "items.cpp"), encoding="utf-8") as f:
        for line in f:
            m = DEF_RE.match(line)
            if m:
                items.append((m.group(1), int(m.group(3))))

    sheet_path = os.path.join(args.upstream, "core", "src", "main", "assets",
                              "sprites", "items.png")
    font_path = os.path.join(ROOT, "tools", "fonts",
                             "fusion-pixel-12px-monospaced-zh_hans.ttf")
    sheet = Image.open(sheet_path).convert("RGBA")
    if sheet.size != (256, 512):
        sys.exit(f"unexpected items.png size: {sheet.size}")

    scale = args.scale
    cols = 8
    cw, ch = 18 * scale, 34 * scale
    rows = (len(items) + cols - 1) // cols
    out = Image.new("RGBA", (cols * cw, rows * ch), (24, 24, 28, 255))
    draw = ImageDraw.Draw(out)
    font = ImageFont.truetype(font_path, 12)

    for i, (name, idx) in enumerate(items):
        cx = (i % cols) * cw
        cy = (i // cols) * ch
        if idx >= 0:
            sx = (idx % 16) * 16
            sy = (idx // 16) * 16
            sprite = sheet.crop((sx, sy, sx + 16, sy + 16))
            sprite = sprite.resize((16 * scale, 16 * scale), Image.NEAREST)
            out.alpha_composite(sprite, (cx + scale, cy + scale))
        draw.text((cx + 2, cy + 18 * scale), str(idx), font=font,
                  fill=(255, 220, 80, 255))
        draw.text((cx + 2, cy + 30 * scale), name, font=font,
                  fill=(240, 240, 240, 255))

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    out.save(args.out)
    print(f"wrote {args.out} ({len(items)} items)")

    used = {idx for _, idx in items if idx >= 0}
    for label, (lo, hi) in GROUPS.items():
        stray = sorted(i for i in used if lo <= i <= hi)
        if stray:
            print(f"  {label}: {stray}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
