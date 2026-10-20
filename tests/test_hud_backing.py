#!/usr/bin/env python3
"""Compare all ten static scene entries before/after a HUD backing correction.

GX diagnostic entry calls load actual stage/HUD data. Region copies change only
header bytes; compare world pixels exactly and permit only blackening within the HUD. This is
not a physical PAL-clone test or a gameplay playthrough.
"""
import argparse,concurrent.futures,hashlib,json,re,struct,subprocess,sys,tempfile
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser(description=__doc__)
for n in ('before','after','core','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
before_rom=(a.before/'RollingThunderMD.bin').read_bytes();after_rom=(a.after/'RollingThunderMD.bin').read_bytes()
for atlas in (Path(__file__).resolve().parents[1]/'src').glob('hud??_data.c'):
 words=[int(v,0) for v in re.search(r'patterns\[\]=\{(.*?)\}',atlas.read_text())[1].split(',') if v]
 assert all((word>>shift)&15 for word in words for shift in range(0,32,4)), atlas
 old=[sum((0 if (word>>shift)&15==1 else (word>>shift)&15)<<shift for shift in range(0,32,4)) for word in words]
 assert struct.pack('>'+str(len(words))+'I',*words) in after_rom, atlas
 assert struct.pack('>'+str(len(old))+'I',*old) in before_rom, atlas

with tempfile.TemporaryDirectory() as temp:
 root=Path(temp)
 for name,folder in [('before',a.before),('after',a.after)]:
  for region in ('U','E'):
   out=root/name/region;out.mkdir(parents=True)
   raw=bytearray((folder/'RollingThunderMD.bin').read_bytes());raw[0x1f0:0x200]=region.encode().ljust(16,b' ')
   (out/'RollingThunderMD.bin').write_bytes(raw)
 def capture(job):
  name,region,stage=job;folder=a.before if name=='before' else a.after;out=root/name/region
  cmd=[sys.executable,str(Path(__file__).with_name('test_banked_scenes.py')),'--rom',str(out/'RollingThunderMD.bin'),'--symbols',str(folder/'RollingThunderMD.symbols.txt'),'--core',str(a.core),'--stage',str(stage),'--capture-dir',str(out),'--players','2']
  r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
  list(pool.map(capture,[(n,r,s) for n in ('before','after') for r in ('U','E') for s in range(10)]))
 rows=[]
 for region in ('U','E'):
  for stage in range(10):
   images=[np.frombuffer((root/n/region/f'{stage}-stage.raw').read_bytes(),dtype='<u2').reshape(224,320) for n in ('before','after')]
   before,after=images
   assert np.array_equal(before[24:216],after[24:216]),(region,stage,'world changed')
   hud=np.zeros((224,320),dtype=bool);hud[:24]=True;hud[216:]=True
   assert np.all(after[:24,:16]==0) and np.all(after[216:,:16]==0),(region,stage,'HUD margin not black')
   # Transparent backing exposed both the backdrop and terrain behind it.
   # Every changed pixel must become black; glyph preservation is additionally
   # checked against the linked atlas words below.
   changed=hud&(before!=after)
   assert np.all(after[changed]==0),(region,stage,'non-black HUD change')
   rows.append(dict(region=region,stage=stage,world_exact=True,only_blackening=True,backing_black=True,corrected_pixels=int(np.count_nonzero(before!=after))))
   if region=='E' and stage in (0,1):
    for name,pixels in zip(('before','after'),images):
     rgb=np.stack([((pixels>>11)&31)*255//31,((pixels>>5)&63)*255//63,(pixels&31)*255//31],axis=-1).astype('uint8')
     Image.fromarray(rgb).resize((640,448),Image.Resampling.NEAREST).save(a.output/f'pal-stage{stage+1}-{name}.png')
 report=dict(before_sha256=hashlib.sha256((a.before/'RollingThunderMD.bin').read_bytes()).hexdigest(),after_sha256=hashlib.sha256((a.after/'RollingThunderMD.bin').read_bytes()).hexdigest(),cases=rows,scope=__doc__)
 (a.output/'hud.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
