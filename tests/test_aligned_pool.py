"""Exact, contiguous raw spans at native word-aligned DMA/CPU addresses."""
import random
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from aligned_pool import pack


class AlignedPoolTests(unittest.TestCase):
    def check(self, spans, alignment=2):
        blob, offsets = pack(spans, alignment)
        for name, raw in spans.items():
            self.assertEqual(offsets[name] % alignment, 0)
            self.assertEqual(blob[offsets[name]:offsets[name] + len(raw)], raw)
        self.assertLessEqual(len(blob), sum(map(len, spans.values())))
        # Input dictionary order must not affect the resulting ROM.
        self.assertEqual((blob, offsets), pack(dict(reversed(list(spans.items()))), alignment))
        return blob, offsets

    def test_partial_tile_edges(self):
        blob, offsets = self.check({'a': b'abcdefghijkl', 'b': b'klmnopqrstuv'})
        self.assertEqual(len(blob), 22)
        self.assertEqual(offsets['b'], 10)

    def test_containment_and_alignment(self):
        blob, offsets = self.check({'a': b'abcdefgh', 'b': b'cdef', 'c': b'bcde', 'd': b'abcdefgh'})
        self.assertEqual(offsets['a'], offsets['d'])
        self.assertEqual(offsets['b'], offsets['a'] + 2)
        self.assertNotEqual(offsets['c'], offsets['a'] + 1)

    def test_random_overlaps(self):
        randomizer = random.Random(0x5254504f)
        for alignment in (2, 4, 32):
            for trial in range(40):
                source = bytes(randomizer.randrange(8) for _ in range(256 * alignment))
                spans = {}
                for index in range(30):
                    start = randomizer.randrange(128) * alignment
                    size = randomizer.randrange(1, 64) * alignment
                    spans[str(index)] = source[start:start + size]
                self.check(spans, alignment)

    def test_invalid_spans(self):
        for spans in ({'odd': b'abc'}, {'empty': b''}):
            with self.assertRaises(ValueError):
                pack(spans)
        self.assertEqual(pack({}), (b'', {}))


if __name__ == '__main__':
    unittest.main()
