#!/usr/bin/env python3
"""Compile Albatross's cover-inspired portrait into its Genesis tile allocation.

Preserve Leila's tiles, map and palette, plus the shared palette line.
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
    # Gold hair, red shirt, warm skin, steel gun, and the navy backdrop.
    private = [(0, 0, 0), (0, 1, 2), (1, 0, 1), (3, 0, 1),
               (6, 0, 0), (7, 1, 1), (4, 2, 0), (7, 4, 0),
               (7, 6, 1), (5, 3, 1), (7, 5, 2), (7, 6, 3),
               (1, 1, 2), (2, 3, 4), (4, 4, 5), (7, 7, 7)]
    palette[16:32] = [sum(c << s for c, s in zip(rgb, (1, 5, 9))) for rgb in private]
    rgb = np.array([[((c >> s) & 7) * 255 // 7 for s in (1, 5, 9)]
                    for c in palette], dtype=np.int32).reshape(3, 16, 3)
    image = Image.open(ROOT / 'assets/ui/albatross/portrait-source.png').convert('RGB')
    scale = max(112 / image.width, 176 / image.height)
    w, h = 112 / scale, 176 / scale
    box = ((image.width-w)/2, (image.height-h)/2,
           (image.width+w)/2, (image.height+h)/2)
    pixels = np.array(image.resize((112, 176), Image.Resampling.LANCZOS, box=box), dtype=np.int32)
    for ty in range(22):
        for tx in range(14):
            tile = pixels[ty*8:ty*8+8, tx*8:tx*8+8].reshape(64, 3)
            choices = []
            for bank in (0, 1):
                distance = ((tile[:, None, :] - rgb[bank, None, 1:, :]) ** 2).sum(axis=2)
                pens = distance.argmin(axis=1) + 1
                choices.append((distance.min(axis=1).sum(), bank, pens))
            _, bank, pens = min(choices, key=lambda item: item[0])
            index = ty * 14 + tx
            maps[index] = index | bank << 13
            tiles[index*32:(index+1)*32] = bytes((pens[::2] << 4 | pens[1::2]).astype('uint8'))
    resources['portrait_tiles.bin'] = tiles.hex()
    resources['portrait_map.bin'] = struct.pack('>616H', *maps).hex()
    resources['portrait_palette.bin'] = struct.pack('>48H', *palette).hex()
    path.write_text(json.dumps(resources, indent=2) + '\n')


if __name__ == '__main__':
    build()
