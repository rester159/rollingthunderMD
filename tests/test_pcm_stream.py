#!/usr/bin/env python3
"""Lossless PCM-format boundaries, including all sample values and bank seams."""
import random
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from pcm_stream import pack,unpack

class PcmStream(unittest.TestCase):
    def test_boundaries(self):
        randomizer=random.Random(0x52545043)
        for size in (257,258,259,260,511,512,513,32767,32768,32769,131324,262401):
            for alphabet in ((128,), (127,128,129), tuple(range(256))):
                raw=bytes(randomizer.choice(alphabet) for _ in range(size))
                for delta in (False,True):
                    packed=pack(raw,delta=delta)
                    self.assertEqual(unpack(packed,size,delta=delta),raw)
    def test_every_literal_and_escape(self):
        raw=bytes(range(256))*513

        for delta in (False,True):self.assertEqual(unpack(pack(raw,delta=delta),len(raw),delta=delta),raw)
    def test_owner_clips(self):
        folder=Path(__file__).resolve().parents[1]/'.build/audio'
        for name in ('victory','ending','high_cold','high_warm'):
            path=folder/(name+'.pcm')
            if path.exists():
                raw=path.read_bytes()
                for delta in (False,True):self.assertEqual(unpack(pack(raw,delta=delta),len(raw),delta=delta),raw)

if __name__=='__main__':unittest.main()
