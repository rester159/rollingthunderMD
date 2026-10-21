#!/usr/bin/env python3
"""Measure sound-test output peaks, including each cue's initial attack.

Clipped emulator samples are a lead, not proof of analog hardware distortion.
Core mixing settings and console revisions can produce different results.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path

import numpy as np
from test_soft_reset import Machine

p = argparse.ArgumentParser(description=__doc__)
for name in ("rom", "symbols", "core", "output"):
    p.add_argument("--" + name, type=Path, required=True)
p.add_argument("--tracks", type=int, nargs="+", default=list(range(1, 14)))
p.add_argument("--frames", type=int, default=1200)
p.add_argument("--restarts", type=int, default=1)
p.add_argument("--psg-muted", action="store_true")
a = p.parse_args()
assert a.frames > 0 and a.restarts > 0 and all(1 <= t <= 13 for t in a.tracks)
rom_hash = hashlib.sha256(a.rom.read_bytes()).hexdigest()
m = Machine(a)
for button in (5, 3, 5, 5, 3, 5, 5, 3):
    m.press(button)
assert m.number("page") == 8
if a.psg_muted:
    m.press(8)

class Geometry(C.Structure):
    _fields_ = [(name, C.c_uint) for name in
                ("base_width", "base_height", "max_width", "max_height")] + [
                    ("aspect_ratio", C.c_float)]

class Timing(C.Structure):
    _fields_ = [("fps", C.c_double), ("sample_rate", C.c_double)]

class AV(C.Structure):
    _fields_ = [("geometry", Geometry), ("timing", Timing)]

av = AV()
m.core.retro_get_system_av_info(C.byref(av))
chunks = []
zram = (C.c_uint8 * 8192).in_dll(m.core, "zram")
def services():
    return zram[0x1F09] | (zram[0x1F0A] << 8)
callback_type = C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)

def audio(data, count):
    chunks.append(np.frombuffer(C.string_at(data, count * 4), dtype="<i2").copy())
    return count

callback = callback_type(audio)
m.core.retro_set_audio_sample_batch(callback)
rows = []
for track in a.tracks:
    while m.number("soundTrack") != track:
        m.press(7)
    for repeat in range(a.restarts):
        chunks.clear()
        before = services()
        m.press(3)
        steady_before = services()
        m.run(a.frames)
        samples = np.concatenate(chunks).reshape(-1, 2)
        values = samples.astype(float)
        rows.append(dict(track=track, restart=repeat, stereo_samples=len(samples),
                         peak=int(np.abs(values).max()),
                         clipped_channel_samples=int(np.sum((samples == 32767) | (samples == -32768))),
                         music_services=(services() - before) & 65535,
                         steady_music_services=(services() - steady_before) & 65535,
                         pending_music_services=zram[0x19AC],
                         rms=float(np.sqrt(np.mean(values * values)))))
assert hashlib.sha256(a.rom.read_bytes()).hexdigest() == rom_hash
report = dict(
    rom_sha256=rom_hash, core_sha256=hashlib.sha256(a.core.read_bytes()).hexdigest(),
    sample_rate=av.timing.sample_rate, refresh_rate=av.timing.fps,
    psg_muted=a.psg_muted, additional_capture_refreshes=a.frames,
    initial_attack_included=True, tracks=rows,
    limits="Emulator digital output only; not analog-console or arcade-fidelity validation.")
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))
m.core.retro_unload_game()
m.core.retro_deinit()
