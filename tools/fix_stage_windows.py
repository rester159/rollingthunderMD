#!/usr/bin/env python3
"""Make Area 1's background windows opaque black, as in the arcade.

The arcade's Area 1 upper-floor windows show black. In the converted maps both
display planes hold the empty tile there, so the Genesis backdrop (the stage's
teal wall color) showed through instead. Only cells that are empty on both
planes, inside the visible rows of the two Area 1 maps (Story 1 and Story 2),
are changed: plane 1 receives the stage's existing opaque black tile. No tile,
palette, door patch or plane-0 cell changes. Other stages are reported only.
Recompile and repin the affected stage-bank hashes after running this tool.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BLACK = [0x11111111] * 8


def words(text, name):
    body = re.search(r'\b' + name + r'\[\]=\{(.*?)\n\};', text, re.S)[1]
    return [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)', body)]


def fix(path):
    text = path.read_text()
    patterns, palette = words(text, 'patterns'), words(text, 'palettes')
    assert palette[1] == 0, 'palette 0 color 1 must be black'
    tiles = [patterns[i:i+8] for i in range(0, len(patterns), 8)]
    empty = {i for i, t in enumerate(tiles) if not any(t)}
    width, height = map(int, re.search(
        r'rtDisplayOpening=\{patterns,\{plane0,plane1\},palettes,(\d+),(\d+),', text).groups())
    plane0, plane1 = words(text, 'plane0'), words(text, 'plane1')
    assert len(plane0) == len(plane1) == width * height
    cells = [y * width + x for y in range(height - 1) for x in range(width)
             if (plane0[y*width+x] & 0x7ff) in empty and (plane1[y*width+x] & 0x7ff) in empty]
    if not cells:
        return dict(stage=path.parent.name, cells=0)
    assert BLACK in tiles, 'no opaque black tile to reuse'
    black = tiles.index(BLACK)
    for i in cells:
        plane1[i] = black
    # Low priority, palette 0: sprites on the walkway still draw in front.
    rows = [plane1[r:r+16] for r in range(0, len(plane1), 16)]
    body = '\n'.join('    ' + ','.join(f'0x{v:04x}' for v in row) + ',' for row in rows)
    text, count = re.subn(r'(\bplane1\[\]=\{\n).*?(\n\};)',
                          lambda m: m[1] + body + m[2], text, flags=re.S)
    assert count == 1
    path.write_text(text)
    ys = sorted({i // width for i in cells})
    return dict(stage=path.parent.name, black_tile=black, cells=len(cells), rows=[ys[0], ys[-1]])


if __name__ == '__main__':
    paths = sorted((ROOT / 'assets/stages').glob('[01]_0/*display_data.c'))
    print(json.dumps([fix(p) for p in paths], indent=2))
