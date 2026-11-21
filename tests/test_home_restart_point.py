#!/usr/bin/env python3
"""Home Campaign continues use the arcade's own mid-area restart point.

Source CPU1 $A592 restarts at area-header word $7C once DP $CF records that the
restart window was reached, otherwise at entry word $7A. The native world
tables carry those as rtWorldStages[].entrances[1] and [0]. Area 7 (story 2
area 2) has $7C == $7A in the arcade, so its only restart is the entrance.

Cases place the camera near authored terrain via the entry table, walk right
through production code, then seed an exhausted life so the production Home
auto-continue rebuilds the scene. Not a full playthrough.
"""
import argparse, ctypes as C, hashlib, json, struct
from pathlib import Path
from test_soft_reset import Machine
from stage_entry_probe import enter_campaign_stage
p = argparse.ArgumentParser(description=__doc__)
for n in ('rom', 'symbols', 'core', 'output'): p.add_argument('--' + n, type=Path, required=True)
p.add_argument('--case', choices=('reached', 'not-reached', 'area7'), required=True)
a = p.parse_args(); m = Machine(a)
syms = {r[2].split('.lto_priv')[0]: int(r[0], 16) for l in a.symbols.read_text().splitlines() if len(r := l.split()) == 3}
class Map(C.Structure): _fields_ = [(n, C.c_void_p) for n in ('base', 'read8', 'read16', 'write8', 'write16')]
maps = (Map * 256).in_dll(m.core, 'm68k')
def rom(at, n): return bytes(C.c_ubyte.from_address(maps[(at+i) >> 16].base + (((at+i) & 65535) ^ 1)).value for i in range(n))
def write(at, data):
    for i, v in enumerate(data): C.c_ubyte.from_address(maps[(at+i) >> 16].base + (((at+i) & 65535) ^ 1)).value = v
def entrances(stage):
    return struct.unpack('>hhhh', rom(syms['rtWorldStages'] + stage*24 + 12, 8))
stage = 6 if a.case == 'area7' else 0
original = entrances(stage)
ex, ey, cx, cy = original
m.start('campaign_1p')
start_x = (cx - 160) if a.case == 'reached' else ex
if a.case == 'area7': start_x = ex
write(syms['rtWorldStages'] + stage*24 + 12, struct.pack('>hh', start_x, ey))
enter_campaign_stage(m, a.rom, a.symbols, stage)
write(syms['rtWorldStages'] + stage*24 + 12, struct.pack('>hh', ex, ey))  # restore authored entry
def protect():
    m.patch('scene', b'\xff\xff', 104); m.patch('scene', b'\x40', 104+7)
walk = 420 if a.case == 'reached' else 60
m.buttons = {(0, 7)}
for _ in range(walk): protect(); m.run(1)
m.buttons = set(); protect(); m.run(2)
credits = m.number('scene', 2, 124)
m.patch('scene', b'\0\0', 104); m.patch('scene', b'\0', 104+7); m.patch('scene', b'\1', 104+9)
for _ in range(240):
    m.run(1)
    if m.number('rtAppState', 2) == 1 and m.number('scene', 1, 104+7): break
assert m.number('rtAppState', 2) == 1, 'Continue did not resume play'
assert m.number('scene', 2, 124) == credits - 1, ('credit not spent', credits)
x = struct.unpack('>i', m.read('scene', 4, 20))[0]
expected = {'reached': cx, 'not-reached': ex, 'area7': ex}[a.case]
assert abs(x - (expected*16 + 0x600)) <= 2*16, ('restart x', x/16, expected)
r = dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(), case=a.case, stage=stage,
         authored_entrances=original, restart_world_x=x//16 - 0x60, expected_window_x=expected,
         area7_checkpoint_equals_entry=entrances(6)[0:2] == entrances(6)[2:4],
         limits='Seeds camera entry and lethal state; walks via production input only.')
a.output.write_text(json.dumps(r, indent=2) + '\n'); print(json.dumps(r))
m.core.retro_unload_game(); m.core.retro_deinit()
