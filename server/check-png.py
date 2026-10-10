#!/usr/bin/env python3
"""check-png.py — is a smaller copy of a picture one the game can read, showing the same?

  check-png.py <original.png> <copy.png>

Says "ok" and exits 0, or says what is wrong with the copy and exits 1. The game reads PNGs
itself (src/core/Png.cpp): only 8 bits a channel, not interlaced, and a transparency chunk
only with a palette (on any other kind it is not read, and what was see-through would show).
Its colours must be the original's, but where nothing shows: the game multiplies each colour
by its alpha as it reads, so under alpha 0 the colour does not count.
"""
import struct
import sys

import numpy as np
from PIL import Image


def unreadable(path):
    """Why the game could not read the picture as it is, or None."""
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        return 'not a PNG'
    at, header, chunks = 8, None, set()
    while at + 8 <= len(data):
        length, = struct.unpack('>I', data[at:at + 4])
        kind = data[at + 4:at + 8]
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', data[at + 8:at + 21])
        chunks.add(kind)
        at += 12 + length
    if header is None:
        return 'no header'
    _, _, depth, colour, _, _, interlace = header
    if depth != 8:
        return f'{depth} bits a channel'
    if interlace:
        return 'interlaced'
    if colour not in (0, 2, 3, 4, 6):
        return f'colour type {colour}'
    if b'tRNS' in chunks and colour != 3:
        return 'a transparency chunk without a palette'
    return None


def shown(path):
    """The picture as the game takes it: alpha, and colour multiplied by it."""
    rgba = np.asarray(Image.open(path).convert('RGBA')).astype(np.uint32)
    return rgba[..., 3], rgba[..., :3] * rgba[..., 3:]


def main():
    original, copy = sys.argv[1], sys.argv[2]
    why = unreadable(copy)
    if why is None:
        (alpha, colour), (alpha2, colour2) = shown(original), shown(copy)
        if alpha.shape != alpha2.shape or not (np.array_equal(alpha, alpha2) and np.array_equal(colour, colour2)):
            why = 'not the same picture'
    print(why or 'ok')
    return 1 if why else 0


if __name__ == '__main__':
    sys.exit(main())
