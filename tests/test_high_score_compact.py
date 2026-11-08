#!/usr/bin/env python3
"""Frozen high-score round-trip plus production panel/DMA equivalence."""
import argparse,json,os,struct,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from packed_high_score import pack
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
j=json.loads((root/'assets/ui/high_score.json').read_text())
raw=b''.join(struct.pack('>'+str(len(j[k]))+'H',*j[k]) for k in ('scene','panel','maps'))
packed,report=pack(raw)
assert report['reference_sha256']==report['restored_sha256']
a.output.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(dir=a.output.parent) as folder:
 folder=Path(folder);(folder/'old.bin').write_bytes(raw);(folder/'packed.bin').write_bytes(packed)
 exe=folder/'check'
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-Wno-pointer-to-int-cast','-I'+str(root/'tests/host'),'-I'+str(root/'inc'),str(root/'tests/test_high_score_compact.c'),'-o',str(exe)],check=True)
 result=subprocess.check_output([str(exe),str(folder/'old.bin'),str(folder/'packed.bin')],text=True)
report['production_panel_checks']=result.strip();report['passed']=True
a.output.write_text(json.dumps(report,indent=2)+'\n');print(result.strip())
