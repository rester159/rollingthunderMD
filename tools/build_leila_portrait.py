#!/usr/bin/env python3
"""Compile the authored Leila portrait to the existing Genesis tile allocation.

Keep Albatross's tiles, map and two shared palette lines byte-for-byte intact.
Only Leila's 308 tiles and her private palette line are replaced.
"""
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def build():
    path = ROOT / 'assets/ui/resources.json'
    resources = json.loads(path.read_text())
    tiles = bytearray.fromhex(resources['portrait_tiles.bin'])
    maps = list(struct.unpack('>616H', bytes.fromhex(resources['portrait_map.bin'])))
    palette = list(struct.unpack('>48H', bytes.fromhex(resources['portrait_palette.bin'])))
    # Three purple shades, red clothing, warm skin and blue/white eye detail.
    private = [(0, 0, 0), (0, 1, 2), (1, 0, 2), (2, 1, 4),
               (4, 2, 6), (1, 0, 1), (3, 0, 1), (6, 0, 1),
               (7, 1, 1), (4, 2, 1), (5, 3, 2), (7, 5, 3),
               (7, 6, 4), (7, 7, 7), (1, 3, 5), (4, 5, 6)]
    palette[32:] = [sum(c << s for c, s in zip(rgb, (1, 5, 9))) for rgb in private]
    rgb = np.array([[((c >> s) & 7) * 255 // 7 for s in (1, 5, 9)]
                    for c in palette], dtype=np.int32).reshape(3, 16, 3)
    image = Image.open(ROOT / 'assets/ui/leila/portrait-source.png').convert('RGB')
    scale = max(112 / image.width, 176 / image.height)
    w, h = 112 / scale, 176 / scale
    box = ((image.width-w)/2, (image.height-h)/2,
           (image.width+w)/2, (image.height+h)/2)
    pixels = np.array(image.resize((112, 176), Image.Resampling.LANCZOS, box=box), dtype=np.int32)
    for ty in range(22):
        for tx in range(14):
            tile = pixels[ty*8:ty*8+8, tx*8:tx*8+8].reshape(64, 3)
            choices = []
            for bank in (0, 2):
                distance = ((tile[:, None, :] - rgb[bank, None, 1:, :]) ** 2).sum(axis=2)
                pens = distance.argmin(axis=1) + 1
                choices.append((distance.min(axis=1).sum(), bank, pens))
            _, bank, pens = min(choices, key=lambda item: item[0])
            index = 308 + ty * 14 + tx
            maps[index] = index | bank << 13
            tiles[index*32:(index+1)*32] = bytes((pens[::2] << 4 | pens[1::2]).astype('uint8'))
    resources['portrait_tiles.bin'] = tiles.hex()
    resources['portrait_map.bin'] = struct.pack('>616H', *maps).hex()
    resources['portrait_palette.bin'] = struct.pack('>48H', *palette).hex()
    path.write_text(json.dumps(resources, indent=2) + '\n')


if __name__ == '__main__':
    build()
