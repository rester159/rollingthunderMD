#!/usr/bin/env python3
"""Losslessly intern generated stage geometry and 16-cell background blocks.

The frozen source and bank hashes remain the reference. This module writes only
build-directory derivatives, and verifies every pointer slice/map expansion.
"""
from collections import defaultdict
import ast
import hashlib
import re
import struct
from pathlib import Path

ARRAY = re.compile(r'static const (RT_SpritePiece|RT_PackedSpritePiece) (\w+)\[[^\]]*\]=\{(.*?)\n\};', re.S)


def values(body):
    return ast.literal_eval('[' + body.replace('{', '[').replace('}', ']') + ']')


def initializer(value):
    if isinstance(value, list):
        return '{' + ','.join(map(initializer, value)) + '}'
    return str(value)


def compact_stage(folder, sources, output, intern_static_pixels=False):
    output.mkdir(parents=True, exist_ok=True)
    pool = {}
    raw_arrays = set()
    stats = dict(geometry_before=0, geometry_after=0, maps_before=0, maps_after=0)
    for name in sources:
        text = (folder / name).read_text()
        declarations = []
        if intern_static_pixels:
            # Give identical formerly file-local raw pixel arrays stable names.
            # SharedGeometry can then alias them without changing any pixels or
            # requiring runtime decompression. Definitions stay stage-local on
            # the first pass; a later link selects only duplicate global data.
            arrays = re.compile(r'^static const u32 (\w+)\[[^\]]*\]=\{(.*?)\n\};', re.M|re.S)
            for match in list(arrays.finditer(text)):
                old,body=match.groups()
                words=values(re.sub(r'(0x[0-9a-fA-F]+|[0-9]+)(?:UL|U|L)\b',r'\1',body))
                raw=struct.pack('>'+str(len(words))+'I',*words)
                label='rtStaticPixels_'+hashlib.sha256(raw).hexdigest()
                definition=('extern const u32 '+label+'[];' if label in raw_arrays else
                            'const u32 '+label+'[]={'+body+'\n};')
                raw_arrays.add(label)
                text=text.replace(match[0],definition)
                text=re.sub(r'\b'+re.escape(old)+r'\b',label,text)
        # Enemy rendering exclusively consumes packed geometry. Players and
        # stage objects still use the general renderer, so retain those records.
        packed_enemy = bool(re.fullmatch(r'(?:area\d\d_)?(?:enemy_b\d_g\d+|high\d+)_poses.c', name))
        if packed_enemy and 'const RT_PackedSpritePose ' not in text:
            raise ValueError('Missing packed enemy geometry: ' + name)
        for match in list(ARRAY.finditer(text)):
            typ, symbol, body = match.groups()
            if packed_enemy and typ == 'RT_SpritePiece':
                text = text.replace(match.group(0), '')
                text = re.sub(r'\b' + symbol + r'\+\d+\b', '0', text)
                if re.search(r'\b' + symbol + r'\b', text):
                    raise ValueError('Unexpected general enemy geometry reference')
                continue
            records = values(body)
            # Generated arrays use explicit offsets for every pose. Retain the
            # entire slice up to the next offset (including both facings).
            refs = re.compile(r'\b' + symbol + r'\+(\d+)\b')
            offsets = sorted(set(int(m.group(1)) for m in refs.finditer(text)))
            if not offsets:
                continue
            if offsets[0] != 0 or offsets[-1] >= len(records):
                raise ValueError('Unexpected geometry offsets in ' + name)
            aliases = {}
            stride = 22 if typ == 'RT_SpritePiece' else 12
            stats['geometry_before'] += len(records) * stride
            for start, end in zip(offsets, offsets[1:] + [len(records)]):
                chunk = records[start:end]
                canonical = initializer(chunk)
                key = (typ, canonical)
                if key not in pool:
                    label = 'rtGeometry_' + hashlib.sha256((typ + canonical).encode()).hexdigest()
                    pool[key] = label
                    stats['geometry_after'] += (end - start) * stride
                aliases[start] = pool[key]
                declarations.append('extern const ' + typ + ' ' + pool[key] + '[];')
                if values(canonical[1:-1]) != chunk:
                    raise ValueError('Geometry round trip failed')
            text = text.replace(match.group(0), '')
            text = refs.sub(lambda m: aliases[int(m.group(1))], text)
            if re.search(r'\b' + symbol + r'\b', text):
                raise ValueError('Unrecognized geometry reference in ' + name)
        if 'const RT_DisplayStage ' in text:
            blocks = []
            indices = {}
            for plane in ('plane0', 'plane1'):
                pattern = re.compile(r'static const u16 ' + plane + r'\[\]=\{(.*?)\n\};', re.S)
                match = pattern.search(text)
                if not match:
                    raise ValueError('Missing display plane in ' + name)
                cells = values(match.group(1))
                if len(cells) % 16:
                    raise ValueError('Map must be block aligned')
                packed = []
                for start in range(0, len(cells), 16):
                    block = tuple(cells[start:start + 16])
                    if block not in indices:
                        indices[block] = len(blocks)
                        blocks.append(block)
                    packed.append(indices[block])
                expanded = [cell for index in packed for cell in blocks[index]]
                if expanded != cells:
                    raise ValueError('Map round trip failed')
                stats['maps_before'] += len(cells) * 2
                stats['maps_after'] += len(packed) * 2
                text = text[:match.start()] + 'static const u16 ' + plane + '[]=' + initializer(packed) + ';' + text[match.end():]
            if len(blocks) > 65536:
                raise ValueError('Block index overflow')
            stats['maps_after'] += len(blocks) * 32
            # The final descriptor field is the optional block dictionary.
            descriptor = re.compile(r'(const RT_DisplayStage \w+=\{(?:patterns|rtStaticPixels_\w+),\{plane0,plane1\},palettes,\d+,\d+,\d+,\{\d+,\d+\},\d+,\d+)(?:,0)?\};')
            block_symbol=('rtSharedMap_'+hashlib.sha256(struct.pack('>'+str(len(blocks)*16)+'H',*(v for b in blocks for v in b))).hexdigest()) if intern_static_pixels else 'rtMapBlocks'
            text, count = descriptor.subn(r'\1,'+block_symbol+'};', text)
            if count != 1:
                raise ValueError('Unrecognized display descriptor')
            declarations.append(('const' if intern_static_pixels else 'static const')+' u16 '+block_symbol+'[]=' + initializer([v for b in blocks for v in b]) + ';')
        # Insert after includes, before any descriptors referring to interned data.
        at = max((m.end() for m in re.finditer(r'^#include[^\n]*\n', text, re.M)), default=0)
        text = text[:at] + '\n'.join(dict.fromkeys(declarations)) + '\n' + text[at:]
        (output / name).write_text(text)
    lines = ['#include "rt_sprites.h"']
    for (typ, canonical), label in pool.items():
        lines.append('const ' + typ + ' ' + label + '[]=' + canonical + ';')
    (output / 'geometry_pool.c').write_text('\n'.join(lines) + '\n')
    # Local stage headers are part of the frozen sources too.
    for header in folder.glob('*.h'):
        (output / header.name).write_bytes(header.read_bytes())
    return stats


