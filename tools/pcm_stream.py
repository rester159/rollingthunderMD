"""Lossless four-sample PCM groups with absolute or previous-sample prediction.

The builder chooses the smaller encoding per clip. Delta codes 0/1/2 mean
unchanged/-1/+1 modulo 256; code 3 carries the original absolute sample.
Both modes retain every sample and use the same 227-cycle Z80 output cadence.
"""
import struct
from collections import Counter

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

def pack_phrases(raw):
    """Intern common sequences of absolute four-sample masks, without changing
    samples. Z80 dispatch programs expand each phrase into the existing timed
    pair states. Rare reserved masks use an explicit literal-mask escape.
    """
    groups=[]
    for at in range(256,len(raw),4):
        mask=0;literals=bytearray()
        for value in raw[at:at+4].ljust(4,b'\x80'):
            code={128:0,127:1,129:2}.get(value,3)
            mask=(mask<<2)|code
            if code==3:literals.append(value)
        groups.append((mask,bytes(literals)))
    masks=bytes(mask for mask,literals in groups);counts=Counter(masks)
    grams=Counter(masks[i:i+n] for n in (2,3,4,6,8) for i in range(len(masks)-n+1))
    reserved=sorted(range(256),key=lambda value:(counts[value],value))[:65]
    escape=reserved[-1];phrases={};budget=1532
    for phrase in sorted(grams,key=lambda value:(-(len(value)-1)*grams[value],value)):
        if len(phrases)==64:break
        size=4*len(phrase)+2
        if size<=budget:
            phrases[phrase]=reserved[len(phrases)];budget-=size
    commands=[];at=0
    while at<len(groups):
        phrase=next((masks[at:at+n] for n in (8,6,4,3,2) if len(masks[at:at+n])==n and masks[at:at+n] in phrases),None)
        if phrase:
            command=bytes([phrases[phrase]])+b''.join(lit for mask,lit in groups[at:at+len(phrase)])
            at+=len(phrase)
        else:
            mask,literals=groups[at];command=(bytes([escape,mask]) if mask in reserved else bytes([mask]))+literals;at+=1
        commands.append(command)
    result=bytearray(raw[:256]);index=0
    while index<len(commands):
        start=len(result);result+=b'\0\0';count=0;limit=(start//32768+1)*32768
        while index<len(commands) and count<32767 and len(result)+len(commands[index])<=limit:
            result+=commands[index];index+=1;count+=1
        struct.pack_into('<H',result,start,count|(0x8000 if index==len(commands) else 0))
        if index<len(commands):result+=bytes(limit-len(result))
    dictionary=[[code,*phrase] for phrase,code in phrases.items()]+[[escape]]
    assert unpack(result,len(raw),phrases=dictionary)==raw
    return bytes(result),dictionary

def unpack(blob,count,delta=False,phrases=None):
    dictionary={row[0]:row[1:] for row in phrases or ()}
    result=bytearray(blob[:256]);at=256
    while True:
        header=struct.unpack_from('<H',blob,at)[0];at+=2
        for _ in range(header&32767):
            control=blob[at];at+=1
            masks=dictionary.get(control,[control])
            if not masks:masks=[blob[at]];at+=1
            for mask in masks:
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

def driver(source,linear=False):
    from pcm_mask_driver import driver as generate
    return generate(source,linear=linear)
