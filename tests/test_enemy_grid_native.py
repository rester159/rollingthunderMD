#!/usr/bin/env python3
"""Actual 68000 grid admission: exact state, boundary wrapping and C ABI."""
import argparse,ctypes as C,hashlib,json,random,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'experiments/everdrive_streaming'))
from probe import Diagnostic
p=argparse.ArgumentParser(description=__doc__)
for key in ['rom','symbols','core','output']:p.add_argument('--'+key,type=Path,required=True)
a=p.parse_args();m=Diagnostic(a)
s={v[2].split('.lto_priv')[0]:int(v[0],16) for line in a.symbols.read_text().splitlines() if len(v:=line.split())==3}
rom=a.rom.read_bytes();parked=next(i for i in range(512,0x80000,2) if rom[i:i+2]==b'\x60\xfe')
m.setup(s['RT_enemyActivateGrid']);assert m.stop_pc is not None
entries=rom[s['rtEnemyEntries']:s['rtEnemyEntries']+192]
rng=random.Random(0x52475448);digest=hashlib.sha256();cycles=[]
def write(at,data):
 for i,v in enumerate(data):C.c_ubyte.from_address(m.ram+(((at+i)&65535)^1)).value=v
def read(at,n):return bytes(C.c_ubyte.from_address(m.ram+(((at+i)&65535)^1)).value for i in range(n))
def signed16(n):return (n+32768)%65536-32768
for trial in range(16000):
 raw=bytearray(rng.randbytes(36));cx=rng.randrange(-32768,32768);cy=rng.randrange(-32768,32768)
 dx=rng.choice([-769,-768,-767,5375,5376,5377,rng.randrange(-65536,65536)])
 dy=rng.choice([-1025,-1024,-1023,4095,4096,4097,rng.randrange(-65536,65536)])
 x=cx*16+dx;y=(cy+256)*16-dy
 struct.pack_into('>ii',raw,0,x,y)
 raw[8]=rng.choice([4,8,12,0,16,255]);raw[9]=rng.choice([128,129,130,131,rng.randrange(256)])
 raw[10]=rng.choice([*range(12),60,61,62,63,rng.randrange(256)])
 enemy=0 if trial%71==0 else 0xff1000;camera=0 if trial%79==0 else 0xff1100
 expected=bytearray(raw);selector=raw[10];base=selector&252;answer=0
 if enemy and camera and raw[9]&252==128 and raw[8] in (4,8,12) and base in (0,4,8,60) and (raw[8]!=12 or selector==6):
  answer=1
  if -1024<=signed16((cy+256)*16-y)<4096:
   entering=signed16(x-cx*16)<5376 if selector&2 else signed16(x-cx*16)>=-768
   if not entering and not selector&3:selector+=raw[9]&3
   if base==4:
    speed=20+((raw[29]&48)>>2)
    struct.pack_into('>hh',expected,24,-speed if selector&2 else speed,0)
    expected[35]=30 if selector&2 else 20
   elif not entering and base in (8,60):
    expected[35]=0 if raw[29]&3 else 2
    if not raw[29]&3:expected[32]|=4
   expected[11]=0
   if entering:
    expected[9]=selector;expected[12:14]=entries[base:base+2];struct.pack_into('>H',expected,18,12+(base>>2))
   else:expected[10]=selector
 write(0xff0ff0,bytes([165])*16+raw+bytes([165])*16);write(0xff1100,struct.pack('>hh',cx,cy))
 stack=0xfff000;write(stack,struct.pack('>III',parked,enemy,camera))
 preserved={r:0x12340000+r for r in [*range(2,8),*range(10,15)]}
 for r,v in preserved.items():m.core.m68k_set_reg(r,v)
 m.core.m68k_set_reg(17,0x2700);m.core.m68k_set_reg(18,stack);m.core.m68k_set_reg(16,s['RT_enemyActivateGrid'])
 m.cpu.cycles,m.cpu.refresh,m.cpu.end=0,896,0;m.stop_pc.value=parked;m.core.m68k_run(1000000);m.stop_pc.value=0
 assert m.cpu.pc==parked and m.core.m68k_get_reg(0)&255==answer,('return',trial)
 actual=read(0xff1000,36);assert actual==expected,('state',trial,raw.hex(),actual.hex(),expected.hex())
 assert read(0xff0ff0,16)==bytes([165])*16 and read(0xff1024,16)==bytes([165])*16
 assert m.core.m68k_get_reg(18)==stack+4 and all(m.core.m68k_get_reg(r)==v for r,v in preserved.items()),('ABI',trial)
 digest.update(bytes([answer])+actual);cycles.append(m.cpu.cycles)
m.core.retro_unload_game();m.core.retro_deinit()
r=dict(cases=16000,exact=True,state_sha256=digest.hexdigest(),mean_master_cycles=sum(cycles)/len(cycles),rom_sha256=hashlib.sha256(rom).hexdigest())
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
