#!/usr/bin/env python3
"""Cut the Adventurer's Journal entries out of the user's own Journal.pdf.

Usage: python3 tools/journal/make_journal.py <game> <Journal.pdf>
Needs: pip install pymupdf pillow  (runs on Windows, macOS and Linux)

Writes games/<game>/journal/<key>.jpg for every entry in
configs/<game>/journal.json (1..N, intro, map), scaled for the Vita's 960x544
screen. Nothing here ships with the VPK: the images come from the user's own PDF.
"""
import json, os, sys

try:
    import pymupdf
    from PIL import Image
except ImportError:
    sys.exit('Missing libraries. Run: pip install pymupdf pillow')

GAP = 14          # white rows between stacked segments
MAX_W = 940       # Vita screen width less a margin
SCALE = 1.5       # column text at 150 dpi is too small 1:1 on the Vita


def render(doc, page, dpi):
    pix = doc[page - 1].get_pixmap(dpi=dpi, colorspace=pymupdf.csRGB, alpha=False)
    if pix.stride != pix.width * 3:
        raise SystemExit('Unexpected page format on page %d' % page)
    return pix.width, pix.height, pix.samples


def crop(img, x0, y0, x1, y1):
    w, h, pix = img
    x1, y1 = min(x1, w), min(y1, h)
    rows = [pix[(y * w + x0) * 3:(y * w + x1) * 3] for y in range(y0, y1)]
    return x1 - x0, rows


def is_blank(row):
    # No dark or saturated pixel: paper only. Sample every 4th pixel.
    for i in range(0, len(row) - 2, 12):
        r, g, b = row[i], row[i + 1], row[i + 2]
        if r + g + b < 520 or max(r, g, b) - min(r, g, b) > 70:
            return False
    return True


def trim(rows):
    a, z = 0, len(rows)
    while a < z and is_blank(rows[a]):
        a += 1
    while z > a and is_blank(rows[z - 1]):
        z -= 1
    return rows[max(0, a - 6):min(len(rows), z + 6)]


BANNER_Y = 100    # crops starting above this begin inside the page's top banner


def drop_banner(rows):
    # Every page has a dark banner whose ragged bottom edge ends ~y=75-85, just
    # above the text. Cut down to the first blank gap after it, if there is
    # one close to the top (the map has none and is left whole).
    for r in range(1, 45):
        if all(is_blank(x) for x in rows[r:r + 3]):
            if any(not is_blank(x) for x in rows[:r]):
                return rows[r:]
            return rows
    return rows


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.split('\n\n')[1])
    game, pdf = sys.argv[1:3]
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    cfg = json.load(open(os.path.join(root, 'configs', game, 'journal.json'), encoding='utf-8'))
    outdir = os.path.join(root, 'games', game, 'journal')
    dpi = cfg.get('dpi', 150)
    os.makedirs(outdir, exist_ok=True)
    pages = sorted({s[0] for segs in cfg['entries'].values() for s in segs})
    doc = pymupdf.open(pdf)
    imgs = {p: render(doc, p, dpi) for p in pages}
    for key, segs in cfg['entries'].items():
        parts = []
        for p, x0, y0, x1, y1 in segs:
            w, rows = crop(imgs[p], x0, y0, x1, y1)
            if y0 < BANNER_Y:
                rows = drop_banner(rows)
            parts.append((w, trim(rows)))
        width = max(w for w, _ in parts)
        out = []
        for i, (w, rows) in enumerate(parts):
            if i:
                out += [b'\xff' * (width * 3)] * GAP
            pad = b'\xff' * ((width - w) * 3)
            out += [r + pad for r in rows]
        img = Image.frombytes('RGB', (width, len(out)), b''.join(out))
        target = min(int(width * SCALE), MAX_W)
        img = img.resize((target, round(len(out) * target / width)), Image.LANCZOS)
        img.save(os.path.join(outdir, key + '.jpg'), quality=85)
        print(key, target, 'px wide')
    print('Done. Copy', outdir, 'to ux0:data/goldbox/%s/journal/ on the Vita.' % game)


if __name__ == '__main__':
    main()
