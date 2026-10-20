#!/usr/bin/env python3
"""Production 68000 packed-column copy: full-width indices, bounds and C ABI."""
import argparse,ctypes as C,hashlib,json,random,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'experiments/everdrive_streaming'))
from probe import Diagnostic
p=argparse.ArgumentParser(description=__doc__)
for key in ['rom','symbols','core','output']:p.add_argument('--'+key,type=Path,required=True)
a=p.parse_args();m=Diagnostic(a)
s={v[2]:int(v[0],16) for line in a.symbols.read_text().splitlines() if len(v:=line.split())==3}
rom=a.rom.read_bytes();parked=next(i for i in range(512,0x80000,2) if rom[i:i+2]==b'\x60\xfe')
m.setup(s['RT_copyPackedColumn']);assert m.stop_pc is not None
rng=random.Random(0x434f4c55);digest=hashlib.sha256();cycles=[]
def write(at,data):
 for i,v in enumerate(data):C.c_ubyte.from_address(m.ram+(((at+i)&65535)^1)).value=v
def read(at,n):return bytes(C.c_ubyte.from_address(m.ram+(((at+i)&65535)^1)).value for i in range(n))
for trial in range(4096):
 count=trial%32+1;stride=rng.choice([2,8,16,32,76,128]);column=rng.randrange(16)
 base=0x100000+column*2;indices=[rng.randrange(65536) for _ in range(count)]
 expected=b''.join(rom[base+i*32:base+i*32+2] for i in indices)
 for i,index in enumerate(indices):write(0xff1000+i*stride,struct.pack('>H',index))
 write(0xff2ff0,bytes([165])*(16+count*2+16))
 stack=0xfff000;write(stack,struct.pack('>6I',parked,0xff1000,base,0xff3000,stride,count))
 preserved={r:0x12340000+r for r in [*range(2,8),*range(10,15)]}
 for r,v in preserved.items():m.core.m68k_set_reg(r,v)
 m.core.m68k_set_reg(17,0x2700);m.core.m68k_set_reg(18,stack);m.core.m68k_set_reg(16,s['RT_copyPackedColumn'])
 m.cpu.cycles,m.cpu.refresh,m.cpu.end=0,896,0;m.stop_pc.value=parked;m.core.m68k_run(1000000);m.stop_pc.value=0
 actual=read(0xff3000,count*2)
 assert m.cpu.pc==parked and actual==expected,('pixels',trial,actual.hex(),expected.hex())
 assert read(0xff2ff0,16)==bytes([165])*16 and read(0xff3000+count*2,16)==bytes([165])*16
 assert m.core.m68k_get_reg(18)==stack+4 and all(m.core.m68k_get_reg(r)==v for r,v in preserved.items()),('ABI',trial)
 digest.update(actual);cycles.append(m.cpu.cycles)
m.core.retro_unload_game();m.core.retro_deinit()
report=dict(cases=4096,exact=True,output_sha256=digest.hexdigest(),mean_master_cycles=sum(cycles)/len(cycles),rom_sha256=hashlib.sha256(rom).hexdigest())
a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
