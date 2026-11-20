"""tools/make_icon.py [font] [out.ico]: (default: Impact, gui/bbb_kit.ico). 256 to 48 px: the badge (gui/icon_badge.png);
32, 24, 16 px: the BBB disc below, drawn in code so it stays readable that small. the exe's icon, drawn in code, in the 1950s poster look of the kit's title card: a red disc
with a soft halftone and sparkle stars, a checkered ring and a cream rim round it, and "BBB" in front, the middle B big and
the outer two small: yellow letters with a thick dark outline and a dark drop shadow. No teal; no red letters.
Drawn at 4x and reduced; writes the .ico (256 ... 16) and a preview sheet."""
from PIL import Image, ImageDraw, ImageFont
import math, sys

RED, REDD, CREAM, YELLOW, INK = (214, 64, 46), (178, 48, 36), (244, 230, 200), (250, 206, 58), (28, 24, 26)
FONT = sys.argv[1] if len(sys.argv) > 1 else "C:/Windows/Fonts/impact.ttf"          # a heavy condensed sign face, like the card's


def letter(ch, h, font):
    """one letter about h px tall: yellow face, thick dark outline, dark shadow down-right"""
    f = ImageFont.truetype(font, int(h * 1.22)); ow = max(2, int(h * 0.085)); sh = max(1, int(h * 0.08))
    b = f.getbbox(ch, stroke_width=ow); w0, h0 = b[2] - b[0] + sh + 4, b[3] - b[1] + sh + 4
    im = Image.new("RGBA", (w0, h0), (0, 0, 0, 0)); d = ImageDraw.Draw(im); x, y = -b[0] + 2, -b[1] + 2
    d.text((x + sh, y + sh), ch, font=f, fill=INK, stroke_width=ow, stroke_fill=INK)
    d.text((x, y), ch, font=f, fill=YELLOW, stroke_width=ow, stroke_fill=INK)
    return im


def star(d, x, y, r, c):
    """a four-point sparkle"""
    t = r * 0.18
    d.polygon([(x, y - r), (x + t, y - t), (x + r, y), (x + t, y + t), (x, y + r), (x - t, y + t), (x - r, y), (x - t, y - t)], fill=c)


def draw(S):
    k = 4; W = S * k; im = Image.new("RGBA", (W, W), (0, 0, 0, 0)); d = ImageDraw.Draw(im); c = W / 2
    r = W * 0.48; lw = max(2, int(W * 0.018))
    d.ellipse((c - r, c - r, c + r, c + r), fill=INK)                                          # the outer edge
    rc = r - lw; d.ellipse((c - rc, c - rc, c + rc, c + rc), fill=CREAM)                       # the cream rim
    if S >= 32:          # the checkered ring
        ro, ri, n = rc - W * 0.025, rc - W * 0.085, 28 if S >= 64 else 18
        for i in range(n):
            a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
            for j, (q0, q1) in enumerate(((ri, (ri + ro) / 2), ((ri + ro) / 2, ro))):
                if (i + j) % 2: continue
                pts = [(c + q0 * math.cos(a0), c + q0 * math.sin(a0)), (c + q1 * math.cos(a0), c + q1 * math.sin(a0)),
                       (c + q1 * math.cos(a1), c + q1 * math.sin(a1)), (c + q0 * math.cos(a1), c + q0 * math.sin(a1))]
                d.polygon(pts, fill=INK)
        rd = ri - W * 0.012
    else: rd = rc - W * 0.05
    d.ellipse((c - rd - lw, c - rd - lw, c + rd + lw, c + rd + lw), fill=INK)
    d.ellipse((c - rd, c - rd, c + rd, c + rd), fill=RED)                                      # the red disc
    if S >= 64:          # a soft halftone, heavier toward the lower right
        step = W * 0.035
        y = c - rd
        while y < c + rd:
            x = c - rd
            while x < c + rd:
                if (x - c) ** 2 + (y - c) ** 2 < (rd - step) ** 2:
                    t = ((x - c) + (y - c)) / (2 * rd) + 0.5; rr = step * 0.32 * max(0.0, t - 0.25)
                    if rr > 0.6: d.ellipse((x - rr, y - rr, x + rr, y + rr), fill=REDD)
                x += step
            y += step
    if S >= 48:
        for (sx, sy, sr, col) in ((0.3, 0.27, 0.06, CREAM), (0.73, 0.3, 0.045, YELLOW), (0.7, 0.74, 0.05, CREAM), (0.27, 0.72, 0.035, YELLOW)):
            star(d, sx * W, sy * W, sr * W, col)
    big, small = letter("B", W * 0.5, FONT), letter("B", W * 0.29, FONT)
    yb, ys = int(c - big.size[1] / 2), int(c - small.size[1] / 2 + W * 0.04)
    xl = int(c - big.size[0] / 2 - small.size[0] + W * 0.02); xr = int(c + big.size[0] / 2 - W * 0.02)
    im.alpha_composite(small, (max(0, xl), ys)); im.alpha_composite(small, (min(W - small.size[0], xr), ys))
    im.alpha_composite(big, (int(c - big.size[0] / 2), yb))                                    # the big one in front
    return im.resize((S, S), Image.LANCZOS)


def badge(S, path):
    """the badge picture (gui/icon_badge.png, 1950s poster art) cut to its circle, the dark corners transparent, at S x S"""
    im = Image.open(path).convert("RGBA"); W = im.size[0]; k = 4
    m = Image.new("L", (W * k, W * k), 0); ImageDraw.Draw(m).ellipse((W * k / 2 - 0.493 * W * k, W * k / 2 - 0.493 * W * k, W * k / 2 + 0.493 * W * k, W * k / 2 + 0.493 * W * k), fill=255)
    im.putalpha(m.resize((W, W), Image.LANCZOS))
    return im.resize((S, S), Image.LANCZOS)


if __name__ == "__main__":
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "gui", "bbb_kit.ico")
    sizes = [256, 128, 64, 48, 32, 24, 16]
    bp = os.path.join(here, "..", "gui", "icon_badge.png")
    ims = {s: (badge(s, bp) if s >= 48 and os.path.exists(bp) else draw(s)) for s in sizes}          # the badge where it can be seen, BBB where it is small
    if out: ims[256].save(out, format="ICO", sizes=[(s, s) for s in sizes], append_images=[ims[s] for s in sizes[1:]])
    sheet = Image.new("RGB", (256 + 128 + 64 + 48 * 3 + 32 * 3 + 24 * 3 + 16 * 3 + 90, 270), (32, 34, 40)); x = 10
    for s in sizes:
        im = ims[s] if s >= 64 else ims[s].resize((s * 3, s * 3), Image.NEAREST)
        sheet.paste(im, (x, 10), im); x += im.size[0] + 10
    tag = FONT.replace("\\", "/").split("/")[-1].split(".")[0]
    sheet.save(os.path.join(here, "..", "work", f"icon_preview_{tag}.png") if os.path.isdir(os.path.join(here, "..", "work")) else f"icon_preview_{tag}.png"); print("ok", tag)
