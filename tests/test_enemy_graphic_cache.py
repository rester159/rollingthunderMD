#!/usr/bin/env python3
"""Compare cached enemy graphics with the production 68000 atlas selectors.

Pause at sprite publication after exercising a stage, then call the original
selector for every populated cache entry. No cartridge bytes are patched.
"""
import argparse,ctypes as C,hashlib,json,struct,sys
from pathlib import Path
from test_soft_reset import Machine
from stage_entry_probe import enter_campaign_stage
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'experiments/everdrive_streaming'))
from probe import CPU
p=argparse.ArgumentParser(description=__doc__)
for key in ['rom','symbols','core','output']:p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--stage',type=int,choices=range(10),required=True)
p.add_argument('--players',type=int,choices=[1,2],default=1)
a=p.parse_args();m=Machine(a)
s={v[2].split('.lto_priv')[0]:int(v[0],16) for l in a.symbols.read_text().splitlines() if len(v:=l.split())==3}
raw=a.rom.read_bytes();layout=json.loads(a.rom.with_name('build.json').read_text())
parked=next(i for i in range(512,0x80000,2) if raw[i:i+2]==b'\x60\xfe')
m.start('campaign_'+str(a.players)+'p');enter_campaign_stage(m,a.rom,a.symbols,a.stage,0)
for frame in range(600):
 u=m.number('rtAppUpdates',4);m.buttons={(0,7),(0,0),(1,7),(1,0)}
 if u%90<4:m.buttons|={(0,1),(1,1)}
 m.run(1)
stop=C.c_uint.in_dll(m.core,'rt_diagnostic_stop_pc');stop.value=s['RT_spritesPublishPrepared']
for frame in range(300):
 m.run(1)
 if m.core.m68k_get_reg(16)==stop.value:break
else:raise AssertionError('No sprite publication reached')
stop.value=0
keys=m.read('graphicKeys',39*4);pointers=m.read('graphics',39*4)
graphics=struct.unpack_from('>I',raw,layout['compact']['stages'][a.stage][0]+4)[0]
cpu=CPU.in_dll(m.core,'m68k');assert cpu.pc==m.core.m68k_get_reg(16)
m.core.m68k_run.argtypes=[C.c_uint];m.core.m68k_run.restype=None
count=0;families=set()
for index in range(39):
 pose,kind,color=struct.unpack_from('>HBB',keys,index*4)
 if kind==255:continue
 expected=struct.unpack_from('>I',pointers,index*4)[0]
 selector=struct.unpack_from('>I',raw,graphics+56+(4 if kind>=32 else 0))[0]
 stack=0xfff000;data=struct.pack('>4I',parked,kind,color,pose)
 for i,value in enumerate(data):C.c_ubyte.from_address(m.ram+(((stack+i)&65535)^1)).value=value
 m.core.m68k_set_reg(17,0x2700);m.core.m68k_set_reg(18,stack);m.core.m68k_set_reg(16,selector)
 cpu.cycles,cpu.refresh,cpu.end=0,896,0;stop.value=parked;m.core.m68k_run(1000000);stop.value=0
 assert cpu.pc==parked
 actual=m.core.m68k_get_reg(0)
 assert actual==expected,('Stale graphic',a.stage,index,kind,color,pose,hex(actual),hex(expected))
 count+=1;families.add(kind)
m.core.retro_unload_game();m.core.retro_deinit()
report=dict(stage=a.stage,players=a.players,exact_entries=count,families=sorted(families),passed=True,rom_sha256=hashlib.sha256(raw).hexdigest())
a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
