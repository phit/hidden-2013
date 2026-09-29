#!/usr/bin/env python3
"""Build the Hidden: Rebuild logo, banner and icons.

After Hidden: Source's intro logo: HIDDEN REBUILD in white with a black rim, a pale outer glow and
a drop shadow, and a KA-BAR, tip down, standing in for the I. The letters are Anton squeezed
to 80% width, an open stand-in for Haettenschweiler, converted to outlines so the SVGs need no
font. The knife is our own tracing (kabar.py).

  py branding/build.py [--launcher <hidden-2013-launcher checkout>]

Writes the masters to branding/ and copies the site and game icons into place. With --launcher it
also refreshes the launcher's gui/logo.png and gui/favicon.ico.
Needs: pip install fonttools resvg-py pillow
"""
import argparse
import io
from pathlib import Path

import resvg_py
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from PIL import Image

import kabar

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent

ANTON = TTFont(HERE / "src" / "Anton-Regular.ttf")
SQUEEZE = 0.8           # horizontal scale applied to Anton
TRACKING = -0.03        # letter spacing, in font sizes
WORD_GAP = 0.15         # between HIDDEN and REBUILD, in font sizes
KNIFE_LEN = 1.6         # pommel to tip, in cap heights
KNIFE_ABOVE = 0.12      # how far the pommel rises over the caps, in cap heights
KNIFE_CLEAR = 0.015     # gap between the guard and the H and D, in font sizes
WHITE = "#f2f0ee"

# The rim, glow and shadow, and the banner's blood splash. {…} values scale with the artwork.
DEFS = """<defs>
  <filter id="fx" x="-20%" y="-30%" width="140%" height="160%" color-interpolation-filters="sRGB">
    <feMorphology in="SourceAlpha" operator="dilate" radius="{rim}" result="thick"/>
    <feComposite in="thick" in2="SourceAlpha" operator="out" result="rimA"/>
    <feFlood flood-color="#000"/>
    <feComposite in2="rimA" operator="in" result="rim"/>
    <feGaussianBlur in="thick" stdDeviation="{glow}" result="glowA"/>
    <feFlood flood-color="#cfcfcf" flood-opacity="0.55"/>
    <feComposite in2="glowA" operator="in" result="glow"/>
    <feGaussianBlur in="SourceAlpha" stdDeviation="{sblur}" result="sb"/>
    <feOffset in="sb" dx="{sdx}" dy="{sdy}" result="so"/>
    <feFlood flood-color="#000" flood-opacity="0.95"/>
    <feComposite in2="so" operator="in" result="shadow"/>
    <feMerge><feMergeNode in="shadow"/><feMergeNode in="glow"/><feMergeNode in="rim"/><feMergeNode in="SourceGraphic"/></feMerge>
  </filter>
  <radialGradient id="blot" cx="50%" cy="50%" r="50%">
    <stop offset="0" stop-color="#fff" stop-opacity="1"/><stop offset="0.6" stop-color="#fff" stop-opacity="0.8"/>
    <stop offset="1" stop-color="#fff" stop-opacity="0"/>
  </radialGradient>
  <filter id="splash" x="0" y="0" width="100%" height="100%" color-interpolation-filters="sRGB">
    <feTurbulence type="fractalNoise" baseFrequency="0.018 0.05" numOctaves="4" seed="11" result="n"/>
    <feColorMatrix in="n" type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  1.3 0 0 0 0" result="nA"/>
    <feComposite in="SourceAlpha" in2="nA" operator="arithmetic" k1="1.6" k2="0" k3="0" k4="-0.35" result="m"/>
    <feComponentTransfer in="m" result="mt"><feFuncA type="table" tableValues="0 0 0.55 0.8 0.9 1"/></feComponentTransfer>
    <feFlood flood-color="#6e0000"/>
    <feComposite in2="mt" operator="in"/>
  </filter>
</defs>"""


