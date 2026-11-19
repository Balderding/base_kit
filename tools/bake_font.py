"""Bakes a pixel font for the bbb_kit window into a 1-bit atlas + widths (gui\\pixelfont.bin, gui\\pixelfont_bold.bin), which
gui\\bbb_kit.rc embeds as RCDATA so bbb_kit.exe stays one file. Run it again only if the font or the characters change:

    C:\\Python315\\python.exe tools\\bake_font.py <font.ttf> <out.bin> [--size 8] [--preview out.png]

The font: Silkscreen, regular and bold (SIL Open Font License 1.1, docs\\fonts\\OFL.txt), from the Google Fonts repository
(github.com/google/fonts, ofl/silkscreen). At size 8 one design pixel is exactly one atlas pixel (the font is drawn on an
8-unit grid), so the window draws it at any whole scale with nearest-neighbour and it stays crisp.

File layout (little-endian): "BPF1", u8 rows, u8 baseline row, u8 cap height, u8 first char, u8 count, 3 bytes 0,
u16 atlas width, then per character u16 atlas x, u8 ink width, s8 x offset, u8 advance; then the atlas, rows x width
bytes of 0 or 1."""
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FIRST, LAST = 32, 126
PAD = 8          # room left of the pen for glyphs that start left of it


def glyph(font, ch, rows, base):
    im = Image.new("L", (48, rows), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = "1"          # no anti-aliasing: every pixel on or off
    d.text((PAD, base), ch, font=font, fill=255, anchor="ls")
    return im.point(lambda v: 1 if v > 127 else 0)


def fix_period(im, base):
    """Silkscreen draws the full stop one pixel above the line (the bold one only 1 pixel tall), so at small sizes 28.5 reads
    as 28-5. The bake puts it on the line and, in the bold, makes it a 2 x 2 square. The OFL allows this (no Reserved
    Font Name); docs\\fonts\\README.txt says so."""
    bb = im.getbbox()
    w = bb[2] - bb[0]
    out = Image.new("L", im.size, 0)
    h = 2 if w >= 2 else 1
    for y in range(base - h, base):
        for x in range(bb[0], bb[2]):
            out.putpixel((x, y), 1)
    return out


FOUR = ["##.##", "##.##", "#####", "...##", "...##"]


def fix_four(im, base, cap):
    """Silkscreen Bold's 4 has no gap between its two strokes at this size, so 40 reads as 90. If its top row is solid,
    the bake draws it with a gap (5 wide, like the other bold digits)."""
    bb = im.getbbox()
    top = [im.getpixel((x, bb[1])) for x in range(bb[0], bb[2])]
    while top and top[-1] == 0:
        top.pop()
    if 0 in top or cap != len(FOUR):
        return im
    out = Image.new("L", im.size, 0)
    for j, row in enumerate(FOUR):
        for i, ch in enumerate(row):
            if ch == "#":
                out.putpixel((bb[0] + i, base - cap + j), 1)
    return out


def main():
    args = sys.argv[1:]
    size, preview = 8.0, None
    if "--size" in args:
        i = args.index("--size"); size = float(args[i + 1]); del args[i:i + 2]
    if "--preview" in args:
        i = args.index("--preview"); preview = args[i + 1]; del args[i:i + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    src, out = args[0], Path(args[1])
    font = ImageFont.truetype(src, size)
    asc, desc = font.getmetrics()
    rows, base = asc + desc, asc
    cap = base - glyph(font, "H", rows, base).getbbox()[1]
    cells, x = [], 0
    for c in range(FIRST, LAST + 1):
        ch = chr(c)
        im = glyph(font, ch, rows, base)
        if ch == "." and im.getbbox():          # see fix_period
            im = fix_period(im, base)
        if ch == "4" and im.getbbox():          # see fix_four
            im = fix_four(im, base, cap)
        bb = im.getbbox()
        adv = int(round(font.getlength(ch)))
        if bb:
            ink = im.crop((bb[0], 0, bb[2], rows)); xoff = bb[0] - PAD
        else:
            ink = Image.new("L", (0, rows), 0); xoff = 0
        cells.append((x, ink, xoff, adv)); x += ink.width
    atlas = Image.new("L", (max(1, x), rows), 0)
    for ax, ink, _, _ in cells:
        if ink.width:
            atlas.paste(ink, (ax, 0))
    blob = bytearray(b"BPF1") + struct.pack("<BBBBB3xH", rows, base, cap, FIRST, LAST - FIRST + 1, atlas.width)
    for ax, ink, xoff, adv in cells:
        blob += struct.pack("<HBbB", ax, ink.width, xoff, adv)
    blob += atlas.tobytes()
    out.write_bytes(blob)
    print(f"{out}: {len(blob)} bytes, rows {rows}, baseline {base}, cap {cap}, atlas {atlas.width}x{rows}")
    if preview:          # a 3x picture of some of the window's words, to check the bake by eye
        text = ["Balderding's Ballin Base Kit", "STYLE  badlands infestation 28.5 60x35.5 170x109",
                "0123456789  5S 8B 2Z  Show 15 new choices", "abcdefghijklmnopqrstuvwxyz ?!.,:;()-+/'\"_<>"]
        pv = Image.new("L", (440, rows * len(text) + 4), 0)
        for r, line in enumerate(text):
            px = 2
            for ch in line:
                ax, ink, xoff, adv = cells[ord(ch) - FIRST]
                if ink.width:
                    pv.paste(ink.point(lambda v: v * 255), (px + xoff, 2 + r * rows))
                px += adv
        pv.resize((pv.width * 3, pv.height * 3), Image.NEAREST).save(preview)


if __name__ == "__main__":
    main()
