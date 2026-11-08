#!/usr/bin/env python3
"""v0.91 menu regressions: rendered slot digits, cadence and sound actions.
Uses a GX core with exported VRAM; all actions use normal controller input.
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();m=Machine(a)
start=m.number('rtTitleMenuFrames',4);m.run(600)
cadence=m.number('rtTitleMenuFrames',4)-start
assert cadence>=590,('title cast slower than refresh rate',cadence)
for b in (5,3,3):m.press(b)
assert m.number('page')==12
# Check each displayed digit in the actual six-sprite text atlas. Compare the
# foreground mask against the font in this ROM, independently of string buffers.
syms={r[2].split('.lto_priv')[0]:int(r[0],16) for l in a.symbols.read_text().splitlines() if len(r:=l.split())==3}
raw=a.rom.read_bytes();font=syms['rtMenuFont'];words=(C.c_uint16*32768).in_dll(m.core,'vram')
vram=b''.join(struct.pack('>H',w) for w in words)
def pen(x,y):
 chunk,x=divmod(x,32);tx,px=divmod(x,8);ty,py=divmod(y,8)
 tile=16+chunk*16+tx*4+ty
 return (vram[tile*32+py*4+px//2]>>(0 if px&1 else 4))&15
for row in range(3):
 glyph=raw[font+(ord('1')+row-32)*32:font+(ord('1')+row-31)*32]
 for y in range(8):
  bits=int.from_bytes(glyph[y*4:y*4+4],'big')
  for x in range(8):
   assert (pen(18+5*9+x,row*11+1+y)==15)==bool((bits>>((7-x)*4))&15),('wrong slot digit',row+1,x,y)
# Sound-test start/change/stop and saved PSG toggles at varied actor phases.
m.press(0)
for b in (5,5,3,5,5,3):m.press(b)
assert m.number('page')==8
before=m.number('rtTitleMenuRasterRecoveries',4)
for i in range(40):
 m.run(i%11);m.press((3,7,8,6)[i%4])
 assert m.number('rtTitleMenuFault',2)==0
assert m.number('rtTitleMenuRasterRecoveries',4)==before,'sound actions missed palette deadlines'
m.press(0);assert m.number('page')==5
# Arcade submenu enters the existing attract loop by itself; Home does not.
m.core.retro_reset();m.run(300);m.press(3);assert m.number('page')==9
for _ in range(800):
 m.run(1)
 # The phase flag is set before the title finishes loading. Wait for the
 # interactive attract state before testing a short press on its prompt.
 if m.number('rtArcadeAttract')==1 and m.number('rtAppState',2)==10:break
assert m.number('rtArcadeAttract')==1,'idle Arcade menu did not start attract'
m.press(3);m.run(180);assert m.number('rtAppState',2)==1 and not m.number('rtArcadeAttract')
m.core.retro_reset();m.run(300);m.press(5);m.press(3);m.run(900)
assert m.number('page')==1 and not m.number('rtArcadeAttract')
report=dict(rom_sha256=hashlib.sha256(raw).hexdigest(),title_updates_per_600_refreshes=cadence,three_distinct_rendered_slot_digits=True,sound_actions=40,no_raster_recoveries=True,arcade_idle_attract=True,home_idle_unchanged=True,physical_hardware_tested=False)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
m.core.retro_unload_game();m.core.retro_deinit()
