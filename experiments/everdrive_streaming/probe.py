#!/usr/bin/env python3
"""Build and verify a 68000 streaming diagnostic against every baseline pose.

Uses an existing compact build as read-only input. The diagnostic ROM contains
only a parked reset loop and the decoder. It is NOT a playable game build.
Native timings exclude DMA, bank switching, interrupts and concurrent audio.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from codec import encode, decode

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests'))
from test_soft_reset import Machine


class MemoryMap(C.Structure):
    _fields_ = [(name, C.c_void_p) for name in ('base', 'read8', 'read16', 'write8', 'write16')]


class CPU(C.Structure):
    # Genesis Plus GX core/m68k/m68k.h; validated against exported registers
    # before using cycle counters. No writes to a game ROM or game save data.
    _fields_ = [('map', MemoryMap * 256), ('poll', C.c_uint * 3),
                ('cycles', C.c_int), ('refresh', C.c_int), ('end', C.c_uint),
                ('dar', C.c_uint * 16), ('pc', C.c_uint)]


class Diagnostic(Machine):
    def run(self, frames):
        for _ in range(frames):
            self.core.retro_run()

    def setup(self, entry):
        core = self.core
        core.m68k_get_reg.argtypes = [C.c_int]
        core.m68k_get_reg.restype = C.c_uint
        core.m68k_set_reg.argtypes = [C.c_int, C.c_uint]
        core.m68k_run.argtypes = [C.c_uint]
        core.m68k_run.restype = None
        self.cpu = CPU.in_dll(core, 'm68k')
        assert self.cpu.pc == core.m68k_get_reg(16), 'Core CPU layout mismatch'
        core.m68k_set_reg(0, 0x12345678)
        assert self.cpu.dar[0] == 0x12345678, 'Core CPU layout mismatch'
        self.entry = entry
        try:
            self.stop_pc = C.c_uint.in_dll(core, 'rt_diagnostic_stop_pc')
        except ValueError:
            self.stop_pc = None

    def native(self, packed, raw, consumed=None, colors=0):
        core, cpu = self.core, self.cpu
        assert len(packed) <= 65536 and len(raw) < 0x8000
        # The scratch input page is mapped ROM, not work RAM. Change only this
        # standalone diagnostic image between calls, emulating distinct poses.
        swapped = bytearray(packed)
        swapped[0::2], swapped[1::2] = packed[1::2], packed[0::2]
        C.memmove(cpu.map[1].base, bytes(swapped), len(swapped))
        destination, stack, parked = 0xff1000, 0xfff000, 0x200
        C.memset(self.ram + 0x0ff0, 0xa5, len(raw) + 32)
        arguments = struct.pack('>4I', parked, 0x10000, destination, len(raw)|(colors<<16))
        for i, value in enumerate(arguments):
            C.c_ubyte.from_address(self.ram + (((stack+i)&65535)^1)).value = value
        core.m68k_set_reg(17, 0x2700)
        core.m68k_set_reg(18, stack)
        preserved = {register:0x12340000+register for register in list(range(2,8))+list(range(10,15))}
        for register,value in preserved.items():
            core.m68k_set_reg(register,value)
        core.m68k_set_reg(16, self.entry)
        cpu.cycles, cpu.refresh, cpu.end = 0, 896, 0
        instructions = 0
        if self.stop_pc is not None:
            self.stop_pc.value = parked
            core.m68k_run(10000000)
            self.stop_pc.value = 0
            assert cpu.pc == parked, ('Decoder failed to return', hex(cpu.pc))
        else:
            while cpu.pc != parked:
                core.m68k_run(cpu.cycles + 1)
                instructions += 1
                assert instructions < 100000, ('Decoder failed to return', hex(cpu.pc))
        actual = C.string_at(self.ram + 0x1000, len(raw))
        actual = b''.join(actual[i:i+2][::-1] for i in range(0, len(raw), 2))
        assert actual == raw, 'Native decode changed pixels'
        assert core.m68k_get_reg(8) == 0x10000+(len(packed) if consumed is None else consumed)
        assert core.m68k_get_reg(9) == destination+len(raw)
        assert core.m68k_get_reg(18) == stack+4
        assert all(core.m68k_get_reg(register)==value for register,value in preserved.items()), 'C ABI register clobber'
        assert C.string_at(self.ram + 0xff0, 16) == bytes([0xa5])*16
        assert C.string_at(self.ram + 0x1000 + len(raw), 16) == bytes([0xa5])*16
        return cpu.cycles


def poses(baseline, sdk):
    pool = (baseline / '.build/compact/patterns.bin').read_bytes()
    offsets = json.loads((baseline / '.build/compact/pattern_offsets.json').read_text())
    sizes = {}
    for row in json.loads((baseline / 'assets/stages/manifest.json').read_text()):
        elf = baseline / f'.build/stages/{row["stage"]}/bank.elf'
        output = subprocess.check_output([str(sdk/'bin/m68k-elf-nm'), '-S', str(elf)], text=True)
        for line in output.splitlines():
            fields = line.split()
            if len(fields) == 4 and fields[3].startswith('rtSharedPose_'):
                sizes[fields[3]] = int(fields[1], 16)
    assert sizes.keys() == offsets.keys()
    result = {}
    for name in sorted(offsets):
        raw = pool[offsets[name]:offsets[name]+sizes[name]]
        assert hashlib.sha256(raw).hexdigest() == name[13:]
        result[name] = raw
    return result, len(pool)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--sdk', required=True, type=Path)
    parser.add_argument('--core', required=True, type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'.build/streaming-probe')
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).parent
    subprocess.run([str(args.sdk/'bin/m68k-elf-as'), '-m68000', str(source/'decode.s'),
                    '-o', str(out/'decode.o')], check=True)
    subprocess.run([str(args.sdk/'bin/m68k-elf-ld'), '-Ttext=0x220', '-e', 'decode',
                    str(out/'decode.o'), '-o', str(out/'decode.elf')], check=True)
    subprocess.run([str(args.sdk/'bin/m68k-elf-objcopy'), '-O', 'binary',
                    str(out/'decode.elf'), str(out/'decode.bin')], check=True)
    code = (out/'decode.bin').read_bytes()
    rom = bytearray(0x20000)
    struct.pack_into('>II', rom, 0, 0xffff00, 0x200)
    rom[0x100:0x110] = b'SEGA MEGA DRIVE '
    rom[0x1f0:0x1f3] = b'USA'
    struct.pack_into('>II', rom, 0x1a0, 0, len(rom)-1)
    rom[0x200:0x202] = bytes.fromhex('60fe')
    rom[0x220:0x220+len(code)] = code
    args.rom = out/'diagnostic.bin'; args.rom.write_bytes(rom)
    args.symbols = out/'diagnostic.symbols.txt'; args.symbols.write_text('00ff0000 B rtAppFault\n')
    machine = Diagnostic(args); machine.setup(0x220)
    records, raw_pool_size = poses(args.baseline, args.sdk)
    rows = []
    blob = bytearray()
    for index, (name, raw) in enumerate(records.items()):
        packed = encode(raw)
        assert decode(packed, len(raw)) == raw
        cycles = machine.native(packed, raw)
        rows.append(dict(name=name, offset=len(blob), raw=len(raw), packed=len(packed),
                         master_cycles=cycles))
        blob.extend(packed)
        if index % 2000 == 0:
            print(f'{index}/{len(records)} poses verified on the 68000', flush=True)
    # Deliberate boundary fixtures, independent of the artwork corpus.
    fixtures = [b'\x12\x34\x56\x78'*1000,
                b''.join(struct.pack('>I', x) for x in range(1024))*2,
                b'\x00'*4, bytes(range(256))*20]
    for raw in fixtures:
        packed = encode(raw)
        assert decode(packed, len(raw)) == raw
        machine.native(packed, raw)
    machine.core.retro_unload_game(); machine.core.retro_deinit()
    baseline_rom = (args.baseline/'dist/RollingThunderMD.bin').read_bytes()
    layout = json.loads((args.baseline/'.build/compact/layout.json').read_text())
    ordered = sorted(row['master_cycles'] for row in rows)
    report = dict(status='diagnostic_only', poses=len(rows), native_exact_checks=len(rows)+len(fixtures),
                  baseline_sha256=hashlib.sha256(baseline_rom).hexdigest(),
                  core_sha256=hashlib.sha256(args.core.read_bytes()).hexdigest(),
                  decoder_code_bytes=len(code), decoder_stack_bytes=12,
                  maximum_single_pose_output_bytes=max(row['raw'] for row in rows),
                  uncompressed_shared_pool_bytes=raw_pool_size, packed_pose_bytes=len(blob),
                  baseline_before_pool_bytes=layout['pattern_pool'],
                  projected_rom_bytes_before_index_alignment=layout['pattern_pool']+len(blob),
                  native_decode_master_cycles=dict(min=ordered[0], median=ordered[len(ordered)//2],
                                                   p95=ordered[int(len(ordered)*.95)], max=ordered[-1]),
                  ntsc_refresh_master_cycles=896040, physical_hardware_tested=False,
                  limitations=['Independent pose decode, not gameplay integration.',
                               'No DMA, bank switching, interrupts or concurrent audio in timing.',
                               'Frame staging needs the sum of queued outputs, not just one pose.',
                               'Size projection excludes index/alignment and does not compress other ROM data.'])
    (out/'poses.lz32').write_bytes(blob)
    (out/'poses.json').write_text(json.dumps(rows, indent=2)+'\n')
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
