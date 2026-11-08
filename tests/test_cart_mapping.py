#!/usr/bin/env python3
"""Check live SSF2 detection, including a mapper-less GX cartridge model."""
import argparse,ctypes as C,json
from pathlib import Path
from check_compatibility import Probe
from test_everdrive_model import Map
p=argparse.ArgumentParser(description=__doc__)
for name in ('rom','symbols','core'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--no-mapper',action='store_true');p.add_argument('--linear',action='store_true');a=p.parse_args()
mapper_writes=[]
m=Probe(a);m.ready=True;m.frame=0
if a.no_mapper:
 maps=(Map*256).in_dll(m.core,'m68k');wt=C.CFUNCTYPE(None,C.c_uint,C.c_uint)
 old8,old16=wt(maps[0xa1].write8),wt(maps[0xa1].write16)
 def write(original,address,value):
  if 0xa130f2<=address<=0xa130ff:
   mapper_writes.append((address,value));return
  original(address,value)
 cb8=wt(lambda a,d:write(old8,a,d));cb16=wt(lambda a,d:write(old16,a,d))
 maps[0xa1].write8=C.cast(cb8,C.c_void_p);maps[0xa1].write16=C.cast(cb16,C.c_void_p)
m.run(600)
assert m.number('rtMapperSupported')==int(a.linear or not a.no_mapper)
before=m.number('vtimer',4);m.run(60);assert m.number('vtimer',4)>before
assert m.number('rtAppFault',2)==0
if a.linear or not a.no_mapper:m.start('campaign_2p')
if a.linear and a.no_mapper:assert not mapper_writes, mapper_writes
print(json.dumps(dict(no_mapper=a.no_mapper,passed=True,linear=a.linear,mapper_writes=len(mapper_writes),graceful_unsupported_screen=a.no_mapper and not a.linear)))
m.core.retro_unload_game();m.core.retro_deinit()