def defs(scale):
    """The filters, sized for artwork set at font size 100 * scale."""
    vals = {"rim": 0.7, "glow": 3, "sblur": 2.5, "sdx": 2.5, "sdy": 3.5}
    out = DEFS
    for k, v in vals.items():
        out = out.replace("{" + k + "}", f"{v * scale:g}")
    return out


def cap_height(size):
    return ANTON["OS/2"].sCapHeight * size / ANTON["head"].unitsPerEm


def text(s, size, x, y):
    """Squeezed Anton outlines with the baseline at y. Returns (svg, width)."""
    cmap, glyphs, hmtx = ANTON.getBestCmap(), ANTON.getGlyphSet(), ANTON["hmtx"]
    k = size / ANTON["head"].unitsPerEm
    tracking = size * TRACKING / SQUEEZE
    parts, pen = [], x / SQUEEZE
    for ch in s:
        name = cmap[ord(ch)]
        sp = SVGPathPen(glyphs)
        glyphs[name].draw(TransformPen(sp, (k, 0, 0, -k, pen, y)))
        parts.append(sp.getCommands())
        pen += hmtx[name][0] * k + tracking
    width = (pen - tracking) * SQUEEZE - x
    return f'<path transform="scale({SQUEEZE} 1)" d="{" ".join(parts)}"/>', width


def knife(cx, top, length):
    """The knife, tip down, pommel centred at (cx, top)."""
    k = length / (kabar.TIP_Y - kabar.TOP_Y)
    return (f'<path transform="translate({cx:.2f} {top:.2f}) scale({k:.5f}) '
            f'translate({-kabar.CX} {-kabar.TOP_Y})" d="{kabar.path()}"/>')


def hidden(size, x, y):
    """HIDDEN, baseline at y, the knife in its own gap between H and D.
    Returns (letters, knife, width, cap, knife top, knife bottom, knife x)."""
    cap = cap_height(size)
    length = cap * KNIFE_LEN
    guard = kabar.GUARD_W * length / (kabar.TIP_Y - kabar.TOP_Y)
    clear = size * KNIFE_CLEAR
    sh, wh = text("H", size, x, y)
    cx = x + wh + clear + guard / 2
    sd, wd = text("DDEN", size, cx + guard / 2 + clear, y)
    top = y - cap - cap * KNIFE_ABOVE
    return sh + sd, knife(cx, top, length), cx + guard / 2 + clear + wd - x, cap, top, top + length, cx


def wordmark(size, x, y):
    """HIDDEN REBUILD on one line, baseline at y. Returns what hidden() does, for both words."""
    letters, kn, w1, cap, top, bottom, cx = hidden(size, x, y)
    gap = size * WORD_GAP
    sr, wr = text("REBUILD", size, x + w1 + gap, y)
    return letters + sr, kn, w1 + gap + wr, cap, top, bottom, cx


def logo_stacked(size=170):
    """HIDDEN over REBUILD, which is sized to the same width and set below the knife's tip."""
    _, _, w1, cap, top, bottom, _ = hidden(size, 0, 0)
    _, wr100 = text("REBUILD", 100, 0, 0)
    sub = 100 * w1 / wr100
    below = bottom + cap * 0.12                     # HIDDEN's baseline to the top of REBUILD
    pad = size * 0.15
    y = pad - top
    letters, kn, *_ = hidden(size, pad, y)
    sr, _ = text("REBUILD", sub, pad, y + below + cap_height(sub))
    parts = [defs(size / 100), f'<g filter="url(#fx)" fill="{WHITE}">{letters}{sr}</g>',
             f'<g filter="url(#fx)" fill="{WHITE}">{kn}</g>']
    return svg_doc(w1 + 2 * pad, y + below + cap_height(sub) + pad, "".join(parts))


