#!/usr/bin/env python3
"""Byte-exact packed-source overlap, mapper seams and raw DMA alignment."""
import sys,unittest,hashlib,struct,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from packed_patterns import PackedPatterns,PAGE
class StorageTests(unittest.TestCase):
 def make(self,physical,spares=()):
  p=PackedPatterns.__new__(PackedPatterns);p.codec='fast'
  p.streams={'a':b'abcdefghijklmno1234567','b':b'1234567ABCDEFG','c':b'ABCDEFGz','d':b'z'+bytes(range(128))}
  raw=bytes(range(32));identity='raw:'+hashlib.sha256(raw).hexdigest();p.raw_streams={identity:raw}
  p.records={k:(k,bytes(8),32) for k in p.streams};p.records[identity]=(identity,bytes(8),32)
  p.colors={k:16 for k in p.streams};p.descriptor_bytes=4*6;p.palette_offsets={bytes(8):0};p.palettes=bytes(8)
  blob,descriptors,report=p.materialize(physical,spares)
  for key,data in {**p.streams,**p.raw_streams}.items():
   at=p.locations[key];segments=[(physical,blob),*p.spare_chunks]
   self.assertTrue(any(start<=at and at+len(data)<=start+len(chunk) and chunk[at-start:at-start+len(data)]==data for start,chunk in segments))
   if key in p.streams:self.assertLessEqual((at&(PAGE-1))+len(data),PAGE)
   else:self.assertEqual(at&1,0)
  self.assertEqual(len(descriptors),24)
  return p,report
 def test_overlap(self):
  p,r=self.make(PAGE*2);self.assertGreaterEqual(r['compressed_overlap_bytes'],14)
  self.assertTrue(any(p.locations[k]&1 for k in p.streams))
 def test_fixed_spare(self):
  p,r=self.make(2*PAGE,[(0xeccd4,13100)])
  self.assertEqual(r['fixed_spare_bytes'],sum(map(len,p.streams.values())))
 def test_palette_prefixes(self):
  from pose_codec_fast import decode
  images=[]
  for colors in range(1,17):
   values=list(range(1,colors))+[0]
   pixels=[values[i%colors] for i in range(128)]
   images.append(bytes((pixels[i]<<4)|pixels[i+1] for i in range(0,128,2)))
  images.append(bytes(range(256)))
  raw=b''.join(images);offsets={};table=[];cursor=0
  for data in images:
   name='rtSharedPose_'+hashlib.sha256(data).hexdigest();offsets[name]=cursor;table.append((cursor,len(data),name));cursor+=len(data)
  with tempfile.TemporaryDirectory() as tmp:
   p=PackedPatterns(raw,offsets,[table],Path(tmp),codec='fast')
   blob,descriptors,report=p.materialize(PAGE)
   for i,(name,(identity,palette,size)) in enumerate(p.records.items()):
    physical,index=struct.unpack_from('>IH',descriptors,i*6)
    self.assertEqual(physical>>24,p.colors[identity]);physical&=0xffffff
    self.assertEqual(len(p.palettes[index:index+8]),8)
    stream=blob[physical-PAGE:physical-PAGE+len(p.streams[identity])]
    self.assertEqual(decode(stream,size,p.palettes[index:index+8]),raw[offsets[name]:offsets[name]+size])
 def test_bank_seams(self):
  for offset in range(1,160):self.make(2*PAGE-offset)
if __name__=='__main__':unittest.main()
