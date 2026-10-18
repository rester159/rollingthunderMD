"""Share exact pose shapes before their many-to-one color conversions.

The existing palette-normalized encoder only shares bijective permutations.
Two converted images may instead merge different source pens. Refine their
pixel partitions into a common, at-most-16-pen source, and retain an exact
per-image mapping. Transparency, every output nibble and all pose sizes are
verified. This changes storage only; it does not select or discard poses.
"""
from collections import defaultdict
import hashlib
import json
from pose_codec import normalize, mapping


def sources(raws, cache):
    key = hashlib.sha256(''.join(sorted(raws)).encode()).hexdigest()
    path = cache.parent / ('refined-sources-v1-' + key + '.json')
    if path.exists():
        saved = json.loads(path.read_text())
        bases = {name: bytes.fromhex(raw) for name, raw in saved['bases'].items()}
        result = {name: (bases[base], bytes.fromhex(palette))
                  for name, (base, palette) in saved['records'].items()}
        assert result.keys() == raws.keys()
    else:
        groups = defaultdict(dict)
        normalized = {}
        for name, raw in sorted(raws.items()):
            canon, palette = normalize(raw)
            normalized[name] = (canon, palette)
            nibs = bytes(c for b in canon for c in (b >> 4, b & 15))
            masks = [int.from_bytes(bytes(c == pen for c in nibs), 'big')
                     for pen in range(max(nibs) + 1)]
            groups[len(raw), masks[0]][canon] = masks
        references = {}
        for choices in groups.values():
            clusters = []
            for canon, masks in sorted(choices.items(), key=lambda item: (-len(item[1]), item[0])):
                for cluster in clusters:
                    finer = [a & b for a in cluster[0] for b in masks if a & b]
                    if len(finer) <= 16:
                        cluster[0] = finer
                        cluster[1].append(canon)
                        break
                else:
                    clusters.append([masks, [canon]])
            for masks, children in clusters:
                assert masks[0] == choices[children[0]][0]
                nibs = bytearray(len(children[0]) * 2)
                for pen, mask in enumerate(masks):
                    for i, present in enumerate(mask.to_bytes(len(nibs), 'big')):
                        if present:
                            nibs[i] = pen
                source = bytes(a << 4 | b for a, b in zip(nibs[::2], nibs[1::2]))
                for target in children:
                    inverse = [next(i for i, bits in enumerate(choices[target])
                                    if mask & bits == mask) for mask in masks]
                    references[target] = (source, inverse)
        result = {}
        for name, (canon, palette) in normalized.items():
            source, indices = references[canon]
            original = [c for b in palette for c in (b >> 4, b & 15)]
            inverse = [original[i] for i in indices] + [0] * (16 - len(indices))
            result[name] = (source, bytes(inverse[i] << 4 | inverse[i + 1] for i in range(0, 16, 2)))
        bases = {}
        records = {}
        for name, (source, palette) in result.items():
            identity = hashlib.sha256(source).hexdigest()
            bases[identity] = source.hex()
            records[name] = (identity, palette.hex())
        path.write_text(json.dumps(dict(bases=bases, records=records), separators=(',', ':')))
    # Also validate cached conversions against the actual frozen input bytes.
    for name, (source, palette) in result.items():
        assert len(source) == len(raws[name])
        assert source.translate(mapping(palette)) == raws[name], name
        assert palette[0] >> 4 == 0
    return result
