#!/usr/bin/env python3
"""Execute the Pro ROM in GX with a software model of its extended-SSF bus.

GX does not implement the cartridge's write gate/banked battery RAM. This
adapter models the documented register rules and logs unsafe ROM writes. It
is a protocol regression test, NOT physical-console or FPGA certification.
Reference: krikzz/mega-ed-pub fpga/mapper/map_ssf/map_ssf.sv.
"""
import argparse
import copy
import ctypes as C
import hashlib
import json
from pathlib import Path
import test_soft_reset as reset_tests


class Map(C.Structure):
    _fields_ = [(name, C.c_void_p) for name in ('base', 'read8', 'read16', 'write8', 'write16')]


class ProMachine(reset_tests.Machine):
    def run(self, frames):
        if not getattr(self, 'model_ready', False):
            return
        super().run(frames)
        assert not self.bus_errors, self.bus_errors[:5]
        assert self.pages[0] == 0, 'Fixed code bank changed'
        assert not self.write_enabled, 'Cartridge write gate left enabled'

    def __init__(self, args):
        core_args = copy.copy(args)
        core_args.sram_load = None
        super().__init__(core_args)
        self.maps = maps = (Map * 256).in_dll(self.core, 'm68k')
        self.rom_base = maps[0].base
        saved = args.sram_load.read_bytes() if getattr(args, 'sram_load', None) else bytes([255])*65536
        assert len(saved) == 65536
        self.bram = C.create_string_buffer(saved, 65536)
        self.pages = list(range(8))
        self.reserve_os_banks = getattr(args,'reserve_os_banks',False)
        self.write_enabled = False
        self.bus_errors = []
        self.controls = []
        self.ram_writes = 0
        write_t = C.CFUNCTYPE(None, C.c_uint, C.c_uint)
        original8, original16 = write_t(maps[0xa1].write8), write_t(maps[0xa1].write16)

        def write_ram(address, value, width):
            page = self.pages[(address >> 19) & 7]
            if not self.write_enabled:
                self.bus_errors.append('Unexpected write while protected: '+hex(address))
                return
            if page != 31:
                self.bus_errors.append('ROM write enabled outside battery bank: '+hex(address))
                return
            if self.reserve_os_banks and (address & 0x7ffff)<0x40000:
                self.bus_errors.append('Write to OS portion of bank 31: '+hex(address))
                return
            offset = address & 65535
            if width == 1:
                C.c_ubyte.from_buffer(self.bram, offset ^ 1).value = value & 255
            else:
                C.c_uint16.from_buffer(self.bram, offset & ~1).value = value & 65535
            self.ram_writes += 1

        self.w8 = write_t(lambda a, d: write_ram(a, d, 1))
        self.w16 = write_t(lambda a, d: write_ram(a, d, 2))

        def map_page(window, page):
            if self.reserve_os_banks and (page==30 or (page==31 and window!=4)):
                self.bus_errors.append('OS bank used outside the save window: '+str((window,page)))
            self.pages[window] = page
            for chunk in range(8):
                entry = maps[window*8+chunk]
                entry.base = (C.addressof(self.bram) if page == 31 else
                              self.rom_base+page*0x80000+chunk*0x10000)
                entry.read8 = entry.read16 = None
                entry.write8 = C.cast(self.w8, C.c_void_p)
                entry.write16 = C.cast(self.w16, C.c_void_p)

        def io(original, address, value, width):
            if 0xa130f0 <= address <= 0xa130ff:
                window = (address >> 1) & 7
                if window == 0:
                    # Protected 16-bit control: bank 0 + write gate.
                    if width == 2 and value & 0x8000:
                        self.controls.append(value)
                        self.write_enabled = bool(value & 0x2000)
                        map_page(0, value & 31)
                else:
                    map_page(window, value & 31)
                return
            original(address, value)

        self.io8 = write_t(lambda a, d: io(original8, a, d, 1))
        self.io16 = write_t(lambda a, d: io(original16, a, d, 2))
        maps[0xa1].write8 = C.cast(self.io8, C.c_void_p)
        maps[0xa1].write16 = C.cast(self.io16, C.c_void_p)
        for window in range(8):
            map_page(window, window)
        original_reset = self.core.retro_reset

        def reset():
            retained = self.pages[:]
            original_reset()
            interrupted = getattr(args, 'interrupt_save_on_reset', False)
            self.write_enabled = interrupted
            for window in range(8):
                map_page(window, (31 if window == 4 else retained[window]) if interrupted else window)

        self.core.retro_reset = reset
        ProMachine.last_instance = self
        self.model_ready = True
        self.run(600)
        assert self.number('rtSaveAvailable') == 1, 'Banked SRAM probe failed'

    def sram(self):
        return bytes(self.bram)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', required=True, type=Path)
    parser.add_argument('--rom', required=True, type=Path)
    parser.add_argument('--symbols', required=True, type=Path)
    parser.add_argument('--interrupt-save-on-reset', action='store_true')
    parser.add_argument('--reserve-os-banks', action='store_true',help='Require the 15 MiB v2/X7 layout and protect OS banks 30-31')
    parser.add_argument('--case', default='campaign_2p', choices=['arcade','campaign_1p','campaign_2p','survival_1p','survival_2p'])
    args = parser.parse_args()
    assert args.rom.read_bytes()[0x100:0x108] == b'SEGA SSF'
    assert len(args.rom.read_bytes()) <= 31*0x80000
    if args.reserve_os_banks:assert len(args.rom.read_bytes()) <= 30*0x80000
    reset_tests.Machine = ProMachine
    reset_tests.check(args)
    machine = ProMachine.last_instance
    assert set(machine.controls) == {0x8000, 0xa000}
    assert machine.ram_writes > 0
    print(json.dumps(dict(protocol_model_passed=True, physical_hardware_tested=False,
                         cartridge_control_writes=len(machine.controls), battery_ram_writes=machine.ram_writes,
                         unsafe_writes=len(machine.bus_errors), interrupted_save_reset=args.interrupt_save_on_reset,
                         sha256=hashlib.sha256(args.rom.read_bytes()).hexdigest())))


if __name__ == '__main__':
    main()
