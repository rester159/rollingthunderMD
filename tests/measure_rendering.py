#!/usr/bin/env python3
"""Deterministic opening-game pacing sample; rates use emulated refreshes.

This is one workload, not a campaign-wide benchmark. Inputs follow simulation
updates so two builds are not given different input timing when one drops draws.
"""
import argparse
import hashlib
import json
from pathlib import Path
from test_soft_reset import Machine

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--core',type=Path,required=True)
p.add_argument('--rom',type=Path,required=True)
p.add_argument('--symbols',type=Path,required=True)
p.add_argument('--case',choices=['campaign_1p','campaign_2p','survival_2p'],default='campaign_2p')
p.add_argument('--align-update',type=int,default=0,help='Begin measurement at this simulation update, after menu/loading differences')
p.add_argument('--agent',type=int,choices=[0,1],default=0)
p.add_argument('--input-pattern',choices=['opening','jump'],default='opening')
p.add_argument('--stage',type=int,choices=range(10),default=None,help='Diagnostic entry through the production stage loader')
p.add_argument('--refresh-phase',type=int,choices=range(256),default=None,help='Align enemy decision phase after diagnostic stage entry')
p.add_argument('--frames',type=int,default=1800)
p.add_argument('--updates',type=int,default=0,help='Measure a fixed number of simulation updates, with a bounded refresh timeout')
p.add_argument('--output',type=Path,required=True)
p.add_argument('--everdrive-model',action='store_true')
p.add_argument('--reserve-os-banks',action='store_true')
a=p.parse_args()
if a.frames<1 or a.updates<0:p.error('--frames must be positive and --updates nonnegative')
if a.refresh_phase is not None and a.stage is None:p.error('--refresh-phase requires --stage')
rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest()
symbol_sha256=hashlib.sha256(a.symbols.read_bytes()).hexdigest()
if a.reserve_os_banks and not a.everdrive_model:p.error('--reserve-os-banks requires --everdrive-model')
if a.everdrive_model:
    from test_everdrive_model import ProMachine
    m=ProMachine(a)
else:m=Machine(a)
assert m.core.retro_get_region()==0, 'This benchmark expects an NTSC ROM/region'
m.start(a.case,a.agent)
if a.stage is not None:
    assert a.case.startswith('campaign')
    from stage_entry_probe import enter_campaign_stage
    enter_campaign_stage(m,a.rom,a.symbols,a.stage,a.refresh_phase)
if a.align_update:
    assert m.number('rtAppUpdates',4)<=a.align_update
    for _ in range(a.align_update*3):
        if m.number('rtAppUpdates',4)>=a.align_update:break
        m.run(1)
    assert m.number('rtAppUpdates',4)==a.align_update
names=['rtAppUpdates','rtAppPresentations','rtAppFullPresentations','rtAppReusedPresentations','rtAppSkippedPresentations']
def snapshot():return {name:m.number(name,4) for name in names}
before=snapshot();last=before['rtAppUpdates'];stalls=compact=0;longest=run=0
play_frames=play_updates=play_presentations=0
refreshes=0
for frame in range(max(a.frames,a.updates*5+600) if a.updates else a.frames):
    if a.updates and m.number('rtAppUpdates',4)-before['rtAppUpdates']>=a.updates:break
    refreshes+=1
    update=m.number('rtAppUpdates',4)
    m.buttons={(0,7),(0,0),(1,7),(1,0)} if (update//180)%3<2 else {(0,0),(1,0)}
    if a.input_pattern=='jump' and update%90<4:m.buttons|={(0,1),(1,1)}  # RetroPad Y maps to Genesis A (jump)
    state_before=m.number('rtAppState',2);presented=m.number('rtAppPresentations',4)
    m.run(1)
    current=m.number('rtAppUpdates',4)
    if state_before==1 and m.number('rtAppState',2)==1:
        play_frames+=1;play_updates+=current-last;play_presentations+=m.number('rtAppPresentations',4)-presented
    if current==last:stalls+=1;run+=1;longest=max(longest,run)
    else:run=0
    last=current;compact+=int(m.number('rtCoopCompact')!=0)
after=snapshot();delta={name:after[name]-before[name] for name in names}
# The installed NTSC cores use approximately 59.9227 refreshes per second.
seconds=refreshes/59.922743
if a.updates:assert delta['rtAppUpdates']>=a.updates, ('Simulation update target was not reached',delta,m.number('rtAppState',2),m.number('rtAppFault',2),m.number('rtHudFault') if 'rtHudFault' in m.symbols else None)
assert hashlib.sha256(a.rom.read_bytes()).hexdigest()==rom_sha256, 'ROM changed during measurement'
assert hashlib.sha256(a.symbols.read_bytes()).hexdigest()==symbol_sha256, 'Symbols changed during measurement'
report=dict(rom_sha256=rom_sha256,case=a.case,agent=a.agent,input_pattern=a.input_pattern,
            extended_ssf_software_model=a.everdrive_model,reserved_os_bank_guard=a.reserve_os_banks,
            stage=a.stage,refresh_phase=a.refresh_phase,refreshes=refreshes,target_updates=a.updates,seconds=seconds,play_refreshes=play_frames,play_updates=play_updates,play_presentations=play_presentations,initial_counters=before,aligned_start_update=a.align_update,counters=delta,
            simulation_updates_per_second=delta['rtAppUpdates']/seconds,
            presentations_per_second=delta['rtAppPresentations']/seconds,
            stalled_refreshes=stalls,longest_stall_refreshes=longest,compact_refreshes=compact,
            final_state=m.number('rtAppState',2),fault=m.number('rtAppFault',2),
            scope='One NTSC movement/shooting workload at the selected stage; emulated timing, not host execution speed or a full-game playthrough.')
report['packed_graphics']={name:m.number(name,2) for name in ('rtPackedBufferHighWater','rtPackedBufferFailures','rtPackedBufferWaits') if name in m.symbols}
assert not report['packed_graphics'].get('rtPackedBufferFailures',0), 'Compressed graphics exhausted DMA staging'
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
m.core.retro_unload_game();m.core.retro_deinit()
