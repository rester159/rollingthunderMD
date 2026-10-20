#!/usr/bin/env python3
"""Remove lipstick from both Leila costumes in all frozen stage sprite banks.

Coordinates are manually reviewed mouth pixels, relative to the actor's foot
anchor, in the 73 populated poses (the six null entries are skipped). All other
pixels, palettes, geometry and Albatross variants remain byte-for-byte intact.
Left-facing art uses the same patterns through the hardware flip attribute.
Re-running validates the edited pixels and makes no further changes. Frozen
bank hashes must be deliberately refreshed after compiling the changed assets.
"""
import ast
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
POSE = re.compile(r'\{(rtSharedPose_\w+),pieces\+(\d+),(\d+),(\d+),0\}')
TABLE = re.compile(r'(const RT_SpritePose \w+\[4\]\[\w+\]=\{)(.*?)(\n\};)', re.S)
# One bright lip pixel except the frontal poses and the machine-gun profile.
MOUTHS = {
    0: [(3, -51)], 1: [(5, -51)], 2: [(5, -50)], 3: [(5, -51)],
    4: [(5, -51)], 5: [(5, -50)], 6: [(5, -51)], 7: [(2, -52)],
    8: [(10, -39)], 9: [(6, -32)], 10: [(-1, -43)], 11: [(-1, -43)],
    14: [(3, -33)], 15: [(3, -33)], 18: [(13, -30)], 19: [(12, -32)],
    20: [(10, -39)], 21: [(2, -52)], 23: [(7, -46)], 27: [(13, -37)],
    28: [(6, -32)], 29: [(10, -39)], 32: [(-1, -49)], 33: [(-1, -51)],
    34: [(-1, -42)], 55: [(-1, -50), (0, -50)],
    58: [(-1, -50), (0, -50)], 69: [(-10, -43)], 70: [(-10, -43)],
}
# The outer lip shade in these two poses also uses the costume color ramp.
LIP_SHADOWS = {69: [(-9, -43)], 70: [(-9, -43)]}


def pixel_addresses(raw, pieces):
    result = {}
    for xs, y, _, _, tile, attrs, size, *_ in pieces:
        width, height = (size >> 2) + 1, (size & 3) + 1
        for tx in range(width):
            for ty in range(height):
                for py in range(8):
                    for px in range(8):
                        address = (tile + tx * height + ty) * 32 + py * 4 + px // 2
                        shift = 4 if px % 2 == 0 else 0
                        pen = (raw[address] >> shift) & 15
                        xoff, yoff = tx * 8 + px, ty * 8 + py
                        if attrs[0] & 0x800:
                            xoff = width * 8 - 1 - xoff
                        if attrs[0] & 0x1000:
                            yoff = height * 8 - 1 - yoff
                        if pen:
                            result[xs[0] + xoff, y + yoff] = (address, shift)
    return result


def main():
    report = []
    for row in json.loads((ROOT / 'assets/stages/manifest.json').read_text()):
        folder = ROOT / row['folder']
        prefix = 'area%d%d' % divmod(row['stage'], 5)
        target = folder / (prefix + '_agent_poses.c')
        text = target.read_text()
        display = (folder / (prefix + '_display_data.c')).read_text()
        colors = [int(v, 16) for v in re.findall(r'0x[\da-f]+',
                  re.search(r'palettes\[\]=\{(.*?)\};', display, re.S)[1])]
        palette = colors[(colors.index(0xa6e) // 16) * 16:][:16]
        piece_text = re.search(r'pieces\[\]=\{(.*?)\n\};', text, re.S)[1]
        pieces = ast.literal_eval('[' + piece_text.replace('{', '[').replace('}', ']') + ']')
        definitions = {}
        for source in row['sources']:
            for m in re.finditer(r'const u32 (rtSharedPose_\w+)\[\d*\]=\{(.*?)\};',
                                 (folder / source).read_text(), re.S):
                raw = bytes.fromhex(''.join(v.zfill(8) for v in re.findall(r'0x([\da-f]+)', m[2])))
                assert hashlib.sha256(raw).hexdigest() == m[1][13:]
                definitions[m[1]] = raw
        table = TABLE.search(text)
        variants = table[2].split('\n },')
        assert len(variants) == 5
        additions, replacements = {}, {}
        changed_pixels = 0
        for variant in (1, 2):
            poses = list(POSE.finditer(variants[variant]))
            assert len(poses) == 73
            for index, match in enumerate(poses):
                name, offset, tiles, count = match.groups()
                before = definitions[name]
                assert len(before) == int(tiles) * 32
                raw = bytearray(before)
                addresses = pixel_addresses(raw, pieces[int(offset):int(offset) + int(count)])
                edits = [(xy, palette.index(0xe if variant == 1 else 0xe00), palette.index(0x68a)) for xy in MOUTHS.get(index, [])]
                edits += [(xy, palette.index(0x8 if variant == 1 else 0x800), palette.index(0x468)) for xy in LIP_SHADOWS.get(index, [])]
                allowed = set()
                for xy, old, new in edits:
                    address, shift = addresses[xy]
                    pen = (raw[address] >> shift) & 15
                    assert pen in (old, new), (row['stage'], variant, index, xy, pen)
                    raw[address] = (raw[address] & ~(15 << shift)) | (new << shift)
                    changed_pixels += pen != new
                    allowed.add((address, shift))
                # Explicitly ensure no clothing/hair/transparent pixel was edited.
                for address, (a, b) in enumerate(zip(before, raw)):
                    for shift in (0, 4):
                        if ((a ^ b) >> shift) & 15:
                            assert (address, shift) in allowed
                label = 'rtSharedPose_' + hashlib.sha256(raw).hexdigest()
                if name in replacements:
                    assert replacements[name] == label
                replacements[name] = label
                if label not in definitions:
                    additions[label] = bytes(raw)
            variants[variant] = re.sub(r'rtSharedPose_\w+', lambda m: replacements[m[0]], variants[variant])
        if changed_pixels:
            shared = folder / 'shared_pose_patterns.c'
            with shared.open('a') as stream:
                for label, raw in additions.items():
                    words = ','.join('0x' + raw[i:i+4].hex() + 'UL' for i in range(0, len(raw), 4))
                    stream.write('const u32 ' + label + '[]={' + words + '};\n')
            declarations = ''.join('extern const u32 ' + label + '[];\n' for label in additions)
            edited = text[:table.start()] + declarations + table[1] + '\n },'.join(variants) + table[3] + text[table.end():]
            # Only the Leila table references and new externs may differ here.
            old_variants = table[2].split('\n },')
            assert variants[0] == old_variants[0] and variants[3:] == old_variants[3:]
            target.write_text(edited)
        report.append(dict(stage=row['stage'], poses_checked=146, changed_pixels=changed_pixels,
                           new_patterns=len(additions)))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
