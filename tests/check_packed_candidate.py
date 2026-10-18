#!/usr/bin/env python3
from pathlib import Path
import sys,subprocess,json,hashlib,concurrent.futures
import argparse
p=argparse.ArgumentParser(description="Validate an assembled packed candidate without modifying its ROM.")
p.add_argument('--rom',type=Path,required=True)
p.add_argument('--gx',type=Path,required=True)
p.add_argument('--picodrive',type=Path,required=True)
p.add_argument('--trace-core',type=Path,required=True)
p.add_argument('--reference-scenes',type=Path,required=True)
p.add_argument('--output',type=Path,default=Path('.build/seven-mib-validation'))
a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
rom=a.rom.resolve();symbols=rom.with_suffix('.symbols.txt');layout=rom.parent/'build.json'
gx=a.gx.resolve();pico=a.picodrive.resolve();diagnostic=a.trace_core.resolve()
sha=hashlib.sha256(rom.read_bytes()).hexdigest();base=['--rom',rom,'--symbols',symbols]
jobs=[]
def add(name,script,args):jobs.append((name,[sys.executable,script,*args,'--output',out/(name+'.json')]))
for clip in ['victory','ending','high_cold','high_warm']:
 add('audio-'+clip,'experiments/everdrive_streaming/check_audio_stream.py',[*base,'--core',diagnostic,'--layout',layout,'--clip',clip,*(['--scene'] if clip!='high_warm' else [])])
for mode,clip in [('rearm','high_cold'),('replace','high_warm')]:add('audio-'+mode,'experiments/everdrive_streaming/check_audio_stream.py',[*base,'--core',diagnostic,'--layout',layout,'--clip',clip,'--transition',mode])
add('scenes','tests/test_banked_scenes.py',[*base,'--core',gx,'--audio-soak'])
add('resets','tests/test_soft_reset.py',[*base,'--core',gx])
add('cold-save','tests/test_cold_save.py',[*base,'--core',gx])
add('boot','tests/check_compatibility.py',[*base,'--gx',gx,'--picodrive',pico])
for case in ['campaign_1p','campaign_2p','survival_2p']:
 add(case,'tests/measure_rendering.py',[*base,'--core',gx,'--case',case])
add('leila','tests/measure_rendering.py',[*base,'--core',gx,'--case','campaign_1p','--agent',1])
add('jump','tests/measure_rendering.py',[*base,'--core',gx,'--case','campaign_1p','--input-pattern','jump'])
add('aligned-campaign_1p','tests/measure_rendering.py',[*base,'--core',gx,'--case','campaign_1p','--align-update',512])
def run(job):
 name,cmd=job;p=subprocess.run(list(map(str,cmd)),cwd=root,capture_output=True,text=True,timeout=240)
 (out/(name+'.log')).write_text(p.stdout+p.stderr)
 assert p.returncode==0,name+': '+(p.stdout+p.stderr)[-3000:]
 report=json.loads((out/(name+'.json')).read_text());assert report['rom_sha256']==sha
 print(name+' PASS',flush=True);return name,report
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 results=dict(pool.map(run,jobs))
reference=json.loads(a.reference_scenes.read_text())
assert len(results['scenes']['cases'])==len(reference['cases'])==10
for new,old in zip(results['scenes']['cases'],reference['cases']):
 for key in ['stage','stage_frame_sha256','high_score_frame_sha256']:assert new[key]==old[key],(key,new['stage'])
assert hashlib.sha256(rom.read_bytes()).hexdigest()==sha
(out/'summary.json').write_text(json.dumps(dict(rom_sha256=sha,passed=list(results),scene_frames_match_accepted=20,reference_scene_rom_sha256=reference['rom_sha256'],physical_hardware_tested=False),indent=2)+'\n')
print('ALL CHECKS PASS '+sha)
