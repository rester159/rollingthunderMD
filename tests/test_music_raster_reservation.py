#!/usr/bin/env python3
"""A real DMA call must preserve an already-owned Z80 RAM-playback window."""
import argparse
import ctypes as C
import hashlib
import json
import struct
from pathlib import Path

from test_soft_reset import Machine

p = argparse.ArgumentParser(description=__doc__)
for name in ("rom", "symbols", "core", "output"):
    p.add_argument("--" + name, type=Path, required=True)
a = p.parse_args()
m = Machine(a)
m.start("campaign_1p")
m.patch("rtAppPaused", b"\1")
m.run(3)
ram = (C.c_uint8 * 8192).in_dll(m.core, "zram")
symbols = {row[2].split(".lto_priv")[0]: int(row[0], 16)
           for text in a.symbols.read_text().splitlines() if len(row := text.split()) == 3}
raw = a.rom.read_bytes()
parked = next(i for i in range(512, 0x80000, 2) if raw[i:i+2] == b"\x60\xfe")
m.core.m68k_get_reg.argtypes = [C.c_int]
m.core.m68k_get_reg.restype = C.c_uint
m.core.m68k_set_reg.argtypes = [C.c_int, C.c_uint]

def flush():
    stack = 0xFFFC00
    for i, value in enumerate(struct.pack(">I", parked)):
        C.c_uint8.from_address(m.ram + (((stack + i) & 65535) ^ 1)).value = value
    m.core.m68k_set_reg(17, 0x2300)
    m.core.m68k_set_reg(18, stack)
    m.core.m68k_set_reg(16, symbols["DMA_flushQueue"])
    for _ in range(10):
        m.run(1)
        if m.core.m68k_get_reg(16) == parked:
            return
    raise AssertionError("DMA call did not return")

# Inject only the outer owner's request. The actual Z80 code acknowledges it;
# the real 68000 DMA function performs its normal guard acquisition/release.
ram[0x1F30] = 1
m.run(2)
assert ram[0x1F31] == 1
for _ in range(3):
    flush()
    assert ram[0x1F30] == ram[0x1F31] == 1, "Nested DMA released the raster reservation"
ram[0x1F30] = 0
m.run(2)
assert ram[0x1F31] == 0
flush()
assert ram[0x1F30] == 0, "Unnested DMA left its own reservation active"
report = dict(rom_sha256=hashlib.sha256(raw).hexdigest(),
              nested_dma_calls=3, outer_reservation_preserved=True,
              normal_dma_released=True,
              scope="Actual ROM DMA routine and Z80 handshake; injected outer request, no ROM patches.")
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))
m.core.retro_unload_game()
m.core.retro_deinit()
