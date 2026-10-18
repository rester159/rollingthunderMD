#!/usr/bin/env python3
"""Give only Leila's native hair pixels a violet ramp in every stage.

The four CRAM lines are shared with terrain and enemies. Ordered pixel pairs
blend the existing pink/blue pens into violet without changing shared colors.
Skin, clothing, outlines, transparency and all sprite geometry stay exact.
Run once against the original interned agent arrays; repeat runs are no-ops.
"""
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]


def recolor(raw, palette):
    bright, shade, blue, dark = [palette.index(c) for c in (0xa6e, 0x426, 0xe00, 0x600)]
    result = bytearray(raw)
    changed = 0
    for i, byte in enumerate(raw):
        pens = [byte >> 4, byte & 15]
        for x in range(2):
            if ((i // 4) + x) & 1:
                if pens[x] == bright:
                    pens[x] = blue
                    changed += 1
                elif pens[x] == shade:
                    pens[x] = dark
                    changed += 1
        result[i] = pens[0] << 4 | pens[1]
    return bytes(result), changed


def main():
    report = []
    for row in json.loads((ROOT / 'assets/stages/manifest.json').read_text()):
        folder = ROOT / row['folder']
        prefix = 'area%d%d' % divmod(row['stage'], 5)
        target = folder / (prefix + '_agent_poses.c')
        text = target.read_text()
        if 'Leila violet hair' in text:
            continue
        display = (folder / (prefix + '_display_data.c')).read_text()
        colors = [int(v, 16) for v in re.findall(r'0x[\da-f]+',
                  re.search(r'palettes\[\]=\{(.*?)\};', display, re.S)[1])]
        palette = colors[(colors.index(0xa6e)//16)*16:][:16]
        definitions = {}
        for source in row['sources']:
            for match in re.finditer(r'const u32 (rtSharedPose_\w+)\[\d*\]=\{(.*?)\};',
                                     (folder / source).read_text(), re.S):
                words = [int(v, 16) for v in re.findall(r'0x[\da-f]+', match[2])]
                definitions[match[1]] = struct.pack('>%dI' % len(words), *words)
        match = re.search(r'(const RT_SpritePose \w+\[4\]\[\w+\]=\{)(.*?)(\n\};)', text, re.S)
        variants = match[2].split('\n },')
        assert len(variants) == 5
        new = {}
        changed = 0
        for variant in (1, 2):
            def replace(m):
                nonlocal changed
                name = m[0]
                if name not in new:
                    raw, count = recolor(definitions[name], palette)
                    label = 'rtSharedPose_' + hashlib.sha256(raw).hexdigest()
                    new[name] = (label, raw)
                    changed += count
                return new[name][0]
            variants[variant] = re.sub(r'rtSharedPose_\w+', replace, variants[variant])
        definitions_text = []
        shared = folder / 'shared_pose_patterns.c'
        shared_text = shared.read_text()
        declarations = []
        for label, raw in dict(new.values()).items():
            words = struct.unpack('>%dI' % (len(raw)//4), raw)
            body = ','.join('0x%08xUL' % v for v in words)
            if label not in shared_text:
                definitions_text.append('const u32 '+label+'[]={'+body+'};\n')
            declarations.append('extern const u32 '+label+'[];\n')
        shared.write_text(shared_text + ''.join(definitions_text))
        replacement = '/* Leila violet hair uses palette-safe ordered pixel pairs. */\n' + ''.join(declarations) + match[1] + '\n },'.join(variants) + match[3]
        target.write_text(text[:match.start()] + replacement + text[match.end():])
        report.append(dict(stage=row['stage'],unique_poses=len(new),changed_hair_pixels=changed))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
