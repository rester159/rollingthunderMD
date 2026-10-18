#!/usr/bin/env python3
"""Validate paired workload controls and retain a compact feasibility report."""
import argparse
import hashlib
import json
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe',type=Path,required=True)
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
load=lambda name:json.loads((a.probe/name).read_text())
graphics=load('report.json');audio=load('audio-report.json')
rom=(a.baseline/'dist/RollingThunderMD.bin').read_bytes()
sha=hashlib.sha256(rom).hexdigest()
assert sha==graphics['baseline_sha256'], 'Baseline changed during experiment'
workloads=[]
for name in ['campaign_1p','campaign_2p','survival_2p']:
    trace=load(name+'-trace.json');control=load(name+'-control.json')
    for key in ['source_rom_sha256','core_sha256','refreshes','active_refreshes',
                'counters','final_ram_sha256','final_video_sha256','final_state','faults']:
        assert trace[key]==control[key], (name,'Recorder changed guest behavior',key)
    assert trace['faults']==0 and not trace['unmatched_uploads']
    assert trace['source_rom_sha256']==sha
    trace['recorder_control_exact_match']=True
    workloads.append(trace)
combined=graphics['projected_rom_bytes_before_index_alignment']-audio['saved_bytes']
sources=Path(__file__).parent
report=dict(status='prototype_complete_compatibility_not_qualified',
            accepted_rom_unchanged_sha256=sha,accepted_rom_bytes=len(rom),
            graphics=graphics,audio=audio,workloads=workloads,
            combined_projected_bytes_before_integration_overheads=combined,
            combined_projection_fits_7_mib=combined<=7*1024*1024,
            physical_hardware_tested=False,
            decision='Do not ship either diagnostic as a compatible game ROM. The tested codecs do not meet the 7 MiB target; audio packing is a promising separate path for larger cartridges.',
            remaining=['Integrate PCM delivery with exact original output cadence and without bus/visual regressions.',
                       'Find substantially greater savings for the 7 MiB target without content cuts.',
                       'Measure a playable candidate against the accepted build across stages and busy modes.',
                       'Validate actual mapper, SRAM and ROM limits per cartridge and test hardware revisions.'],
            sources_sha256={x.name:hashlib.sha256(x.read_bytes()).hexdigest()
                            for x in sorted(sources.iterdir()) if x.suffix in ('.s','.py')})
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(accepted_mib=len(rom)/1048576,
                     graphics_projection_mib=graphics['projected_rom_bytes_before_index_alignment']/1048576,
                     audio_only_projection_mib=audio['projected_audio_only_used_bytes']/1048576,
                     combined_projection_mib=combined/1048576,
                     recorder_controls='all three exact matches'),indent=2))
