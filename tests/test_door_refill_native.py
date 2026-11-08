#!/usr/bin/env python3
"""Stage 2 shared AMMO-door race fixtures through the actual cartridge loop.

RAM positions both players at refill boundaries, using Area 2's primary supply
flags (12). The production player update, object service, HUD and audio run.
This isolates overlap/consumption races; it does not reproduce an unspecified
physical-hardware door report or test traversal to the authored door.
"""
import argparse,ctypes as C,hashlib,json,struct,tempfile
from pathlib import Path
from test_soft_reset import Machine
from stage_entry_probe import enter_campaign_stage
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--region',choices=('U','E'),default='E')
p.add_argument('--boundary',choices=('both','overlap','just-consumed'),default='both');a=p.parse_args()
original=a.rom;rows=[]
with tempfile.TemporaryDirectory() as folder:
 raw=bytearray(a.rom.read_bytes());raw[0x1f0:0x200]=a.region.encode().ljust(16,b' ')
 a.rom=Path(folder)/'region.bin';a.rom.write_bytes(raw);m=Machine(a)
 for boundary in (('overlap','just-consumed') if a.boundary=='both' else (a.boundary,)):
  if rows:m.core.retro_reset();m.run(300)
  m.start('campaign_2p');enter_campaign_stage(m,a.rom,a.symbols,1)
  # Stable 42-byte agent ABI; new refill latch occupies former padding at +33.
  slot=next(i for i in range(48) if m.number('scene',1,224+i*16+8)==255)
  x,y=struct.unpack('>ii',m.read('scene',8,20))
  m.patch('scene',struct.pack('>ii8B',x-224,y-1008,1,12,4,0,2,0,1,0),224+slot*16)
  m.patch('scene',bytes([m.number('scene',1,1088)+1]),1088)
  for player in (0,1):
   base=20+42*player
   m.patch('scene',struct.pack('>ii',x,y),base)
   m.patch('scene',b'\0',base+16)
   m.patch('scene',bytes([18,1,6,0]),base+18)
   m.patch('scene',struct.pack('>HH',100,7),base+34)
   m.patch('scene',bytes([slot+1]),base+40)
  if boundary=='just-consumed':
   m.patch('scene',bytes([7,1]),40);m.patch('scene',bytes([12]),53)
  for frame in range(180):
   m.run(1)
   if all(m.number('scene',1,38+42*player)==19 for player in (0,1)):break
  else:raise AssertionError((boundary,'refill did not exit promptly'))
  ammo=[(m.number('scene',2,54+42*player),m.number('scene',2,56+42*player)) for player in (0,1)]
  expected=[(130,7),(130,7)] if boundary=='overlap' else [(102,7),(100,7)]
  assert ammo==expected,(boundary,ammo,expected)
  rows.append(dict(boundary=boundary,refreshes=frame+1,ammo=ammo))
 m.core.retro_unload_game();m.core.retro_deinit()
report=dict(rom_sha256=hashlib.sha256(original.read_bytes()).hexdigest(),region=a.region,cases=rows,scope=__doc__)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
