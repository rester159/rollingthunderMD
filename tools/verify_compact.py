"""Compare linked compact pose pixels/geometry with frozen reference banks."""
import re
import json
import struct

def verify(root, sdk, run, rows, stages, rom, shared=None, packed=None):
 out=root/'.build/compact'
 count=packed_count=map_cells=0
 for row,(physical,size) in zip(rows,stages):
  old=(root/f'.build/stages/{row["stage"]}/bank.bin').read_bytes();new=rom[physical:physical+size];base=physical if shared and shared.get("linear_rom") else 0x100000+(physical&0x7ffff)
  def syms(p):
   result={}
   for line in run([sdk/'bin/m68k-elf-nm','-S',p]).splitlines():
    v=line.split()
    if len(v)==4:result[v[3]]=(int(v[0],16),int(v[1],16))
    elif len(v)==3 and v[1]=='A':result[v[2]]=(int(v[0],16),0)
   return result
  a=syms(root/f'.build/stages/{row["stage"]}/bank.elf');b=syms(out/f'stages/{row["stage"]}/bank.elf')
  def read(data,address,n,base):
   if data is new and shared and shared['cpu']<=address<shared['cpu']+shared['size']:
    offset=address-shared['cpu'];assert offset+n<=shared['size']
    physical=shared['physical']+offset
    return rom[physical:physical+n]
   assert address>=base and address+n<=base+len(data),(hex(address),n,hex(base))
   return data[address-base:address-base+n]
  def pose(data,address,base):return struct.unpack('>IIHBB',read(data,address,12,base))
  # Independently decode the linked map representation, not the C transform.
  for name in row['sources']:
   text=(root/row['folder']/name).read_text()
   for sym in re.findall(r'^const RT_DisplayStage (\w+)=',text,re.M):
    old_display=struct.unpack('>IIIIHHHHHBBI',read(old,a[sym][0],32,0x100000))
    new_display=struct.unpack('>IIIIHHHHHBBI',read(new,b[sym][0],32,base))
    assert old_display[4:11]==new_display[4:11],(sym,'display dimensions/palette selection')
    assert read(old,old_display[0],old_display[6]*32,0x100000)==read(new,new_display[0],new_display[6]*32,base),(sym,'background pixels')
    assert read(old,old_display[3],128,0x100000)==read(new,new_display[3],128,base),(sym,'palette')
    cells=old_display[4]*old_display[5]
    for plane in (1,2):
     expected=read(old,old_display[plane],cells*2,0x100000)
     indices=struct.unpack('>'+str(cells//16)+'H',read(new,new_display[plane],cells//8,base))
     actual=b''.join(read(new,new_display[11]+index*32,32,base) for index in indices)
     assert expected==actual,(sym,plane,'map cells')
     map_cells+=cells
  for sym,(address,length) in a.items():
   if sym.startswith('rtStageWorld') and (sym.endswith('Cells') or sym.endswith('Directory')):
    other,other_length=b[sym]
    assert length==other_length or (other_length==0 and shared and shared['cpu']<=other and other+length<=shared['cpu']+shared['size'])
    assert read(old,address,length,0x100000)==read(new,other,length,base),(sym,'collision/world data')
  indexed_path=out/f'stages/{row["stage"]}/indexed_poses.json'
  indexed=json.loads(indexed_path.read_text()) if packed and indexed_path.exists() else dict(poses={},graphics={})
  expected_pose_addresses={}
  for filename in row['sources']:
   source=(root/row['folder']/filename).read_text()
   for sym in re.findall(r'^const RT_SpritePose (\w+)\[',source,re.M):
    if sym not in a:continue
    old_address,n=a[sym];new_address,_=b[sym]
    for index in range(n//12):
     mapped=indexed['poses'][sym][index] if sym in indexed['poses'] else index
     expected_pose_addresses[old_address+index*12]=new_address+mapped*12
  packed_old=set();packed_new=set()
  for name in row['sources']:
   text=(root/row['folder']/name).read_text()
   for sym in re.findall(r'^const RT_PackedSpritePose (\w+)\[',text,re.M):
    if sym not in a:continue
    aa,n=a[sym];bb,nn=b[sym];assert nn==(8 if sym in indexed["graphics"] else n)
    for at in range(0,n,8):
     op,og=struct.unpack('>II',read(old,aa+at,8,0x100000))
     if sym in indexed['graphics']:
      records,indices=struct.unpack('>II',read(new,bb,8,base))
      index=struct.unpack('>H',read(new,indices+at//4,2,base))[0]
      assert index%8==0
      target=records+index
     else:target=bb+at
     np,ng=struct.unpack('>II',read(new,target,8,base))
     assert bool(op)==bool(np) and bool(og)==bool(ng)
     if not op:continue
     assert np==expected_pose_addresses[op],(sym,at,'animation pose selection')
     oo=pose(old,op,0x100000);no=pose(new,np,base);assert oo[2:]==no[2:]
     assert read(old,og,oo[3]*24,0x100000)==read(new,ng,no[3]*24,base),(sym,at,'packed geometry')
     packed_old.add(op);packed_new.add(np);packed_count+=1
  for name in row['sources']:
   text=(root/row['folder']/name).read_text()
   for sym in re.findall(r'^const RT_SpritePose (\w+)\[',text,re.M):
    if sym not in a:continue
    aa,n=a[sym];bb,nn=b[sym]
    assert sym in indexed["poses"] or nn==n or (nn==0 and shared and shared['cpu']<=bb and bb+n<=shared['cpu']+shared['size'])
    for at in range(0,n,12):
     target=bb+(indexed["poses"][sym][at//12]*12 if sym in indexed["poses"] else at)
     op,og,ot,oc,orr=pose(old,aa+at,0x100000);np,ng,nt,nc,nr=pose(new,target,base)
     assert (ot,oc,orr)==(nt,nc,nr) and bool(op)==bool(np),(sym,at,'shape')
     if not op:continue
     expected=read(old,op,ot*32,0x100000)
     if (np&0xfe000000)==0x84000000:
      assert packed
      if packed['codec'].endswith('_byte_lz_fast'):
       from pose_codec_fast import decode
      else:
       from pose_codec import decode
      descriptor=read(new,np&0x00ffffff,packed.get('descriptor_stride',12),base)
      if 'stream_table_cpu' in packed:
       stream=struct.unpack('>I',read(new,packed['stream_table_cpu']+struct.unpack_from('>H',descriptor)[0],4,base))[0]
      else:stream=struct.unpack_from('>I',descriptor)[0]
      if stream&0x80000000:
       physical=stream&0x1ffffff
       assert packed['raw_pose_streams'][str(physical)]==nt*32
       actual=rom[physical:physical+nt*32]
      else:
       colors=stream>>24 if packed.get('color_count_in_address') else 16
       stream=stream&0xffffff if packed.get('color_count_in_address') else stream
       assert 1<=colors<=16
       length=packed['stream_lengths'][str(stream)]
       assert (stream&0x7ffff)+length<=0x80000
       palette=read(new,packed['palette_cpu']+struct.unpack_from('>H',descriptor,2 if 'stream_table_cpu' in packed else 4)[0]*packed.get('palette_stride',1),8,base) if 'palette_cpu' in packed else descriptor[4:]
       canonical=decode(rom[stream:stream+length],nt*32)
       assert all(max(byte>>4,byte&15)<colors for byte in canonical)
       actual=decode(rom[stream:stream+length],nt*32,palette)
     else:actual=rom[np&0x1ffffff:(np&0x1ffffff)+nt*32] if np&0x80000000 else read(new,np,nt*32,base)
     assert expected==actual,(sym,at,'pixels')
     if ng:assert read(old,og,oc*22,0x100000)==read(new,ng,nc*22,base),(sym,at,'general geometry')
     else:assert aa+at in packed_old and target in packed_new,(sym,at,'removed geometry has no packed equivalent')
     count+=1

 return dict(poses=count,packed_geometry=packed_count,background_map_cells=map_cells)
