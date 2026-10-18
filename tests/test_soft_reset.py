#!/usr/bin/env python3
"""PicoDrive / Genesis Plus GX: real soft resets, no ROM reload or savestates.

Requires a locally built cartridge, matching symbols, and an installed libretro
core. Menu settings are changed by controller. Scores/initials are seeded in
work RAM to exercise saves without completing a campaign. No ROM is modified.
"""
import argparse
import ctypes as C
import hashlib
import json
import subprocess
import sys
from pathlib import Path


class Game(C.Structure):
    _fields_ = [('path', C.c_char_p), ('data', C.c_void_p),
                ('size', C.c_size_t), ('meta', C.c_char_p)]


class Machine:
    def __init__(self, args):
        self.core = core = C.CDLL(str(args.core.resolve()))
        self.symbols = {}
        for line in args.symbols.read_text().splitlines():
            row = line.split()
            if len(row) >= 3:
                self.symbols[row[2].split('.lto_priv')[0]] = int(row[0], 16) & 65535
        self.buttons = set()
        env_t = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
        video_t = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
        audio_t = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
        batch_t = C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)
        poll_t = C.CFUNCTYPE(None)
        input_t = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)

        def environment(command, data):
            if command == 10:
                return C.cast(data, C.POINTER(C.c_int)).contents.value in (0, 1, 2)
            if command in (3, 17):
                C.cast(data, C.POINTER(C.c_bool)).contents.value = command == 3
                return True
            return False

        self.callbacks = [env_t(environment), video_t(lambda *unused: None),
                          audio_t(lambda *unused: None), batch_t(lambda data, count: count),
                          poll_t(lambda: None),
                          input_t(lambda port, device, index, button:
                                  int(device == 1 and (port, button) in self.buttons))]
        names = ['environment', 'video_refresh', 'audio_sample',
                 'audio_sample_batch', 'input_poll', 'input_state']
        for name, callback in zip(names, self.callbacks):
            getattr(core, 'retro_set_' + name)(callback)
        core.retro_init()
        self.rom = C.create_string_buffer(args.rom.read_bytes())
        game = Game(str(args.rom.resolve()).encode(), C.cast(self.rom, C.c_void_p), len(self.rom)-1, None)
        core.retro_load_game.argtypes = [C.POINTER(Game)]
        core.retro_load_game.restype = C.c_bool
        assert core.retro_load_game(C.byref(game)), 'Core rejected ROM'
        for port in (0, 1):
            core.retro_set_controller_port_device(port, 1)
        core.retro_get_memory_data.argtypes = [C.c_uint]
        core.retro_get_memory_data.restype = C.c_void_p
        core.retro_get_memory_size.argtypes = [C.c_uint]
        core.retro_get_memory_size.restype = C.c_size_t
        self.ram = core.retro_get_memory_data(2)
        assert core.retro_get_memory_size(2) == 65536
        if getattr(args, 'sram_load', None):
            saved = args.sram_load.read_bytes()
            assert core.retro_get_memory_size(0) >= len(saved)
            assert core.retro_get_memory_data(0)
            C.memmove(core.retro_get_memory_data(0), saved, len(saved))
        self.run(600)

    def read(self, symbol, length=1, offset=0):
        address = self.symbols[symbol] + offset
        return bytes(C.c_ubyte.from_address(self.ram + ((address+i) ^ 1)).value for i in range(length))

    def number(self, symbol, length=1, offset=0):
        return int.from_bytes(self.read(symbol, length, offset), 'big')

    def patch(self, symbol, data, offset=0):
        address = self.symbols[symbol] + offset
        for i, value in enumerate(data):
            C.c_ubyte.from_address(self.ram + ((address+i) ^ 1)).value = value

    def run(self, frames):
        for _ in range(frames):
            self.core.retro_run()
            assert self.number('rtAppFault', 2) == 0, 'Runtime fault'

    def press(self, button, port=0):
        self.buttons.add((port, button)); self.run(6)
        self.buttons.clear(); self.run(70)

    def sram(self):
        size = self.core.retro_get_memory_size(0)
        assert size >= 128, 'Core exposes less than the two 64-byte save slots'
        return C.string_at(self.core.retro_get_memory_data(0), size)

    def reset(self, best, records):
        before = self.sram()
        self.buttons.clear(); self.core.retro_reset(); self.run(300)
        assert self.number('rtAppState', 2) == 0, 'Reset did not reach menu'
        assert self.number('rtAppPaused') == 0, 'Pause survived reset'
        assert self.number('rtSaveAvailable') == 1, 'Save capability lost on reset'
        assert self.number('rtHomeStartingCredits') == 7
        assert self.number('psgMuted') == 1
        assert self.number('score', 4) == best
        assert self.read('highRecords', 35) == records
        assert self.sram() == before, 'Reset rewrote the committed save'

    def start(self, case, agent=0):
        if case == 'arcade':
            self.press(3)
        else:
            self.press(5); self.press(3)  # Home
            if case.startswith('survival'):
                self.press(5)
            self.press(3)  # agents
            if case.endswith('2p'):
                self.press(3, 1)
            if agent:self.press(7)
            self.press(3)
        self.run(180)
        assert self.number('rtAppState', 2) == 1, 'Game did not restart'
        assert self.number('rtSession', offset=0) == int(case != 'arcade')
        assert self.number('rtSession', offset=1) == int(case.endswith('2p'))
        assert self.number('rtSession', offset=2) == agent, 'Selected player did not start'
        assert self.number('rtSession', offset=5) == int(case.startswith('survival'))
        before = self.number('rtAppUpdates', 4)
        self.run(30)
        assert self.number('rtAppUpdates', 4) > before, 'Pipeline stuck'
        assert self.number('rtGameMusicTrack') != 0, 'Gameplay music did not restart'


