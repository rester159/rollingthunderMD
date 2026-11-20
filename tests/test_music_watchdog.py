#!/usr/bin/env python3
"""Z80 music driver heartbeat and 68000 watchdog.

v0.92 reports: no sound at all on a Genesis Model 3 VA1, and sound on only
some power-ons on an Analogue SG. The whole driver is clocked by YM2612 timer
flags, so a driver/timer start failure is total silence. This test checks:

1. In gameplay the driver's HEART byte ($1F0B) advances ~60 Hz and the
   watchdog never fires on a healthy start (no false reloads).
2. A park held by the 68000 (PROTECT high, as the title raster's long
   RAM-only reservation does) never triggers a reload.
3. A dead Z80 driver is recovered: 'halt' jumps the Z80 into an endless
   loop (its program overwritten), as if it never reached its main loop.
   The watchdog must reload the driver within ~1.5 s and music must resume.
Uses the diagnostic GX core's Z80 RAM export; not an analog/hardware test.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
from test_soft_reset import Machine

HEART, STATOUT, PROTECT = 0x1F0B, 0x1F08, 0x1F30

p = argparse.ArgumentParser(description=__doc__)
for name in ('rom', 'symbols', 'core', 'output'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
m = Machine(a)
ram = (C.c_uint8 * 8192).in_dll(m.core, 'zram')
m.start('campaign_1p')
rows = {}


def heart_rate(frames):
    beats, last = 0, ram[HEART]
    for _ in range(frames):
        m.run(1)
        beats += (ram[HEART] - last) & 255
        last = ram[HEART]
    return beats


try:
    m.run(60)
    rate = heart_rate(120)
    assert 100 <= rate <= 140, ('heartbeat not ~60 Hz', rate)
    assert m.number('rtGameMusicWatchdogReloads', 2) == 0, 'false watchdog reload'
    assert ram[STATOUT] == 1, 'music not sounding'
    rows['healthy'] = dict(heartbeats_per_120_frames=rate, reloads=0)
    # A park held by the 68000 (PROTECT) legitimately stops music ticks, as
    # the title raster's reservation does: it must never trigger a reload.
    before = m.number('rtGameMusicWatchdogReloads', 2)
    ram[PROTECT] = 1
    m.run(120)
    assert m.number('rtGameMusicWatchdogReloads', 2) == before, 'reload during a 68000-held park'
    ram[PROTECT] = 0
    m.run(30)
    rate = heart_rate(120)
    assert 100 <= rate <= 140, ('heartbeat after held park', rate)
    rows['held_park'] = dict(reloads=0, heartbeats_after=rate)
    for case in ('halt',):
        before = m.number('rtGameMusicWatchdogReloads', 2)
        if case == 'park':
            ram[PROTECT] = 1
        else:
            ram[0] = 0x18; ram[1] = 0xFE        # JR $ at 0000
            ram[0x38] = 0x18; ram[0x39] = 0xFE
            # Divert the running program: every main-loop byte becomes JR $.
            for i in range(0x40, 0x1800, 2):
                ram[i] = 0x18; ram[i + 1] = 0xFE
        recovered = None
        for frame in range(120):
            m.run(1)
            if m.number('rtGameMusicWatchdogReloads', 2) > before:
                recovered = frame
                break
        assert recovered is not None, (case, 'watchdog did not reload driver')
        m.run(60)
        rate = heart_rate(120)
        assert 100 <= rate <= 140, (case, 'heartbeat after reload', rate)
        assert ram[STATOUT] == 1, (case, 'music did not resume after reload')
        assert m.number('rtGameMusicWatchdogReloads', 2) == before + 1, (case, 'repeated reloads')
        rows[case] = dict(reload_after_frames=recovered, heartbeats_after=rate)
finally:
    m.core.retro_unload_game()
    m.core.retro_deinit()
report = dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),
              heartbeat_watchdog=True, cases=rows,
              scope='Emulated driver stalls in Z80 RAM; not a physical-console test.')
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
