#!/usr/bin/env python3
"""Load all six static backgrounds with the production 68000 path; compare VRAM."""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();m=Machine(a)
s={v[2].split('.lto_priv')[0]:int(v[0],16) for l in a.symbols.read_text().splitlines() if len(v:=l.split())==3}
rom=a.rom.read_bytes();parked=next(i for i in range(512,0x80000,2) if rom[i:i+2]==b'\x60\xfe')
m.core.m68k_get_reg.argtypes=[C.c_int];m.core.m68k_get_reg.restype=C.c_uint;m.core.m68k_set_reg.argtypes=[C.c_int,C.c_uint]
def call(name,*args):
 stack=0xfffc00;data=struct.pack('>'+str(len(args)+1)+'I',parked,*args)
 for i,value in enumerate(data):C.c_ubyte.from_address(m.ram+(((stack+i)&65535)^1)).value=value
 m.core.m68k_set_reg(17,0x2300);m.core.m68k_set_reg(18,stack);m.core.m68k_set_reg(16,s[name])
 for _ in range(300):
  m.run(1)
  if m.core.m68k_get_reg(16)==parked:return m.core.m68k_get_reg(0)&255
 raise AssertionError(name+' did not return')
frozen=bytearray(Path('.build/completion/bank.bin').read_bytes());patch=json.loads(Path('assets/completion/ending_footer.json').read_text())
frozen+=struct.pack('>'+str(len(patch['extra_words']))+'H',*patch['extra_words'])
struct.pack_into('>IIIHHH',frozen,patch['descriptor_offset'],*patch['descriptor'])
struct.pack_into('>H',frozen,patch['actor_upper_offset'],patch['actor_upper'])
assert hashlib.sha256(frozen).hexdigest()==patch['sha256']
m.start('campaign_1p');call('endHud');call('RT_gameMusicStop');rows=[]
for i in range(6):
 assert call('RT_sceneBackgroundUploadIO',s['rtSceneBackgrounds']+i*18,0,0)==1
 pixels,maps,palettes,tiles,_,_=struct.unpack_from('>IIIHHH',frozen,i*18)
 raw_vram=bytes((C.c_ubyte*65536).in_dll(m.core,'vram'));vram=b''.join(raw_vram[k:k+2][::-1] for k in range(0,65536,2))
 actual=vram[512*32:(512+tiles)*32]
 expected=bytes(frozen[pixels-0x100000:pixels-0x100000+tiles*32])
 assert actual==expected,('Background pixels changed',i)
 for plane,base in [(0,0xc000),(1,0xe000)]:
  for row in range(28):
   expected=frozen[maps-0x100000+(plane*1008+row*36)*2:maps-0x100000+(plane*1008+row*36+36)*2]
   actual=vram[base+(row*64+2)*2:base+(row*64+38)*2]
   assert actual==expected,('Background map changed',i,plane,row)
 rows.append(dict(scene=i,tiles=tiles,pixels_exact=True,maps_exact=True))
a.output.write_text(json.dumps(dict(rom_sha256=hashlib.sha256(rom).hexdigest(),scenes=rows,scope='Production static-background upload, exact native VRAM pixels and maps; no playthrough or physical hardware claim.'),indent=2)+'\n')
print('Six backgrounds: exact pixels and maps in native VRAM')
m.core.retro_unload_game();m.core.retro_deinit()
