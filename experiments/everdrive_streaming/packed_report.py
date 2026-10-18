#!/usr/bin/env python3
"""Collect evidence for the experimental packed cartridge; never mark it released."""
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
a=p.parse_args()
def read(path):return json.loads(path.read_text())
build=read(ROOT/'dist/packed-experiment/build.json');layout=build['compact']
rom=(ROOT/'dist/packed-experiment'/build['artifact']).read_bytes()
assert hashlib.sha256(rom).hexdigest()==build['sha256']
baseline_hash=hashlib.sha256((a.baseline/'dist/RollingThunderMD.bin').read_bytes()).hexdigest()
assert baseline_hash=='283f35e272fe3da63d6b16ec46f8b04ed1db46842a1eac85b978c1e2ce449274'
assert hashlib.sha256((a.baseline/'dist/RollingThunderMD-MegaEverDrivePro.bin').read_bytes()).hexdigest()=='0e1a62c8b159d5449eb8edb977156a54144d6a1bd49d40bb7f32e9dac2bfc2b2'
scenes=read(ROOT/'.build/packed-final-scenes.json')
reference=read(a.baseline/'.build/release-candidate/scenes-standard.json')
assert scenes['rom_sha256']==build['sha256']
for actual,expected in zip(scenes['cases'],reference['cases']):
 assert actual['stage_frame_sha256']==expected['stage_frame_sha256']
 assert actual['high_score_frame_sha256']==expected['high_score_frame_sha256']
assert len(scenes['cases'])==10
resets=read(ROOT/'.build/packed-final-resets.json');assert resets['rom_sha256']==build['sha256']
audio=[]
for clip,address in layout['audio_addresses'].items():
 raw=(a.baseline/'.build/audio'/f'{clip}.pcm').read_bytes()
 assert rom[address:address+len(raw)]==raw
 audio.append(dict(clip=clip,bytes=len(raw),exact=True,sha256=hashlib.sha256(raw).hexdigest()))
performance=[]
for mode in ['campaign_1p','campaign_2p','survival_2p']:
 before=read(ROOT/f'.build/shared-perf-{mode}-baseline.json')
 after=read(ROOT/f'.build/packed-final-perf-{mode}.json')
 assert before['rom_sha256']==baseline_hash and after['rom_sha256']==build['sha256']
 assert after['fault']==0 and after['packed_graphics']['rtPackedBufferFailures']==0
 performance.append(dict(mode=mode,baseline=before,experiment=after))
audio_probe=read(ROOT/'.build/streaming-probe/audio-report.json')
# Victory occupies fixed spare space below the completion assets; reducing it
# alone does not move the remaining cartridge data. Do not count that saving.
replaceable=[r for r in audio_probe['clips'] if r['clip']!='victory']
saving=sum(r['sample_count']-r['packed_bytes'] for r in replaceable)
projection=layout['used_bytes']-saving
packed=dict(layout['packed_patterns']);packed.pop('stream_lengths')
report=dict(status='experimental_not_release_qualified',rom_sha256=build['sha256'],
 rom_bytes=build['bytes'],used_bytes=layout['used_bytes'],fixed_span=build['fixed_span'],
 graphics=packed,shared_metadata=layout['shared_metadata'],verified_art=layout['verified_art'],
 native_graphics=read(ROOT/'.build/fast-canonical-probe/report.json'),
 native_core_parity=read(ROOT/'.build/fast-canonical-probe/core-parity.json'),
 original_accepted_standard_sha256=baseline_hash,original_accepted_roms_unchanged=True,
 audio_byte_checks=audio,scenes=scenes,scene_images_exact_against_baseline=20,
 soft_resets=resets,performance=performance,
 storage_projection=dict(target_bytes=7*1024**2,existing_variable_audio_savings=saving,
   projected_used_bytes=projection,headroom_bytes=7*1024**2-projection,
   fixed_victory_savings_excluded=True,playable_7_mib_rom=False,
   limitations=['Audio compression is a native-tested prototype, not integrated playback.',
    'Projection excludes new streaming code/data and revised placement/alignment.',
    'Audio sample cadence, delivery, mailbox and mapper ownership need validation.']),
 campaign_performance_accepted=False,physical_hardware_tested=False,
 rejected_experiment='Earlier decoding with a shared circular DMA arena did not improve pacing; all ring changes were removed.',
 next_work=['Integrate exact finite-PCM streaming while retaining every sample and original cadence.',
   'Reduce the Campaign 1P decoder cost and restore Campaign 2P presentation rate.',
   'Check the complete 7 MiB layout, queue lifetimes, broad gameplay and cartridge mapper/save profiles.'])
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['rom_sha256','rom_bytes','used_bytes','storage_projection']},indent=2))