def logo(w=None, h=None, size=150, bg=None, splash=False, margin=0.1):
    """The wordmark, fitted into w x h if given, else sized by `size` with a margin."""
    _, _, w100, _, t100, b100, _ = wordmark(100, 0, 0)
    if h:
        size = min(size, h * (1 - 2 * margin) / (b100 - t100) * 100)
    if w:
        size = min(size, w * 0.9 / w100 * 100)
    _, _, width, cap, top, bottom, _ = wordmark(size, 0, 0)
    pad = size * 0.15
    W = w or width + 2 * pad
    H = h or bottom - top + 2 * pad
    y = (H - (bottom - top)) / 2 - top              # centre the knife's span vertically
    letters, kn, width, cap, top, bottom, kx = wordmark(size, (W - width) / 2, y)
    parts = [defs(size / 100)]
    if bg:
        parts.append(f'<rect width="100%" height="100%" fill="{bg}"/>')
    if splash:
        parts.append(f'<ellipse cx="{kx + width * 0.1:.1f}" cy="{y - cap * 0.45:.1f}" rx="{width * 0.34:.1f}" '
                     f'ry="{cap * 1.5:.1f}" fill="url(#blot)" filter="url(#splash)"/>')
    parts.append(f'<g filter="url(#fx)" fill="{WHITE}">{letters}</g>')
    parts.append(f'<g filter="url(#fx)" fill="{WHITE}">{kn}</g>')   # in front, with its own rim
    return svg_doc(W, H, "".join(parts))


def icon(n=256, fill=0.94):
    """The knife alone, tip down, on a transparent square."""
    length = n * fill
    scale = length / (KNIFE_LEN * cap_height(100))
    scale = max(scale, (0.9 if n <= 16 else 1.1) / 0.7)   # keep the rim and glow about a pixel wide
    body = f'<g filter="url(#fx)" fill="{WHITE}">{knife(n / 2, (n - length) / 2, length)}</g>'
    return svg_doc(n, n, defs(scale) + body)


def svg_doc(w, h, body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.1f} {h:.1f}" '
            f'width="{w:.1f}" height="{h:.1f}">{body}</svg>')


def png(svg, width=None):
    data = resvg_py.svg_to_bytes(svg_string=svg, **({"width": width} if width else {}))
    return Image.open(io.BytesIO(bytes(data))).convert("RGBA")


def ico(path, sizes):
    """Each size rendered on its own, so the small ones keep their rim."""
    imgs = [png(icon(s)) for s in sorted(sizes, reverse=True)]
    imgs[0].save(path, sizes=[im.size for im in imgs], append_images=imgs[1:])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--launcher", type=Path, help="hidden-2013-launcher checkout to update")
    args = ap.parse_args()

    masters = {"logo.svg": logo(), "logo-stacked.svg": logo_stacked(), "icon.svg": icon()}
    for name, svg in masters.items():
        (HERE / name).write_text(svg + "\n")
    png(masters["logo.svg"], width=1600).save(HERE / "logo.png")
    png(masters["logo-stacked.svg"], width=1024).save(HERE / "logo-stacked.png")
    png(masters["icon.svg"]).save(HERE / "icon.png")

    # The 763x174 header of the site and launcher, rendered at 2x for high-DPI screens.
    banner = logo(763, 174, size=92, bg="#000", splash=True)
    (HERE / "banner.svg").write_text(banner + "\n")
    banner_png = png(banner, width=763 * 2).convert("RGB")
    banner_png.save(HERE / "banner.png")

    all_sizes = [16, 24, 32, 48, 64, 128, 256]
    ico(HERE / "icon.ico", all_sizes)
    ico(ROOT / "src" / "launcher_main" / "res" / "hidden2013.ico", all_sizes)

    site = ROOT / "web" / "src"
    banner_png.save(site / "img" / "logo.png")
    ico(site / "favicon.ico", [16, 32, 48])

    if args.launcher:
        gui = args.launcher / "gui"
        banner_png.save(gui / "logo.png")
        ico(gui / "favicon.ico", all_sizes)
    print("ok")


if __name__ == "__main__":
    main()
