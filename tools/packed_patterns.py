"""Experimental lossless canonical-palette pose storage; raw artwork is the oracle."""
import hashlib
import struct
import re
from collections import defaultdict

PAGE=0x80000
TAG=0x84000000

def hot_animations(root, rows):
    """Keep frequent movement, firing, jumping and guard-entry poses raw.

    Animation numbers come from the existing player/guard state tables. This
    selection covers every stage, player costume and guard color; it does
    not select individual images from a benchmark.
    """
    names=set()
    for row in rows:
        for filename in row['sources']:
            player=filename.endswith('agent_poses.c')
            guard=bool(re.fullmatch(r'(?:area\d\d_)?enemy_b\d_g\d+_poses.c',filename))
            if not (player or guard):continue
            source=(root/row['folder']/filename).read_text()
            source=source[source.index('const RT_SpritePose '):]
            body=source[:source.index('\n};')]
            records=re.findall(r'^\s*\{([^{}\n]+)\},?$',body,re.M)
            if player:
                assert len(records)==4*79
                selected=[base+i for base in range(0,4*79,79) for i in [*range(0,7),*range(10,14),18,19,65,66,70]]
            else:
                assert len(records)==383
                selected=[*range(0,6),*range(69,86),*range(91,97),*range(204,208)]
                if re.search(r'enemy_b0_',filename):selected.extend([20,*range(124,133)])
            for index in selected:
                pattern=records[index].split(',')[0]
                if pattern!='0':
                    assert pattern.startswith('rtSharedPose_')
                    names.add(pattern)
    return names

def shared_player_animations(root, rows):
    """Keep common player poses raw when all ten terrain palettes share them.

    Also retain guard walk cycles whose exact palette appears in at least
    half the stages. Their pixels need only one ROM copy per costume. Moving these small,
    globally shared animations out of the decoder also leaves the stage-time
    RAM cache available for the palette-specific art.
    """
    stages=defaultdict(set)
    direct=set()
    for row in rows:
        for filename in row['sources']:
            if not filename.endswith('agent_poses.c'):continue
            source=(root/row['folder']/filename).read_text()
            source=source[source.index('const RT_SpritePose '):]
            records=re.findall(r'^\s*\{([^{}\n]+)\},?$',source[:source.index('\n};')],re.M)
            assert len(records)==4*79
            for base in range(0,4*79,79):
                for index in [*range(7),*range(10,14),18,19,65,66,70]:
                    name=records[base+index].split(',')[0]
                    if name.startswith('rtSharedPose_'):
                        stages[name].add(row['stage'])
                        # Standing and airborne player frames are frequent even
                        # when terrain palettes give each stage distinct pixels.
                        if index==19:direct.add(name)
    selected=direct|{name for name,seen in stages.items() if len(seen)==len(rows)}
    guard_stages=defaultdict(set)
    for row in rows:
        for filename in row['sources']:
            if not re.fullmatch(r'(?:area\d\d_)?enemy_b\d_g\d+_poses.c',filename):continue
            source=(root/row['folder']/filename).read_text()
            source=source[source.index('const RT_SpritePose '):]
            records=re.findall(r'^\s*\{([^{}\n]+)\},?$',source[:source.index('\n};')],re.M)
            assert len(records)==383
            for index in range(91,97):
                name=records[index].split(',')[0]
                if name.startswith('rtSharedPose_'):guard_stages[name].add(row['stage'])
    selected.update(name for name,seen in guard_stages.items() if len(seen)*2>=len(rows))
    return selected

