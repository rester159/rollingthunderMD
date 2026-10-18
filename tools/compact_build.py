"""Build compact, shared and losslessly packed cartridges from frozen banks.

All input banks must first pass build.py's frozen-byte reproduction check.
Derived files stay under .build; the public assets remain the reference.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json
import hashlib
import re
import struct
from compact_assets import compact_stage, pattern_pool

PAGE = 0x80000
ROM_BYTES = 31 * PAGE
HIGH_ADDRESS = 0xd0000
COMPLETION_OFFSET = 0x4000


def symbols(elf, sdk, run):
    return [(int(v[0], 16), int(v[1], 16), v[3])
            for line in run([sdk/'bin/m68k-elf-nm', '-S', elf]).splitlines()
            if len(v := line.split()) == 4]


def link_stage(folder, objects, base, pool_base, offsets, sdk, run, shared_exports=None, pattern_addresses=None, linear=False, provisional=False):
    assignments = '\n'.join(f'{name} = {(pattern_addresses[name] if pattern_addresses is not None else 0x80000000 + pool_base + off):#x};'
                            for name, off in offsets.items())
    discard=''
    if shared_exports:
        assignments+='\n'+'\n'.join(f'{name} = {address:#x};' for name,address in shared_exports.items())
        discard=' '.join(f'*(.rodata.{name})' for name in shared_exports)
    script = assignments + f'''
OUTPUT_ARCH(m68k)
ENTRY(rtStageBankImage)
SECTIONS {{
 /DISCARD/ : {{ *(.rodata.rtSharedPose_*) {discard} *(.comment .note* .eh_frame*) }}
 .stage {base:#x} : {{
  KEEP(*(.stage.header)) *(.text .text.*) *(.rodata .rodata.*)
  . = ALIGN(4); rtStageBankEnd = .;
 }}
 .forbidden : {{ *(.data .data.* .bss .bss.* COMMON) }}
 ASSERT(SIZEOF(.forbidden)==0,"Stage banks cannot own mutable storage")
 ASSERT(rtStageBankImage=={base:#x},"Stage header moved")
 ASSERT(rtStageBankEnd<={"0x1000000" if provisional else "0x400000" if linear else "0x380000"},"Stage exceeds cartridge mapping")
 {'ASSERT(rtStageBankEnd<=0x280000,"Shared metadata requires windows 5 and 6")' if shared_exports and not linear else ''}
}}
'''
    (folder/'compact.ld').write_text(script)
    run([sdk/'bin/m68k-elf-ld', '-T', folder/'compact.ld', '--gc-sections',
         *objects, '-o', folder/'bank.elf'])
    run([sdk/'bin/m68k-elf-objcopy', '-O', 'binary', folder/'bank.elf', folder/'bank.bin'])
    return (folder/'bank.bin').read_bytes()


def relocate_completion(root, sdk, reference, objects_dir, output, delta, run):
    """Relink pointer-bearing scene assets; preserve the frozen footer pixels."""
    output.mkdir(parents=True, exist_ok=True)
    script=(root/'assets/completion/bank.ld').read_text().replace('0x100000',hex(0x100000+delta))
    (output/'bank.ld').write_text(script)
    objects=[objects_dir/(name+'.o') for name in ('scene_background','scene_art','transition_art','completion_monitor_data')]
    run([sdk/'bin/m68k-elf-ld','-T',output/'bank.ld',*objects,'-o',output/'bank.elf'])
    run([sdk/'bin/m68k-elf-objcopy','-O','binary',output/'bank.elf',output/'bank.bin'])
    data=bytearray((output/'bank.bin').read_bytes())
    patch=json.loads((root/'assets/completion/ending_footer.json').read_text())
    data+=struct.pack('>'+str(len(patch['extra_words']))+'H',*patch['extra_words'])
    descriptor=[value+delta if i<3 else value for i,value in enumerate(patch['descriptor'])]
    struct.pack_into('>IIIHHH',data,patch['descriptor_offset'],*descriptor)
    struct.pack_into('>H',data,patch['actor_upper_offset'],patch['actor_upper'])
    if len(data)!=len(reference): raise ValueError('Completion relocation changed size')
    covered=set()
    for offset in range(0,len(data)-3,2):
        old=struct.unpack_from('>I',reference,offset)[0]
        new=struct.unpack_from('>I',data,offset)[0]
        if 0x100000<=old<0x100000+len(data) and new==old+delta:
            covered.update(range(offset,offset+4))
    if any(a!=b and i not in covered for i,(a,b) in enumerate(zip(reference,data))):
        raise ValueError('Completion relocation changed non-pointer data')
    return bytes(data)


def prepare(root, sdk, build, game, rows, images, completion, high, audio, jobs, run, shared=False, packed=False, packed_codec="canonical"):
    linear = packed and packed_codec == 'refined'
    stage_base = lambda address: address if linear else 0x100000+(address & (PAGE-1))
    output = build/'compact'
    output.mkdir(parents=True, exist_ok=True)
    tables = [symbols(build/'stages'/str(row['stage'])/'bank.elf', sdk, run) for row in rows]
    pool, offsets = pattern_pool(images, tables)
    (output/'patterns.bin').write_bytes(pool)
    (output/'pattern_offsets.json').write_text(json.dumps(offsets))
    raw_audio=audio
    audio_report={}
    if packed:
        import pcm_stream
        audio={}
        for page,name in [(51,'victory'),(52,'ending'),(55,'high_cold'),(58,'high_warm')]:
            samples=raw_audio[page][:-16]
            absolute=pcm_stream.pack(samples);difference=pcm_stream.pack(samples,delta=True)
            delta=len(difference)<len(absolute);blob=difference if delta else absolute;phrases=None
            if linear and name.startswith('high_'):
                alternative,dictionary=pcm_stream.pack_phrases(samples)
                if len(alternative)<len(blob):blob=alternative;phrases=dictionary;delta=False
            assert pcm_stream.unpack(blob,len(samples),delta=delta,phrases=phrases)==samples
            (build/'audio'/(name+'.qpcm')).write_bytes(blob);audio[page]=blob
            audio_report[name]=dict(samples=len(samples),sha256=hashlib.sha256(samples).hexdigest(),packed_bytes=len(blob),delta=delta,phrases=phrases)
        (game/'src/pcm_rom_z80.s80').write_text(pcm_stream.driver((root/'src/pcm_rom_z80.s80').read_text(),linear=linear))
    for path in (game/'src').glob('pcm_stream_*.s'):path.unlink()
    victory_address = (HIGH_ADDRESS + len(high) + 32767) & ~32767
    completion_offset=max(0,((victory_address+len(audio[51])+31)&~31)-0x100000) if packed else COMPLETION_OFFSET
    completion = relocate_completion(root,sdk,completion,build/'completion',output/'completion',completion_offset,run)
    exports=(root/'src/completion_exports.s').read_text()
    exports=re.sub(r'(\.set \w+, )(\d+)',lambda m:m[1]+str(int(m[2])+completion_offset),exports)
    (game/'src/completion_exports.s').write_text(exports)
    if packed:audio[51]=pcm_stream.place(audio[51],victory_address)
    chunks = [(HIGH_ADDRESS, high), (victory_address, audio[51]),
              (0x100000+completion_offset, completion)]
    if victory_address + len(audio[51]) > 0x100000+completion_offset:
        raise ValueError('UI and victory audio exceed fixed-ROM spare space')
    cursor = 0x100000 + completion_offset + len(completion)
    stages, objects_by_stage, stats, probes = [], [], [], []
    for row in rows:
        folder = output/'stages'/str(row['stage'])
        stats.append(compact_stage(root/row['folder'], row['sources'], folder,intern_static_pixels=packed))
        if packed:
            from indexed_poses import compact as index_poses
            stats[-1]['indexed_pose_savings']=index_poses(folder,row['sources'])
        def compile_one(name):
            target = folder/(Path(name).stem + '.o')
            run([sdk/'bin/m68k-elf-gcc', '-m68000', '-Os', '-fno-builtin',
                 '-ffunction-sections', '-fdata-sections', '-c', folder/name,
                 '-I'+str(folder), '-I'+str(game/'inc'), '-isystem'+str(sdk/'inc'), '-o', target])
            return target
        with ThreadPoolExecutor(max_workers=jobs) as executor:
            objects = list(executor.map(compile_one, row['sources'] + ['geometry_pool.c']))
        base = stage_base(cursor)
        data = link_stage(folder, objects, base, 0, offsets, sdk, run, linear=linear, provisional=shared)
        stages.append((cursor, len(data)))
        objects_by_stage.append(objects)
        if shared:
            probes.append((row['stage'],folder,data,base,symbols(folder/'bank.elf',sdk,run)))
        cursor += len(data)
        print(f'  Compact stage {row["stage"]+1}: {len(data)} metadata bytes', flush=True)
    shared_pool=None
    if shared:
        reference_cursor=cursor
        for page in (52,55,58):
            reference_cursor=((reference_cursor+32767)&~32767)+len(audio[page])
        reference_pixel_base=(reference_cursor+31)&~31
        from shared_geometry import SharedGeometry
        shared_pool=SharedGeometry(probes,share_world=packed,linear=linear)
        # GCC may resolve references within a translation unit to section
        # symbols. Recompile shared definitions as extern declarations so the
        # linker can bind those references to the common CPU mapping.
        for row in rows:
            folder=output/'stages'/str(row['stage'])
            names=shared_pool.aliases[row['stage']]
            selected='|'.join(re.escape(name) for name in sorted(names))
            if not selected:
                continue
            pattern=re.compile(r'^(const (?:RT_SpritePose|RT_PackedSpritePose|u32|u16) ('+selected+r')\[[^\n]*?)=\{.*?\};',re.M|re.S)
            for name in row['sources']:
                path=folder/name;text=path.read_text()
                edited=pattern.sub(lambda m:'extern '+m[1]+';' if m[2] in names else m[0],text)
                if edited!=text:
                    path.write_text(edited)
                    run([sdk/'bin/m68k-elf-gcc','-m68000','-Os','-fno-builtin',
                         '-ffunction-sections','-fdata-sections','-c',path,
                         '-I'+str(folder),'-I'+str(game/'inc'),'-isystem'+str(sdk/'inc'),
                         '-o',folder/(Path(name).stem+'.o')])
        cursor=0x100000+completion_offset+len(completion)
        stages=[]
        for row,objects in zip(rows,objects_by_stage):
            folder=output/'stages'/str(row['stage'])
            data=link_stage(folder,objects,stage_base(cursor),0,offsets,sdk,run,
                            shared_pool.exports(row['stage'],0x280000),linear=linear)
            stages.append((cursor,len(data)));cursor+=len(data)
    clips = {}
    for page in (52, 55, 58):
        cursor = (cursor + 32767) & ~32767
        # The ending's existing admission contract permits at most two
        # adjacent windows. A smaller preceding stage region can otherwise
        # place it too close to a bank end and accidentally require three.
        if page==52 and (cursor&(PAGE-1))+len(raw_audio[page])>2*PAGE:
            cursor=(cursor+PAGE-1)&~(PAGE-1)
        clips[page] = cursor
        if packed:audio[page]=pcm_stream.place(audio[page],cursor)
        chunks.append((cursor, audio[page]))
        cursor += len(audio[page])
    packed_pool=None
    if packed:
        assert shared_pool
        from packed_patterns import PackedPatterns, hot_animations, shared_player_animations
        packed_pool=PackedPatterns(pool,offsets,tables,build/('refined-speed-v1/encoded' if packed_codec=='refined' else 'fast-pose-cycles-v1/encoded' if packed_codec=='fast' else 'canonical-probe/encoded'),packed_codec,shared_player_animations(root,rows) if packed_codec=='refined' else hot_animations(root,rows),offsets.keys() if packed_codec=='refined' else ())
    shared_address=shared_cpu_base=0
    if shared_pool:
        shared_address=(cursor+3)&~3
        shared_cpu_base=shared_address if linear else 0x280000+(shared_address & (PAGE-1))
        cursor=shared_address+shared_pool.size+(len(packed_pool.palettes)+packed_pool.descriptor_bytes if packed_pool else 0)
    pool_base = (cursor + 31) & ~31
    if shared_pool and not packed_pool:
        # Retain every pixel stream's position inside its 512 KiB bank. This
        # also preserves all 128 KiB DMA split boundaries from the accepted ROM.
        pool_base+=((reference_pixel_base-pool_base)&(PAGE-1))
    pattern_addresses=None
    packed_report=None
    if packed_pool:
        # Reuse alignment gaps between immutable sections. The fixed runtime
        # owns everything below HIGH_ADDRESS; stage and metadata spans are
        # reserved before placing any compressed sources in the remaining gaps.
        occupied=sorted([(0,HIGH_ADDRESS),*( (at,at+len(data)) for at,data in chunks),
                         *((at,at+size) for at,size in stages),(shared_address,pool_base)])
        spare_regions=[];end=0
        for start,stop in occupied:
            if start>end:spare_regions.append((end,start-end))
            assert start>=end, 'Reserved compact sections overlap'
            end=stop
        pool,descriptors,packed_report=packed_pool.materialize(pool_base,spare_regions)
        chunks.extend(packed_pool.spare_chunks)
        palette_cpu=shared_cpu_base+shared_pool.size
        descriptor_cpu=palette_cpu+len(packed_pool.palettes)
        packed_report['palette_cpu']=palette_cpu
        pattern_addresses=packed_pool.addresses(descriptor_cpu)
        packed_report['descriptor_cpu']=descriptor_cpu
        if packed_codec=='refined':packed_report['stream_table_cpu']=descriptor_cpu+packed_report['stream_table_offset']
        if not linear and shared_pool.size+len(packed_pool.palettes)+len(descriptors)>PAGE:
            raise ValueError('Shared metadata plus packed descriptors exceeds two-window capacity')
    used_bytes = pool_base + len(pool)
    rom_bytes=((used_bytes+PAGE-1)//PAGE)*PAGE if packed_pool else (30*PAGE if shared else ROM_BYTES)
    if linear and used_bytes>0x400000:
        raise ValueError(f"Linear cartridge exceeds 32 Mbit: {used_bytes} bytes")
    if packed_pool and used_bytes>7*1024*1024:
        raise ValueError(f'Packed image exceeds the 7 MiB cartridge target: {used_bytes}')
    if used_bytes > rom_bytes:
        raise ValueError(f'Compact image exceeds {rom_bytes} bytes: {used_bytes}')
    chunks.append((pool_base, pool))
    if shared_pool:
        metadata=shared_pool.materialize(shared_cpu_base,pool_base,
            packed_pool.relocated_original_addresses(descriptor_cpu) if packed_pool else None)
        if packed_pool:metadata+=packed_pool.palettes+descriptors
        chunks.append((shared_address,metadata))
    for row, (address, size), objects in zip(rows, stages, objects_by_stage):
        folder = output/'stages'/str(row['stage'])
        data = link_stage(folder, objects, stage_base(address), pool_base, offsets, sdk, run,
                          shared_pool.exports(row['stage'],shared_cpu_base) if shared_pool else None,pattern_addresses,linear=linear)
        if len(data) != size:
            raise ValueError('Physical pattern relocation changed stage size')
        chunks.append((address, data))
    (game/'inc/rt_campaign_bank_pages.h').write_text(
        'static const u8 rtCampaignBankPages[10]={' + ','.join(str(a//PAGE) for a, _ in stages) + '};\n')
    (game/'inc/rt_rom_layout.h').write_text(f'''#ifndef RT_ROM_LAYOUT_H
#define RT_ROM_LAYOUT_H
#define RT_ROM_BYTES {rom_bytes:#x}UL
{'#define RT_LINEAR_ROM 1' if linear else ''}
{'#define RT_PACKED_PATTERNS 1'+chr(10)+'#define RT_STREAMED_PCM 1' if packed_pool else ''}
{('#define RT_PACKED_PALETTE_BASE '+hex(palette_cpu)+'UL') if packed_pool else ''}
{'#define RT_PACKED_PALETTE_WORD_OFFSETS 1' if packed_pool and packed_pool.palette_stride==2 else ''}
{'#define RT_REFINED_PATTERNS 1' if packed_codec=='refined' else ''}
{('#define RT_PACKED_STREAM_TABLE '+hex(packed_report['stream_table_cpu'])+'UL') if packed_codec=='refined' else ''}
{('#define RT_SHARED_METADATA_PAGE '+str(shared_address//PAGE)) if shared_pool else ''}
#define RT_FINAL_VICTORY_WINDOWS 2
#define RT_COMPLETION_PAGE 2
#define RT_HIGH_SCORE_PAGE 1
#define RT_HIGH_SCORE_BASE {HIGH_ADDRESS if linear else 0x100000+HIGH_ADDRESS-PAGE:#x}UL
#define RT_HIGH_COLD_ADDRESS {clips[55]:#x}UL
#define RT_HIGH_WARM_ADDRESS {clips[58]:#x}UL

#ifndef __ASSEMBLER__
static const u32 rtCampaignBankOffsets[10]={{{','.join(hex(a if linear else a & (PAGE-1))+'UL' for a, _ in stages)}}};
#endif
#endif
''')
    for header in (game/'inc').glob('*.h'):
        text = header.read_text()
        if '#define RT_FINAL_VICTORY_ADDRESS ' in text:
            text = re.sub(r'#define RT_FINAL_VICTORY_ADDRESS .*',
                          f'#define RT_FINAL_VICTORY_ADDRESS {victory_address:#x}UL', text)
            text = re.sub(r'#define RT_FINAL_ENDING_AFTER_VICTORY_ADDRESS .*',
                          f'#define RT_FINAL_ENDING_AFTER_VICTORY_ADDRESS {clips[52]:#x}UL', text)
            header.write_text(text)
    report = dict(linear_rom=linear,stages=stages, pattern_pool=pool_base, pattern_bytes=len(pool),rom_bytes=rom_bytes,
                  used_bytes=used_bytes, completion_offset=completion_offset,
                  audio_addresses=dict(victory=victory_address,ending=clips[52],high_cold=clips[55],high_warm=clips[58]), geometry_and_maps=stats, reserved_sram_page=31)
    if packed:
        addresses=[victory_address,clips[52],clips[55],clips[58]]
        counts=[len(raw_audio[page])-16 for page in (51,52,55,58)]
        (game/'inc/rt_pcm_stream_locations.h').write_text('static const u32 rtPcmStreamClips[4][5]={'+','.join('{'+str(at)+'UL,'+str(count)+'UL,'+str((at+len(audio[page])-1)>>19)+','+str(2 if audio_report[name]['phrases'] else int(audio_report[name]['delta']))+','+str(len(audio[page]))+'UL}' for at,count,page,name in zip(addresses,counts,[51,52,55,58],['victory','ending','high_cold','high_warm']))+'};\n')
        phrase_headers=[];phrase_names=[]
        for i,name in enumerate(('victory','ending','high_cold','high_warm')):
            dictionary=audio_report[name]['phrases']
            if dictionary:
                data=bytes(value for row in dictionary for value in (row[0],len(row)-1,*row[1:]))
                assert sum(4*(len(row)-1)+2 for row in dictionary[:-1])+4<=1536
                symbol='rtPcmPhraseDictionary'+str(i)
                phrase_headers.append('static const u8 '+symbol+'[]={'+','.join(map(str,data))+'};')
                phrase_names.append(symbol)
            else:phrase_names.append('0')
        phrase_headers.append('static const u8 *const rtPcmPhraseDictionaries[4]={'+','.join(phrase_names)+'};')
        (game/'inc/rt_pcm_phrase_tables.h').write_text('\n'.join(phrase_headers)+'\n')
        report['streamed_audio']=audio_report
    if shared_pool:
        report['shared_metadata']=dict(linear_rom=linear,physical=shared_address,cpu=shared_cpu_base,size=shared_pool.size+(len(packed_pool.palettes)+packed_pool.descriptor_bytes if packed_pool else 0),
                                      geometry_bytes=len(shared_pool.geometry),pose_tables=len(shared_pool.tables),packed_pose_tables=len(shared_pool.packed_tables),
                                      raw_tables=len(shared_pool.raw_tables),pixel_bank_offsets_preserved=not bool(packed_pool))
        report['reserved_os_pages']=[30,31]
    if packed_report:
        report['packed_patterns']=packed_report
        decoder=(root/'experiments/32mbit/refined_decode.s' if packed_codec=='refined' else root/'experiments/everdrive_streaming'/('fast_decode.s' if packed_codec=='fast' else 'canonical_decode.s')).read_text()
        (game/'src/packed_pose_decode.s').write_text(decoder.replace('.globl decode','.local decode'))
    else:
        (game/'src/packed_pose_decode.s').unlink(missing_ok=True)
    (output/'layout.json').write_text(json.dumps(report, indent=2)+'\n')
    return chunks, report
