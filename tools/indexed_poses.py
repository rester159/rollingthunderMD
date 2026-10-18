"""Intern repeated enemy pose records without changing any animation indices.

Only generated stage derivatives are rewritten. The existing renderer still
receives ordinary immutable RT_PackedSpritePose / RT_SpritePose records.
"""
import json
import hashlib
import struct
import re

HEADER = '''#ifndef RT_INDEXED_POSES_H
#define RT_INDEXED_POSES_H
#include "rt_sprites.h"
typedef struct {
    const RT_PackedSpritePose *records;
    const u16 *indices;
} RT_IndexedSpritePoses;
static inline const RT_PackedSpritePose *RT_indexedSpritePose(const RT_IndexedSpritePoses *table,u16 index)
{return (const RT_PackedSpritePose*)((const u8*)table->records+table->indices[index]);}
#endif
'''
ARRAY = re.compile(r'^const (RT_SpritePose|RT_PackedSpritePose) (\w+)\[[^\]]*\]=\{(.*?)\n\};',re.M|re.S)


def compact(folder, sources):
    pose_maps={};graphics_maps={};saved=0;index_labels=set()
    for filename in sources:
        path=folder/filename;text=path.read_text()
        if not any(m[1]=='RT_PackedSpritePose' for m in ARRAY.finditer(text)):continue
        for match in list(ARRAY.finditer(text)):
            typ,name,body=match.groups()
            records=re.findall(r'^\{([^\n]+)\},?$',body,re.M)
            if not records:raise ValueError('Unrecognized pose table '+name)
            assert len(records)==body.count('{')
            unique=[];indices=[];seen={}
            for record in records:
                if typ=='RT_PackedSpritePose':
                    record=re.sub(r'(\w+)\+(\d+)',lambda m:m[1]+'+'+str(pose_maps[m[1]][int(m[2])]),record)
                if record not in seen:
                    seen[record]=len(unique);unique.append(record)
                indices.append(seen[record])
            assert all(unique[index]==(record if typ=='RT_SpritePose' else re.sub(r'(\w+)\+(\d+)',lambda m:m[1]+'+'+str(pose_maps[m[1]][int(m[2])]),record)) for index,record in zip(indices,records))
            packed='\n'.join('{'+record+'},' for record in unique)
            if typ=='RT_SpritePose':
                pose_maps[name]=indices
                replacement=f'const RT_SpritePose {name}[]={{\n{packed}\n}};'
                saved+=(len(records)-len(unique))*12
            else:
                graphics_maps[name]=indices
                index_label='rtPoseIndex_'+hashlib.sha256(struct.pack('>'+str(len(indices))+'H',*(i*8 for i in indices))).hexdigest()
                index_definition=('extern const u16 '+index_label+'[];' if index_label in index_labels else 'const u16 '+index_label+'[]={'+','.join(str(i*8) for i in indices)+'};')
                index_labels.add(index_label)
                replacement=(f'const RT_PackedSpritePose {name}Records[]={{\n{packed}\n}};\n'
                    +index_definition+'\n'+
                    f'const RT_IndexedSpritePoses {name}={{{name}Records,{index_label}}};')
                saved+=len(records)*8-len(unique)*8-len(indices)*2-8
            text=text.replace(match[0],replacement)
        path.write_text('#include "rt_indexed_poses.h"\n'+text)
    for path in folder.glob('*.h'):
        text=path.read_text()
        for name in pose_maps:
            text=re.sub(r'(extern const RT_SpritePose '+name+r')\[[^\]]*\]',r'\1[]',text)
        for name in graphics_maps:
            text=re.sub(r'extern const RT_PackedSpritePose '+name+r'\[[^\]]*\];',f'extern const RT_IndexedSpritePoses {name};',text)
        if 'RT_IndexedSpritePoses' in text:text='#include "rt_indexed_poses.h"\n'+text
        path.write_text(text)
    for filename in sources:
        path=folder/filename;text=path.read_text()
        if filename.endswith('enemy_atlas.c'):
            text=text.replace('static const RT_PackedSpritePose *const variants','static const RT_IndexedSpritePoses *const variants')
            text=text.replace('const RT_PackedSpritePose *list=variants','const RT_IndexedSpritePoses *list=variants')
            text=text.replace('list[pose].pose','RT_indexedSpritePose(list,pose)->pose').replace('list+pose','RT_indexedSpritePose(list,pose)')
            text=re.sub(r'(\[\d+\]=)(\w+PosesGraphics)',r'\1&\2',text)
            text='#include "rt_indexed_poses.h"\n'+text
        elif filename.endswith('high_enemy_graphics.c'):
            text=re.sub(r'(\w+PosesGraphics)\+pose',r'RT_indexedSpritePose(&\1,pose)',text)
            text='#include "rt_indexed_poses.h"\n'+text
        path.write_text(text)
    (folder/'rt_indexed_poses.h').write_text(HEADER)
    (folder/'indexed_poses.json').write_text(json.dumps(dict(poses=pose_maps,graphics=graphics_maps,saved_bytes=saved)))
    return saved
