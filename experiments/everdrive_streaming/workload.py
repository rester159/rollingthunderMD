#!/usr/bin/env python3
"""Observe baseline DMA demand; estimate decoder cost without changing gameplay.

The native per-pose timings are a cost estimate, NOT measured candidate FPS.
Run recorder enabled and disabled and compare guest RAM, video and counters.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tests'))
from test_soft_reset import Machine

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline', type=Path, required=True)
p.add_argument('--core', type=Path, required=True)
p.add_argument('--probe', type=Path, required=True)
p.add_argument('--case', choices=['campaign_1p','campaign_2p','survival_2p'], required=True)
p.add_argument('--frames', type=int, default=1800)
p.add_argument('--trace', action='store_true')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.rom = a.baseline/'dist/RollingThunderMD.bin'
a.symbols = a.baseline/'dist/RollingThunderMD.symbols.txt'
m = Machine(a)
full_symbols = {v[2].split('.lto_priv')[0]: int(v[0],16) for line in a.symbols.read_text().splitlines()
                if len(v:=line.split()) == 3}
watch = C.c_uint.in_dll(m.core, 'rt_dma_watch_pc')
used = C.c_uint.in_dll(m.core, 'rt_dma_used')
events = (C.c_uint * (16384*3)).in_dll(m.core, 'rt_dma_events')
layout = json.loads((a.baseline/'.build/compact/layout.json').read_text())
offsets = json.loads((a.baseline/'.build/compact/pattern_offsets.json').read_text())
rows = json.loads((a.probe/'poses.json').read_text())
lookup = {(0x80000000+layout['pattern_pool']+offsets[r['name']],r['raw']):r for r in rows}
last_video = ''
video_t = C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)
def video(data,w,h,pitch):
    global last_video
    if data:
        last_video = hashlib.sha256(C.string_at(data,h*pitch)).hexdigest()
callback = video_t(video)
m.core.retro_set_video_refresh(callback)
m.start(a.case)
names = ['rtAppUpdates','rtAppPresentations','rtAppFullPresentations',
         'rtAppReusedPresentations','rtAppSkippedPresentations']
before = {name:m.number(name,4) for name in names}

def heap_free():
    at = m.number('heap',4) & 65535
    total, largest, contiguous = 0, 0, 0
    while True:
        assert at < 65535, 'Heap chain outside work RAM'
        size = (C.c_ubyte.from_address(m.ram+(at^1)).value << 8) | C.c_ubyte.from_address(m.ram+((at+1)^1)).value
        if not size:
            return total, largest
        assert (size & ~1) >= 2
        if size & 1:
            contiguous = 0
        else:
            total += size
            contiguous += size
            largest = max(largest, contiguous)
        at += size & ~1

frames = []; unmatched = set()
watch.value = full_symbols['DMA_queueDmaFast'] if a.trace else 0
for frame in range(a.frames):
    update = m.number('rtAppUpdates',4)
    was_playing = m.number('rtAppState',2)==1
    m.buttons = {(0,7),(0,0),(1,7),(1,0)} if (update//180)%3<2 else {(0,0),(1,0)}
    used.value = 0
    m.run(1)
    assert used.value <= 16384, 'Trace overflow'
    raw_bytes = cycles = count = 0
    for i in range(used.value):
        address, length, time = events[i*3:i*3+3]
        if address & 0xfe000000 != 0x80000000:
            continue
        count += 1; raw_bytes += length
        record = lookup.get((address,length))
        if record:
            cycles += record['master_cycles']
        else:
            unmatched.add((address,length))
    free, largest = heap_free()
    frames.append(dict(frame=frame, pose_uploads=count, output_bytes=raw_bytes,
                       estimated_decode_master_cycles=cycles, heap_free=free,
                       largest_heap_free=largest,active=was_playing and m.number('rtAppState',2)==1))
    if m.number('rtAppState',2)==0:
        break
after = {name:m.number(name,4)-before[name] for name in names}
active = [row for row in frames if row['active']]
costs = sorted(row['estimated_decode_master_cycles'] for row in active)
report = dict(case=a.case, trace=a.trace, requested_refreshes=a.frames, refreshes=len(frames),
              active_refreshes=len(active),counters=after,
              final_ram_sha256=hashlib.sha256(C.string_at(m.ram,65536)).hexdigest(),
              final_video_sha256=last_video, final_state=m.number('rtAppState',2),
              faults=m.number('rtAppFault',2),
              source_rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),
              core_sha256=hashlib.sha256(a.core.read_bytes()).hexdigest(),
              pose_uploads=sum(row['pose_uploads'] for row in active),
              max_pose_uploads_per_refresh=max(row['pose_uploads'] for row in active),
              max_pose_output_bytes_per_refresh=max(row['output_bytes'] for row in active),
              minimum_heap_free=min(row['heap_free'] for row in active),
              minimum_largest_heap_free=min(row['largest_heap_free'] for row in active),
              dma_buffer_bytes=m.number('dataBufferSize',2)*2,
              estimated_added_frame_cpu_percent=dict(mean=sum(costs)/len(costs)/896040*100,
                  median=costs[len(costs)//2]/896040*100,
                  p95=costs[int(len(costs)*.95)]/896040*100,max=costs[-1]/896040*100),
              unmatched_uploads=sorted(unmatched),
              limitations=['One opening workload per mode, not a campaign-wide benchmark.',
                           'Stops at menu; cost and heap summaries exclude continue/transition refreshes.',
                           'CPU overhead is summed isolated native decode cost, not candidate FPS.',
                           'Physical-refresh output total is not a DMA buffer high-water measurement.',
                           'No decoder cache, mapper or queue overhead included.'])
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(report,indent=2)+'\n')
a.output.with_suffix('.frames.json').write_text(json.dumps(frames)+'\n')
m.core.retro_unload_game();m.core.retro_deinit()
print(json.dumps(report,indent=2))
