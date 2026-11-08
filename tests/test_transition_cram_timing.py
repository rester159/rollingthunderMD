#!/usr/bin/env python3
"""Measure visible-line CRAM writes with the local diagnostic Genesis Plus GX.

Requires the unstripped macOS diagnostic core used by the rendering probes.
The callback observes the core's VDP FIFO schedule without advancing emulated
time. H40 has 3420 master clocks/line, with blanking slots at 352 and 820.
This is an emulator timing check, not physical-console qualification.
"""
import argparse
import ctypes as C
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

from test_soft_reset import Machine

p = argparse.ArgumentParser(description=__doc__)
for name in ("rom", "symbols", "core", "output"):
    p.add_argument("--" + name, type=Path, required=True)
p.add_argument("--selector", type=int, choices=range(1, 10), default=1)
a = p.parse_args()
m = Machine(a)

class Map(C.Structure):
    _fields_ = [(name, C.c_void_p) for name in
                ("base", "read8", "read16", "write8", "write16")]

base = C.addressof((Map * 256).in_dll(m.core, "m68k"))
cycles = C.c_uint.from_address(base + C.sizeof(Map) * 256 + 12)
line = C.c_uint16.in_dll(m.core, "v_counter")
vbase = C.c_uint.in_dll(m.core, "mcycles_vdp")
symbols = {row[2]: int(row[0], 16)
           for text in subprocess.check_output(["nm", "-a", str(a.core)], text=True).splitlines()
           if len(row := text.split()) == 3}
slide = C.addressof(vbase) - symbols["_mcycles_vdp"]
code = C.c_uint8.from_address(slide + symbols["_code"])
addr = C.c_uint16.from_address(slide + symbols["_addr"])
fifo_index = C.c_int.from_address(slide + symbols["_fifo_idx"])
fifo_cycles = (C.c_int * 4).in_dll(m.core, "fifo_cycles")
reg = (C.c_uint8 * 32).in_dll(m.core, "reg")
cram = (C.c_uint16 * 64).in_dll(m.core, "cram")
source = (Path(__file__).resolve().parents[1] / "assets/completion/scene_background.c").read_text()
palettes = {int(n): [int(v, 16) for v in re.findall(r"0x[\da-f]+", text)]
            for n, text in re.findall(r"palettes(\d)\[3\]\[64\]=\{(.*?)\};", source)}
palette_checks, palette_errors = 0, []
function = C.CFUNCTYPE(None, C.c_uint)
pointer = C.c_void_p.in_dll(m.core, "vdp_68k_data_w")
original_address = pointer.value
original = function(original_address)
phase = "title"
counts = {}
bad = []
clock_min, clock_max = 3420, 0
last_body_line, last_footer_line = 0, 0
first_footer_line = 224
previous_body_line, current_body_start, slow_body_start = 0, 0, 0
slow_body_irq = 0

