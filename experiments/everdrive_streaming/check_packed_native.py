#!/usr/bin/env python3
"""Run every assembled compressed pose through the actual 68000 decoder.

Reads the final ROM's stream and shared palette prefix, then compares native
output and canaries to the accepted frozen pixels. The ROM itself is unchanged.
"""
import argparse,hashlib,json,statistics,struct,subprocess
from pathlib import Path
from types import SimpleNamespace
from probe import Diagnostic,poses
p=argparse.ArgumentParser(description=__doc__)
for flag in ['rom','layout','core','sdk','baseline','diagnostic','stages','output']:p.add_argument('--'+flag,type=Path,required=True)
a=p.parse_args();rom=a.rom.read_bytes();layout=json.loads(a.layout.read_text())['compact'];packed=layout['packed_patterns'];shared=layout['shared_metadata']
addresses={}
for elf in sorted(a.stages.glob('*/bank.elf')):
 for line in subprocess.check_output([str(a.sdk/'bin/m68k-elf-nm'),str(elf)],text=True).splitlines():
  v=line.split()
  if len(v)==3 and v[2].startswith('rtSharedPose_'):
   address=int(v[0],16)
   assert v[2] not in addresses or addresses[v[2]]==address
   addresses[v[2]]=address
# Verify the independently invoked decoder is the exact linked game routine.
diag_symbols={v[2]:int(v[0],16) for line in subprocess.check_output([str(a.sdk/'bin/m68k-elf-nm'),str(a.diagnostic/'decode.elf')],text=True).splitlines() if len(v:=line.split())==3}
game_symbols={v[2]:int(v[0],16) for line in a.rom.with_suffix('.symbols.txt').read_text().splitlines() if len(v:=line.split())==3}
decoder=(a.diagnostic/'decode.bin').read_bytes()[diag_symbols['RT_decodeCanonicalPose']-diag_symbols['decode']:]
entry=game_symbols['RT_decodeCanonicalPose']
assert rom[entry:entry+len(decoder)]==decoder, 'Production decoder differs from native diagnostic'
reference,_=poses(a.baseline,a.sdk)
m=Diagnostic(SimpleNamespace(core=a.core,rom=a.diagnostic/'diagnostic.bin',symbols=a.diagnostic/'diagnostic.symbols.txt'));m.setup(0x220)
cycles=[];raw_count=0
for name,raw in reference.items():
 address=addresses[name]
 if address&0xfe000000!=0x84000000:
  assert rom[address&0x1ffffff:(address&0x1ffffff)+len(raw)]==raw
  raw_count+=1;continue
 at=shared['physical']+(address&0xffffff)-shared['cpu']
 if 'stream_table_cpu' in packed:
  stream_index,index=struct.unpack_from('>HH',rom,at)
  stream=struct.unpack_from('>I',rom,shared['physical']+packed['stream_table_cpu']+stream_index-shared['cpu'])[0]
 else:stream,index=struct.unpack_from('>IH',rom,at)
 colors=stream>>24;stream&=0xffffff
 assert 1<=colors<=16
 palette_at=shared['physical']+packed['palette_cpu']+index*packed.get('palette_stride',1)-shared['cpu']
 palette=rom[palette_at:palette_at+8]
 length=packed['stream_lengths'][str(stream)];payload=palette+rom[stream:stream+length]
 cycles.append(m.native(payload+bytes(len(payload)&1),raw,len(payload),colors=colors))
m.core.retro_unload_game();m.core.retro_deinit()
report=dict(rom_sha256=hashlib.sha256(rom).hexdigest(),core_sha256=hashlib.sha256(a.core.read_bytes()).hexdigest(),production_decoder_matches_diagnostic=True,compressed_native_exact=len(cycles),raw_exact=raw_count,total_exact=len(reference),native_master_cycles=dict(min=min(cycles),median=statistics.median(cycles),max=max(cycles)),physical_hardware_tested=False,scope='Every final linked compressed pose with its actual shared palette prefix; isolated native decoding with output canaries and preserved registers. Raw poses checked byte-for-byte.')
a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
