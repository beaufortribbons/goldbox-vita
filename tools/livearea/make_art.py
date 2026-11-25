#!/usr/bin/env python3
"""LiveArea art for a game, drawn from scratch (no box art, no SSI logos).

A stone gateway at dusk in a mountain frontier, the title in gold.
Usage: make_art.py <out_dir> "<TITLE LINE 1>" "<TITLE LINE 2>"
Writes bg.png (840x500) and startup.png (280x158), 8-bit palette PNGs as
LiveArea expects. The bubble, icon0.png (128x128), is drawn separately.
"""
import math
import random
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

FONT = "/System/Library/Fonts/Supplemental/Copperplate.ttc"
GOLD = (232, 190, 92)
GOLD_DARK = (120, 82, 28)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def ridge(w, h, base, amp, seed, rough):
    rnd = random.Random(seed)
    phases = [rnd.uniform(0, 6.28) for _ in range(4)]
    pts = []
    for x in range(0, w + 1, 2):
        y = base
        for i, p in enumerate(phases):
            f = (i + 1) * rough / w
            y -= amp / (i + 1.4) * math.sin(x * f * 6.28 + p)
        pts.append((x, y))
    return pts + [(w, h), (0, h)]


def scene(w, h, seed=7):
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    top, mid, low = (18, 22, 52), (88, 52, 92), (226, 128, 70)
    for y in range(h):
        t = y / h
        c = lerp(top, mid, t / 0.55) if t < 0.55 else lerp(mid, low, (t - 0.55) / 0.45)
        d.line([(0, y), (w, y)], fill=c)
    rnd = random.Random(seed)
    for _ in range(w * h // 900):
        x, y = rnd.randrange(w), rnd.randrange(int(h * 0.45))
        b = rnd.randrange(150, 255)
        d.point((x, y), fill=(b, b, b))
    # Moon
    r = h * 0.07
    mx, my = w * 0.78, h * 0.2
    d.ellipse([mx - r, my - r, mx + r, my + r], fill=(250, 236, 200))
    # Mountain ranges, far to near
    ranges = [
        (h * 0.58, h * 0.16, (70, 50, 96), 3.0),
        (h * 0.68, h * 0.12, (46, 34, 70), 4.5),
        (h * 0.80, h * 0.07, (26, 20, 44), 7.0),
    ]
    for i, (base, amp, col, rough) in enumerate(ranges):
        d.polygon(ridge(w, h, base, amp, seed + i, rough), fill=col)
    # Pines on the near slope
    for i in range(int(w / 9)):
        x = rnd.uniform(0, w)
        if abs(x - w / 2) < w * 0.13:
            continue
        th = rnd.uniform(0.07, 0.15) * h
        yb = h * rnd.uniform(0.86, 0.98)
        d.polygon([(x, yb - th), (x - th * 0.28, yb), (x + th * 0.28, yb)], fill=(12, 10, 24))
    # Road to the gate
    d.polygon([(w * 0.47, h * 0.80), (w * 0.53, h * 0.80), (w * 0.66, h), (w * 0.34, h)], fill=(58, 40, 44))
    # Stone gateway: two pillars and an arch, light through the opening
    gw, gh = w * 0.20, h * 0.42
    gx, gy = w / 2 - gw / 2, h * 0.83 - gh
    opening = [gx + gw * 0.24, gy + gw * 0.30, gx + gw * 0.76, gy + gh]
    light = Image.new("L", (w, h), 0)
    ld = ImageDraw.Draw(light)
    ld.ellipse([opening[0] - gw * 0.4, opening[1] - gw * 0.3, opening[2] + gw * 0.4, opening[3] + gw * 0.2], fill=150)
    light = light.filter(ImageFilter.GaussianBlur(w / 40))
    img = Image.composite(Image.new("RGB", (w, h), (255, 196, 110)), img, light)
    d = ImageDraw.Draw(img)
    stone, edge = (64, 58, 66), (30, 26, 34)
    d.rectangle([gx, gy + gw * 0.25, gx + gw, gy + gh], fill=stone, outline=edge, width=max(1, int(w / 300)))
    d.ellipse([gx, gy, gx + gw, gy + gw * 0.55], fill=stone, outline=edge, width=max(1, int(w / 300)))
    d.rectangle([opening[0], opening[1] + gw * 0.13, opening[2], opening[3]], fill=(255, 214, 140))
    d.ellipse([opening[0], opening[1], opening[2], opening[1] + gw * 0.30], fill=(255, 214, 140))
    for row in range(1, 6):
        yy = gy + gw * 0.25 + (gh - gw * 0.25) * row / 6
        d.line([(gx, yy), (opening[0], yy)], fill=edge, width=1)
        d.line([(opening[2], yy), (gx + gw, yy)], fill=edge, width=1)
    return img


def text_centered(img, y, text, size, fill=GOLD):
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(FONT, size, index=1)
    while d.textlength(text, font=font) > img.width * 0.92 and size > 8:
        size -= 1
        font = ImageFont.truetype(FONT, size, index=1)
    x = (img.width - d.textlength(text, font=font)) / 2
    sw = max(1, size // 12)
    d.text((x, y), text, font=font, fill=fill, stroke_width=sw, stroke_fill=(20, 12, 8))
    return size


def frame(img, width):
    d = ImageDraw.Draw(img)
    w, h = img.size
    for i in range(width):
        c = lerp(GOLD, GOLD_DARK, i / max(1, width - 1))
        d.rectangle([i, i, w - 1 - i, h - 1 - i], outline=c)


def save8(img, path):
    img.convert("RGB").quantize(colors=256, method=Image.Quantize.MEDIANCUT).save(path, optimize=True)


def main():
    out, line1, line2 = sys.argv[1], sys.argv[2], sys.argv[3]
    # bg.png: the scene, title across the sky
    bg = scene(840, 500)
    text_centered(bg, 30, line1, 64)
    text_centered(bg, 104, line2, 30)
    save8(bg, f"{out}/bg.png")
    # startup.png: the gate button on the LiveArea
    st = scene(280, 158, seed=3)
    text_centered(st, 8, line1, 30)
    text_centered(st, 42, line2, 14)
    frame(st, 3)
    save8(st, f"{out}/startup.png")


if __name__ == "__main__":
    main()
