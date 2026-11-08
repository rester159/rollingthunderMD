#!/usr/bin/env python3
"""Make cleared open-door cells opaque black without changing scenery.

The arcade's cleared doorway is black. The converted all-zero tile instead
exposes the Genesis backdrop (green). Reuse an opaque black tile where possible;
otherwise append one tile. Closed doors, palettes and both world maps stay intact.
Recompile and review the ten stage-bank hashes after running this tool.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def fix(path):
    text = path.read_text()
    def words(name):
        return [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)',
            re.search(r'\b'+name+r'\[\]=\{(.*?)\n\};', text, re.S)[1])]
    palette, patterns = words('palettes'), words('patterns')
    assert palette[1] == 0 and len(patterns) % 8 == 0
    tiles = [patterns[i:i+8] for i in range(0, len(patterns), 8)]
    black = [0x11111111]*8
    added = black not in tiles
    tile = len(tiles) if added else tiles.index(black)
    if added:
        match = re.search(r'\bpatterns\[\]=\{(.*?)\n\};', text, re.S)
        text = text[:match.end(1)]+'\n    '+','.join(['0x11111111UL']*8)+','+text[match.end(1):]
        # The final display descriptor owns the pattern count.
        text, count = re.subn(r'(,\d+,\d+,)'+str(len(tiles))+r'(,\{)',
                             lambda m: m[1]+str(len(tiles)+1)+m[2], text)
        assert count == 1
    for kind in range(3):
        name = 'door'+str(kind)+'_1'
        values = words(name)
        assert len(values) == 32
        # Cave stages also have authored textured interiors. Keep them.
        values = [v if v else tile for v in values]
        text, count = re.subn(r'(\b'+name+r'\[\]=\{).*?(\n\};)',
            lambda m: m[1]+'\n    '+','.join(f'0x{v:04x}' for v in values)+','+m[2], text, flags=re.S)
        assert count == 1
    path.write_text(text)
    return dict(stage=path.parent.name, black_tile=tile, tile_added=added)


if __name__ == '__main__':
    print(json.dumps([fix(p) for p in sorted((ROOT/'assets/stages').glob('*/*display_data.c'))], indent=2))
