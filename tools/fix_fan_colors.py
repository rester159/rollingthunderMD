#!/usr/bin/env python3
"""Recolor Area 1's ceiling fans with the arcade's blue sprite colors.

The arcade draws the fans with sprite color group 65 (PROM rt1-4.5v), a blue
ramp from (0,0,1) to (0,5,7) in Genesis 3-bit units. The original palette fit
put the fan on palette 1, whose closest entries are teal (0,3,3)/(0,4,4). On
the teal Area 1 wall those blades vanish, so only the darker half of each fan
pose stayed visible. The fan pixels and geometry are unchanged; only their pen
indices and sprite palette bits are remapped to the stage palette whose
existing colors best match the arcade ramp. No CRAM entry is altered.

Fans exist only in Area 1 (Story 1 and Story 2: stage events of kind 8).
Recompile and repin the affected stage-bank hashes after running this tool.
"""
import json
import re
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGES = {'0_0': 'area00', '1_0': 'area10'}
FAN_GROUP = 65


def rgb3(word):
    return ((word >> 1) & 7, (word >> 5) & 7, (word >> 9) & 7)


def arcade_ramp(rom):
    import sys
    sys.path.insert(0, str(ROOT / 'tools'))
    from extract_assets import rgb_palette
    with zipfile.ZipFile(rom) as z:
        proms = b''.join(z.read(n) for n in ['rt1-1.3r', 'rt1-2.3s', 'rt1-3.4v', 'rt1-4.5v', 'rt1-5.6u'])
    pal, look = rgb_palette(proms), proms[0xC00:0x1400]
    ramp = []
    for pen in range(15):
        c = tuple(v * 7 // 255 for v in pal[256 + look[FAN_GROUP*16 + pen]])
        if c not in ramp:
            ramp.append(c)
    return ramp  # dark to light


def words(text, pattern):
    return [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)', re.search(pattern, text, re.S)[1])]


def remap_words(values, table):
    out = []
    for w in values:
        n = 0
        for k in range(8):
            n |= table[(w >> (4*k)) & 15] << (4*k)
        out.append(n)
    return out


def fix(stage, prefix, ramp):
    folder = ROOT / 'assets/stages' / stage
    display = (folder / f'{prefix}_display_data.c').read_text()
    palette = [rgb3(w) for w in words(display, r'\bpalettes\[\]=\{(.*?)\n\};')]
    fan_path, stream_path = folder / f'{prefix}_fan_poses.c', folder / f'{prefix}_stream_stage_poses.c'
    fan, stream = fan_path.read_text(), stream_path.read_text()
    fan_words = [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)UL', fan)]
    stream_words = words(stream, r'static const u32 patterns\[\]=\{(.*?)\n\};')
    assert stream_words[9*256:] == fan_words, 'stream fan records must match the resident fan art'
    used = sorted({(w >> (4*k)) & 15 for w in fan_words for k in range(8)} - {0})
    old_palette = 1
    assert all(m == '8192' for m in re.findall(r'\{(\d+),\d+\},15,0,3,0\}', fan))
    # Arcade order: assign the ramp by the current colors' brightness.
    by_light = sorted(used, key=lambda i: (sum(palette[16*old_palette+i]), i))
    targets = {}
    picks = [round(j*(len(ramp)-1)/(len(by_light)-1)) for j in range(len(by_light))]
    for index, pick in zip(by_light, picks):
        targets[index] = ramp[pick]
    def cost(entry, target):
        # RGB distance plus a hue term: the arcade ramp is blue-dominant, and
        # a blue-to-teal shift is exactly what hid the blades on the teal wall.
        hue = ((target[2]-target[1]) - (entry[2]-entry[1]))**2
        return sum((a-b)**2 for a, b in zip(entry, target)) + 2*hue
    # Never choose the colors that make up most of the area's background
    # (Story 1: the teal wall is palette 1 pen 11); a blade drawn in them is
    # invisible against the wall, which is the reported half-fan.
    from collections import Counter
    use = Counter()
    pats = words(display, r'\bpatterns\[\]=\{(.*?)\n\};')
    for e in words(display, r'\bplane0\[\]=\{(.*?)\n\};') + words(display, r'\bplane1\[\]=\{(.*?)\n\};'):
        base = (e & 0x7ff) * 8
        for w in pats[base:base+8]:
            for k in range(8):
                pen = (w >> (4*k)) & 15
                use[palette[16*((e >> 13) & 3) + pen] if pen else palette[0]] += 1
    wall = {c for c, _ in use.most_common(3)}
    def usable(entry):
        return all(sum((a-b)**2 for a, b in zip(entry, w)) >= 3 for w in wall) or entry == (0, 0, 0)
    # Keep the arcade's shading: map the fan's pens, dark to light, onto a
    # palette's distinct blue-dominant shades (plus black). No stage palette
    # has the arcade's light sky blues, so the pure-blue ramp is the closest
    # hue that stays distinct from the wall.
    best = None
    for p in range(4):
        entries = [e for e in range(1, 16) if usable(palette[16*p+e])]
        black = next((e for e in entries if palette[16*p+e] == (0, 0, 0)), None)
        blues = sorted({palette[16*p+e]: e for e in entries
                        if palette[16*p+e][2] >= palette[16*p+e][1] + 2 and palette[16*p+e][0] <= 1}.values(),
                       key=lambda e: (sum(palette[16*p+e]), e))
        if black is None or len(blues) < 3:
            continue
        table, error = list(range(16)), 0
        lit = [i for i in by_light if targets[i] != (0, 0, 0)]
        for i in by_light:
            if targets[i] == (0, 0, 0):
                table[i] = black
        for j, i in enumerate(lit):
            table[i] = blues[round(j*(len(blues)-1)/max(1, len(lit)-1))]
        for i in used:
            error += cost(palette[16*p+table[i]], targets[i])
        key = (-len(blues), error)
        if best is None or key < best[0]:
            best = (key, p, table)
    assert best, 'no stage palette offers a blue ramp for the fan'
    _, new_palette, table = best
    error = sum(cost(palette[16*new_palette+table[i]], targets[i]) for i in used)
    new_fan = remap_words(fan_words, table)
    it = iter(f'0x{w:08x}UL' for w in new_fan)
    fan = re.sub(r'0x[0-9a-fA-F]+UL', lambda m: next(it), fan)
    attribute = str(new_palette << 13)
    fan = re.sub(r'\{8192,8192\}(,15,0,3,0\})', '{'+attribute+','+attribute+'}\\1', fan)
    fan_path.write_text(fan)
    # Stream records 9..12 (pieces 18..25) are the same fan poses.
    body = re.search(r'(static const u32 patterns\[\]=\{)(.*?)(\n\};)', stream, re.S)
    tokens = re.findall(r'0x[0-9a-fA-F]+(?:UL)?', body[2])
    assert len(tokens) == 13*256
    replaced = iter(new_fan)
    def sub_token(m, counter=[0]):
        i = counter[0]; counter[0] += 1
        if i < 9*256:
            return m[0]
        suffix = 'UL' if m[0].endswith('UL') else ''
        return f'0x{next(replaced):08x}' + suffix
    new_body = re.sub(r'0x[0-9a-fA-F]+(?:UL)?', sub_token, body[2])
    stream = stream[:body.start(2)] + new_body + stream[body.end(2):]
    pieces = re.search(r'(static const RT_SpritePiece pieces\[\]=\{\n)(.*?)(\n\};)', stream, re.S)
    lines = pieces[2].split('\n')
    assert len(lines) == 26
    for i in range(18, 26):
        assert '{8192,8192}' in lines[i]
        lines[i] = lines[i].replace('{8192,8192}', '{'+attribute+','+attribute+'}')
    stream = stream[:pieces.start(2)] + '\n'.join(lines) + stream[pieces.end(2):]
    stream_path.write_text(stream)
    return dict(stage=stage, palette=new_palette,
                pens={str(i): dict(old=palette[16*old_palette+i], arcade=targets[i],
                                   new_index=table[i], new=palette[16*new_palette+table[i]]) for i in used},
                squared_error=error)


if __name__ == '__main__':
    import sys
    ramp = arcade_ramp(Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'rthunder.zip')
    print(json.dumps(dict(arcade_ramp=ramp, stages=[fix(s, p, ramp) for s, p in STAGES.items()]), indent=1))
