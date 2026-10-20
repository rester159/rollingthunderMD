"""Compare every populated native pose-cache entry with the exact ROM decoder."""
import ctypes as C
import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from pose_codec_fast import decode


def verify_preloaded_pose_cache(machine, rom, layout):
    workspace=machine.number('workspace',4)
    assert workspace, 'Stage did not activate its pose workspace'
    packed=layout['compact']['packed_patterns']
    extra=machine.number('extra',4) if 'extra' in machine.symbols else 0
    extra_bytes=machine.number('rtPackedExtraBytes',2) if 'rtPackedExtraBytes' in machine.symbols else 0
    def read(address,size):
        return bytes(C.c_ubyte.from_address(machine.ram+(((address+i)&65535)^1)).value for i in range(size))
    regions=[(workspace,12288)]
    if extra:regions.append((extra,extra_bytes))
    if 'borrowed' in machine.symbols:
        for i in range(2):
            address=machine.number('borrowed',4,i*4)
            if address:regions.append((address,machine.number('borrowedWords',2,i*2)*2))
    count=0
    for index in range(64):
        key,pixels,words=struct.unpack('>IIH',machine.read('entries',10,index*10))
        if not key:continue
        assert key&0xfe000000==0x84000000
        assert any(start<=pixels and pixels+words*2<=start+length for start,length in regions)
        if 'stream_table_cpu' in packed:
            stream_index,palette_index=struct.unpack_from('>HH',rom,key&0xffffff)
            encoded=struct.unpack_from('>I',rom,packed['stream_table_cpu']+stream_index)[0]
        else:encoded,palette_index=struct.unpack_from('>IH',rom,key&0xffffff)
        source=encoded&0xffffff
        length=packed['stream_lengths'][str(source)]
        palette=packed['palette_cpu']+palette_index*packed.get('palette_stride',1)
        expected=decode(rom[source:source+length],words*2,rom[palette:palette+8])
        assert read(pixels,words*2)==expected, ('Cached pixels differ',index,hex(key))
        count+=1
    # A mode whose startup poses are all direct ROM transfers legitimately
    # leaves the decode cache empty. Validate every populated entry above.
    return count