class PackedPatterns:
    def __init__(self, pool, offsets, tables, cache, codec="canonical", raw_names=(), fast_names=()):
        self.codec=codec
        if codec in ("fast", "refined"):
            from pose_codec_fast import normalize,encode,decode
        else:
            from pose_codec import normalize,encode,decode
        cache.mkdir(parents=True,exist_ok=True)
        sizes={name:size for table in tables for _,size,name in table if name.startswith('rtSharedPose_')}
        assert sizes.keys()==offsets.keys()
        refined={}
        if codec=="refined":
            from refined_poses import sources
            refined=sources({name:pool[offsets[name]:offsets[name]+sizes[name]]
                             for name in sorted(offsets)},cache)
        fast_sources={hashlib.sha256(refined[name][0]).hexdigest() for name in fast_names if name in refined and name not in raw_names}
        self.records={};self.streams={};self.raw_streams={};self.original_offsets=offsets
        fast_cache=cache.parent/'hot0';fast_cache.mkdir(exist_ok=True)
        palette_lengths={};self.colors={}
        for name in sorted(offsets):
            raw=pool[offsets[name]:offsets[name]+sizes[name]]
            assert hashlib.sha256(raw).hexdigest()==name[13:]
            if name in raw_names:
                identity='raw:'+hashlib.sha256(raw).hexdigest()
                self.raw_streams[identity]=raw
                self.records[name]=(identity,bytes(8),len(raw))
                continue
            canonical,palette=refined[name] if refined else normalize(raw)
            used=1+max(max(byte>>4,byte&15) for byte in canonical)
            palette_lengths[palette]=max(palette_lengths.get(palette,0),(used+1)//2)
            identity=hashlib.sha256(canonical).hexdigest()
            self.colors[identity]=used
            if identity not in self.streams:
                path=(fast_cache if identity in fast_sources else cache)/identity
                packed=path.read_bytes() if path.exists() else encode(canonical,byte_weight=0 if identity in fast_sources else 64) if codec=="refined" else encode(canonical)
                assert decode(packed,len(raw))==canonical
                if not path.exists():path.write_bytes(packed)
                self.streams[identity]=packed
            assert decode(self.streams[identity],len(raw),palette)==raw
            self.records[name]=(identity,palette,len(raw))
        # Only the canonical indices actually present in a pose are read from
        # its palette map. Intern those prefixes, with seven final guard bytes
        # so the unchanged native decoder may always read its eight-byte map.
        prefixes={palette:palette[:length] for palette,length in palette_lengths.items()}
        palette_pool=bytearray();prefix_offsets={}
        palette_stride=2 if codec=='refined' else 1
        for prefix in sorted(set(prefixes.values()),key=lambda value:(-len(value),value)):
            at=palette_pool.find(prefix)
            while at>=0 and at%palette_stride:at=palette_pool.find(prefix,at+1)
            if at<0:
                if len(palette_pool)%palette_stride:palette_pool.append(0)
                at=len(palette_pool);palette_pool+=prefix
            prefix_offsets[prefix]=at//palette_stride
        self.palette_stride=palette_stride
        self.palette_offsets={palette:prefix_offsets[prefix] for palette,prefix in prefixes.items()}
        assert not self.palette_offsets or max(self.palette_offsets.values())<=65535
        palette_pool+=bytes(7)
        if len(palette_pool)&1:palette_pool.append(0)
        self.palettes=bytes(palette_pool)
        self.descriptor_stride=4 if codec=='refined' else 6
        self.descriptor_count=sum(identity not in self.raw_streams for identity,palette,size in self.records.values())
        self.stream_indices={identity:index*4 for index,identity in enumerate(sorted(self.streams))}
        assert len(self.stream_indices)*4<=65536
        self.descriptor_bytes=self.descriptor_stride*self.descriptor_count+(4*len(self.streams) if codec=='refined' else 0)

    def addresses(self, cpu):
        result={};index=0
        for name,(identity,palette,size) in self.records.items():
            if identity in self.raw_streams:result[name]=0x80000000|self.locations[identity]
            else:result[name]=TAG+cpu+index*self.descriptor_stride;index+=1
        return result

    def relocated_original_addresses(self,cpu):
        addresses=self.addresses(cpu)
        return {0x80000000+self.original_offsets[name]:address for name,address in addresses.items()}

    def materialize(self, physical, spare_regions=()):
        blob=bytearray();locations={};padding=0
        self.spare_chunks=[]
        pending=dict(self.streams)
        for start,capacity in spare_regions:
            spare=bytearray()
            for identity,data in sorted(pending.items(),key=lambda item:(-len(item[1]),item[0])):
                at=start+len(spare)
                if len(data)<=capacity-len(spare) and (at&(PAGE-1))+len(data)<=PAGE:
                    locations[identity]=at;spare+=data;del pending[identity]
            if spare:self.spare_chunks.append((start,bytes(spare)))
        # Compressed sources are read bytewise, so identical suffix/prefix
        # bytes can overlap without changing the decoder or its timing. Keep
        # starts distinct (no complete-stream overlap), and retain page bounds.
        ordered=sorted(pending.items())
        prefixes=defaultdict(list)
        for index,(_,data) in enumerate(ordered):
            for length in range(1,min(32,len(data)-1)+1):prefixes[data[:length]].append(index)
        for choices in prefixes.values():choices.reverse()
        remaining_streams=set(range(len(ordered)));overlap_bytes=0
        while remaining_streams:
            index=None;overlap=0
            for length in range(min(32,len(blob)),0,-1):
                choices=prefixes.get(bytes(blob[-length:]))
                if choices:
                    while choices and choices[-1] not in remaining_streams:choices.pop()
                    if choices:index=choices[-1];overlap=length;break
            if index is None:index=min(remaining_streams)
            remaining_streams.remove(index)
            identity,packed=ordered[index]
            at=physical+len(blob)-overlap
            remaining=PAGE-(at&(PAGE-1))
            if len(packed)>remaining:
                # The overlap is unusable at a bank seam. Start the complete
                # stream in the next bank; no source straddles its window.
                gap=PAGE-((physical+len(blob))&(PAGE-1))
                if gap==PAGE:gap=0
                blob+=bytes(gap);padding+=gap;overlap=0;at=physical+len(blob)
            locations[identity]=at
            blob+=packed[overlap:];overlap_bytes+=overlap
        for identity,data in pending.items():
            at=locations[identity]-physical
            assert blob[at:at+len(data)]==data
            assert (locations[identity]& (PAGE-1))+len(data)<=PAGE
        # Whole raw poses can overlap identical prefix/suffix tiles and remain
        # contiguous DMA sources, exactly as in the uncompressed shared layout.
        if self.raw_streams:
            if (physical+len(blob))&1:blob.append(0);padding+=1
            from compact_assets import pattern_pool
            raw_image=bytearray();raw_symbols=[]
            for identity,data in self.raw_streams.items():
                name='rtSharedPose_'+identity[4:]
                raw_symbols.append((0x100000+len(raw_image),len(data),name));raw_image+=data
            raw_pool,raw_offsets=pattern_pool([bytes(raw_image)],[raw_symbols])
            for identity in self.raw_streams:locations[identity]=physical+len(blob)+raw_offsets['rtSharedPose_'+identity[4:]]
            blob+=raw_pool
        self.locations=locations
        descriptors=bytearray()
        for identity,palette,size in self.records.values():
            if identity not in self.raw_streams:
                descriptors+=struct.pack('>HH',self.stream_indices[identity],self.palette_offsets[palette]) if self.codec=='refined' else struct.pack('>IH',locations[identity]|(self.colors[identity]<<24),self.palette_offsets[palette])
        if self.codec=='refined':
            for identity in sorted(self.streams):descriptors+=struct.pack('>I',locations[identity]|(self.colors[identity]<<24))
        assert len(descriptors)==self.descriptor_bytes
        return bytes(blob),bytes(descriptors),dict(
            codec=('refined_palette_byte_lz_fast' if self.codec=='refined' else 'canonical_palette_byte_lz_fast' if self.codec=='fast' else 'canonical_palette_byte_lz'),poses=len(self.records),canonical_streams=len(self.streams),
            stream_bytes=len(blob),descriptor_bytes=len(descriptors),padding_bytes=padding,compressed_overlap_bytes=overlap_bytes,
            fixed_spare_bytes=sum(len(data) for _,data in self.spare_chunks),
            descriptor_stride=self.descriptor_stride,stream_table_offset=self.descriptor_count*self.descriptor_stride if self.codec=='refined' else None,color_count_in_address=True,palette_bytes=len(self.palettes),palette_stride=self.palette_stride,
            physical=physical,raw_pose_streams={str(locations[key]):len(value) for key,value in self.raw_streams.items()},stream_lengths={str(locations[key]):len(value) for key,value in self.streams.items()})
