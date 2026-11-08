"""Intern immutable byte spans without changing their contiguous representation."""
from collections import defaultdict


def pack(spans, alignment=2, max_overlap=2048):
    """Return a pool and offsets; every source stays intact and aligned.

    Contained spans share an existing occurrence. Otherwise a deterministic
    greedy suffix/prefix overlap joins spans. This changes only build-time
    placement: consumers need no new header, decoder, or indirection.
    """
    if alignment < 1 or max_overlap < alignment:
        raise ValueError('Invalid pool alignment/overlap')
    ordered = sorted(spans.items(), key=lambda item: (-len(item[1]), item[0]))
    if any(not data or len(data) % alignment for _, data in ordered):
        raise ValueError('Spans must be nonempty and alignment-sized')
    prefixes = defaultdict(list)
    for index, (_, data) in enumerate(ordered):
        for length in range(alignment, min(max_overlap, len(data)) + 1, alignment):
            prefixes[data[:length]].append(index)
    for choices in prefixes.values():
        choices.reverse()
    remaining = set(range(len(ordered)))
    pool = bytearray()
    offsets = {}
    while remaining:
        index, overlap = None, 0
        for length in range(min(max_overlap, len(pool)) // alignment * alignment, 0, -alignment):
            choices = prefixes.get(bytes(pool[-length:]))
            if choices:
                while choices and choices[-1] not in remaining:
                    choices.pop()
                if choices:
                    index, overlap = choices[-1], length
                    break
        if index is None:
            index = min(remaining)
        remaining.remove(index)
        name, data = ordered[index]
        at = pool.find(data)
        while at >= 0 and at % alignment:
            at = pool.find(data, at + 1)
        if at < 0:
            at = len(pool) - overlap
            pool.extend(data[overlap:])
        offsets[name] = at
    for name, data in ordered:
        at = offsets[name]
        assert at % alignment == 0 and pool[at:at + len(data)] == data
    return bytes(pool), offsets
