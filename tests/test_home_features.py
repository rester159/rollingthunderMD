#!/usr/bin/env python3
"""Controller-driven Home slots, Arcade options and cold SRAM persistence.
Inventory/score are seeded only to check saved-area restoration.
"""
import argparse,hashlib,json,subprocess,sys,tempfile
from pathlib import Path
from test_soft_reset import Machine

def worker(a):
 m=Machine(a)
 assert not any('achievement' in name.lower() for name in m.symbols)
 # Arcade has exactly three entries, wrapping directly between Start and DIPs.
 m.press(3);m.press(4);assert m.number('cursor')==2
 m.press(5);assert m.number('cursor')==0
 m.press(0);assert m.number('page')==0
 def reset():m.buttons.clear();m.core.retro_reset();m.run(300)
 def choose(slot):
  while m.number('cursor')!=slot:m.press(5)
 def resume(slot):
  m.press(5);m.press(5);m.press(3);assert m.number('page')==12
  choose(slot);m.press(3);m.run(180);assert m.number('rtAppState',2)==1
 if a.phase=='write':
  for slot in range(3):
   for b in (5,3,3):m.press(b)
   assert m.number('page')==12;choose(slot);m.press(3);assert m.number('page')==2
   if slot==1:m.press(3,1)
   if slot==2:m.press(7)
   m.press(3);m.run(180);assert m.number('rtAppState',2)==1
   reset()
  # Arcade settings, independent of Home mode, commit through the real menu.
  for b in (3,5,5,3):m.press(b)
  assert m.number('page')==11
  for b in (7,5,7,5,7):m.press(b) # lives5, later bonus, timer120
  assert m.number('rtArcadeDips')==7;reset()
  resume(2)
  # Changes after the checkpoint must not replace the saved area entrance.
  m.patch('score',(10000).to_bytes(4,'big'),4)
  m.patch('scene',(255).to_bytes(2,'big'),54)
  m.patch('scene',b'\1',47)
  m.patch('scene',(80).to_bytes(2,'big'),56)
  m.run(3)
  a.sram_output.write_bytes(m.sram())
 else:
  assert m.number('rtArcadeDips')==7
  for slot in range(3):
   resume(slot)
   assert m.number('rtHomeSaveSlot')==slot
   assert m.number('rtSession',offset=1)==(slot==1)
   assert m.number('rtSession',offset=2)==(slot==2)
   assert m.number('scene',2,54)==(75 if slot==1 else 50)
   assert m.number('score',4,4)==0,'Continue must restore the saved area entry, not later seeded score'
   reset()
  # Arcade follows persisted gameplay switches.
  for b in (3,3,3):m.press(b)
  m.run(180);assert m.number('rtAppState',2)==1
  assert m.number('arcade',offset=0)==4,('lives',m.read('arcade',12).hex())
 m.core.retro_unload_game();m.core.retro_deinit()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--phase',choices=['write','load']);p.add_argument('--sram-load',type=Path);p.add_argument('--sram-output',type=Path)
 a=p.parse_args()
 if a.phase:return worker(a)
 with tempfile.TemporaryDirectory() as t:
  save=Path(t)/'home.srm';base=[sys.executable,__file__,'--rom',str(a.rom),'--symbols',str(a.symbols),'--core',str(a.core),'--output',str(a.output)]
  for phase in ('write','load'):
   r=subprocess.run(base+['--phase',phase,'--sram-output' if phase=='write' else '--sram-load',str(save)],capture_output=True,text=True)
   assert r.returncode==0,r.stdout+r.stderr
 report=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),three_slots=True,cold_sram_restore=True,characters_and_ammo=True,achievement_code_absent=True,arcade_three_entries=True,arcade_settings_persist=True,physical_hardware_tested=False)
 a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
