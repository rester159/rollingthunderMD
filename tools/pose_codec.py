"""Lossless palette-normalized pose storage, with independent byte LZ streams.

Zero stays transparent. The original 16 color indices are recovered exactly;
no palette quantization or image approximation is performed. Canonical pose
streams can be shared, while every original pose has its own 8-byte permutation.
Byte LZ: 0xxxxxxx = 1..128 literals; 1llllldd dddddddd = 3..34 copied bytes,
with a 1..1024-byte backward distance. Matches can overlap.
"""
from collections import defaultdict, deque
import struct


def normalize(raw):
    forward = {0: 0}
    inverse = [0]
    canonical = bytearray()
    for byte in raw:
        pair = []
        for color in (byte >> 4, byte & 15):
            if color not in forward:
                forward[color] = len(inverse)
                inverse.append(color)
            pair.append(forward[color])
        canonical.append(pair[0] << 4 | pair[1])
    inverse += [0] * (16-len(inverse))
    palette = bytes(inverse[i] << 4 | inverse[i+1] for i in range(0,16,2))
    return bytes(canonical), palette


def mapping(palette):
    if len(palette) != 8:
        raise ValueError('Palette permutation must contain eight bytes')
    inverse = [c for byte in palette for c in (byte >> 4, byte & 15)]
    return bytes(inverse[i >> 4] << 4 | inverse[i & 15] for i in range(256))


def encode(raw):
    """Minimum encoded byte count for this token format, via dynamic programming."""
    if not raw:
        raise ValueError('Empty pose')
    positions = defaultdict(list)
    matches = []
    for i in range(len(raw)):
        best, distance = 0, 0
        if i+3 <= len(raw):
            key = raw[i:i+3]
            for at in reversed(positions[key]):
                if i-at > 1024:
                    break
                n = 3
                while n < 34 and i+n < len(raw) and raw[at+n] == raw[i+n]:
                    n += 1
                if n > best:
                    best, distance = n, i-at
                    if n == 34:
                        break
            positions[key].append(i)
        matches.append((best,distance))
    costs = [0] * (len(raw)+1)
    steps = [None] * len(raw)
    literals = deque()
    for i in range(len(raw)-1,-1,-1):
        # Sliding minimum of j+cost[j], for i<j<=i+128. Keeping older
        # equal-cost entries favors longer tokens without rescanning 128 ends.
        j = i+1
        key = j+costs[j]
        while literals and literals[-1][1] > key:
            literals.pop()
        literals.append((j,key))
        while literals[0][0] > i+128:
            literals.popleft()
        end,key = literals[0]
        cost,count,distance = 1-i+key,end-i,0
        longest, back = matches[i]
        for n in range(3,longest+1):
            candidate = (2+costs[i+n], -n, n, back)
            if candidate[:2] < (cost,-count):
                cost,_,count,distance = candidate
        costs[i] = cost
        steps[i] = count,distance
    result = bytearray(); i = 0
    while i < len(raw):
        count,distance = steps[i]
        if distance:
            result += struct.pack('>H',0x8000 | ((count-3)<<10) | (distance-1))
        else:
            result.append(count-1)
            result += raw[i:i+count]
        i += count
    assert len(result) == costs[0]
    return bytes(result)


def decode(packed, size, palette=bytes.fromhex('0123456789abcdef')):
    table = mapping(palette)
    result = bytearray(); cursor = 0
    while len(result) < size:
        if cursor >= len(packed):
            raise ValueError('Truncated token')
        token = packed[cursor]; cursor += 1
        if token & 128:
            if cursor >= len(packed):
                raise ValueError('Truncated match')
            count = ((token >> 2) & 31)+3
            distance = ((token & 3)<<8 | packed[cursor])+1; cursor += 1
            if distance > len(result):
                raise ValueError('Match precedes output')
            for _ in range(count):
                result.append(result[-distance])
        else:
            count = token+1
            if cursor+count > len(packed):
                raise ValueError('Truncated literal')
            result += packed[cursor:cursor+count].translate(table)
            cursor += count
    if len(result) != size or cursor != len(packed):
        raise ValueError('Stream length mismatch')
    return bytes(result)
