"""Losslessly pack static scene backgrounds; relocate only ELF-proven pointers.

The patched frozen bank remains the independent reference. Animation artwork,
maps, palettes and scripts stay raw. Each 32-tile block decodes independently
before the existing bounded scene-upload window.
"""
import hashlib
import re
import struct
from pose_codec_fast import encode, decode

BASE = 0x100000
TAG = 0x86000000


def pack(root, sdk, reference, objects_dir, output, physical, run):
    reference = bytes(reference)
    output.mkdir(parents=True, exist_ok=True)
    objects = [objects_dir/(name+'.o') for name in
               ('scene_background', 'scene_art', 'transition_art', 'completion_monitor_data')]
    elf = output/'reference-relocations.elf'
    run([sdk/'bin/m68k-elf-ld', '--emit-relocs', '-T', root/'assets/completion/bank.ld',
         *objects, '-o', elf])
    relocations = []
    for line in run([sdk/'bin/m68k-elf-readelf', '-rW', elf]).splitlines():
        if 'R_68K_' not in line:
            continue
        fields = line.split()
        assert fields[2] == 'R_68K_32', line
        relocations.append(int(fields[0], 16)-BASE)
    assert len(relocations) == len(set(relocations)) and relocations

    blocks = []
    for scene in range(6):
        address, maps, palette, tiles, first, count = struct.unpack_from('>IIIHHH', reference, scene*18)
        start = address-BASE
        raw = reference[start:start+tiles*32]
        assert len(raw) == tiles*32
        packed = bytearray()
        for offset in range(0, len(raw), 1024):
            chunk = raw[offset:offset+1024]
            payload = encode(chunk)
            assert decode(payload, len(chunk)) == chunk
            packed += struct.pack('>H', len(payload))+payload+bytes(len(payload)&1)
        blocks.append(dict(start=start, end=start+len(raw), payload=bytes(packed), scene=scene))
    # The frozen footer patch appends a replacement image for scene six. Its
    # original image is dead storage. Use the unpatched link to identify it;
    # mapped() below rejects every surviving pointer/export into this interval.
    unpatched_path=output/'unpatched.bin'
    run([sdk/'bin/m68k-elf-objcopy','-O','binary',elf,unpatched_path])
    unpatched=unpatched_path.read_bytes()
    original,_,_,tiles,_,_=struct.unpack_from('>IIIHHH',unpatched,5*18)
    replacement=struct.unpack_from('>I',reference,5*18)[0]
    dead_bytes=0
    if original!=replacement:
        dead_bytes=tiles*32
        blocks.append(dict(start=original-BASE,end=original-BASE+dead_bytes,payload=b'',scene=None))
    blocks.sort(key=lambda b: b['start'])
    result = bytearray()
    cursor = 0
    for block in blocks:
        assert block['start'] >= cursor
        result += reference[cursor:block['start']]
        block['destination'] = len(result)
        result += block['payload']
        cursor = block['end']
    result += reference[cursor:]

    def mapped(offset, pointer=False):
        saved = 0
        for block in blocks:
            if offset < block['start']:
                break
            if offset < block['end']:
                assert block['payload'] and pointer and offset == block['start'], ('Invalid compacted pointer', hex(offset))
                return (physical+block['destination']) | TAG
            saved += block['end']-block['start']-len(block['payload'])
        return physical+offset-saved

    covered = set()
    for offset in relocations:
        old = struct.unpack_from('>I', reference, offset)[0]
        assert BASE <= old < BASE+len(reference)
        destination = mapped(offset)-physical
        struct.pack_into('>I', result, destination, mapped(old-BASE, True))
        covered.update(range(offset, offset+4))

    # Verify every unchanged byte directly, and every compressed image through
    # an independent decode of the finished bank. No guessed pointer scanning.
    cursor = 0
    for block in blocks+[dict(start=len(reference), end=len(reference))]:
        destination = mapped(cursor)-physical
        for offset in range(cursor, block['start']):
            if offset not in covered:
                assert result[destination+offset-cursor] == reference[offset]
        if block.get('payload'):
            at = block['destination']; restored = bytearray()
            while len(restored) < block['end']-block['start']:
                length = struct.unpack_from('>H', result, at)[0]; at += 2
                raw_length = min(1024, block['end']-block['start']-len(restored))
                restored += decode(result[at:at+length], raw_length)
                at += length+(length&1)
            assert bytes(restored) == reference[block['start']:block['end']]
            assert at == block['destination']+len(block['payload'])
        cursor = block['end']

    exports = {name: mapped(int(address)-BASE) for name, address in
               re.findall(r'\.set (\w+), (\d+)', (root/'src/completion_exports.s').read_text())}
    report = dict(raw_bytes=len(reference), packed_bytes=len(result), saved_bytes=len(reference)-len(result),
                  reference_sha256=hashlib.sha256(reference).hexdigest(),
                  background_bytes=sum(b['end']-b['start'] for b in blocks if b['payload']),dead_background_bytes=dead_bytes,
                  verified_pointer_relocations=len(relocations), exports=exports,
                  patterns=[dict(scene=b['scene'], address=physical+b['destination'],
                                 raw_bytes=b['end']-b['start'], packed_bytes=len(b['payload'])) for b in blocks if b['payload']])
    return bytes(result), exports, report
