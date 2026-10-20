#!/usr/bin/env python3
"""Solo Area 2 authored supply doors via normal controller entry/refill/exit.

The emulator's entrance table is moved near each door; production stage loaders
instantiate the real objects. Only player position, inventory and protection are
seeded. Door flags, action, phase and timers are never injected. This is a local
emulator fixture, not a hardware playthrough.
"""
import argparse,ctypes as C,hashlib,json,struct,tempfile
from pathlib import Path
from test_soft_reset import Machine
from stage_entry_probe import enter_campaign_stage
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--region',choices=('U','E'),default='E')
p.add_argument('--agent',type=int,choices=(0,1),default=0)
p.add_argument('--capture-dir',type=Path)
a=p.parse_args();original=a.rom;rows=[]
class Map(C.Structure):_fields_=[(n,C.c_void_p) for n in ('base','read8','read16','write8','write16')]
symbols={r[2].split('.lto_priv')[0]:int(r[0],16) for l in a.symbols.read_text().splitlines() if len(r:=l.split())==3}
with tempfile.TemporaryDirectory() as folder:
 raw=bytearray(a.rom.read_bytes());raw[0x1f0:0x200]=a.region.encode().ljust(16,b' ')
 a.rom=Path(folder)/'region.bin';a.rom.write_bytes(raw);m=Machine(a)
 last=[None];vt=C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)
 def video(data,w,h,pitch):
  if data:last[0]=(C.string_at(data,h*pitch),w,h,pitch)
 cb=vt(video);m.core.retro_set_video_refresh(cb)
 def inventory():return (m.number('scene',2,54),m.number('scene',2,56),m.number('scene',1,47))
 def digits(cell):
  glyphs=m.read('hudState',6,cell*2)[::2]
  assert all(x<10 or x==255 for x in glyphs),list(glyphs)
  return sum((0 if x==255 else x)*10**(2-i) for i,x in enumerate(glyphs))
 def hud():
  primary,alternate,mode=inventory()
  assert digits(115)==primary%1000,('pistol refill hidden',inventory(),digits(115))
  if mode:
   assert m.read('hudState',4,144)==bytes([22,252,16,252])
   assert digits(75)==alternate%1000
  else:assert m.read('hudState',12,144)==bytes([255,3])*6
 def visit(cancel=False):
  m.buttons={(0,4)};entered=exited=False;cancelled=False
  for frame in range(240):
   m.run(1);action=m.number('scene',1,38);phase=m.number('scene',1,40)
   if action==18:
    entered=True;m.buttons.clear()
    if cancel and phase==5 and not cancelled:m.buttons={(0,5)};cancelled=True
   if action==19:exited=True;m.buttons.clear()
   if entered and exited and action not in (18,19):
    m.buttons.clear();m.run(3);hud();return frame+1
  raise AssertionError(('door visit did not finish',entered,exited,inventory()))
 for case in ('pistol','machine-active','cancel-retry','machine-pickup'):
  if rows:m.core.retro_reset();m.run(300)
  m.start('campaign_1p',a.agent)
  machine=case=='machine-pickup';x,y=(576,289) if machine else (928,321)
  maps=(Map*256).in_dll(m.core,'m68k');address=symbols['rtWorldStages']+24+12
  for i,value in enumerate(struct.pack('>hh',x-128,192)):
   C.c_ubyte.from_address(maps[(address+i)>>16].base+(((address+i)&65535)^1)).value=value
  enter_campaign_stage(m,a.rom,a.symbols,1)
  slot=next(i for i in range(48) if m.number('scene',4,224+i*16)==x*16 and m.number('scene',1,224+i*16+8)==1)
  assert m.number('scene',1,224+slot*16+9)==(52 if machine else 12)
  m.patch('scene',struct.pack('>ii',(x+14)*16,(y+63)*16),20);m.patch('scene',b'\2',36)
  m.patch('scene',b'\xff\xff',104)
  active=case=='machine-active'
  m.patch('scene',bytes([int(active)]),47);m.patch('scene',struct.pack('>HH',50,80 if active else 0),54)
  m.run(3);hud();before=inventory()
  if case=='cancel-retry':
   visit(cancel=True);assert inventory()==before
   assert m.number('scene',1,224+slot*16+9)&24
  elapsed=visit();expected=(50,80,1) if machine else (70,80 if active else 0,int(active))
  assert inventory()==expected,(case,inventory(),expected)
  visit();assert inventory()==expected,('spent door granted more ammo',case)
  rows.append(dict(case=case,before=before,after=inventory(),visit_refreshes=elapsed,hud_primary=digits(115),hud_machine=digits(75) if inventory()[2] else None,spent_door_no_refill=True))
  if a.capture_dir and active:
   import numpy as np
   from PIL import Image
   data,w,h,pitch=last[0];pixels=np.frombuffer(data,dtype='<u2').reshape(h,pitch//2)[:,:w]
   rgb=np.stack([((pixels>>11)&31)*255//31,((pixels>>5)&63)*255//63,(pixels&31)*255//31],-1).astype('uint8')
   a.capture_dir.mkdir(parents=True,exist_ok=True);Image.fromarray(rgb).resize((640,448),Image.Resampling.NEAREST).save(a.capture_dir/f'solo-ammo-{a.region}-agent{a.agent}.png')
 m.core.retro_unload_game();m.core.retro_deinit()
report=dict(rom_sha256=hashlib.sha256(original.read_bytes()).hexdigest(),region=a.region,agent=a.agent,cases=rows,scope=__doc__)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
