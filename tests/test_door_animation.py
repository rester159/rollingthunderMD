#!/usr/bin/env python3
"""Exercise arcade-derived doors through the real opening/closing renderer.

Only RAM door kind/open-request fields are seeded at the first visible doorway;
state stepping, background patches, sprite selection, DMA and SAT are production
code. Compare published animation frames against owner-supplied arcade descriptors,
then require open/closed endpoints. This is a diagnostic fixture, not a playthrough.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from test_soft_reset import Machine
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from build_door_art import load_arcade, compile_doors, DOOR_DESCRIPTORS


class Map(C.Structure):
    _fields_ = [(name, C.c_void_p) for name in ('base', 'read8', 'read16', 'write8', 'write16')]


def check(a):
    arcade = load_arcade(a.arcade_rom)
    m = Machine(a)
    m.start(a.case)
    maps = (Map*256).in_dll(m.core, 'm68k')
    def read_cpu(at, count):
        return bytes(C.c_ubyte.from_address(maps[((at+i)>>16)&255].base+(((at+i)&65535)^1)).value for i in range(count))
    def vram():
        raw = bytes((C.c_ubyte*65536).in_dll(m.core, 'vram'))
        return b''.join(raw[i:i+2][::-1] for i in range(0, len(raw), 2))
    # Pinned m68000 ABI: scene.camera=8, scene.objects.objects=224, Anchor=16.
    slots = []
    for i in range(48):
        record = struct.unpack('>ii8B', m.read('scene', 16, 224+i*16))
        if record[2] < 3 and record[8]: slots.append(i)
    assert slots, 'No visible authored door at the opening'
    slot = slots[0]; offset = 224+slot*16
    assert struct.unpack('>ii', m.read('scene', 8, offset)) == (128*16, 161*16), 'Scene ABI mismatch'
    m.patch('scene', bytes([a.kind, 32, 0, 0]), offset+8)
    # The live display descriptor supplies the current stage's unchanged palette.
    stage = m.number('view', 4)
    palette_address = struct.unpack('>I', read_cpu(stage+12, 4))[0]
    palette = struct.unpack('>64H', read_cpu(palette_address, 128))
    expected = compile_doors(arcade, palette)
    seen = set(); endpoints = set(); checks = 0; sequence = []
    for frame in range(64):
        if frame < 28:
            m.patch('scene', bytes([m.number('scene', 1, offset+9) | 1]), offset+9)
        m.run(1)
        kind, flags, phase, timer, layer, delay, visible, pose_index = m.read('scene', 8, offset+8)
        direction = 'open' if frame < 28 else 'close'
        if phase in (0, 4): endpoints.add(phase)
        # Observe published SAT/VRAM, not heldGroups: the next frame can already
        # be prepared in RAM while its queued pixels are still awaiting VBlank.
        actual = vram(); commands = []; index = 0
        for _ in range(80):
            y, size, link, attr, x = struct.unpack_from('>HBBHH', actual, 0xf400+index*8)
            commands.append((y, size, attr, x))
            if not link: break
            index = link
        else: raise AssertionError('SAT loop')
        world_x, world_y = struct.unpack('>ii', m.read('scene', 8, offset))
        cam_x, cam_y = struct.unpack('>hh', m.read('scene', 4, 8))
        local_x, local_y = world_x//16-cam_x, world_y//16-cam_y
        selected = []
        for _, geometry in expected[a.kind*3]:
            x, y, cx, cy, attr, size, priority = geometry
            screen_y, screen_x = ((local_y+y-16)&255)+112, ((local_x+x+16)&511)+128
            hits = [command for command in commands if (command[0], command[1], command[3]) == (screen_y, size, screen_x)]
            assert len(hits) <= 1, ('duplicate door component', frame)
            selected.extend(hits)
        assert len(selected) in (0, 2), ('partial door', frame, selected)
        rendered = 0
        if selected:
            matches = []
            for candidate in (1, 2, 3):
                reference = expected[a.kind*3+candidate-1]
                good = True
                for command, (pixels, geometry) in zip(selected, reference):
                    tile = command[2] & 2047
                    attr = geometry[4] | (0x8000 if (geometry[6] | layer) >= 3 else 0)
                    good &= (command[2] & ~2047) == attr and actual[tile*32:tile*32+len(pixels)] == pixels
                if good: matches.append(candidate)
            assert len(matches) == 1, (a.case, a.kind, frame, 'published door differs from every arcade pose', matches)
            rendered = matches[0]; seen.add((direction, rendered)); checks += 1
        if not sequence or sequence[-1] != rendered: sequence.append(rendered)
    assert seen == {(direction, phase) for direction in ('open', 'close') for phase in (1, 2, 3)}, seen
    assert endpoints == {0, 4}, endpoints
    assert sequence == [0, 1, 2, 3, 0, 3, 2, 1, 0], sequence
    assert m.number('rtAppFault', 2) == 0
    if 'rtPackedBufferFailures' in m.symbols: assert m.number('rtPackedBufferFailures', 2) == 0
    m.core.retro_unload_game(); m.core.retro_deinit()
    return dict(case=a.case, kind=a.kind, checks=checks, published_sequence=sequence, arcade_descriptors=[hex(n) for n in DOOR_DESCRIPTORS[a.kind]], opening_and_closing=True, exact_door_vram_and_sat=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--symbols', type=Path, required=True)
    p.add_argument('--arcade-rom', type=Path, required=True)
    p.add_argument('--core', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--case', choices=['arcade', 'campaign_1p', 'campaign_2p'])
    p.add_argument('--kind', type=int, choices=range(3))
    a = p.parse_args()
    if a.case:
        assert a.kind is not None
        print(json.dumps(check(a))); return
    rows = []
    for case in ('arcade', 'campaign_1p', 'campaign_2p'):
        for kind in range(3):
            command = [sys.executable, str(Path(__file__).resolve()), '--rom', str(a.rom), '--symbols', str(a.symbols),
                       '--arcade-rom', str(a.arcade_rom), '--core', str(a.core), '--output', str(a.output), '--case', case, '--kind', str(kind)]
            result = subprocess.run(command, text=True, capture_output=True, timeout=60)
            assert result.returncode == 0, result.stdout+result.stderr
            rows.append(next(json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')))
            print(case, kind, 'PASS', flush=True)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(), cases=rows,
        scope='RAM-seeded door kinds/open requests at opening doorway; real state/renderer/DMA with arcade-derived pose pixels and SAT; not a full playthrough.'), indent=2)+'\n')


if __name__ == '__main__':
    main()
