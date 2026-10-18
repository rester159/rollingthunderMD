#!/usr/bin/env python3
"""Build the native port from source plus an owner-supplied arcade ROM set."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from zipfile import ZipFile
import argparse,hashlib,importlib.util,json,os,shutil,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
PAGE=0x80000

def digest(data):return hashlib.sha256(data).hexdigest()
def load(name,path):
 spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
def run(args,**kwargs):
 result=subprocess.run([str(x) for x in args],capture_output=True,text=True,**kwargs)
 if result.returncode:raise RuntimeError('Command failed: '+' '.join(map(str,args))+'\n'+result.stdout+'\n'+result.stderr)
 return result.stdout

def build_audio(game,rom,audio,cc,cxx):
 print('Extracting and converting owner-supplied arcade audio...',flush=True)
 audio.mkdir(parents=True,exist_ok=True);(game/'assets/extracted/logic').mkdir(parents=True,exist_ok=True)
 (game/'out/research').mkdir(parents=True,exist_ok=True)
 link=game/'rthunder.zip';link.unlink(missing_ok=True);link.symlink_to(rom)
 extractor=load('extract_assets',ROOT/'tools/extract_assets.py')
 with ZipFile(rom) as archive:
  for name,expected in extractor.EXPECTED_SHA1.items():
   raw=archive.read(name)
   if hashlib.sha1(raw).hexdigest()!=expected:raise ValueError('Unsupported arcade ROM chip: '+name)
  samples=[archive.read(n) for n in extractor.SAMPLE_ROMS]
 voices=audio/'voices';voices.mkdir(exist_ok=True);manifest=extractor.decode_samples(samples,voices);(voices/'manifest.json').write_text(json.dumps(manifest))
 run([sys.executable,ROOT/'tools/audio/build_voice_z80_blob.py',voices,game/'src/voice_z80_blob.c'])
 over=load('extract_game_over_music',ROOT/'tools/extract_game_over_music.py');over.ROOT=game
 over.extract()
 graph=load('extract_music_graph',ROOT/'tools/extract_music_graph.py');graph.root=game
 records,_=graph.extract();(game/'assets/extracted/logic/music_graph.json').write_text(json.dumps(records))
 bank=load('build_music_event_bank',ROOT/'tools/build_music_event_bank.py');bank.root=game
 (game/'src/arcade_music_bank.c').write_text(bank.build_all());(game/'src/final_music_bank.c').write_text(bank.build_all((7,9,11),'rtFinalMusicBank'))
 (audio/'genesis.h').write_text('#pragma once\n#include <stdint.h>\n#include <stdbool.h>\ntypedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef int8_t s8;typedef int16_t s16;typedef int32_t s32;\n#define TRUE true\n#define FALSE false\n')
 flags=['-O2','-I'+str(audio),'-I'+str(game/'inc')]
 maps=(game/'inc/rt_game_music_maps.h').read_text();(audio/'rt_game_music_maps_rows.h').write_text(maps.split('={',1)[1].rsplit('};',1)[0]);packer=audio/'pack_music';run([cc,*flags,ROOT/'tools/audio/build_music_z80_blob.c',game/'src/arcade_music_bank.c','-lm','-o',packer]);run([packer,game/'src/music_z80_blob.c',game/'inc/rt_game_music_maps.h'])
 outputs={};objects=[]
 for name in ['music_sequence','music_control','music_clock','music_registers']:
  obj=audio/(name+'.o');run([cc,*flags,'-c',game/'src'/(name+'.c'),'-o',obj]);objects.append(obj)
 renderers={}
 for kind in ['arcade','final']:
  obj=audio/(kind+'.o');run([cc,*flags,'-DrtFinalMusicBank=rtArcadeMusicBank','-c',game/'src'/(kind+'_music_bank.c'),'-o',obj]);exe=audio/('render_'+kind);run([cxx,'-std=c++17',*flags,'-I'+str(ROOT/'vendor/ymfm'),ROOT/'tools/audio/render_finite_music.cpp',ROOT/'vendor/ymfm/ymfm_opm.cpp',*objects,obj,'-o',exe]);renderers[kind]=exe
 import numpy as np
 resample=load('resample',ROOT/'tools/audio/resample.py').resample
 clips=[('victory','arcade',7,None,[],51,'8a800de7f2921d1c775e83fe03b462756f883379f56ab9940cc95b1750c18c7c'),('ending','arcade',9,2247,[(7,295)],52,'2edc66bb661ff61992a637edc16b9823aec47b365e8c075804b3f84e7e85af70'),('high_cold','arcade',11,4300,[],55,'5f5623f8e2b6efd302218388f9e5c3b3aeb885533d6df30b10e8518dd15f3076'),('high_warm','final',11,4300,[(7,294),(0,1),(9,2246)],58,'2c8a655c1e42b81f7152654151f66bc4442933b50314cddbe7ebb4023d989e4d')]
 for name,kind,track,ticks,history,page,expected in clips:
  raw=audio/(name+'.raw');cmd=[renderers[kind],raw,track]
  if ticks:cmd.append(ticks)
  for command,count in history:cmd.extend([command,count])
  state=json.loads(run(cmd));mono=np.fromfile(raw,dtype='<i2').reshape(-1,2)[:,1].astype(float)*0.60
  values=np.rint(resample(mono,53693175/3405,start_sample=state['warm_prefix_samples'])/256)
  if np.any((values<-128)|(values>127)):raise ValueError('PCM clipping: '+name)
  data=(values+128).astype('u1').tobytes()
  if expected and digest(data)!=expected:raise ValueError('PCM reproduction differs: '+name+' '+digest(data))
  (audio/(name+'.pcm')).write_bytes(data);outputs[page]=data+bytes([128])*16
  print('  '+name+': '+str(len(data))+' samples, '+digest(data),flush=True)
 return outputs

def build_stage(row,sdk,build,includes,jobs):
 folder=ROOT/row['folder'];out=build/'stages'/str(row['stage']);out.mkdir(parents=True,exist_ok=True)
 def compile_one(name):
  target=out/(Path(name).stem+'.o');run([sdk/'bin/m68k-elf-gcc','-m68000','-Os','-fno-builtin','-ffunction-sections','-fdata-sections','-c',folder/name,'-I'+str(folder),'-I'+str(includes),'-isystem'+str(sdk/'inc'),'-o',target]);return target
 with ThreadPoolExecutor(max_workers=jobs) as pool:objects=list(pool.map(compile_one,row['sources']))
 run([sdk/'bin/m68k-elf-ld','-T',folder/'bank.ld','--gc-sections',*objects,'-o',out/'bank.elf']);run([sdk/'bin/m68k-elf-objcopy','-O','binary',out/'bank.elf',out/'bank.bin']);data=(out/'bank.bin').read_bytes()
 if len(data)>6*PAGE:raise ValueError('Stage exceeds its mapper window')
 # Exact bank bytes pin source selection, linker order and current toolchain.
 if digest(data)!=row['sha256']:raise ValueError('Stage '+str(row['stage'])+' differs from frozen bank: '+digest(data))
 print('Stage '+str(row['stage']+1)+'/10 reproduced',flush=True);return data

def build_completion(sdk,build,inc):
 folder=ROOT/'assets/completion';out=build/'completion';out.mkdir(parents=True,exist_ok=True);objects=[]
 for name in ['scene_background','scene_art','transition_art','completion_monitor_data']:
  obj=out/(name+'.o');run([sdk/'bin/m68k-elf-gcc','-m68000','-Os','-fno-common','-c',folder/(name+'.c'),'-I'+str(inc),'-isystem'+str(sdk/'inc'),'-o',obj]);objects.append(obj)
 run([sdk/'bin/m68k-elf-ld','-T',folder/'bank.ld',*objects,'-o',out/'bank.elf']);run([sdk/'bin/m68k-elf-objcopy','-O','binary',out/'bank.elf',out/'bank.bin']);data=bytearray((out/'bank.bin').read_bytes());patch=json.loads((folder/'ending_footer.json').read_text())
 if digest(data)!=patch['base_sha256']:raise ValueError('Completion bank layout differs')
 data+=struct.pack('>'+str(len(patch['extra_words']))+'H',*patch['extra_words']);struct.pack_into('>IIIHHH',data,patch['descriptor_offset'],*patch['descriptor']);struct.pack_into('>H',data,patch['actor_upper_offset'],patch['actor_upper'])
 if digest(data)!=patch['sha256']:raise ValueError('Completion footer differs')
 return data

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',required=True,type=Path,help='Owner-supplied Rolling Thunder rev. 3 arcade ZIP');p.add_argument('--sgdk',type=Path,default=os.environ.get('GDK'));p.add_argument('--cart',choices=['standard','mega-everdrive-pro','mega-everdrive-v2-x7'],default='standard');p.add_argument('--output-dir',type=Path,default=ROOT/'dist');p.add_argument('--layout',choices=['compact','shared','packed','frozen'],default='compact');p.add_argument('--jobs',type=int,default=min(8,os.cpu_count() or 1));p.add_argument('--pose-codec',choices=['canonical','fast','refined'],default='canonical',help='Experimental packed-layout sprite codec');a=p.parse_args()
 if not a.rom.is_file():p.error('ROM archive not found; supply your own rthunder.zip')
 if not a.sgdk or not (a.sgdk/'makefile.gen').is_file():p.error('Pass --sgdk PATH or set GDK to an SGDK 2.12 installation')
 if a.pose_codec!='canonical' and a.layout!='packed':p.error('--pose-codec requires --layout packed')
 if a.jobs<1:p.error('--jobs must be positive')
 if a.cart!='standard' and a.layout=='frozen':p.error('Extended-SSF profiles require a compact layout')
 if a.cart=='mega-everdrive-v2-x7' and a.layout not in ('shared','packed'):p.error('Mega EverDrive v2/X7 requires --layout shared to reserve OS banks 30-31')
 rom=a.rom.resolve();sdk=a.sgdk.resolve();build=ROOT/'.build';game=build/'game';game.mkdir(parents=True,exist_ok=True)
 for name in ['src','inc','res']:shutil.copytree(ROOT/name,game/name,dirs_exist_ok=True)
 (game/'tools').mkdir(exist_ok=True)
 for name in ['cart_boot.py','runtime.mk']:shutil.copy2(ROOT/'tools'/name,game/'tools'/name)
 assets=game/'assets';assets.mkdir(exist_ok=True)
 for name,data in json.loads((ROOT/'assets/ui/resources.json').read_text()).items():(assets/name).write_bytes(bytes.fromhex(data))
 audio=build_audio(game,rom,build/'audio',os.environ.get('CC','cc'),os.environ.get('CXX','c++'))
 rows=json.loads((ROOT/'assets/stages/manifest.json').read_text());images=[build_stage(row,sdk,build,game/'inc',a.jobs) for row in rows]
 completion=build_completion(sdk,build,game/'inc')
 high=json.loads((ROOT/'assets/ui/high_score.json').read_text());high_data=b''.join(struct.pack('>'+str(len(high[n]))+'H',*high[n]) for n in ['scene','panel','maps'])
 compact_report=None
 if a.layout in ('compact','shared','packed'):
  import compact_build
  chunks,compact_report=compact_build.prepare(ROOT,sdk,build,game,rows,images,completion,high_data,audio,a.jobs,run,shared=a.layout in ('shared','packed'),packed=a.layout=='packed',packed_codec=a.pose_codec)
 layout=game/'inc/rt_rom_layout.h';text=layout.read_text()
 import re
 text=re.sub(r'^#define RT_CART_EXTENDED_SSF .*\n','',text,flags=re.M)
 text=text.replace('#define RT_ROM_LAYOUT_H','#define RT_ROM_LAYOUT_H\n#define RT_CART_EXTENDED_SSF '+str(int(a.cart!='standard')))
 layout.write_text(text)
 pcm_check=build/'test-pcm-layout'
 run([os.environ.get('CC','cc'),'-std=c99','-I'+str(ROOT/'tests/host'),'-I'+str(game/'inc'),ROOT/'src/pcm_rom_plan.c',ROOT/'src/pcm_segmented_plan.c',ROOT/'tests/test_pcm_layout.c','-o',pcm_check])
 print(run([pcm_check]).strip(),flush=True)
 print('Compiling the Mega Drive runtime...',flush=True)
 env=dict(os.environ,GDK=str(sdk));result=subprocess.run(['make','-j'+str(a.jobs),'-f',str(sdk/'makefile.gen'),'-f',str(ROOT/'tools/runtime.mk'),'release'],cwd=game,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(build/'compile.log').write_text(result.stdout)
 if result.returncode:raise RuntimeError(result.stdout[-12000:])
 symbols={v[2]:int(v[0],16) for line in (game/'out/release/symbol.txt').read_text().splitlines() if len(v:=line.split())==3}
 if symbols['__wrap__reset_entry']>=0x80000:raise ValueError('Soft reset must execute in fixed mapper window 0')
 if (symbols['_bend']&0xffff)>=0xf000:raise ValueError('Soft reset needs stack space above C globals')
 span=symbols['_stext']+symbols['_sdata'];fixed=(game/'out/release/rom.bin').read_bytes()
 if span>0x100000 or len(fixed)>0x100000:raise ValueError('Fixed code overlaps banked graphics')
 if compact_report and span>compact_build.HIGH_ADDRESS:raise ValueError('Fixed code overlaps compact UI/audio storage')
 raw=bytearray(compact_report['rom_bytes'] if compact_report else 0x2000000);raw[:span]=fixed[:span]
 occupied=[(0,span)]
 def place_at(start,data):
  if any(start<end and start+len(data)>begin for begin,end in occupied):raise ValueError('Overlapping ROM bank at '+str(start))
  if start+len(data)>len(raw):raise ValueError('ROM overflow')
  raw[start:start+len(data)]=data
  occupied.append((start,start+len(data)))
 if compact_report:
  for start,data in chunks:place_at(start,data)
 else:
  for row,data in zip(rows,images):place_at(row['page']*PAGE,data)
  place_at(50*PAGE,completion);place_at(54*PAGE,high_data)
  for page,data in audio.items():place_at(page*PAGE,data)
 raw[0x100:0x110]=b'SEGA SSF        ' if a.cart!='standard' else b'SEGA MEGA DRIVE ';raw[0x1a4:0x1a8]=struct.pack('>I',len(raw)-1);raw[0x1b0:0x1bc]=b'RA'+struct.pack('>HII',0xf820,0x240001 if a.cart!='standard' else 0x200001,0x24ffff if a.cart!='standard' else 0x20ffff);raw[0x18e:0x190]=struct.pack('>H',sum(struct.unpack('>'+str((len(raw)-512)//2)+'H',raw[512:]))&65535)
 if compact_report:
  from verify_compact import verify
  compact_report['verified_art']=verify(ROOT,sdk,run,rows,compact_report['stages'],raw,compact_report.get('shared_metadata'),compact_report.get('packed_patterns'))
  if compact_report.get('streamed_audio'):
   from pcm_stream import unpack
   for name,clip in compact_report['streamed_audio'].items():
    address=compact_report['audio_addresses'][name]
    decoded=unpack(raw[address:address+clip['packed_bytes']],clip['samples'],delta=clip.get('delta',False),phrases=clip.get('phrases'))
    if digest(decoded)!=clip['sha256']:raise ValueError('Assembled PCM differs: '+name)
   compact_report['verified_audio_samples']=sum(x['samples'] for x in compact_report['streamed_audio'].values())
 dist=a.output_dir;dist.mkdir(parents=True,exist_ok=True);names={'standard':('RollingThunderMD.bin','build.json'),'mega-everdrive-pro':('RollingThunderMD-MegaEverDrivePro.bin','build-mega-everdrive-pro.json'),'mega-everdrive-v2-x7':('RollingThunderMD-MegaEverDriveV2-X7.bin','build-mega-everdrive-v2-x7.json')}
 dest=dist/names[a.cart][0];dest.write_bytes(raw)
 shutil.copy2(game/'out/release/symbol.txt',dest.with_suffix('.symbols.txt'))
 report=dict(sha256=digest(raw),artifact=dest.name,cartridge_profile=a.cart,bytes=len(raw),fixed_span=span,sgdk_version='2.12',compiler=run([sdk/'bin/m68k-elf-gcc','-dumpfullversion']).strip(),stage_banks_reproduced=10,layout=a.layout,compact=compact_report,boot_intros=False,rom_archives_distributed=False)
 report['experimental_layout']=a.layout in ('shared','packed');report['physical_hardware_tested']=False
 (dist/names[a.cart][1]).write_text(json.dumps(report,indent=2)+'\n')
 sums=dist/'SHA256SUMS';lines=sums.read_text().splitlines() if sums.exists() else []
 lines=[line for line in lines if not line.endswith('  '+dest.name)]+[report['sha256']+'  '+dest.name]
 sums.write_text('\n'.join(sorted(lines))+'\n');print(str(dest)+'\nSHA256 '+report['sha256'],flush=True)
if __name__=='__main__':
 try:main()
 except (RuntimeError,ValueError,KeyError,FileNotFoundError) as e:raise SystemExit(str(e))
