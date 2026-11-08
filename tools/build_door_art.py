#!/usr/bin/env python3
"""Regenerate door art from an owner-supplied arcade set, preserving fan art.

CF80, CFAF and CFD4 are animation-phase tables, each indexed by door kind.
No CPU ROM data is emitted. This updates converted C artwork; stage bank hashes
must subsequently be reviewed and repinned with the documented cross compiler.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from zipfile import ZipFile
from extract_assets import SPRITE_ROMS, PROM_ROMS, EXPECTED_SHA1, decode_sprites, rgb_palette

ROOT = Path(__file__).resolve().parents[1]
PHASE_TABLES = (0xcf80, 0xcfaf, 0xcfd4)
# Independently traced CPU2 descriptor sequence, indexed by kind then phase.
DOOR_DESCRIPTORS = ((0x7667, 0x7672, 0x767d), (0x770c, 0x7717, 0x7722), (0x77b1, 0x77bc, 0x77c7))
PROGRAM_HASHES = {'rt3_3.12d': '8987174e364d20eeab706c3e0d4e0d3c2b96723c',
                  'rt3_2b.12c': '52e8757aacb4e01f8432125729e2323c48ebc4f5',
                  'rt3_1b.9c': 'cb92709e94eb273b3ce44c55cd252170ad1017f4'}


def load_arcade(path):
    with ZipFile(path) as archive:
        chips = {name: archive.read(name) for name in [*SPRITE_ROMS, *PROM_ROMS, *PROGRAM_HASHES]}
    for name, data in chips.items():
        assert hashlib.sha1(data).hexdigest() == {**EXPECTED_SHA1, **PROGRAM_HASHES}[name], name
    _, pixels = decode_sprites(b''.join(chips[n] for n in SPRITE_ROMS))
    proms = b''.join(chips[n] for n in PROM_ROMS)
    colors = [sum(((channel * 7 + 127) // 255) << shift for channel, shift in zip(rgb, (1, 5, 9))) for rgb in rgb_palette(proms)]
    groups = chips['rt3_1b.9c'][0xd816-0x8000:0xd819-0x8000]
    assert groups == bytes.fromhex('809496')
    descriptors = []
    for kind in range(3):
        for phase, table in enumerate(PHASE_TABLES):
            address = struct.unpack_from('>H', chips['rt3_2b.12c'], table-0x8000+kind*2)[0]
            assert address == DOOR_DESCRIPTORS[kind][phase]
            at = 0x2000+address-0x6000
            assert chips['rt3_3.12d'][at] == 2
            descriptors.append(chips['rt3_3.12d'][at+1:at+11])
    return pixels, colors, proms[0xc00:0x1400], groups, descriptors


def palette_for(colors, lookup, group, palette):
    source = [colors[256+lookup[group*16+i]] for i in range(15)]
    def distance(x, y):
        return sum((((x >> s) & 7)-((y >> s) & 7))**2 for s in (1, 5, 9))
    choices = []
    for line in range(4):
        mapping = [min(range(1, 16), key=lambda pen: (distance(c, palette[line*16+pen]), palette[line*16+pen], pen)) for c in source]+[0]
        choices.append((sum(distance(c, palette[line*16+p]) for c, p in zip(source, mapping)), line, mapping))
    _, line, mapping = min(choices)
    return line, mapping


def compile_doors(arcade, palette):
    pixels, colors, lookup, groups, descriptors = arcade
    poses = []
    for index, raw in enumerate(descriptors):
        line, mapping = palette_for(colors, lookup, groups[index//3] >> 1, palette)
        pieces = []
        for at in range(0, 10, 5):
            ax, f, ay, dx, dy = raw[at:at+5]
            width, height = (16, 8, 32, 4)[ax >> 6], (16, 8, 32, 4)[(ay >> 1) & 3]
            sx, sy = (ax & 24) & ~(width-1), (ay & 24) & ~(height-1)
            frame = (f & 127)+(ax & 7)*128
            # All nine authored doors contain two complete 32x32 components.
            assert (width, height, sx, sy) == (32, 32, 0, 0)
            blob = bytearray()
            for tx in range(4):
                for ty in range(4):
                    pens = [mapping[pixels[frame*1024+(ty*8+y)*32+tx*8+x]] for y in range(8) for x in range(8)]
                    blob.extend((pens[i] << 4) | pens[i+1] for i in range(0, 64, 2))
            signed = lambda n: n-256 if n >= 128 else n
            attr = (line << 13) | ((ax & 32) << 6) | ((ay & 1) << 12)
            pieces.append((bytes(blob), (signed(dx), -signed(dy)-height, signed(dx), signed(dy), attr, 15, ay >> 5)))
        poses.append(pieces)
    return poses


def c_piece(piece, tile):
    x, y, cx, cy, attr, size, priority = piece
    return '{{%d,%d},%d,{%d,%d},%d,%d,{%d,%d},%d,%d,3,0},' % (x, x, y, cx, cx, cy, tile, attr, attr, size, priority)


def c_words(blob):
    return ','.join('0x%08xUL' % n for n in struct.unpack('>'+str(len(blob)//4)+'I', blob))


def array_words(text, name):
    body = re.search(r'\b'+name+r'\[\]=\{(.*?)\n\};', text, re.S)[1]
    return [int(n, 16) for n in re.findall(r'0x([0-9a-fA-F]+)', body)]


def update_stage(root, row, arcade):
    folder = root/row['folder']
    door = next(folder/n for n in row['sources'] if n.endswith('_door_poses.c'))
    stream = next(folder/n for n in row['sources'] if n.endswith('_stream_stage_poses.c'))
    display = next(folder/n for n in row['sources'] if n.endswith('_display_data.c'))
    palette = array_words(display.read_text(), 'palettes')
    assert len(palette) == 64
    poses = compile_doors(arcade, palette)
    text = door.read_text(); prefix = re.search(r'const u32 (\w+)rtDoorPatterns', text)[1]
    blob = bytearray(); ids = {}; pieces = []
    for pose in poses:
        for data, geometry in pose:
            if data not in ids:
                ids[data] = len(blob)//32; blob.extend(data)
            pieces.append(c_piece(geometry, ids[data]))
    count = len(blob)//32
    output = ['/* Arcade phase tables indexed by door kind; stage clip rule3. */', '#include "'+prefix+'rt_door_poses.h"',
              'const u32 '+prefix+'rtDoorPatterns[]={', c_words(blob), '};',
              'static const RT_SpritePiece pieces[]={', *pieces, '};',
              'const RT_SpritePose '+prefix+'rtDoorPoses[3][3]={']
    for kind in range(3):
        output.append('{'+','.join('{%srtDoorPatterns,pieces+%d,%d,2,0}' % (prefix, (kind*3+phase)*2, count) for phase in range(3))+'},')
    door.write_text('\n'.join([*output, '};', '']))
    header = folder/(prefix+'rt_door_poses.h')
    header.write_text(re.sub(r'(RT_DOOR_PATTERN_COUNT )\d+', r'\g<1>'+str(count), header.read_text()))
    # The last four streamed records are fans. Retain their pixels and geometry.
    text = stream.read_text(); words = array_words(text, 'patterns')
    assert len(words) == 13*256
    old_pieces = re.search(r'static const RT_SpritePiece pieces\[\]=\{(.*?)\n\};', text, re.S)[1].strip().splitlines()
    assert len(old_pieces) == 26
    stream_blob = b''.join(data for pose in poses for data, _ in pose)+struct.pack('>1024I', *words[9*256:])
    stream_pieces = [c_piece(geometry, i*16) for pose in poses for i, (_, geometry) in enumerate(pose)]+old_pieces[18:]
    text = re.sub(r'(static const u32 patterns\[\]=\{).*?(\n\};)', lambda m: m[1]+'\n'+c_words(stream_blob)+m[2], text, flags=re.S)
    text = re.sub(r'(static const RT_SpritePiece pieces\[\]=\{).*?(\n\};)', lambda m: m[1]+'\n'+'\n'.join(stream_pieces)+m[2], text, flags=re.S)
    stream.write_text(text)
    return dict(stage=row['stage'], door_tiles=count, door_bytes=len(blob), streamed_door_bytes=9*1024, fan_art_unchanged=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--root', type=Path, default=ROOT)
    a = p.parse_args()
    arcade = load_arcade(a.rom)
    rows = json.loads((a.root/'assets/stages/manifest.json').read_text())
    reports = [update_stage(a.root, row, arcade) for row in rows]
    # Keep the legacy unbanked include aliases consistent with stages 0 and 1.
    for filename, report in zip(('rt_door_poses.h', 'area01_rt_door_poses.h'), reports):
        header = a.root/'inc'/filename
        header.write_text(re.sub(r'(RT_DOOR_PATTERN_COUNT )\d+', r'\g<1>'+str(report['door_tiles']), header.read_text()))
    print(json.dumps(reports, indent=2))


if __name__ == '__main__':
    main()