def pattern_pool(images, symbol_tables):
    """Share whole, uncompressed poses; each remains contiguous DMA-readable pixels."""
    patterns = {}
    for data, symbols in zip(images, symbol_tables):
        for address, size, name in symbols:
            if name.startswith('rtSharedPose_'):
                raw = data[address - 0x100000:address - 0x100000 + size]
                if len(raw) != size or hashlib.sha256(raw).hexdigest() != name.removeprefix('rtSharedPose_'):
                    raise ValueError('Shared pattern identity differs: ' + name)
                if name in patterns and patterns[name] != raw:
                    raise ValueError('Conflicting shared patterns')
                patterns[name] = raw
    # Merge identical suffix/prefix tiles between whole poses. A pose remains
    # contiguous raw pixels; ordinary 128-KiB DMA boundary splitting still
    # applies. No decompression or extra per-pose descriptor is required.
    ordered = sorted(patterns.items(), key=lambda item: (-len(item[1]), item[0]))
    prefixes = defaultdict(list)
    for index, (_, raw) in enumerate(ordered):
        for length in range(32, len(raw) + 1, 32):
            prefixes[raw[:length]].append(index)
    for choices in prefixes.values(): choices.reverse()
    remaining = set(range(len(ordered)))
    pool = bytearray()
    offsets = {}
    while remaining:
        index, overlap = None, 0
        for length in range(min(2048, len(pool)) // 32 * 32, 0, -32):
            choices = prefixes.get(bytes(pool[-length:]))
            if choices:
                while choices and choices[-1] not in remaining:
                    choices.pop()
                if choices:
                    index, overlap = choices[-1], length
                    break
        if index is None:
            index = min(remaining)
        name, raw = ordered[index]
        remaining.remove(index)
        offsets[name] = len(pool) - overlap
        pool.extend(raw[overlap:])
    for name, raw in ordered:
        at = offsets[name]
        if pool[at:at + len(raw)] != raw:
            raise ValueError('Pattern round trip failed')
    return bytes(pool), offsets
