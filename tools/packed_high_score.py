"""Remove proven duplication from the frozen high-score scene/panel resource.

RTPC v2 stores one shared glyph lookup instead of 336 identical copies, and
references the separately supplied RTGC scene instead of embedding it again.
The glyph records, palettes, pixels and maps remain byte-for-byte unchanged.
"""
import hashlib
import struct


def pack(resource):
    resource=bytes(resource)
    assert resource[:4]==b'RTGC'
    scene_bytes=4176+struct.unpack_from('>H',resource,10)[0]*32
    scene=resource[:scene_bytes]
    panel=resource[scene_bytes:]
    assert panel[:4]==b'RTPC' and struct.unpack_from('>HHH',panel,4)==(1,336,90)
    count=struct.unpack_from('>H',panel,10)[0]
    assert 0<count<=128 and struct.unpack_from('>I',panel,12)[0]==scene_bytes
    assert panel[16:16+scene_bytes]==scene
    lookup=panel[16+scene_bytes:16+scene_bytes+336*90*2]
    row=lookup[:180]
    assert lookup==row*336, 'High-score lookups are no longer identical'
    assert all(value<count for value in struct.unpack('>90H',row))
    records=panel[16+scene_bytes+336*90*2:16+scene_bytes+336*90*2+count*68]
    assert len(records)==count*68
    old_bytes=16+scene_bytes+336*90*2+count*68
    maps=panel[old_bytes:]
    assert len(maps)==4032
    header=bytearray(panel[:16]);struct.pack_into('>H',header,4,2)
    packed=bytes(header)+row+records
    # Reconstruct the entire original resource, including the removed copies.
    restored=panel[:16]+scene+row*336+records
    assert restored==panel[:old_bytes]
    result=scene+packed+maps
    report=dict(raw_bytes=len(resource),packed_bytes=len(result),saved_bytes=len(resource)-len(result),
                scene_bytes=scene_bytes,panel_bytes=len(packed),maps_offset=scene_bytes+len(packed),
                lookup_rows=336,unique_lookup_rows=1,glyph_records=count,
                reference_sha256=hashlib.sha256(resource).hexdigest(),
                restored_sha256=hashlib.sha256(scene+restored+maps).hexdigest())
    return result,report
