#!/usr/bin/env python3
"""Verify palette-normalized sprite streams with the actual 68000 decoder.

This is a standalone diagnostic, not a gameplay/mapper integration test.
It writes a deduplicated pool and an exact per-pose reconstruction manifest.
"""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from probe import Diagnostic,poses
from canonical_codec import normalize,encode,decode

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline',required=True,type=Path)
p.add_argument('--sdk',required=True,type=Path)
p.add_argument('--core',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
p.add_argument('--format',choices=['canonical','fast'],default='canonical')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=Path(__file__).parent
if a.format=='fast':
 from fast_codec import normalize,encode,decode
decoder=source/('fast_decode.s' if a.format=='fast' else 'canonical_decode.s')
for command in [
 [a.sdk/'bin/m68k-elf-as','-m68000',decoder,'-o',a.output/'decode.o'],
 [a.sdk/'bin/m68k-elf-ld','-Ttext=0x220','-e','decode',a.output/'decode.o','-o',a.output/'decode.elf'],
 [a.sdk/'bin/m68k-elf-objcopy','-O','binary',a.output/'decode.elf',a.output/'decode.bin']]:
 subprocess.run(command,check=True)
code=(a.output/'decode.bin').read_bytes()
rom=bytearray(0x20000);struct.pack_into('>II',rom,0,0xffff00,0x200)
rom[0x100:0x110]=b'SEGA MEGA DRIVE ';rom[0x1f0:0x1f3]=b'USA'
struct.pack_into('>II',rom,0x1a0,0,len(rom)-1)
rom[0x200:0x202]=bytes.fromhex('60fe');rom[0x220:0x220+len(code)]=code
a.rom=a.output/'diagnostic.bin';a.rom.write_bytes(rom)
a.symbols=a.output/'diagnostic.symbols.txt';a.symbols.write_text('00ff0000 B rtAppFault\n')
m=Diagnostic(a);m.setup(0x220)
records,pool_size=poses(a.baseline,a.sdk)
cache=a.output/'encoded';cache.mkdir(exist_ok=True)
unique={};rows=[];blob=bytearray()
for i,(name,raw) in enumerate(records.items()):
 canonical,palette=normalize(raw)
 identity=hashlib.sha256(canonical).hexdigest()
 if identity not in unique:
  cached=cache/identity
  packed=cached.read_bytes() if cached.exists() else encode(canonical)
  assert decode(packed,len(canonical))==canonical
  if not cached.exists():cached.write_bytes(packed)
  unique[identity]=(len(blob),packed)
  blob+=packed+bytes(len(packed)&1)
 offset,packed=unique[identity]
 assert decode(packed,len(raw),palette)==raw
 payload=palette+packed
 cycles=m.native(payload+bytes(len(payload)&1),raw,len(payload),colors=1+max(max(b>>4,b&15) for b in canonical))
 rows.append(dict(name=name,canonical=identity,offset=offset,raw=len(raw),packed=len(packed),
                  palette=palette.hex(),master_cycles=cycles))
 if i%500==0:print(f'{i}/{len(records)} exact native poses; {len(blob)} stream bytes',flush=True)
# Boundary and overlap cases include all possible pixel bytes and a nonidentity
# color permutation, literal limit, maximum distance and maximum match length.
fixtures=[bytes(range(256))*8,b'\xff'*2048,bytes(32),bytes.fromhex('efdfcdbcab9a897867564534231201f0')*100,
          bytes((i*73+i//17)%256 for i in range(1024))*2]
for raw in fixtures:
 canonical,palette=normalize(raw);packed=encode(canonical)
 assert decode(packed,len(raw),palette)==raw
 payload=palette+packed
 m.native(payload+bytes(len(payload)&1),raw,len(payload),colors=1+max(max(b>>4,b&15) for b in canonical))
m.core.retro_unload_game();m.core.retro_deinit()
(a.output/'patterns.pclz').write_bytes(blob)
(a.output/'poses.json').write_text(json.dumps(rows,indent=2)+'\n')
ordered=sorted(row['master_cycles'] for row in rows)
report=dict(status='native_diagnostic_only',format=a.format,poses=len(rows),canonical_streams=len(unique),
    native_exact_checks=len(rows)+len(fixtures),raw_pool_bytes=pool_size,
    packed_stream_bytes=len(blob),descriptor_bytes=len(rows)*12,
    total_graphics_storage_bytes=len(blob)+len(rows)*12,
    decoder_code_bytes=len(code),decoder_stack_bytes=312,output_buffer_max_bytes=max(map(len,records.values())),
    native_master_cycles=dict(min=ordered[0],median=ordered[len(ordered)//2],p95=ordered[int(len(ordered)*.95)],max=ordered[-1]),
    core_sha256=hashlib.sha256(a.core.read_bytes()).hexdigest(),physical_hardware_tested=False,
    limitations=['Not integrated into gameplay or the DMA queue.',
      'No mapping, interrupt, audio or concurrent DMA costs in native timing.',
      'A 12-byte descriptor is a physical stream pointer and an exact 8-byte palette permutation.',
      'Pool placement still needs bank-boundary padding.'])
(a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
