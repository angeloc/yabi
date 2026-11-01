#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Draws a progress animation for a yabi theme: a black screen, an optional
logo in the middle and a progress bar below it, one frame per percent.

    make_progress_gif.py out.gif [--logo logo.png] [--size 640x480]

Needs Pillow.
"""

import argparse
import sys

from PIL import Image, ImageDraw

BAR = (230, 230, 230)
TRACK = (60, 60, 60)


def frames(size, logo):
    w, h = size
    bar_w, bar_h = w * 5 // 8, max(6, h // 40)
    base = Image.new("RGB", size, (0, 0, 0))
    if logo is not None:
        scale = min(1.0, (w * 0.6) / logo.width, (h * 0.55) / logo.height)
        logo = logo.resize((round(logo.width * scale), round(logo.height * scale)), Image.LANCZOS)
        top = (h - logo.height - 3 * bar_h) // 2
        base.paste(logo, ((w - logo.width) // 2, top), logo)
        bar_y = top + logo.height + 2 * bar_h
    else:
        bar_y = (h - bar_h) // 2
    bar_x = (w - bar_w) // 2

    for percent in range(101):
        im = base.copy()
        d = ImageDraw.Draw(im)
        d.rectangle([bar_x, bar_y, bar_x + bar_w - 1, bar_y + bar_h - 1], fill=TRACK)
        filled = bar_w * percent // 100
        if filled:
            d.rectangle([bar_x, bar_y, bar_x + filled - 1, bar_y + bar_h - 1], fill=BAR)
        yield im


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("out")
    p.add_argument("--logo")
    p.add_argument("--size", default="640x480")
    p.add_argument("--delay", type=int, default=20, help="ms per frame")
    a = p.parse_args()

    size = tuple(int(v) for v in a.size.split("x"))
    logo = Image.open(a.logo).convert("RGBA") if a.logo else None
    rgb = list(frames(size, logo))

    # One palette for every frame keeps unchanged pixels identical, so each
    # frame only stores the bar. The middle frame has both bar colors.
    palette = rgb[50].quantize(colors=255, method=Image.MEDIANCUT, dither=Image.NONE)
    out = [im.quantize(palette=palette, dither=Image.NONE) for im in rgb]
    out[0].save(a.out, save_all=True, append_images=out[1:], duration=a.delay, loop=0,
                optimize=False, disposal=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
