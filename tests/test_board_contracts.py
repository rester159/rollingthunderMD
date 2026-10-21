#!/usr/bin/env python3
"""Arcade byte contracts and native collision/door regressions for v0.91."""
import argparse,hashlib,json,re,subprocess,tempfile
from pathlib import Path
from zipfile import ZipFile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--arcade-rom',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
with ZipFile(a.arcade_rom) as z:cpu1=z.read('rt3_1b.9c');cpu2=z.read('rt3_2b.12c')
assert hashlib.sha1(cpu1).hexdigest()=='cb92709e94eb273b3ce44c55cd252170ad1017f4'
assert hashlib.sha1(cpu2).hexdigest()=='52e8757aacb4e01f8432125729e2323c48ebc4f5'
# The material decoder alone returns multiple flags. Its bullet caller masks01.
assert cpu2[0x5421:0x5429]==bytes.fromhex('cc01018d2fc40139')
assert cpu1[0x3bc1:0x3bd1]==bytes.fromhex('1948224824242528262427282c242d2c')
rows=[]
with tempfile.TemporaryDirectory() as folder:
 for name,sources in [('projectile_materials',['projectiles','projectile_terrain','projectile_pass','generated_projectile_data']),('type20_wall',['type20_walk','type20_entry','high_enemy_motion','high_animation'])]:
  exe=Path(folder)/name
  subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'inc'),str(root/f'tests/test_{name}.c'),*[str(root/f'src/{n}.c') for n in sources],'-o',str(exe)],check=True)
  rows.append(subprocess.check_output([exe],text=True).strip())
assets=[]
for file in sorted((root/'assets/stages').glob('*/*display_data.c')):
 text=file.read_text()
 def words(n):return [int(v,16) for v in re.findall(r'0x([0-9a-fA-F]+)',re.search(r'\b'+n+r'\[\]=\{(.*?)\n\};',text,re.S)[1])]
 patterns,palette=words('patterns'),words('palettes');cleared=0
 for kind in range(3):
  # These six cave rectangles contain authored texture. The remaining24
  # were transparent clears and must now contain only opaque black pixels.
  if file.parent.name in ('0_2','0_3','1_2') and kind<2:continue
  for attr in words(f'door{kind}_1'):
   assert attr&2047
   for word in patterns[(attr&2047)*8:(attr&2047)*8+8]:
    for shift in range(0,32,4):
     pen=(word>>shift)&15;assert pen and palette[((attr>>13)&3)*16+pen]==0,(file,kind,attr,pen)
   cleared+=1
 assets.append(dict(stage=file.parent.name,opaque_black_cells=cleared))
report=dict(status='pass',arcade_bullet_mask_address='CPU2 D426',arcade_wall_table_address='CPU1 BBC1',native_checks=rows,door_interiors=assets)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
