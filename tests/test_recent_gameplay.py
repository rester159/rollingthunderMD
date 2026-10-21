#!/usr/bin/env python3
"""October 19/20 feedback fixtures through production stage/VDP/gameplay paths.

RAM places actors near authored geometry; vault, crouch, fire, wall response and
opening animation then use production code. These are not full playthroughs.
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from test_soft_reset import Machine
from stage_entry_probe import enter_campaign_stage
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--case',choices=('platform','mutant','mutant-inside','door'),required=True)
a=p.parse_args();m=Machine(a)
syms={r[2].split('.lto_priv')[0]:int(r[0],16) for l in a.symbols.read_text().splitlines() if len(r:=l.split())==3}
class Map(C.Structure):_fields_=[(n,C.c_void_p) for n in ('base','read8','read16','write8','write16')]
maps=(Map*256).in_dll(m.core,'m68k')
def write(at,data):
 for i,v in enumerate(data):C.c_ubyte.from_address(maps[(at+i)>>16].base+(((at+i)&65535)^1)).value=v
def clear_enemies():
 for i in range(39):m.patch('scene',b'\xff',5196+i*36+8)
 m.patch('scene',b'\0',5196+39*36+48)
def enter(stage,cx,cy):
 write(syms['rtWorldStages']+stage*24+12,struct.pack('>hh',cx,cy))
 enter_campaign_stage(m,a.rom,a.symbols,stage)
 m.patch('scene',b'\xff\xff',104)
m.start('campaign_1p')
if a.case=='platform':
 enter(1,1600,0)
 m.patch('scene',struct.pack('>ii',1808*16,224*16),20);m.patch('scene',b'\4',36)
 m.buttons={(0,4),(0,1)};m.run(6);m.buttons.clear();m.run(100)
 assert m.number('scene',4,24)==127*16 and m.number('scene',1,36)==2,'Vault did not reproduce the platform boundary'
 clear_enemies();m.patch('scene',bytes(49*18+2),6768)
 m.buttons={(0,5)};m.run(30);m.buttons.add((0,0))
 shots=[]
 for frame in range(22):
  m.run(1)
  s=struct.unpack('>iih8B',m.read('scene',18,6768))
  if s[3] in (1,3,5):shots.append((s[0],s[3]))
 assert shots and all(kind!=5 for x,kind in shots),('Invisible platform wall',shots)
 assert shots[-1][0]-shots[0][0]>=64*16,shots
 result=dict(vault_landing_y=127,bullet_y=112,travel_pixels=(shots[-1][0]-shots[0][0])//16,no_false_impact=True)
elif a.case.startswith('mutant'):
 embedded=a.case=='mutant-inside'
 enter(2,640 if embedded else 480,-8)
 m.patch('rtAppPaused',b'\1');m.run(3);clear_enemies()
 # Authored yellow mutant definition94, placed before the real column at640.
 definition=syms['rtGridEnemyTemplates']+94*10
 raw=bytearray(36);struct.pack_into('>ii',raw,0,(816 if embedded else 632)*16,232*16)
 raw[8:15]=bytes([32,6 if embedded else 5,6 if embedded else 5,0,6,40,128]);struct.pack_into('>H',raw,18,384);struct.pack_into('>h',raw,24,-20 if embedded else 20);struct.pack_into('>I',raw,20,definition)
 raw[29]=64;raw[30]=20;raw[31]=1;raw[34]=255
 m.patch('scene',raw,5196);m.patch('scene',b'\1',5196+39*36+48)
 m.patch('rtAppPaused',b'\0')
 states=[];positions=[]
 for frame in range(240):
  m.run(1);states.append(m.number('scene',1,5196+9)&252)
  positions.append((m.number('scene',4,5196),m.number('scene',4,5200)))
 assert 0x24 in states,('Mutant never hopped at the authored wall',sorted(set(states)))
 assert min(y for x,y in positions)<220*16,positions
 result=dict(states=sorted(set(states)),highest_y=min(y for x,y in positions)/16,minimum_x=min(x for x,y in positions)/16,final_position=[v/16 for v in positions[-1]],wall_x=800 if embedded else 640,responded=True)
 if embedded:assert min(x for x,y in positions)<800*16,result
else:
 # First authored doorway. A held request isolates panels/interior from enemy AI.
 enter(0,0,-8)
 slot=next(i for i in range(48) if m.number('scene',4,224+i*16)==128*16 and m.number('scene',1,224+i*16+8)==0)
 off=224+slot*16
 for _ in range(50):m.patch('scene',bytes([m.number('scene',1,off+9)|1]),off+9);m.run(1)
 assert m.number('scene',1,off+10)==4
 raw=bytes((C.c_ubyte*65536).in_dll(m.core,'vram'));vram=bytearray(raw);vram[::2]=raw[1::2];vram[1::2]=raw[::2]
 cram=(C.c_ushort*64).in_dll(m.core,'cram')
 black=0
 for y in range(20,28):
  for x in range(16,20):
   attr=struct.unpack_from('>H',vram,0xc000+(y&31)*128+x*2)[0]
   tile,line=attr&2047,(attr>>13)&3
   for b in vram[tile*32:tile*32+32]:
    for pen in (b>>4,b&15):assert pen and cram[line*16+pen]==0,('Door is transparent/colored',x,y,attr,pen);black+=1
 result=dict(opaque_black_interior_pixels=black,green_stage_backdrop_retained=cram[0]!=0)
m.core.retro_unload_game();m.core.retro_deinit()
report=dict(case=a.case,rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),scope=__doc__,**result)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
