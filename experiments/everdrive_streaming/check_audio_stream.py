#!/usr/bin/env python3
"""Host-only DAC recorder: exact finite PCM bytes and emulated output cadence."""
import argparse,ctypes as C,hashlib,json,struct,sys
from pathlib import Path
from collections import Counter
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tests'))
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--core',type=Path,required=True);p.add_argument('--rom',type=Path,required=True)
p.add_argument('--symbols',type=Path,required=True);p.add_argument('--layout',type=Path,required=True)
p.add_argument('--clip',choices=['victory','ending','high_cold','high_warm'],required=True)
p.add_argument('--transition',choices=['start','rearm','replace'],default='start')
p.add_argument('--output',type=Path,required=True);p.add_argument('--scene',action='store_true')
a=p.parse_args();m=Machine(a)
symbols={v[2].split('.lto_priv')[0]:int(v[0],16) for line in a.symbols.read_text().splitlines() if len(v:=line.split())==3}
raw=a.rom.read_bytes();parked=next(i for i in range(512,0x80000,2) if raw[i:i+2]==b'\x60\xfe')
m.core.m68k_get_reg.argtypes=[C.c_int];m.core.m68k_get_reg.restype=C.c_uint;m.core.m68k_set_reg.argtypes=[C.c_int,C.c_uint]
enabled=C.c_uint.in_dll(m.core,'rt_dac_enabled');used=C.c_uint.in_dll(m.core,'rt_dac_used');events=(C.c_uint*(4096*2)).in_dll(m.core,'rt_dac_events')
frame=0;values=bytearray();times=[];bus_events=[]
try:
 bus_used=C.c_uint.in_dll(m.core,'rt_bus_used');bus_data=(C.c_uint*(8192*4)).in_dll(m.core,'rt_bus_events')
except ValueError:bus_used=None
def run():
 global frame
 used.value=0
 if bus_used is not None:bus_used.value=0
 m.run(1);assert used.value<4096
 if bus_used is not None:
  bus_events.extend([[frame,*bus_data[i*4:i*4+4]] for i in range(bus_used.value)])
 for i in range(used.value):values.append(events[2*i]);times.append(frame*896040+events[2*i+1])
 frame+=1

def call(name,*args):
 stack=0xfffc00;data=struct.pack('>'+str(1+len(args))+'I',parked,*args)
 for i,x in enumerate(data):C.c_ubyte.from_address(m.ram+(((stack+i)&65535)^1)).value=x
 m.core.m68k_set_reg(17,0x2300);m.core.m68k_set_reg(18,stack);m.core.m68k_set_reg(16,symbols[name])
 for _ in range(240):
  run()
  if m.core.m68k_get_reg(16)==parked:return m.core.m68k_get_reg(0)&255
 raise AssertionError(name+' failed to return')

m.start('campaign_2p')
class CpuMap(C.Structure):
 _fields_=[(name,C.c_void_p) for name in ('base','read8','read16','write8','write16')]
mapper=(CpuMap*256).in_dll(m.core,'m68k')
previous_window_one=mapper[8].base
if a.scene and a.clip=='high_cold':
 m.patch('score',(654321).to_bytes(4,'big'),4)
else:
 call('endHud');call('RT_gameMusicStop')
values.clear();times.clear();enabled.value=1
layout=json.loads(a.layout.read_text());layout=layout.get('compact',layout)
expected=(ROOT/'.build/audio'/f'{a.clip}.pcm').read_bytes()
old_tail=b''
if a.transition!='start':
 assert not a.scene
 seed=(ROOT/'.build/audio/victory.pcm').read_bytes()
 assert call('RT_startSegmentedPcm',layout['audio_addresses']['victory'],len(seed),len(raw))==1
 if a.transition=='rearm':
  seed_zram=(C.c_ubyte*8192).in_dll(m.core,'zram')
  for _ in range(600):
   run()
   if seed_zram[0x101]==3:break
  assert bytes(values)==seed+b'\x80'
 else:
  for _ in range(17):run()
  assert bytes(values)==seed[:len(values)]
  old_tail=seed[len(values):]
 values.clear();times.clear()
if a.scene and a.clip=='high_cold':
 assert call('beginHighEntry')==1
else:assert call({'start':'RT_startSegmentedPcm','rearm':'RT_rearmSegmentedPcm','replace':'RT_replaceSegmentedPcm'}[a.transition],layout['audio_addresses'][a.clip],len(expected),len(raw))==1
zram=(C.c_ubyte*8192).in_dll(m.core,'zram')
ending_scene=a.scene and a.clip in ('ending','victory')
if ending_scene:
 m.patch('joyBeforeVBlankCallback',b'\x01')
 assert call('begin.part.0',symbols.get('completion',symbols.get('screenWorkspace')),10,1,symbols['finalLoadBegin'],symbols['RT_endExclusivePcmSceneFrame'])==1
scene_steps=0
for _ in range(6000):
 if ending_scene:
  result=call('RT_completionSceneStepIO.part.0',symbols.get('completion',symbols.get('screenWorkspace')),symbols['finalFrameBegin'],symbols['RT_endExclusivePcmSceneFrame'])
  assert result in (1,2), 'Ending scene fault'
  scene_steps+=1
  if result==2:ending_scene=False
 else:run()
 if zram[0x101]==3:break
else:raise AssertionError('PCM did not finish')
# The finite driver emits exactly the original samples then one idle 128.
setup=len(values)-len(expected)-1
assert 0<=setup<=8 and values[:setup]==(old_tail[:setup] if a.transition=='replace' else bytes([128])*setup), 'Unexpected DAC setup writes'
actual=bytes(values[setup:]);times=times[setup:];match=actual==expected+b'\x80'
wrong=next((i for i,(x,y) in enumerate(zip(actual,expected)) if x!=y),None)
intervals=Counter(b-a for a,b in zip(times,times[1:]));exact=intervals==Counter({3405:len(expected)})
report=dict(clip=a.clip,scene=a.scene,transition=a.transition,scene_steps=scene_steps,rom_sha256=hashlib.sha256(raw).hexdigest(),expected_samples=len(expected),dac_writes=len(actual),setup_silence_writes=setup,samples_exact=match,first_mismatch=wrong,cadence_master_cycles=dict(sorted(intervals.items())),cadence_exact=exact,frames=frame,counters={name:m.number(name,2) for name in ['rtPcmStreamFaults','rtPcmStreamPages','rtPcmStreamMaxLag'] if name in m.symbols},physical_hardware_tested=False)
a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='cadence_master_cycles'}))
if bus_events:a.output.with_suffix('.bus.json').write_text(json.dumps(bus_events))
if not exact:a.output.with_suffix('.gaps.json').write_text(json.dumps([(times[i],times[i+1]-times[i]) for i in range(len(times)-1) if times[i+1]-times[i]!=3405]))
if not match:
 a.output.with_suffix('.actual.pcm').write_bytes(actual)
assert call('RT_stopSegmentedPcm')==1
assert call('RT_stopSegmentedPcm')==1, 'Idempotent PCM stop failed'
assert mapper[8].base==previous_window_one, 'PCM did not restore the physical ROM mapping'
report['physical_mapping_restored']=True
report['idempotent_stop']=True
a.output.write_text(json.dumps(report,indent=2)+'\n')
m.core.retro_unload_game();m.core.retro_deinit()
assert match,report
assert exact,report
