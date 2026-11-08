"""Diagnostic stage entry through production loaders, without changing the ROM.

Pause at the main loop, preserve its CPU context/stack, call the existing scene
and video loaders, then resume that loop. Used only by emulated timing tests;
this is not a substitute for a campaign playthrough.
"""
import ctypes as C
import struct


def enter_campaign_stage(machine, rom_path, symbols_path, stage, refresh_phase=None):
    assert 0 <= stage < 10
    m=machine
    symbols={v[2].split('.lto_priv')[0]:int(v[0],16)
             for line in symbols_path.read_text().splitlines() if len(v:=line.split())==3}
    raw=rom_path.read_bytes()
    parked=next(i for i in range(512,0x80000,2) if raw[i:i+2]==b'\x60\xfe')
    m.core.m68k_get_reg.argtypes=[C.c_int];m.core.m68k_get_reg.restype=C.c_uint
    m.core.m68k_set_reg.argtypes=[C.c_int,C.c_uint]
    m.buttons.clear();m.patch('rtAppPaused',b'\x01');m.run(3)
    registers=[m.core.m68k_get_reg(i) for i in range(19)]
    assert registers[18]&65535 >= 0xf600, 'Unexpected main-thread stack range'
    stack_image=C.string_at(m.ram+0xf600,0xa00)
    def call(name,*args):
        stack=0xfffc00
        data=struct.pack('>'+str(1+len(args))+'I',parked,*args)
        for i,value in enumerate(data):
            C.c_ubyte.from_address(m.ram+(((stack+i)&65535)^1)).value=value
        m.core.m68k_set_reg(17,0x2300);m.core.m68k_set_reg(18,stack)
        m.core.m68k_set_reg(16,symbols[name])
        for _ in range(300):
            m.run(1)
            if m.core.m68k_get_reg(16)==parked:return m.core.m68k_get_reg(0)&255
        raise AssertionError(name+' did not return')
    result=call('RT_campaignSceneInitialize.constprop.0',symbols['rtSession'],stage//5,stage%5,0,0,6)
    assert result==0 and m.read('boundStage',2,10)==bytes([stage//5,stage%5])
    assert call('video')==1
    C.memmove(m.ram+0xf600,stack_image,len(stack_image))
    for i in range(16):m.core.m68k_set_reg(i,registers[i])
    m.core.m68k_set_reg(18,registers[18]);m.core.m68k_set_reg(17,registers[17]);m.core.m68k_set_reg(16,registers[16])
    m.patch('rtAppPaused',b'\0');m.patch('rtAppState',b'\0\1')
    m.patch('tick',b'\0');m.patch('rtAppUpdates',bytes(4))
    if refresh_phase is not None:
        # Enemy decisions consume the refresh counter's low byte. Loading cost
        # must not select a different initial enemy workload in comparisons.
        # Advance, never rewind, and reset only the gameplay pacing clocks.
        assert 0 <= refresh_phase < 256
        refresh=((m.number('vtimer',4)+255)&~255)+refresh_phase
        m.patch('vtimer',refresh.to_bytes(4,'big'))
        m.patch('lastRefresh',refresh.to_bytes(4,'big'))
        m.patch('pacer',(refresh-1).to_bytes(4,'big')+b'\0')
    m.run(1)
    assert m.number('rtAppState',2)==1
