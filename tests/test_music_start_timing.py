#!/usr/bin/env python3
"""Main BGM 1 must retain its opening rhythm after chip initialization.

The extracted source plays the first four melody notes at services 0,7,14,21.
Observe actual YM key-ons through the diagnostic GX core; no ROM/RAM patches.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
from test_soft_reset import Machine

p = argparse.ArgumentParser(description=__doc__)
for name in ('rom', 'symbols', 'core', 'output'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
m = Machine(a)
for button in (5, 3, 5, 5, 3, 5, 5, 3):
    m.press(button)
while m.number('soundTrack') != 6:
    m.press(7)
ram = (C.c_uint8 * 8192).in_dll(m.core, 'zram')
callback_type = C.CFUNCTYPE(None, C.c_uint, C.c_uint, C.c_uint)
pointer = C.c_void_p.in_dll(m.core, 'fm_write')
saved = pointer.value
original = callback_type(saved)
latch = [0, 0]
events = []
frame = 0
wanted_sequence = 0

def write(cycles, address, value):
    port = (address >> 1) & 1
    if not address & 1:
        latch[port] = value
    elif (port == 0 and latch[0] == 0x28 and value & 0xF0
          and value & 7 == 0 and ram[0x1916] == wanted_sequence):
        events.append((frame, ram[0x1F09] | (ram[0x1F0A] << 8)))
    original(cycles, address, value)

callback = callback_type(write)
pointer.value = C.cast(callback, C.c_void_p).value
rows = []
try:
    for restart in range(3):
        m.run(restart * 17)
        wanted_sequence = (ram[0x1F01] + 1) & 255
        events.clear()
        m.buttons.add((0, 3))
        for frame in range(100):
            if frame == 6:
                m.buttons.clear()
            m.run(1)
        assert len(events) >= 4, ('opening notes missing', events)
        first = events[:4]
        intervals = [b[0] - a[0] for a, b in zip(first, first[1:])]
        services = [(b[1] - a[1]) & 65535 for a, b in zip(first, first[1:])]
        assert services == [7, 7, 7], ('source note sequence changed', services)
        # Seven 60.53-Hz music ticks span about seven NTSC video refreshes
        # (six PAL refreshes). Allow scheduling jitter without accepting a
        # one-frame catch-up burst after initialization.
        assert all(5 <= delta <= 10 for delta in intervals), ('rushed opening', intervals)
        rows.append(dict(restart=restart, note_frames=[e[0] for e in first],
                         note_intervals=intervals, music_service_intervals=services))
finally:
    pointer.value = saved
    m.core.retro_unload_game()
    m.core.retro_deinit()
report = dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),
              opening_rhythm_preserved=True, starts=rows,
              scope='Main BGM 1 startup/restart key-on timing; no analog audio or timbre claim.')
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