def write(value):
    global clock_min, clock_max, last_body_line, last_footer_line, first_footer_line
    global previous_body_line, current_body_start, slow_body_start
    global slow_body_irq
    global palette_checks
    elapsed = cycles.value - vbase.value
    # Like vdp_hvc_r, account for a CPU instruction crossing the scheduler's
    # current scanline boundary before it advances v_counter.
    actual_line = (line.value + elapsed // 3420) % (313 if m.core.retro_get_region() else 262)
    observe = code.value & 15 == 3 and reg[1] & 64 and actual_line < 224
    address = addr.value
    if observe and actual_line >= 210 and address == 4 and phase in ("title", "sound", "transition"):
        group = (0, 1, 2, 3, 4, 5, 3, 4, 3, 4)[a.selector] if phase == "transition" else 5
        expected = [((v & 0xE00) >> 3) | ((v & 0xE0) >> 2) | ((v & 14) >> 1)
                    for v in palettes[group][64:128]]
        mismatches = [i for i in range(64) if cram[i] != expected[i]]
        palette_checks += 1
        if mismatches and len(palette_errors) < 5:
            palette_errors.append(dict(phase=phase, indices=mismatches))
    original(value)
    if observe:
        # A port write can be in blank while its queued CRAM consumption
        # spills into active pixels. Inspect the slot assigned to this word.
        elapsed = fifo_cycles[(fifo_index.value - 1) & 3] - vbase.value
        actual_line = (line.value + elapsed // 3420) % (313 if m.core.retro_get_region() else 262)
        if actual_line >= 224:
            return
        clock = elapsed % 3420
        counts[phase] = counts.get(phase, 0) + 1
        clock_min, clock_max = min(clock_min, clock), max(clock_max, clock)
        if 100 <= actual_line < 210:
            if not previous_body_line or actual_line < previous_body_line:
                current_body_start = actual_line
            if actual_line > last_body_line:
                slow_body_start = current_body_start
                slow_body_irq = m.number("stats", 2, 4)
            last_body_line = max(last_body_line, actual_line)
            previous_body_line = actual_line
        elif actual_line >= 210:
            first_footer_line = min(first_footer_line, actual_line)
            last_footer_line = max(last_footer_line, actual_line)
        if not 240 <= clock < 860 and len(bad) < 20:
            bad.append(dict(phase=phase, line=actual_line, clock=clock,
                            address=address, value=value))

wrapper = function(write)
pointer.value = C.cast(wrapper, C.c_void_p).value
try:
    m.run(600)
    for button in (5, 3, 5, 5, 3, 5, 5, 3):
        m.press(button)
    assert m.number("page") == 8
    phase = "sound"
    before = m.number("rtTitleMenuRasterRecoveries", 4)
    for i in range(40):
        m.run(i % 11)
        m.press((3, 7, 8, 6)[i % 4])
    assert m.number("rtTitleMenuRasterRecoveries", 4) == before
    phase = "loading"
    m.core.retro_reset()
    m.run(300)
    m.start("campaign_1p")
    # Seed the completed exit walk; use the production transition loader.
    m.patch("scene", b"\1", 8060)
    m.patch("scene", b"\0\x25", 8056)
    x = int.from_bytes(m.read("scene", 2, 8), "big", signed=True) * 16 + 0x1280
    m.patch("scene", struct.pack(">i", x), 20)
    m.patch("scene", bytes([1, 1, 0, 8]), 38)
    m.patch("transitionSelector", bytes([a.selector]))
    seen = False
    for _ in range(1800):
        phase = "transition" if m.number("rtAppState", 2) == 6 else "loading"
        try:
            m.run(1)
        except AssertionError as exc:
            raise AssertionError(dict(app_fault=m.number("rtAppFault", 2),
                                      raster_stats=m.read("stats", 14).hex(),
                                      last_body_line=last_body_line,
                                      last_footer_line=last_footer_line)) from exc
        seen |= m.number("rtAppState", 2) == 6
        if seen and m.number("rtAppState", 2) == 1:
            break
    else:
        raise AssertionError("Transition did not return to gameplay")
    assert all(counts.get(name, 0) > 1000 for name in ("title", "sound", "transition")), counts
    assert not bad, ("Visible CRAM writes outside horizontal blank", bad)
    assert last_body_line < 153, ("Palette arrived after lower sprites", slow_body_irq, slow_body_start, last_body_line)
    assert last_footer_line < 216, ("Palette arrived after footer", last_footer_line)
    assert first_footer_line >= 212, ("Palette arrived before footer split", first_footer_line)
    assert m.number("rtTransitionRasterBusHolds", 4) == 0, "Raster halted audio instead of using RAM playback"
    assert palette_checks > 100 and not palette_errors, ("Lower palette changed", palette_checks, palette_errors)
    report = dict(
        rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),
        core_sha256=hashlib.sha256(a.core.read_bytes()).hexdigest(),
        region="PAL" if m.core.retro_get_region() else "NTSC",
        selector=a.selector, visible_line_cram_writes=counts,
        minimum_clock=clock_min, maximum_clock=clock_max,
        last_body_palette_line=last_body_line, first_footer_palette_line=first_footer_line,
        last_footer_palette_line=last_footer_line,
        active_pixel_start_clock=860, active_cram_writes=0,
        measurement="FIFO consumption slots, not just CPU port-write timestamps",
        sound_actions=40, sound_raster_recoveries=0,
        raster_audio_bus_holds=0,
        complete_lower_palette_checks=palette_checks,
        transition_returned_to_gameplay=True, physical_hardware_tested=False)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))
finally:
    pointer.value = original_address
    m.core.retro_unload_game()
    m.core.retro_deinit()
