#!/usr/bin/env python3
"""GX diagnostic calls into the unmodified ROM to load all ten stage banks.

This bypasses progression via the exported CPU register API, not a playthrough.
The pinned GCC 16 build specializes scene initialization by removing its first
(global scene) argument. Check the resulting story/area before using the video
loader. Also exercise relocated high-score graphics/audio through its real entry
function. Function returns park at an existing BRA-to-self instruction in ROM.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
from test_soft_reset import Machine


def probe(args):
    if args.everdrive_model:
        from test_everdrive_model import ProMachine
        m=ProMachine(args)
    else:m = Machine(args)
    symbols = {v[2].split('.lto_priv')[0]: int(v[0],16)
               for line in args.symbols.read_text().splitlines() if len(v:=line.split())==3}
    raw = args.rom.read_bytes()
    parked = next(i for i in range(512,0x80000,2) if raw[i:i+2] == b'\x60\xfe')
    m.core.m68k_get_reg.argtypes = [C.c_int]
    m.core.m68k_get_reg.restype = C.c_uint
    m.core.m68k_set_reg.argtypes = [C.c_int,C.c_uint]
    hashes = []; frames = []
    video_t = C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)
    def video(data,w,h,pitch):
        if data:
            raw=C.string_at(data,h*pitch);hashes.append(hashlib.sha256(raw).hexdigest())
            if args.capture_dir:frames[:]=[(raw,w,h,pitch)]
    callback = video_t(video)
    m.core.retro_set_video_refresh(callback)
    m.start('campaign_'+str(args.players)+'p',args.agent)

    def call(name, *arguments):
        stack = 0xfffc00
        data = struct.pack('>'+str(1+len(arguments))+'I',parked,*arguments)
        for i,value in enumerate(data):
            C.c_ubyte.from_address(m.ram+(((stack+i)&65535)^1)).value=value
        m.core.m68k_set_reg(17,0x2300)
        m.core.m68k_set_reg(18,stack)
        m.core.m68k_set_reg(16,symbols[name])
        for frame in range(240):
            m.run(1)
            if m.core.m68k_get_reg(16)==parked:
                return m.core.m68k_get_reg(0)&255
        raise AssertionError(name+' failed to return')

    stage = args.stage
    result = call('RT_campaignSceneInitialize.constprop.0',symbols['rtSession'],stage//5,stage%5,0,0,6)
    assert result==0, ('stage initialization',stage,result)
    assert m.read('boundStage',2,10)==bytes([stage//5,stage%5]), 'Specialized call ABI/stage mismatch'
    assert call('video')==1, ('stage video',stage)
    m.run(3)
    stage_hash=hashes[-1]
    cached_poses=0
    if args.packed_cache:
        from preloaded_pose_oracle import verify_preloaded_pose_cache
        cached_poses=verify_preloaded_pose_cache(m,raw,json.loads(args.rom.with_name('build.json').read_text()))
    def capture(label):
        if args.capture_dir:
            raw,w,h,pitch=frames[-1]
            args.capture_dir.mkdir(parents=True,exist_ok=True)
            (args.capture_dir/(str(stage)+'-'+label+'.raw')).write_bytes(b''.join(raw[y*pitch:y*pitch+w*2] for y in range(h)))
    capture('stage')
    m.patch('score',(654321).to_bytes(4,'big'),4)
    assert call('beginHighEntry')==1, ('high-score entry',stage)
    m.run(180)
    assert m.number('rtHighStarted',2)==1
    assert m.number('rtAppState',2)!=1
    result=dict(stage=stage,stage_entry_pass=True,high_score_entry_pass=True,
                stage_frame_sha256=stage_hash,high_score_frame_sha256=hashes[-1],
                fault=m.number('rtAppFault',2),exact_cached_poses=cached_poses)
    capture('high-score')
    if args.audio_soak:
        m.run(4500)
        zram=(C.c_ubyte*8192).in_dll(m.core,'zram')
        assert bytes(zram[0x110:0x114])==b'RTPC' and zram[0x114] in (5,8,9,10)
        assert zram[0x101]==3, 'Finite high-score PCM did not complete'
        call('endHighClock')
        assert m.number('rtHighAudioRestored',4)==1, 'PCM did not restore fixed ROM window'
        result['finite_pcm_complete_and_restored']=True
    m.core.retro_unload_game();m.core.retro_deinit()
    print(json.dumps(result))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--players',type=int,choices=[1,2],default=2)
    p.add_argument('--agent',type=int,choices=[0,1],default=0)
    p.add_argument('--packed-cache',action='store_true',help='Verify every native preloaded image against its ROM pixels')
    p.add_argument('--audio-soak',action='store_true')
    p.add_argument('--everdrive-model',action='store_true')
    p.add_argument('--reserve-os-banks',action='store_true')
    p.add_argument('--core',type=Path,required=True)
    p.add_argument('--rom',type=Path,required=True)
    p.add_argument('--symbols',type=Path,required=True)
    p.add_argument('--output',type=Path,default=Path('.build/banked-scenes.json'))
    p.add_argument('--stage',type=int,choices=range(10))
    p.add_argument('--capture-dir',type=Path,help='Optional RGB565 framebuffer rows for visual regression')
    args=p.parse_args()
    if args.reserve_os_banks and not args.everdrive_model:p.error('--reserve-os-banks requires --everdrive-model')
    if args.stage is not None:probe(args);return
    rows=[]
    for stage in range(10):
        command=[sys.executable,str(Path(__file__).resolve()),'--core',str(args.core),
                 '--rom',str(args.rom),'--symbols',str(args.symbols),'--stage',str(stage),
                 '--players',str(args.players),'--agent',str(args.agent)]
        if args.packed_cache:command.append('--packed-cache')
        if args.capture_dir:command.extend(['--capture-dir',str(args.capture_dir)])
        if args.everdrive_model:command.append('--everdrive-model')
        if args.reserve_os_banks:command.append('--reserve-os-banks')
        if args.audio_soak and stage==0:command.append('--audio-soak')
        run=subprocess.run(command,capture_output=True,text=True,timeout=60)
        assert run.returncode==0,run.stdout+run.stderr
        row=next(json.loads(line) for line in run.stdout.splitlines() if line.startswith('{'))
        rows.append(row);print('stage',stage,'PASS',flush=True)
    report=dict(rom_sha256=hashlib.sha256(args.rom.read_bytes()).hexdigest(),cases=rows,
                extended_ssf_software_model=args.everdrive_model,physical_hardware_tested=False,
                scope='Diagnostic CPU-register calls in an unmodified ROM; all stage loaders and high-score entry, not full campaign playthroughs.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
