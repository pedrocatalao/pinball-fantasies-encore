#!/usr/bin/env python3
"""Installs high-resolution replacements for the intro's pictures.

    hd_import.py [--out <dir>] [--scale <n>] <name>=<picture.png> ...

<name> is one of slide1 ... slide5 (the opening slideshow: 21st Century, Digital Illusions,
Frontline Design, "Presents...", the Pinball Fantasies title), left (the menu's side panel),
table1 ... table4 (the menu's banners, F1 to F4), hiscores (the high-score pages' heading),
or playfield1_on ... playfield4_on and playfield1_off ... playfield4_off (a table's playfield
with every lamp lit, and with every lamp off; the two must line up exactly).
Each picture must show the whole original picture; bands of white or transparency around it
are trimmed, and transparency is flattened onto black. The picture is then resized to <n>
times the original's size (3 by default), or kept at its own size along a side where it is
smaller than that. The results go to assets/hd/<name>.png in this repository, which is
published to the server for every game to fetch once it reaches main (server/README.md); to
see them before that, run the game with --hd-dir assets/hd.

Needs Pillow, from the tools virtual environment, which the script switches to by itself.
"""
import os
import sys
from pathlib import Path

try:
    from PIL import Image
except ModuleNotFoundError:
    # Not running inside the tools virtual environment: restart with its Python.
    venv = Path(__file__).resolve().parent / ".venv"
    venv_python = venv / "bin" / "python"
    if venv_python.exists() and Path(sys.prefix).resolve() != venv.resolve():
        os.execv(str(venv_python), [str(venv_python), *sys.argv])
    sys.exit("Pillow is missing. Set up the tools environment:\n"
             "  python3 -m venv tools/.venv && tools/.venv/bin/pip install -r tools/requirements.txt")

NAMES = ([f"slide{i}" for i in range(1, 6)] + ["left"] + [f"table{i}" for i in range(1, 5)] + ["hiscores"]
         + [f"playfield{i}_{lamps}" for lamps in ("on", "off") for i in range(1, 5)])
DEFAULT_OUT = Path(__file__).resolve().parent.parent / "assets" / "hd"

# The original pictures' sizes, as stored in INTRO.PRG.
ORIGINAL_SIZE = {**{f"slide{i}": (320, 240) for i in range(1, 5)}, "slide5": (640, 480),
                 "left": (130, 240), **{f"table{i}": (440, 95) for i in range(1, 5)},
                 "hiscores": (400, 40),
                 **{f"playfield{i}_{lamps}": (320, 576) for lamps in ("on", "off") for i in range(1, 5)}}


def is_padding(pixel):
    r, g, b, a = pixel
    return a < 16 or min(r, g, b) > 235


def trim(im):
    """Crops rows and columns at the edges that are mostly white or transparent."""
    w, h = im.size
    px = im.load()

    def band(line):
        return sum(is_padding(p) for p in line) > 0.5 * len(line)

    top, bottom, left, right = 0, h, 0, w
    while top < bottom and band([px[x, top] for x in range(w)]):
        top += 1
    while bottom > top and band([px[x, bottom - 1] for x in range(w)]):
        bottom -= 1
    while left < right and band([px[left, y] for y in range(top, bottom)]):
        left += 1
    while right > left and band([px[right - 1, y] for y in range(top, bottom)]):
        right -= 1
    return im.crop((left, top, right, bottom))


def main():
    args = sys.argv[1:]
    out = DEFAULT_OUT
    scale = 3
    while len(args) >= 2 and args[0] in ("--out", "--scale"):
        if args[0] == "--out":
            out = Path(args[1]).expanduser()
        else:
            scale = int(args[1])
        args = args[2:]
    if not args:
        sys.exit(__doc__)
    out.mkdir(parents=True, exist_ok=True)
    for arg in args:
        name, sep, src = arg.partition("=")
        if not sep or name not in NAMES:
            sys.exit(f"expected <name>=<picture> with <name> one of {', '.join(NAMES)}: {arg}")
        im = Image.open(Path(src).expanduser()).convert("RGBA")
        # A transparent picture keeps its full frame (the eagle's slide is a cut-out on a
        # transparent screen); only opaque pictures have their white bands trimmed.
        if im.getextrema()[3][0] == 255:
            im = trim(im)
        flat = Image.new("RGBA", im.size, (0, 0, 0, 255))
        flat.alpha_composite(im)
        ow, oh = ORIGINAL_SIZE[name]
        size = (min(im.width, ow * scale), min(im.height, oh * scale))
        if size != im.size:
            flat = flat.resize(size, Image.LANCZOS, reducing_gap=3.0)
        dst = out / f"{name}.png"
        flat.convert("RGB").save(dst, optimize=True)
        print(f"{name}: {src} ({im.width}x{im.height}) -> {dst} ({size[0]}x{size[1]})")


if __name__ == "__main__":
    main()
