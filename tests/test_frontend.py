#!/usr/bin/env python3
"""Controller-driven selection, saved layouts and actual jump/fire regression."""
import argparse,ctypes as C,json,hashlib
from pathlib import Path
import numpy as np
from test_soft_reset import Machine

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('rom','symbols','core'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--output',type=Path,required=True);a=p.parse_args();m=Machine(a)
 last=[None];light=[]
 vt=C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)
 def video(data,w,h,pitch):
  if data:
   v=np.frombuffer(C.string_at(data,h*pitch),dtype='<u2').reshape(h,pitch//2)[:,:w].copy()
   last[0]=v.tobytes();light.append(int(np.count_nonzero(v)))
 callback=vt(video);m.core.retro_set_video_refresh(callback)
 for b in (5,3,3):m.press(b)
 initial=last[0];light.clear()
 m.press(7);m.press(7)
 assert last[0]==initial,'Switching back did not restore the exact original selection screen'
 for button,port in [(3,1),(7,0),(7,1),(0,1),(3,1),(7,1)]:m.press(button,port)
 assert light and min(light)>1000,'Selection blanked during switch/join/leave'
 frames=len(light)
 m.press(0) # Back to Home.
 def controls():
  for b in (5,5,3,5,3):m.press(b)
 controls();m.press(7);m.press(5);m.press(7);m.press(7)
 assert m.number('rtControlLayout')==9,(m.number('rtControlLayout'),m.number('page'),m.number('cursor'))
 # Repeated commits at varied input timing must not interrupt title raster IRQs.
 for i in range(30):
  m.press(7);m.run(i%5)
  assert m.number('rtTitleMenuFault',2)==0,'Settings save missed a title palette deadline'
 assert m.number('rtControlLayout')==9
 m.core.retro_reset();m.run(300);assert m.number('rtControlLayout')==9,(m.number('rtControlLayout'),m.number('page'),m.number('cursor'))
 m.start('campaign_2p')
 # Offsets are the m68000 ABI: offsetof(scene, players.agents[p].body.y/ammoPrimary).
 def y(p):return int.from_bytes(m.read('scene',4,24+42*p),'big',signed=True)
 def ammo(p):return m.number('scene',2,54+42*p)
 def play(buttons):
  m.buttons=set(buttons);m.run(6);m.buttons.clear();m.run(8)
 y0,y1,a0=y(0),y(1),ammo(0)
 play([(0,8),(1,0)]) # P1 C fires; P2 B jumps.
 assert ammo(0)<a0 and y(0)==y0 and y(1)<y1,('P1 fire/P2 jump',ammo(0),a0,y(0),y0,y(1),y1)
 m.run(100);y0,y1,a1=y(0),y(1),ammo(1)
 play([(0,1),(1,8)]) # P1 A jumps; P2 C fires.
 assert y(0)<y0 and ammo(1)<a1 and y(1)==y1,('P1 jump/P2 fire',y(0),y0,ammo(1),a1,y(1),y1)
 # Select the default on each pad through Settings, then test C in solo modes.
 m.core.retro_reset();m.run(300)
 m.press(5);m.press(3);controls()
 m.press(6);m.press(5);m.press(6);m.press(6)
 assert m.number('rtControlLayout')==0
 for case in ('campaign_1p','survival_1p','arcade'):
  m.core.retro_reset();m.run(300);m.start(case)
  floor=y(0);before=ammo(0);play([(0,8)])
  assert y(0)<floor and ammo(0)==before,(case,'C must jump, not shoot')
 report=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),selection_frames=frames,black_frames=0,selection_roundtrip_exact=True,saved_independent_layouts=True,settings_commits_without_raster_fault=30,p1_p2_jump_fire=True,default_c_jump=['campaign_1p','survival_1p','arcade'])
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
 m.core.retro_unload_game();m.core.retro_deinit()
if __name__=='__main__':main()
