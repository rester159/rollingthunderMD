#!/usr/bin/env python3
"""Survival must keep streaming forward without a death (itch v0.92 report).

The player latches the last door/climb anchor in RT_Agent.anchorIndex and keeps
it after walking away. Checkpoint capture used to reject any latched agent, so
after the first door or ladder no checkpoint was taken, retirement stalled and
the camera stopped at cameraMaxX: an invisible wall that only a death (which
rebuilds the agent) cleared. This fixture seeds that latch as left by a real
door, keeps the player alive and clears enemies so the scripted run is not
interrupted, then holds right through Survival. It requires repeated window
retirements and never a long stop at the retained-terrain edge.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--case',choices=('survival_1p','survival_2p'),required=True)
p.add_argument('--frames',type=int,default=12000)
a=p.parse_args();m=Machine(a);m.start(a.case)
players=2 if a.case.endswith('2p') else 1
# Target-compiler verified scene offsets: agents=20 (42 bytes, anchorIndex +40),
# combat=104 (10 bytes, invincibleTicks +0), enemy slots=5196 (36 bytes, kind +8),
# camera=8. survivalWorld.stage: entrances[1] +16, cameraMaxX +22.
RIGHT,UP,DOWN,JUMP=7,4,5,1
def s32(off):return struct.unpack('>i',m.read('scene',4,off))[0]
def camera():return struct.unpack('>h',m.read('scene',2,8))[0]
def clear_enemies():
 for i in range(39):m.patch('scene',b'\xff',5196+i*36+8)
 m.patch('scene',b'\0',5196+39*36+48)
recover=[{DOWN,JUMP},{UP,JUMP},{JUMP}];plan=[];last=None;still=0;tries=0
edge=longest=checkpoints=0;previousCheckpoint=None
for frame in range(a.frames):
 for q in range(players):
  m.patch('scene',b'\x00\x40',104+10*q)   # stay alive
  m.patch('scene',b'\x07',20+42*q+40)     # door/climb latch, as after a real door
 if frame%8==0:clear_enemies()
 if not plan:
  x=s32(20);still=still+1 if x==last else 0;last=x
  if still>3:still=0;plan=[recover[tries%3]|{RIGHT}]*6+[{RIGHT}]*30;tries+=1
  else:plan=[{RIGHT}]
 held=plan.pop(0);m.buttons={(q,k) for q in range(players) for k in held}
 m.run(1)
 assert m.number('rtAppState',2)==1,'Survival left gameplay'
 limit=struct.unpack('>h',m.read('survivalWorld',2,22))[0]
 edge=edge+1 if camera()>=limit else 0;longest=max(longest,edge)
 assert edge<240,('Camera held at the retained-terrain edge',camera(),limit,
                  struct.unpack('>h',m.read('survivalWorld',2,16))[0])
 cp=struct.unpack('>h',m.read('survivalWorld',2,16))[0]
 if cp!=previousCheckpoint:checkpoints+=1;previousCheckpoint=cp
retired=m.number('rtAppSurvivalRetirements',4)
assert retired>=6,('Too few retirements',retired)
r=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),case=a.case,frames=a.frames,
 retirements=retired,first_chunk=m.number('survivalWorld',4,160),checkpoint_changes=checkpoints,
 longest_edge_hold=longest,deaths=0,
 limits='Seeds the anchor latch and invincibility and clears enemies; input is scripted, not a full human playthrough.')
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
m.core.retro_unload_game();m.core.retro_deinit()
