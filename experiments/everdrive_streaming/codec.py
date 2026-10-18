"""Independent pose streams: four-byte LZ units, no cross-pose state."""
from collections import defaultdict
import struct


def encode(raw):
    if not raw or len(raw) % 4:
        raise ValueError('Expected a positive multiple of four bytes')
    words = struct.unpack('>' + str(len(raw) // 4) + 'I', raw)
    positions = defaultdict(list)
    out = bytearray()
    pending = []

    def flush():
        if pending:
            out.extend(struct.pack('>H', len(pending) - 1))
            out.extend(struct.pack('>' + str(len(pending)) + 'I', *pending))
            pending.clear()

    def remember(i):
        if i + 2 < len(words):
            bucket = positions[words[i:i+3]]
            bucket.append(i)
            if len(bucket) > 64:
                del bucket[0]

    i = 0
    while i < len(words):
        best, distance = 0, 0
        if i + 2 < len(words):
            for at in reversed(positions[words[i:i+3]]):
                if i - at > 1024:
                    break
                n = 3
                while n < 34 and i + n < len(words) and words[at+n] == words[i+n]:
                    n += 1
                if n > best:
                    best, distance = n, i - at
                    if n == 34:
                        break
        if best:
            flush()
            out.extend(struct.pack('>H', 0x8000 | ((best-3) << 10) | (distance-1)))
            for j in range(i, i+best):
                remember(j)
            i += best
        else:
            pending.append(words[i])
            remember(i)
            i += 1
            if len(pending) == 32768:
                flush()
    flush()
    return bytes(out)


def decode(packed, raw_size):
    out = bytearray()
    at = 0
    while len(out) < raw_size:
        token, = struct.unpack_from('>H', packed, at)
        at += 2
        if token & 0x8000:
            count = ((token >> 10) & 31) + 3
            distance = ((token & 1023) + 1) * 4
            if distance > len(out):
                raise ValueError('Match precedes output')
            for _ in range(count):
                start = len(out) - distance
                out.extend(out[start:start+4])
        else:
            count = (token + 1) * 4
            if at + count > len(packed):
                raise ValueError('Truncated literal')
            out.extend(packed[at:at+count])
            at += count
    if len(out) != raw_size or at != len(packed):
        raise ValueError('Stream length mismatch')
    return bytes(out)
