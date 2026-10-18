"""Share immutable geometry and duplicate raw-pixel pose tables across stages.

No decompression. Two stable CPU windows cover the shared metadata; pixel DMA
continues to use window 7. Only pose tables containing tagged pixel references
are eligible, so no stage-local pixel pointer can escape its original mapping.
"""
from collections import defaultdict
import re
import struct


class SharedGeometry:
    def __init__(self, probes, share_world=False):
        geometry = {}
        groups = defaultdict(list)
        raw_groups = defaultdict(list)
        for stage, folder, data, base, table in probes:
            symbols = {name:(address,size) for address,size,name in table}
            local = {}
            for address,size,name in table:
                if name.startswith('rtGeometry_'):
                    raw=data[address-base:address-base+size]
                    assert len(raw)==size
                    if name in geometry:
                        assert geometry[name]==raw
                    geometry[name]=raw;local[address]=name
            names=set()
            for source in folder.glob('*.c'):
                text=source.read_text()
                names.update(re.findall(r'^const RT_SpritePose (\w+)\[',text,re.M))
                raw_names=re.findall(r'^const u32 (\w+Patterns|rtStaticPixels_\w+)\[',text,re.M)
                if share_world:raw_names+=re.findall(r'^const u16 (rtStageWorld\w+|rtSharedMap_\w+|rtPoseIndex_\w+)\[',text,re.M)
                for name in raw_names:
                    if name in symbols:
                        address,size=symbols[name]
                        raw=data[address-base:address-base+size]
                        assert len(raw)==size
                        raw_groups[raw].append((stage,name,size))
            for name in names:
                if name not in symbols:
                    continue
                address,size=symbols[name]
                assert size%12==0
                raw=data[address-base:address-base+size]
                key=[]
                for at in range(0,size,12):
                    pixels,pieces=struct.unpack_from('>II',raw,at)
                    if pixels and not pixels & 0x80000000:
                        break
                    if pieces:
                        assert pieces in local
                    key.append((pixels,local.get(pieces),raw[at+8:at+12]))
                else:
                    groups[tuple(key)].append((stage,name,size))
        self.geometry=bytearray();self.offsets={};self.aliases=defaultdict(dict)
        for name,raw in sorted(geometry.items()):
            self.offsets[name]=len(self.geometry);self.geometry.extend(raw)
        self.tables=[]
        cursor=len(self.geometry)
        for key,aliases in sorted(groups.items(),key=lambda item:sorted(item[1])):
            if len(aliases)<2:
                continue
            self.tables.append((cursor,key))
            for stage,name,size in aliases:
                self.aliases[stage][name]=cursor
            cursor+=len(key)*12
        # Indexed enemy graphics still contain ordinary pose/piece pointers.
        # Share a table only when every target is already in the shared pool;
        # no stage-local pointer may survive a stage-bank switch.
        packed_groups=defaultdict(list)
        if share_world:
            for stage,folder,data,base,table in probes:
                local_geometry={address:name for address,size,name in table if name.startswith('rtGeometry_')}
                local_poses=[(address,size,name) for address,size,name in table
                             if name in self.aliases[stage]]
                for address,size,name in table:
                    if not name.endswith('GraphicsRecords'):continue
                    assert size%8==0
                    records=[]
                    for at in range(address-base,address-base+size,8):
                        pose,pieces=struct.unpack_from('>II',data,at)
                        target=next(((start,label) for start,length,label in local_poses if start<=pose<start+length),None) if pose else None
                        if (pose and target is None) or (pieces and pieces not in local_geometry):break
                        records.append((self.aliases[stage][target[1]]+pose-target[0] if target else None,local_geometry.get(pieces)))
                    else:packed_groups[tuple(records)].append((stage,name))
        self.packed_tables=[]
        for records,aliases in sorted(packed_groups.items(),key=lambda item:sorted(item[1])):
            if len(aliases)<2:continue
            self.packed_tables.append((cursor,records))
            for stage,name in aliases:self.aliases[stage][name]=cursor
            cursor+=len(records)*8
        self.raw_tables=[]
        for raw,aliases in sorted(raw_groups.items(),key=lambda item:sorted(item[1])):
            if len(aliases)<2:
                continue
            self.raw_tables.append((cursor,raw))
            for stage,name,size in aliases:
                self.aliases[stage][name]=cursor
            cursor+=len(raw)
        self.size=cursor
        assert self.size<0x80000, 'Shared metadata needs more than two CPU windows'

    def exports(self, stage, cpu_base):
        return {name:cpu_base+offset for name,offset in
                {**self.offsets,**self.aliases[stage]}.items()}

    def materialize(self, cpu_base, pixel_base, pixel_addresses=None):
        result=bytearray(self.geometry)
        for offset,records in self.tables:
            assert len(result)==offset
            for pixels,pieces,shape in records:
                if pixels:
                    pixels=pixel_addresses[pixels] if pixel_addresses is not None else pixels+pixel_base
                result.extend(struct.pack('>II',pixels,cpu_base+self.offsets[pieces] if pieces else 0)+shape)
        for offset,records in self.packed_tables:
            assert len(result)==offset
            for pose,pieces in records:
                result.extend(struct.pack('>II',cpu_base+pose if pose is not None else 0,
                                          cpu_base+self.offsets[pieces] if pieces else 0))
        for offset,raw in self.raw_tables:
            assert len(result)==offset
            result.extend(raw)
        assert len(result)==self.size
        return bytes(result)
