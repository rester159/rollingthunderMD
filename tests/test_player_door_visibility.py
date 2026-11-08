#!/usr/bin/env python3
"""Seed player at authored door; check published SAT through normal door input."""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--agent',type=int,choices=[0,1],default=0)
a=p.parse_args();m=Machine(a);m.start('campaign_1p',a.agent)
# m68000 layout verified: players.agents=20; body.layer=16; action=18.
anchor=next(m.read('scene',16,224+i*16) for i in range(48) if m.number('scene',1,224+i*16+8)<3 and m.number('scene',1,224+i*16+14))
x,y=struct.unpack_from('>ii',anchor)
assert (x,y)==(128*16,161*16)
m.patch('scene',struct.pack('>ii',x+0xe0,y+(0x3f0 if anchor[9]&4 else 0x470)),20)
m.patch('scene',bytes([anchor[12]]),36)
m.patch('scene',b'\xff\xff',104) # protection prevents unrelated death during fixture
first=m.number('sprites',2,896);capacity=m.number('sprites',2,898)
assert 0<first<1536 and 0<capacity<256
m.buttons={(0,4)}
inside=outside=0;stable=0;seen_inside=False
for frame in range(240):
 m.run(1)
 action=m.number('scene',1,38);layer=m.number('scene',1,36)
 hidden=action in (18,19) and layer==0
 stable=stable+1 if hidden else 0
 raw=bytes((C.c_ubyte*65536).in_dll(m.core,'vram'));v=bytearray(raw)
 v[0::2],v[1::2]=raw[1::2],raw[0::2]
 count=0;index=0
 for _ in range(80):
  y,size,link,attr,x=struct.unpack_from('>HBBHH',v,0xf400+index*8)
  count+=int(first<=attr&2047<first+capacity)
  if not link:break
  index=link
 else:raise AssertionError('SAT loop')
 # Allow the existing queued presentation to retire after transition.
 if stable>=4:
  assert count==0,('player drawn inside door',frame,action,layer,count)
  inside+=1;seen_inside=True
 if seen_inside and not hidden and count:outside+=1
 if frame==120:m.buttons.clear()
assert inside>=10 and outside>=3,(inside,outside)
r=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),agent=a.agent,hidden_sat_frames=inside,visible_exit_frames=outside,limits='First authored supply door, RAM placement/protection only; production input/state/SAT publication.')
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
m.core.retro_unload_game();m.core.retro_deinit()