def check(args):
    m = Machine(args)
    # Home > Settings: credits 6 -> 7; Sound test: PSG mute on, play music.
    for button in (5, 3, 5, 5, 3, 7, 5, 5, 3, 8, 3):
        m.press(button)
    assert m.number('rtHomeStartingCredits') == 7 and m.number('psgMuted') == 1
    records = m.read('highRecords', 35)
    m.reset(3000, records)  # playing sound test
    m.reset(3000, records)  # repeated reset in title menu
    m.start(args.case)
    records = bytes.fromhex('654321') + records[3:4] + bytes([35, 40, 41]) + records[7:]
    m.patch('score', (654321).to_bytes(4, 'big'))
    m.patch('highRecords', records)
    port = int(args.case.endswith('2p'))
    m.press(3, port)  # commit via production pause path
    assert m.number('rtAppPaused') == 1
    m.reset(654321, records)  # reset while paused, with new committed data
    m.start(args.case)
    # Unsaved live score must not replace the last committed score on reset.
    m.patch('score', (700000).to_bytes(4, 'big'))
    m.reset(654321, records)  # reset with active gameplay/render/audio owners
    m.start(args.case)
    m.patch('score', (765432).to_bytes(4, 'big'))
    records = bytes.fromhex('765432') + records[3:]
    m.patch('highRecords', records)
    m.press(3, port)
    m.reset(765432, records)  # saving and slot rotation still work after reset
    # Settings must remain writable, too.
    for button in (5, 3, 5, 5, 3, 7):
        m.press(button)
    assert m.number('rtHomeStartingCredits') == 8
    m.core.retro_reset(); m.run(300)
    assert m.number('rtHomeStartingCredits') == 8 and m.number('psgMuted') == 1
    assert m.number('score', 4) == 765432 and m.read('highRecords', 35) == records
    m.core.retro_unload_game(); m.core.retro_deinit()
    print(json.dumps(dict(case=args.case,soft_resets=6,passed=True)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--core', required=True, type=Path)
    p.add_argument('--rom', type=Path, default=Path('dist/RollingThunderMD.bin'))
    p.add_argument('--symbols', type=Path, default=Path('.build/game/out/release/symbol.txt'))
    p.add_argument('--output', type=Path, default=Path('.build/soft-reset-tests.json'))
    p.add_argument('--case', choices=['arcade','campaign_1p','campaign_2p','survival_1p','survival_2p'])
    args = p.parse_args()
    if args.case:
        check(args); return
    results = []
    for case in ['arcade','campaign_1p','campaign_2p','survival_1p','survival_2p']:
        cmd = [sys.executable, str(Path(__file__).resolve()), '--core', str(args.core),
               '--rom', str(args.rom), '--symbols', str(args.symbols), '--case', case]
        result = subprocess.run(cmd, text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(case + '\n' + result.stdout + result.stderr)
        row = next(json.loads(line) for line in result.stdout.splitlines() if line.startswith('{'))
        results.append(row); print(row, flush=True)
    report = dict(rom_sha256=hashlib.sha256(args.rom.read_bytes()).hexdigest(),
                  core_sha256=hashlib.sha256(args.core.read_bytes()).hexdigest(),cases=results,
                  scope='Real libretro soft resets; controller menu changes; diagnostic RAM-seeded scores/initials; no savestates or ROM changes.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
