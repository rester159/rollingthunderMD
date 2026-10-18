"""Lossless four-sample PCM groups with absolute or previous-sample prediction.

The builder chooses the smaller encoding per clip. Delta codes 0/1/2 mean
unchanged/-1/+1 modulo 256; code 3 carries the original absolute sample.
Both modes retain every sample and use the same 227-cycle Z80 output cadence.
"""
import struct

def pack(raw,delta=False):
    # First 256 samples are preloaded by the 68000. Each subsequent 32 KiB
    # source segment starts with a little-endian group count; bit15 marks last.
    result=bytearray(raw[:256]);groups=[]
    previous=raw[255]
    for at in range(256,len(raw),4):
        block=raw[at:at+4].ljust(4,b'\x80')
        mask=0;literals=bytearray()
        for i,x in enumerate(block):
            code=({0:0,255:1,1:2}.get((x-previous)&255,3) if delta else {128:0,127:1,129:2}.get(x,3))
            mask|=code<<(6-i*2)
            if code==3:literals.append(x)
            previous=x
        groups.append(bytes([mask])+literals)
    index=0
    while index<len(groups):
        start=len(result);result+=b'\0\0';count=0
        limit=(start//32768+1)*32768
        while index<len(groups) and count<32767 and len(result)+len(groups[index])<=limit:
            result+=groups[index];index+=1;count+=1
        struct.pack_into('<H',result,start,count|(0x8000 if index==len(groups) else 0))
        if index<len(groups):result+=bytes(limit-len(result))
    return bytes(result)

def unpack(blob,count,delta=False):
    result=bytearray(blob[:256]);at=256
    while True:
        header=struct.unpack_from('<H',blob,at)[0];at+=2
        for _ in range(header&32767):
            mask=blob[at];at+=1
            for shift in (6,4,2,0):
                code=(mask>>shift)&3
                if code==3:result.append(blob[at]);at+=1
                else:result.append((result[-1]+(0,-1,1)[code])&255 if delta else (128,127,129)[code])
        if header&32768:break
        at=(at+32767)&~32767
    assert at==len(blob)
    return bytes(result[:count])

def place(blob,address):
    assert address%32768==0
    return blob

def driver(source):
    from pcm_mask_driver import driver as generate
    return generate(source)
