#!/usr/bin/env python3
"""Seed a completed exit walk; exercise bonus, cutscene audio and next-area flow."""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--selector',type=int,choices=range(1,10),default=1)
a=p.parse_args();m=Machine(a);m.start('campaign_1p')
# Compiler-verified scene offsets; bypass exit traversal, not transition code.
m.patch('scene',b'\1',8060);m.patch('scene',b'\0\x25',8056)
x=int.from_bytes(m.read('scene',2,8),'big',signed=True)*16+0x1280
m.patch('scene',struct.pack('>i',x),20)
m.patch('scene',bytes([1,1,0,8]),38) # WALK, right, phase0, timer8
m.patch('transitionSelector',bytes([a.selector]))
nonzero=[0];samples=[0];current=[0]
bt=C.CFUNCTYPE(C.c_size_t,C.c_void_p,C.c_size_t)
def audio(data,count):
 if current[0]==6:
  values=(C.c_int16*(count*2)).from_address(data)
  nonzero[0]+=sum(v!=0 for v in values);samples[0]+=count
 return count
cb=bt(audio);m.core.retro_set_audio_sample_batch(cb)
seen=set();tracks=set();bonus=0
for frame in range(1800):
 current[0]=m.number('rtAppState',2);m.run(1);state=m.number('rtAppState',2);seen.add(state)
 if state==5 and m.number('rtAppBonusEvents')&1:bonus+=1
 if state==6:tracks.add(m.number('rtGameMusicTrack'))
 if 6 in seen and state==1:break
else:raise AssertionError(('Did not return to gameplay',seen,m.number('rtAppState',2)))
assert 5 in seen and 6 in seen
if a.selector!=5:
 assert tracks=={5},('missing transition track',tracks)
 assert samples[0]>10000 and nonzero[0]>1000,('silent transition',samples,nonzero)
assert m.number('rtGameMusicTrack')==6,'Next area did not restore music'
r=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),selector=a.selector,states=sorted(seen),transition_tracks=sorted(tracks),audio_samples=samples[0],nonzero_samples=nonzero[0],next_area=True,limits='Seeded exit walk at first area, selected transition; waveform audibility check, not arcade waveform equivalence or full playthrough.')
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
m.core.retro_unload_game();m.core.retro_deinit()
