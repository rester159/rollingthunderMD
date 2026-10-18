#!/usr/bin/env python3
"""Exact PCM block decoding using the smaller of LZ32 and presence masks.

This measures decompression only. It does not replace the Z80 playback driver,
prove its sample cadence, or create a playable cartridge.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from codec import encode, decode
from probe import Diagnostic

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline', required=True, type=Path)
p.add_argument('--core', required=True, type=Path)
p.add_argument('--probe', required=True, type=Path)
p.add_argument('--sdk', required=True, type=Path)
a = p.parse_args()
source = Path(__file__).parent/'pcm_mask.s'
subprocess.run([str(a.sdk/'bin/m68k-elf-as'),'-m68000',str(source),'-o',str(a.probe/'pcm_mask.o')],check=True)
subprocess.run([str(a.sdk/'bin/m68k-elf-ld'),'-Ttext=0x400','-e','decode',str(a.probe/'pcm_mask.o'),'-o',str(a.probe/'pcm_mask.elf')],check=True)
subprocess.run([str(a.sdk/'bin/m68k-elf-objcopy'),'-O','binary',str(a.probe/'pcm_mask.elf'),str(a.probe/'pcm_mask.bin')],check=True)
code = (a.probe/'pcm_mask.bin').read_bytes()
rom = bytearray((a.probe/'diagnostic.bin').read_bytes())
rom[0x400:0x400+len(code)] = code
a.rom = a.probe/'audio-diagnostic.bin'; a.rom.write_bytes(rom)
a.symbols = a.probe/'diagnostic.symbols.txt'
m = Diagnostic(a); m.setup(0x400)

def mask_encode(raw):
    result = bytearray()
    for at in range(0,len(raw),8):
        samples = raw[at:at+8]
        mask = sum(1 << (7-i) for i,x in enumerate(samples) if x != 128)
        result.append(mask)
        result.extend(x for x in samples if x != 128)
    return bytes(result)

def mask_decode(packed, size):
    result = bytearray(); at = 0
    for _ in range(size//8):
        mask = packed[at]; at += 1
        for bit in range(7,-1,-1):
            if mask & (1 << bit):
                result.append(packed[at]); at += 1
            else:
                result.append(128)
    assert at == len(packed)
    return bytes(result)

reports = []
fixture = bytes((bit+1 if mask & (1 << (7-bit)) else 128)
                for mask in range(256) for bit in range(8))
packed_fixture = mask_encode(fixture)
assert mask_decode(packed_fixture,len(fixture)) == fixture
m.native(packed_fixture+bytes(len(packed_fixture)&1),fixture,len(packed_fixture))
for name in ['victory','ending','high_cold','high_warm']:
    raw = (a.baseline/f'.build/audio/{name}.pcm').read_bytes()
    blob = bytearray(); rebuilt = bytearray(); costs = []; selected={'lz32':0,'mask':0}
    for at in range(0,len(raw),256):
        block = raw[at:at+256]
        block += bytes([128]) * (256-len(block))
        candidates=[]
        for kind,entry,pack,unpack in [('lz32',0x220,encode,decode),('mask',0x400,mask_encode,mask_decode)]:
            packed=pack(block); result=unpack(packed,256)
            assert result==block
            m.entry=entry
            cycles=m.native(packed+bytes(len(packed)&1),block,len(packed))
            candidates.append((len(packed)+(len(packed)&1),cycles,kind,packed,result))
        _,cycles,kind,packed,result=min(candidates)
        selected[kind]+=1;costs.append(cycles)
        # Two-byte payload length supports a sequential streamer. The clip
        # descriptor retains the exact sample count, excluding final padding.
        blob += struct.pack('>H',len(packed)|(0x8000 if kind=='mask' else 0)) + packed + bytes(len(packed)&1)
        rebuilt += result
    assert rebuilt[:len(raw)] == raw
    # Independently consume the serialized format, including tag and padding.
    verified=bytearray();cursor=0
    while cursor<len(blob):
        header,=struct.unpack_from('>H',blob,cursor);cursor+=2
        size=header & 0x7fff
        payload=blob[cursor:cursor+size]
        unpack=mask_decode if header & 0x8000 else decode
        verified+=unpack(payload,256)
        cursor+=size
        if size & 1:
            assert blob[cursor]==0
            cursor+=1
    assert cursor==len(blob) and verified[:len(raw)]==raw
    ordered = sorted(costs)
    reports.append(dict(clip=name, sample_count=len(raw), packed_bytes=len(blob),
                        native_exact_blocks_per_decoder=len(costs),selected_blocks=selected,
                        sample_sha256=hashlib.sha256(raw).hexdigest(),
                        decode_master_cycles=dict(mean=sum(costs)/len(costs),
                                                  p95=ordered[int(len(ordered)*.95)],max=ordered[-1]),
                        estimated_cpu_percent_at_original_sample_rate=dict(
                            mean=sum(costs)/len(costs)/(256*3405)*100,
                            max=ordered[-1]/(256*3405)*100)))
    (a.probe/f'{name}.pcmpack').write_bytes(blob)
m.core.retro_unload_game(); m.core.retro_deinit()
raw_total=sum(row['sample_count'] for row in reports)
packed_total=sum(row['packed_bytes'] for row in reports)
layout=json.loads((a.baseline/'.build/compact/layout.json').read_text())
report=dict(status='diagnostic_only',codec='lz32_or_pcm_presence_mask',
            decoder_bytes=len(code)+(a.probe/'decode.bin').stat().st_size,clips=reports,
            exhaustive_mask_fixture_pass=True,raw_sample_bytes=raw_total,
            packed_bytes_with_block_lengths=packed_total,saved_bytes=raw_total-packed_total,
            projected_audio_only_used_bytes=layout['used_bytes']-raw_total+packed_total,
            decode_output_buffer_bytes=256,physical_hardware_tested=False,
            limitations=['No Z80 stream delivery, bus contention or output-cadence validation.',
                         'Projection excludes driver changes, clip headers and placement changes.',
                         'Gameplay graphics remain uncompressed in the audio-only projection.',
                         'CPU timing excludes stream headers, transfer and scheduling.'])
(a.probe/'audio-report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
