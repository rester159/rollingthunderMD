#!/usr/bin/env python3
"""Arcade attract lifecycle, source script, input handoff, and save isolation."""
import argparse,json,hashlib,re
from pathlib import Path
from zipfile import ZipFile
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','arcade-rom','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();m=Machine(a)
# Read-only comparison with the owner-supplied arcade action stream.
source=Path('src/attract.c').read_text().split('static const u8 commands[]={')[1].split('};')[0]
commands=bytes(int(n,16) for n in re.findall(r'0x[0-9a-f]+',source))
with ZipFile(a.arcade_rom) as z:arcade=z.read('rt3_2b.12c')
assert arcade[0x6b00:0x6b00+len(commands)]==commands and arcade[0x6b00+len(commands)]==128
# Keep an existing score across demo loops and subsequent real Starts.
m.patch('score',(543210).to_bytes(4,'big'))
m.press(3);assert m.number('rtArcadeAttract')==1
saved=m.sram();records=m.read('highRecords',50);seen=set();indices=set();events=[];last=None
for frame in range(6500):
 m.run(1);phase=m.number('rtArcadeAttract');seen.add(phase)
 if phase==2:
  index=m.number('rtAttractScriptIndex');indices.add(index)
  if index!=last:events.append((index,m.number('rtAttractDemoUpdates',4)));last=index
 if frame%60==0:
  assert m.sram()==saved and m.read('highRecords',50)==records,'Attract changed saves or scores'
  assert m.number('score',4)==543210 and m.number('rtAppPaused')==0
  assert m.number('rtTitleMenuFault',2)==0
 if m.number('rtAttractLoops',4)>=2:break
assert m.number('rtAttractLoops',4)>=2 and seen=={1,2,3,4},(seen,frame)
assert set(range(38))<=indices,sorted(indices)
first={}
for i,t in events:first.setdefault(i,t)
# A core's video callback can land between the script-index write and the
# update-counter write. Allow that one-update observation boundary.
assert all(abs(first[i+1]-first[i]-n)<=1 for i,n in enumerate((36,22,36,22),1)),first
# Every phase allows Start, with a fresh human game and no held-Start pause.
starts=[]
for target in (1,2,3,4):
 m.core.retro_reset();m.run(300);m.press(3)
 for _ in range(3500):
  if m.number('rtArcadeAttract')==target:break
  m.run(1)
 assert m.number('rtArcadeAttract')==target,target
 old_save=m.sram();old_records=m.read('highRecords',50)
 m.buttons={(0,3)};m.run(190);m.buttons.clear();m.run(10)
 assert m.number('rtArcadeAttract')==0 and m.number('rtAppState',2)==1
 assert m.number('rtAppPaused')==0 and m.number('scene',2,54)==50
 assert m.number('score',4,4)==0 and m.number('scene',4,20)==0x600
 assert m.sram()==old_save and m.read('highRecords',50)==old_records
 m.press(3);assert m.number('rtAppPaused')==1,'Start cannot pause real game after attract'
 starts.append(target)
# Back works in every phase; Home menus never start attract playback.
for target in (1,2,3,4):
 m.core.retro_reset();m.run(300);m.press(3)
 for _ in range(3500):
  if m.number('rtArcadeAttract')==target:break
  m.run(1)
 assert m.number('rtArcadeAttract')==target
 m.press(0);assert m.number('rtArcadeAttract')==0 and m.number('rtAppState',2)==0,(target,m.number('rtArcadeAttract'),m.number('rtAppState',2),m.number('page'),m.number('attractPrevious',2))
 assert m.number('page')==0
m.press(5);m.press(3);m.run(1200)
assert m.number('rtArcadeAttract')==0 and m.number('page')==1
m.core.retro_reset();m.run(300)
assert m.number('rtArcadeAttract')==0 and m.number('rtAppPaused')==0
report=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),original_script_bytes=len(commands),complete_loops=2,phases=[1,2,3,4],all_38_script_commands=True,initial_walk_fire_timing=True,saves_and_records_unchanged=True,start_and_back_phases=starts,held_start_no_pause=True,home_idle_unchanged=True,reset_to_mode_selection=True)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
m.core.retro_unload_game();m.core.retro_deinit()
