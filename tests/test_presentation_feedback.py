#!/usr/bin/env python3
"""v0.92 itch feedback: Area 1 windows, ceiling fans, game-over Maboo.

Controller-driven Arcade Area 1 walk checks the rendered frame for black
window openings and blue fan blades; seeded exhausted lives check that Arcade
and Home game over show the laughing Maboo scene before their next screens.
Optional --shots DIR saves the inspected frames. Emulator evidence only.
"""
import argparse,ctypes as C,hashlib,json
from pathlib import Path
import numpy as np
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--shots',type=Path)
p.add_argument('--case',choices=('area1','arcade','home'),required=True)
a=p.parse_args();m=Machine(a)
frame=[None]
def video(data,w,h,pitch):
 if data:
  v=np.frombuffer(C.string_at(data,h*pitch),'<u2').reshape(h,pitch//2)[:,:w]
  frame[0]=np.stack([((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31],-1).astype('uint8')
cb=C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)(video);m.core.retro_set_video_refresh(cb)
def shot(name):
 if a.shots:
  from PIL import Image
  a.shots.mkdir(parents=True,exist_ok=True);Image.fromarray(frame[0]).save(a.shots/(name+'.png'))
r=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),case=a.case)
if a.case=='area1':
 m.start('arcade');m.run(150);m.patch('scene',b'\0\0',124)
 m.buttons={(0,7)};m.run(390);m.buttons=set();m.run(1);shot('area1-windows')
 f=frame[0].astype(int);top=f[16:120]
 black=(top.sum(2)<24)
 # Largest all-black 48x40 block among the upper-floor window openings.
 ok=any(black[y:y+40,x:x+48].all() for y in range(0,64,4) for x in range(0,272,4))
 r['black_window_opening']=bool(ok)
 # Fans hang at the top of the screen; blue-dominant blade pixels.
 blue=(f[0:40,:,2]>f[0:40,:,1]+60)
 r['blue_fan_pixels']=int(blue.sum())
 assert ok,'No black window opening in Area 1 view'
 assert r['blue_fan_pixels']>=100,('Fan blades not visible',r['blue_fan_pixels'])
else:
 m.start('arcade' if a.case=='arcade' else 'campaign_1p')
 m.patch('scene',b'\0\0',124)
 if a.case=='arcade':m.run(3);m.patch('arcade',b'\0')
 m.patch('scene',b'\0',104+7);m.patch('scene',b'\1',104+9)
 seen=0;states=[]
 for i in range(5000 if a.case=='arcade' else 900):
  m.run(1)
  s=m.number('rtAppState',2)
  if not states or states[-1]!=s:states.append(s)
  if s==11 and not seen:
   m.run(60);seen=1;shot(a.case+'-game-over-maboo')
   assert m.number('rtAppState',2)==11
  if a.case=='home' and m.number('page')==6 and s==0:break
  if a.case=='arcade' and seen and m.number('rtArcadeAttract'):break
 r.update(maboo_shown=m.number('rtGameOverMabooShown',4),state_sequence=states)
 assert seen and r['maboo_shown']==1,('Maboo scene not shown',states)
 if a.case=='arcade':assert m.number('rtHighStarted',2)==1 and m.number('rtArcadeAttract'),states
 else:assert m.number('page')==6,('expected GAME OVER page',m.number('page'))
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
m.core.retro_unload_game();m.core.retro_deinit()
